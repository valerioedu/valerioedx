#include <fat32.h>
#include <kio.h>
#include <heap.h>
#include <string.h>

//TODO: Implement a robust mutex implementation

static inode_ops fat32_ops;

typedef struct {
    u8  status;
    u8  chs_start[3];
    u8  type;
    u8  chs_end[3];
    u32 lba_start;
    u32 lba_length;
} __attribute__((packed)) mbr_partition_t;

// Converts a string to uppercase
static void to_upper(char* s) {
    while (*s) {
        if (*s >= 'a' && *s <= 'z') *s -= 32;
        s++;
    }
}

// Converts a string to lowercase
static void to_lower(char* s) {
    while (*s) {
        if (*s >= 'A' && *s <= 'Z') *s += 32;
        s++;
    }
}

// Converts FAT 8.3 name to readable format
static void fat_get_short_name(char* dest, const u8* src) {
    int i, j = 0;
    // Copy Name (up to 8 chars)
    for (i = 0; i < 8 && src[i] != ' '; i++) dest[j++] = src[i];
    
    // Copy Extension (up to 3 chars)
    if (src[8] != ' ') {
        dest[j++] = '.';
        for (i = 8; i < 11 && src[i] != ' '; i++) dest[j++] = src[i];
    }
    dest[j] = '\0';
}

// Calculates checksum for LFN validation
static u8 lfn_checksum(const u8* short_name) {
    u8 sum = 0;
    for (int i = 0; i < 11; i++) {
        sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + short_name[i];
    }
    return sum;
}

// Extracts characters from LFN entry into buffer
static void lfn_extract_chars(const fat_lfn_entry_t* lfn, char* buf, int* pos) {
    u16 chars[13];
    
    // Copy in order: name1[5], name2[6], name3[2]
    for (int i = 0; i < 5; i++) chars[i] = lfn->name1[i];
    for (int i = 0; i < 6; i++) chars[5 + i] = lfn->name2[i];
    for (int i = 0; i < 2; i++) chars[11 + i] = lfn->name3[i];
    
    for (int i = 0; i < 13; i++) {
        if (chars[i] == 0x0000 || chars[i] == 0xFFFF) break;
        // Simple UTF-16 to ASCII (ignores high bytes)
        buf[(*pos)++] = (char)(chars[i] & 0xFF);
    }
}

// Builds complete LFN from consecutive entries
// Returns the number of directory entries consumed
static int build_lfn(fat_dir_entry_t* entries, int start_idx, int max_entries, 
                     char* lfn_buf, u8* expected_checksum) {
    fat_lfn_entry_t* lfn = (fat_lfn_entry_t*)&entries[start_idx];
    
    if (lfn->attr != FAT_ATTR_LFN) return 0;
    
    // Count LFN entries (they appear in reverse order)
    int lfn_count = lfn->order & 0x3F;  // Mask out the "last" flag
    
    if (start_idx + lfn_count >= max_entries) return 0;
    
    *expected_checksum = lfn->checksum;
    memset(lfn_buf, 0, 256);
    
    // LFN entries are stored in reverse order
    // Entry with order N comes first, then N-1, etc.
    for (int i = 0; i < lfn_count; i++) {
        fat_lfn_entry_t* entry = (fat_lfn_entry_t*)&entries[start_idx + i];
        
        if (entry->attr != FAT_ATTR_LFN) break;
        if (entry->checksum != *expected_checksum) break;
        
        int order = entry->order & 0x3F;
        int pos = (order - 1) * 13;  // Position in final string
        
        lfn_extract_chars(entry, lfn_buf, &pos);
    }
    
    return lfn_count;
}

static int read_cluster(fat32_fs_t* fs, u32 cluster, u8* buffer) {
    if (cluster < 2) return -1;

    u32 lba = fs->cluster_begin_lba + (cluster - 2) * fs->sectors_per_cluster;
    return vfs_read(fs->device, lba * 512, fs->bytes_per_cluster, buffer);
}

static int write_cluster(fat32_fs_t* fs, u32 cluster, const u8* buffer) {
    if (cluster < 2) return -1;

    u32 lba = fs->cluster_begin_lba + (cluster - 2) * fs->sectors_per_cluster;
    return vfs_write(fs->device, lba * 512, fs->bytes_per_cluster, (u8*)buffer);
}

static u32 get_next_cluster(fat32_fs_t* fs, u32 current_cluster) {
    u32 fat_offset = current_cluster * 4;
    u32 fat_sector = fs->fat_begin_lba + (fat_offset / 512);
    u32 ent_offset = fat_offset % 512;

    u8 sector_buf[512];
    vfs_read(fs->device, fat_sector * 512, 512, sector_buf);

    u32 next_cluster = *(u32*)&sector_buf[ent_offset];
    return next_cluster & 0x0FFFFFFF; // Mask out top 4 bits
}

static void set_cluster_value(fat32_fs_t* fs, u32 cluster, u32 value) {
    u32 fat_offset = cluster * 4;
    u32 fat_sector = fs->fat_begin_lba + (fat_offset / 512);
    u32 ent_offset = fat_offset % 512;

    u8 sector_buf[512];
    vfs_read(fs->device, fat_sector * 512, 512, sector_buf);

    // Preserve top 4 bits
    u32* entry = (u32*)&sector_buf[ent_offset];
    *entry = (*entry & 0xF0000000) | (value & 0x0FFFFFFF);

    vfs_write(fs->device, fat_sector * 512, 512, sector_buf);
}

static u32 find_free_cluster(fat32_fs_t* fs) {
    u8 sector_buf[512];
    u32 fat_sectors = (fs->total_clusters * 4 + 511) / 512;
    
    for (u32 sector = 0; sector < fat_sectors; sector++) {
        vfs_read(fs->device, (fs->fat_begin_lba + sector) * 512, 512, sector_buf);
        
        for (u32 i = 0; i < 128; i++) {  // 128 entries per sector
            u32 cluster = sector * 128 + i;
            if (cluster < 2) continue;
            
            u32 value = *(u32*)&sector_buf[i * 4] & 0x0FFFFFFF;
            if (value == 0) return cluster;
        }
    }
    return 0;  // No free clusters
}

// Allocate a new cluster and linking it to prev_cluster if provided
static u32 allocate_cluster(fat32_fs_t* fs, u32 prev_cluster) {
    u32 new_cluster = find_free_cluster(fs);
    if (new_cluster == 0) return 0;
    
    // Mark new cluster as end of chain
    set_cluster_value(fs, new_cluster, FAT_EOC);
    
    // Link previous cluster to new one
    if (prev_cluster >= 2)
        set_cluster_value(fs, prev_cluster, new_cluster);
    
    // Zero out the new cluster
    u8* zero_buf = kmalloc(fs->bytes_per_cluster);

    if (!zero_buf) {
        kprintf("[FAT32] [Error] allocate_cluster: kmalloc failed\n");
        set_cluster_value(fs, new_cluster, 0); 
        return 0;
    }
    
    memset(zero_buf, 0, fs->bytes_per_cluster);
    write_cluster(fs, new_cluster, zero_buf);
    kfree(zero_buf);
    
    return new_cluster;
}

u64 fat32_read_file(inode_t* node, u64 offset, u64 size, u8* buffer) {
    fat32_file_t* file = (fat32_file_t*)node->ptr;
    fat32_fs_t* fs = file->fs;

    if (offset >= node->size) return 0;
    if (offset + size > node->size) size = node->size - offset;
    if (size == 0) return 0;

    u32 cluster = file->first_cluster;
    u32 cluster_size = fs->bytes_per_cluster;
    
    // Skip clusters to reach the offset
    u32 clusters_to_skip = offset / cluster_size;
    u32 chunk_offset = offset % cluster_size;

    for (u32 i = 0; i < clusters_to_skip; i++) {
        cluster = get_next_cluster(fs, cluster);
        if (cluster >= FAT_EOC) return 0;   // End of chain
    }

    u64 read = 0;
    u8* cl_buf = kmalloc(cluster_size);
    if (!cl_buf) return 0;

    while (read < size) {
        if (cluster >= FAT_EOC) break;  // End of chain

        read_cluster(fs, cluster, cl_buf);

        u64 to_copy = cluster_size - chunk_offset;
        if (to_copy > (size - read)) to_copy = size - read;

        memcpy(buffer + read, cl_buf + chunk_offset, to_copy);

        read += to_copy;
        chunk_offset = 0; // Only the first cluster has an offset
        
        cluster = get_next_cluster(fs, cluster);
    }

    kfree(cl_buf);
    return read;
}

u64 fat32_write_file(inode_t* node, u64 offset, u64 size, u8* buffer) {
    fat32_file_t* file = (fat32_file_t*)node->ptr;
    fat32_fs_t* fs = file->fs;
    
    if (size == 0) return 0;
    
    u32 cluster_size = fs->bytes_per_cluster;
    u32 cluster = file->first_cluster;
    
    // If file has no clusters yet, allocate first one
    if (cluster < 2) {
        cluster = allocate_cluster(fs, 0);
        if (cluster == 0) return 0;
        file->first_cluster = cluster;
    }
    
    u32 clusters_to_skip = offset / cluster_size;
    u32 chunk_offset = offset % cluster_size;
    u32 prev_cluster = 0;
    
    for (u32 i = 0; i < clusters_to_skip; i++) {
        prev_cluster = cluster;
        u32 next = get_next_cluster(fs, cluster);
        
        if (next >= FAT_EOC) {
            // Need to extend the file
            next = allocate_cluster(fs, cluster);
            if (next == 0) return 0;
        }
        cluster = next;
    }
    
    u64 written = 0;
    u8* cl_buf = kmalloc(cluster_size);
    if (!cl_buf) return 0;
    
    while (written < size) {
        // Read existing cluster data (for partial writes)
        read_cluster(fs, cluster, cl_buf);
        
        u64 to_write = cluster_size - chunk_offset;
        if (to_write > (size - written)) to_write = size - written;
        
        memcpy(cl_buf + chunk_offset, buffer + written, to_write);
        write_cluster(fs, cluster, cl_buf);
        
        written += to_write;
        chunk_offset = 0;
        
        if (written < size) {
            prev_cluster = cluster;
            u32 next = get_next_cluster(fs, cluster);
            
            if (next >= FAT_EOC) {
                next = allocate_cluster(fs, cluster);
                if (next == 0) break;
            }
            cluster = next;
        }
    }
    
    kfree(cl_buf);
    
    // Update file size if extended
    if (offset + written > node->size) {
        node->size = offset + written;
        
        // Update directory entry
        if (file->dir_entry_cluster >= 2) {
            u8* dir_buf = kmalloc(cluster_size);
            read_cluster(fs, file->dir_entry_cluster, dir_buf);
            
            fat_dir_entry_t* entry = (fat_dir_entry_t*)(dir_buf + file->dir_entry_offset);
            entry->file_size = node->size;
            entry->fst_clus_lo = file->first_cluster & 0xFFFF;
            entry->fst_clus_hi = (file->first_cluster >> 16) & 0xFFFF;
            
            write_cluster(fs, file->dir_entry_cluster, dir_buf);
            kfree(dir_buf);
        }
    }
    
    return written;
}

inode_t* fat32_finddir(inode_t* node, const char* name) {
    fat32_file_t* dir_info = (fat32_file_t*)node->ptr;
    fat32_fs_t* fs = dir_info->fs;

    u32 cluster = dir_info->first_cluster;
    u32 cluster_size = fs->bytes_per_cluster;
    u8* buf = kmalloc(cluster_size);
    
    char search_name[256];
    strncpy(search_name, name, 255);
    search_name[255] = '\0';
    to_upper(search_name);

    char lfn_buffer[256];
    u8 lfn_checksum_expected = 0;
    int lfn_valid = 0;

    inode_t* result = NULL;

    while (cluster < FAT_EOC) {
        read_cluster(fs, cluster, buf);

        fat_dir_entry_t* entries = (fat_dir_entry_t*)buf;
        int entries_per_cluster = cluster_size / sizeof(fat_dir_entry_t);

        for (int i = 0; i < entries_per_cluster; i++) {
            fat_dir_entry_t* entry = &entries[i];

            if (entry->name[0] == 0x00) goto done;  // End of directory
            if (entry->name[0] == 0xE5) {           // Deleted entry
                lfn_valid = 0;
                continue;
            }

            // Check for LFN entry
            if (entry->attr == FAT_ATTR_LFN) {
                fat_lfn_entry_t* lfn = (fat_lfn_entry_t*)entry;
                
                if (lfn->order & 0x40) {
                    // Start of new LFN sequence
                    memset(lfn_buffer, 0, sizeof(lfn_buffer));
                    lfn_checksum_expected = lfn->checksum;
                    lfn_valid = 1;
                }
                
                if (lfn_valid && lfn->checksum == lfn_checksum_expected) {
                    int order = lfn->order & 0x3F;
                    int pos = (order - 1) * 13;
                    lfn_extract_chars(lfn, lfn_buffer, &pos);
                }

                continue;
            }

            char entry_name[256];
            
            if (lfn_valid && lfn_checksum(entry->name) == lfn_checksum_expected) {
                strncpy(entry_name, lfn_buffer, 255);
            } else {
                fat_get_short_name(entry_name, entry->name);
            }
            
            lfn_valid = 0;  // Reset for next entry
            
            // Case-insensitive comparison
            char upper_entry[256];
            strncpy(upper_entry, entry_name, 255);
            to_upper(upper_entry);

            if (strcmp(search_name, upper_entry) == 0) {
                result = kmalloc(sizeof(inode_t));
                memset(result, 0, sizeof(inode_t));
                strncpy(result->name, entry_name, sizeof(result->name) - 1);
                
                result->flags = (entry->attr & FAT_ATTR_DIRECTORY) ? FS_DIRECTORY : FS_FILE;
                result->size = entry->file_size;
                result->ops = &fat32_ops;

                // Setup private data
                fat32_file_t* file_data = kmalloc(sizeof(fat32_file_t));
                file_data->fs = fs;
                file_data->first_cluster = (entry->fst_clus_hi << 16) | entry->fst_clus_lo;
                file_data->parent_cluster = dir_info->first_cluster;
                file_data->dir_entry_cluster = cluster;
                file_data->dir_entry_offset = i * sizeof(fat_dir_entry_t);
                
                result->ptr = file_data;
                goto done;
            }
        }
        cluster = get_next_cluster(fs, cluster);
    }

done:
    kfree(buf);
    return result;
}

// Generate a valid 8.3 short name from a long name
static void generate_short_name(const char* long_name, u8* short_name, int suffix) {
    memset(short_name, ' ', 11);
    
    const char* dot = NULL;
    for (const char* p = long_name; *p; p++) 
        if (*p == '.') dot = p;
    
    // Copy base name (up to 6 chars if we need a numeric tail)
    int base_len = dot ? (dot - long_name) : strlen(long_name);
    int max_base = (suffix > 0) ? 6 : 8;
    
    int j = 0;
    for (int i = 0; i < base_len && j < max_base; i++) {
        char c = long_name[i];
        if (c == ' ' || c == '.') continue;
        if (c >= 'a' && c <= 'z') c -= 32;  // To uppercase

        // Replace invalid chars with underscore
        if (c < 0x20 || strchr("\"*+,/:;<=>?[\\]|", c)) c = '_';
        short_name[j++] = c;
    }
    
    // Add numeric suffix if needed (~1, ~2, etc.)
    if (suffix > 0) {
        char suffix_str[8];
        int suffix_len = 0;
        int temp = suffix;
        while (temp > 0) {
            suffix_str[suffix_len++] = '0' + (temp % 10);
            temp /= 10;
        }
        
        // Make room for ~N
        if (j > 8 - suffix_len - 1) j = 8 - suffix_len - 1;
        short_name[j++] = '~';

        for (int i = suffix_len - 1; i >= 0; i--)
            short_name[j++] = suffix_str[i];
    }
    
    // Copy extension
    if (dot && *(dot + 1)) {
        int k = 8;
        for (const char* p = dot + 1; *p && k < 11; p++) {
            char c = *p;
            if (c >= 'a' && c <= 'z') c -= 32;
            if (c < 0x20 || strchr("\"*+,/:;<=>?[\\]|.", c)) c = '_';
            short_name[k++] = c;
        }
    }
}

// Check if a short name already exists in directory
static int short_name_exists(fat32_fs_t* fs, u32 dir_cluster, const u8* short_name) {
    u32 cluster = dir_cluster;
    u32 cluster_size = fs->bytes_per_cluster;
    u8* buf = kmalloc(cluster_size);
    
    while (cluster < FAT_EOC) {
        read_cluster(fs, cluster, buf);
        fat_dir_entry_t* entries = (fat_dir_entry_t*)buf;
        int entries_per_cluster = cluster_size / sizeof(fat_dir_entry_t);
        
        for (int i = 0; i < entries_per_cluster; i++) {
            if (entries[i].name[0] == 0x00) {
                kfree(buf);
                return 0;
            }

            if (entries[i].name[0] == 0xE5) continue;
            if (entries[i].attr == FAT_ATTR_LFN) continue;
            
            if (memcmp(entries[i].name, short_name, 11) == 0) {
                kfree(buf);
                return 1;
            }
        }
        cluster = get_next_cluster(fs, cluster);
    }
    
    kfree(buf);
    return 0;
}

static void generate_unique_short_name(fat32_fs_t* fs, u32 dir_cluster, 
                                        const char* long_name, u8* short_name) {
    generate_short_name(long_name, short_name, 0);
    
    if (!short_name_exists(fs, dir_cluster, short_name)) return;
    
    for (int i = 1; i < 999999; i++) {
        generate_short_name(long_name, short_name, i);
        if (!short_name_exists(fs, dir_cluster, short_name)) return;
    }
}

static int lfn_entries_needed(const char* name) {
    int len = strlen(name);
    return (len + 12) / 13;  // Ceiling division by 13
}

// Check if name doesn't fit in 8.3
static int needs_lfn(const char* name) {
    int len = strlen(name);
    if (len > 12) return 1;
    
    const char* dot = strchr(name, '.');
    if (dot) {
        int base_len = dot - name;
        int ext_len = strlen(dot + 1);
        if (base_len > 8 || ext_len > 3) return 1;
        if (strchr(dot + 1, '.')) return 1;  // Multiple dots
    } else {
        if (len > 8) return 1;
    }
    
    // Checks for lowercase or special chars
    for (const char* p = name; *p; p++) {
        if (*p >= 'a' && *p <= 'z') return 1;
        if (*p == ' ') return 1;
    }
    
    return 0;
}

static void fill_lfn_entry(fat_lfn_entry_t* lfn, const char* name, int order, 
                           int is_last, u8 checksum) {
    memset(lfn, 0xFF, sizeof(fat_lfn_entry_t));
    
    lfn->order = order | (is_last ? 0x40 : 0);
    lfn->attr = FAT_ATTR_LFN;
    lfn->type = 0;
    lfn->checksum = checksum;
    lfn->first_cluster = 0;
    
    int start = (order - 1) * 13;
    int len = strlen(name);
    
    // Fill name1 (5 chars)
    for (int i = 0; i < 5; i++) {
        int pos = start + i;
        if (pos < len) lfn->name1[i] = (u16)(u8)name[pos];
        else if (pos == len) lfn->name1[i] = 0x0000;
        else lfn->name1[i] = 0xFFFF;
    }
    
    // Fill name2 (6 chars)
    for (int i = 0; i < 6; i++) {
        int pos = start + 5 + i;
        if (pos < len) lfn->name2[i] = (u16)(u8)name[pos];
        else if (pos == len) lfn->name2[i] = 0x0000;
        else lfn->name2[i] = 0xFFFF;
    }
    
    // Fill name3 (2 chars)
    for (int i = 0; i < 2; i++) {
        int pos = start + 11 + i;
        if (pos < len) lfn->name3[i] = (u16)(u8)name[pos];
        else if (pos == len) lfn->name3[i] = 0x0000;
        else lfn->name3[i] = 0xFFFF;
    }
}

// Finds free directory entries (consecutive slots)
// Returns cluster and offset, or 0 if need to extend directory
static int find_free_dir_entries(fat32_fs_t* fs, u32 dir_cluster, int count,
                                  u32* out_cluster, u32* out_offset) {
    u32 cluster = dir_cluster;
    u32 cluster_size = fs->bytes_per_cluster;
    u8* buf = kmalloc(cluster_size);
    int entries_per_cluster = cluster_size / sizeof(fat_dir_entry_t);
    
    u32 consecutive_start_cluster = 0;
    u32 consecutive_start_offset = 0;
    int consecutive_count = 0;
    
    while (cluster < FAT_EOC) {
        read_cluster(fs, cluster, buf);
        fat_dir_entry_t* entries = (fat_dir_entry_t*)buf;
        
        for (int i = 0; i < entries_per_cluster; i++) {
            if (entries[i].name[0] == 0x00 || entries[i].name[0] == 0xE5) {
                if (consecutive_count == 0) {
                    consecutive_start_cluster = cluster;
                    consecutive_start_offset = i * sizeof(fat_dir_entry_t);
                }
                consecutive_count++;
                
                if (consecutive_count >= count) {
                    *out_cluster = consecutive_start_cluster;
                    *out_offset = consecutive_start_offset;
                    kfree(buf);
                    return 1;
                }
            } else {
                consecutive_count = 0;
            }
        }
        
        u32 next = get_next_cluster(fs, cluster);
        if (next >= FAT_EOC) {
            // Need to extend directory
            next = allocate_cluster(fs, cluster);
            if (next == 0) {
                kfree(buf);
                return 0;
            }
        }
        cluster = next;
    }
    
    kfree(buf);
    return 0;
}

// Writes directory entries starting at given location
static int write_dir_entries(fat32_fs_t* fs, u32 start_cluster, u32 start_offset,
                              fat_dir_entry_t* entries, int count) {
    u32 cluster = start_cluster;
    u32 cluster_size = fs->bytes_per_cluster;
    u8* buf = kmalloc(cluster_size);
    int entries_per_cluster = cluster_size / sizeof(fat_dir_entry_t);
    
    int entry_idx = start_offset / sizeof(fat_dir_entry_t);
    int written = 0;

    while (written < count && cluster < FAT_EOC) {
        read_cluster(fs, cluster, buf);
        fat_dir_entry_t* dir_entries = (fat_dir_entry_t*)buf;
        
        while (entry_idx < entries_per_cluster && written < count) {
            memcpy(&dir_entries[entry_idx], &entries[written], sizeof(fat_dir_entry_t));
            entry_idx++;
            written++;
        }
        
        write_cluster(fs, cluster, buf);
        
        if (written < count) {
            cluster = get_next_cluster(fs, cluster);
            entry_idx = 0;
        }
    }
    
    kfree(buf);
    return written == count;
}

static inode_t* fat32_create_entry(inode_t* parent, const char* name, u8 attr) {
    fat32_file_t* parent_info = (fat32_file_t*)parent->ptr;
    fat32_fs_t* fs = parent_info->fs;

    // Checks if entry already exists
    inode_t* existing = fat32_finddir(parent, name);
    if (existing) {
        kfree(existing->ptr);
        kfree(existing);
        return NULL;  // Already exists
    }
    
    // Generates short name
    u8 short_name[11];
    generate_unique_short_name(fs, parent_info->first_cluster, name, short_name);
    u8 checksum = lfn_checksum(short_name);
    
    int lfn_count = needs_lfn(name) ? lfn_entries_needed(name) : 0;
    int total_entries = lfn_count + 1;  // LFN entries + short name entry
    
    u32 entry_cluster, entry_offset;
    if (!find_free_dir_entries(fs, parent_info->first_cluster, total_entries,
                                &entry_cluster, &entry_offset)) {
        return NULL;
    }
    
    u32 new_cluster = 0;
    if (attr & FAT_ATTR_DIRECTORY) {
        new_cluster = allocate_cluster(fs, 0);
        if (new_cluster == 0) {
            return NULL;
        }
    }
    
    // Builds directory entries
    fat_dir_entry_t* entries = kmalloc(total_entries * sizeof(fat_dir_entry_t));
    memset(entries, 0, total_entries * sizeof(fat_dir_entry_t));
    
    for (int i = 0; i < lfn_count; i++) {
        int order = lfn_count - i;
        fill_lfn_entry((fat_lfn_entry_t*)&entries[i], name, order, 
                       (i == 0), checksum);
    }

    fat_dir_entry_t* short_entry = &entries[lfn_count];
    memcpy(short_entry->name, short_name, 11);
    short_entry->attr = attr;
    short_entry->fst_clus_hi = (new_cluster >> 16) & 0xFFFF;
    short_entry->fst_clus_lo = new_cluster & 0xFFFF;
    short_entry->file_size = 0;
    
    // TODO: Set create/modify timestamps
    
    if (!write_dir_entries(fs, entry_cluster, entry_offset, entries, total_entries)) {
        kfree(entries);
        return NULL;
    }
    
    kfree(entries);
    
    // If creating directory, initialize with . and .. entries
    if (attr & FAT_ATTR_DIRECTORY) {
        u8* dir_buf = kmalloc(fs->bytes_per_cluster);

        if (!dir_buf) {
            kprintf("[ [RFAT32 [W] Allocation fail: dir_buf\n");
            return NULL;
        }

        memset(dir_buf, 0, fs->bytes_per_cluster);
        
        fat_dir_entry_t* dot = (fat_dir_entry_t*)dir_buf;
        memset(dot->name, ' ', 11);
        dot->name[0] = '.';
        dot->attr = FAT_ATTR_DIRECTORY;
        dot->fst_clus_hi = (new_cluster >> 16) & 0xFFFF;
        dot->fst_clus_lo = new_cluster & 0xFFFF;
        
        fat_dir_entry_t* dotdot = (fat_dir_entry_t*)(dir_buf + sizeof(fat_dir_entry_t));
        memset(dotdot->name, ' ', 11);
        dotdot->name[0] = '.';
        dotdot->name[1] = '.';
        dotdot->attr = FAT_ATTR_DIRECTORY;

        u32 parent_clus = parent_info->first_cluster;
        if (parent_clus == fs->root_cluster) {
            parent_clus = 0;
        }

        dotdot->fst_clus_hi = (parent_clus >> 16) & 0xFFFF;
        dotdot->fst_clus_lo = parent_clus & 0xFFFF;
        
        write_cluster(fs, new_cluster, dir_buf);
        kfree(dir_buf);
    }
    
    // Create and return inode
    inode_t* node = kmalloc(sizeof(inode_t));

    if (!node) {
        kfree(node);
        kfree(entries);
        kprintf("[ [RFAT32 [W] Allocation fail: node\n");
        return NULL;
    }

    memset(node, 0, sizeof(inode_t));
    strncpy(node->name, name, sizeof(node->name) - 1);
    node->flags = (attr & FAT_ATTR_DIRECTORY) ? FS_DIRECTORY : FS_FILE;
    node->size = 0;
    node->ops = &fat32_ops;
    
    fat32_file_t* file_data = kmalloc(sizeof(fat32_file_t));

    if (!file_data) {
        kfree(node);
        kfree(entries);
        kfree(file_data);
        kprintf("[ [RFAT32 [W] Allocation fail: node\n");
        return NULL;
    }

    file_data->fs = fs;
    file_data->first_cluster = new_cluster;
    file_data->parent_cluster = parent_info->first_cluster;
    file_data->dir_entry_cluster = entry_cluster;
    file_data->dir_entry_offset = entry_offset + (lfn_count * sizeof(fat_dir_entry_t));
    node->ptr = file_data;
    
    return node;
}

inode_t* fat32_create(inode_t* parent, const char* name) {
    return fat32_create_entry(parent, name, FAT_ATTR_ARCHIVE);
}

inode_t* fat32_mkdir(inode_t* parent, const char* name) {
    return fat32_create_entry(parent, name, FAT_ATTR_DIRECTORY);
}

static void free_cluster_chain(fat32_fs_t* fs, u32 cluster) {
    while (cluster >= 2 && cluster < FAT_EOC) {
        u32 next = get_next_cluster(fs, cluster);
        set_cluster_value(fs, cluster, 0);  // Mark as free
        cluster = next;
    }
}

// Checks if directory is empty (only . and ..)
static int is_directory_empty(fat32_fs_t* fs, u32 dir_cluster) {
    u32 cluster = dir_cluster;
    u32 cluster_size = fs->bytes_per_cluster;
    u8* buf = kmalloc(cluster_size);
    int entries_per_cluster = cluster_size / sizeof(fat_dir_entry_t);
    int entry_count = 0;
    
    while (cluster < FAT_EOC) {
        read_cluster(fs, cluster, buf);
        fat_dir_entry_t* entries = (fat_dir_entry_t*)buf;
        
        for (int i = 0; i < entries_per_cluster; i++) {
            if (entries[i].name[0] == 0x00) {
                kfree(buf);
                return 1;  // End of directory, it's empty
            }

            if (entries[i].name[0] == 0xE5) continue;  // Deleted
            if (entries[i].attr == FAT_ATTR_LFN) continue;  // LFN entry
            
            if (entries[i].name[0] == '.' && 
                (entries[i].name[1] == ' ' || entries[i].name[1] == '.')) 
                continue;
            
            kfree(buf);
            return 0;
        }
        cluster = get_next_cluster(fs, cluster);
    }
    
    kfree(buf);
    return 1;
}

// Finds and mark directory entries as deleted
static int delete_dir_entries(fat32_fs_t* fs, u32 dir_cluster, const char* name) {
    u32 cluster = dir_cluster;
    u32 cluster_size = fs->bytes_per_cluster;
    u8* buf = kmalloc(cluster_size);
    int entries_per_cluster = cluster_size / sizeof(fat_dir_entry_t);
    
    char search_name[256];
    strncpy(search_name, name, 255);
    search_name[255] = '\0';
    to_upper(search_name);
    
    char lfn_buffer[256];
    u8 lfn_checksum_expected = 0;
    int lfn_valid = 0;
    
    // Track LFN entries to delete
    u32 lfn_start_cluster = 0;
    u32 lfn_start_idx = 0;
    
    while (cluster < FAT_EOC) {
        read_cluster(fs, cluster, buf);
        fat_dir_entry_t* entries = (fat_dir_entry_t*)buf;
        int modified = 0;
        
        for (int i = 0; i < entries_per_cluster; i++) {
            if (entries[i].name[0] == 0x00) goto done;
            if (entries[i].name[0] == 0xE5) {
                lfn_valid = 0;
                continue;
            }
            
            if (entries[i].attr == FAT_ATTR_LFN) {
                fat_lfn_entry_t* lfn = (fat_lfn_entry_t*)&entries[i];
                
                if (lfn->order & 0x40) {
                    memset(lfn_buffer, 0, sizeof(lfn_buffer));
                    lfn_checksum_expected = lfn->checksum;
                    lfn_valid = 1;
                    lfn_start_cluster = cluster;
                    lfn_start_idx = i;
                }
                
                if (lfn_valid && lfn->checksum == lfn_checksum_expected) {
                    int order = lfn->order & 0x3F;
                    int pos = (order - 1) * 13;
                    lfn_extract_chars(lfn, lfn_buffer, &pos);
                }

                continue;
            }
            
            char entry_name[256];
            if (lfn_valid && lfn_checksum(entries[i].name) == lfn_checksum_expected) {
                strncpy(entry_name, lfn_buffer, 255);
            } else {
                fat_get_short_name(entry_name, entries[i].name);
                lfn_start_cluster = cluster;
                lfn_start_idx = i;
            }
            
            char upper_entry[256];
            strncpy(upper_entry, entry_name, 255);
            to_upper(upper_entry);
            
            if (strcmp(search_name, upper_entry) == 0) {
                u32 file_cluster = (entries[i].fst_clus_hi << 16) | entries[i].fst_clus_lo;
                
                // Marks this entry as deleted
                entries[i].name[0] = 0xE5;
                modified = 1;
                
                // Marks LFN entries as deleted
                if (lfn_start_cluster == cluster) {
                    for (u32 j = lfn_start_idx; j < (u32)i; j++)
                        entries[j].name[0] = 0xE5;
                        
                } else {
                    // LFN spans clusters - need to handle separately
                    u8* lfn_buf = kmalloc(cluster_size);
                    read_cluster(fs, lfn_start_cluster, lfn_buf);
                    fat_dir_entry_t* lfn_entries = (fat_dir_entry_t*)lfn_buf;

                    for (u32 j = lfn_start_idx; j < (u32)entries_per_cluster; j++)
                        lfn_entries[j].name[0] = 0xE5;

                    write_cluster(fs, lfn_start_cluster, lfn_buf);
                    kfree(lfn_buf);
                }
                
                write_cluster(fs, cluster, buf);
                kfree(buf);
                
                // Frees the cluster chain
                if (file_cluster >= 2)
                    free_cluster_chain(fs, file_cluster);
                
                return 1;
            }
            
            lfn_valid = 0;
        }
        
        if (modified)
            write_cluster(fs, cluster, buf);
        
        cluster = get_next_cluster(fs, cluster);
    }
    
done:
    kfree(buf);
    return 0;
}

int fat32_unlink(inode_t* parent, const char* name) {
    fat32_file_t* parent_info = (fat32_file_t*)parent->ptr;
    fat32_fs_t* fs = parent_info->fs;
    
    // Finds the file first to make sure it exists and is not a directory
    inode_t* target = fat32_finddir(parent, name);
    if (!target) return -1;  // Not found
    
    if (target->flags & FS_DIRECTORY) {
        kfree(target->ptr);
        kfree(target);
        return -1;  // Can't unlink directory
    }
    
    kfree(target->ptr);
    kfree(target);
    
    return delete_dir_entries(fs, parent_info->first_cluster, name) ? 0 : -1;
}

int fat32_rmdir(inode_t* parent, const char* name) {
    fat32_file_t* parent_info = (fat32_file_t*)parent->ptr;
    fat32_fs_t* fs = parent_info->fs;
    
    // Finds the directory
    inode_t* target = fat32_finddir(parent, name);
    if (!target) return -1;
    
    if (!(target->flags & FS_DIRECTORY)) {
        kfree(target->ptr);
        kfree(target);
        return -1;  // Not a directory
    }
    
    fat32_file_t* target_info = (fat32_file_t*)target->ptr;
    
    // Check if empty
    if (!is_directory_empty(fs, target_info->first_cluster)) {
        kfree(target->ptr);
        kfree(target);
        return -1;  // Directory not empty
    }
    
    kfree(target->ptr);
    kfree(target);
    
    return delete_dir_entries(fs, parent_info->first_cluster, name) ? 0 : -1;
}

inode_t* fat32_mount(inode_t* device) {
    if (!device) return NULL;

    u8* buf = kmalloc(512);
    if (!buf) return NULL;

    // Read Sector 0
    if (vfs_read(device, 0, 512, buf) != 512) {
        kprintf("[ [CFAT32 [W] [RFailed to read Boot Sector[W\n");
        kfree(buf);
        return NULL;
    }
    
    fat32_bpb_t* bpb = (fat32_bpb_t*)buf;
    u32 partition_offset = 0;

    // Check for 0x55AA signature
    if (buf[510] != 0x55 || buf[511] != 0xAA) {
        kprintf("[ [CFAT32 [W] [RInvalid Boot Signature at Sector 0[W\n");
        kfree(buf);
        return NULL;
    }

    // Detect MBR/VBR
    // A valid VBR must have bytes_per_sector = 512 at offset 11.
    // An MBR usually has 0 there.
    if (bpb->bytes_per_sector == 0) {
        // Looks like MBR. Check Partition 1 (Offset 0x1BE)
        mbr_partition_t* part1 = (mbr_partition_t*)&buf[0x1BE];
        
        // FAT32 LBA Types: 0x0B, 0x0C
        if (part1->type == 0x0B || part1->type == 0x0C) {
            partition_offset = part1->lba_start;
            kprintf("[ [CFAT32 [W] Found Partition at Sector %d\n", partition_offset);
            
            // Read the VBR from the partition start
            vfs_read(device, partition_offset * 512, 512, buf);
            bpb = (fat32_bpb_t*)buf;
        } else {
            kprintf("[ [CFAT32 [W] [RNo FAT32 partition found (Type 0x%x)[W\n", part1->type);
            kfree(buf);
            return NULL;
        }
    }

    // Validate the VBR/BPB
    if (bpb->bytes_per_sector != 512) {
        kprintf("[ [CFAT32 [W] [RUnsupported sector size: %d[W\n", bpb->bytes_per_sector);
        kfree(buf);
        return NULL;
    }

    // Filesystem Initialization
    fat32_fs_t* fs = kmalloc(sizeof(fat32_fs_t));
    memset(fs, 0, sizeof(fat32_fs_t));
    
    fs->device = device;
    fs->sectors_per_cluster = bpb->sectors_per_cluster;
    fs->bytes_per_cluster = bpb->sectors_per_cluster * 512;
    fs->root_cluster = bpb->root_cluster;

    mutex_init(&fs->lock);
    
    u32 fat_size = bpb->sectors_per_fat_32;
    if (fat_size == 0) fat_size = bpb->sectors_per_fat_16;

    fs->fat_begin_lba = partition_offset + bpb->reserved_sectors;
    fs->cluster_begin_lba = partition_offset + bpb->reserved_sectors + (bpb->num_fats * fat_size);
    
    u32 total_sectors = bpb->total_sectors_32 ? bpb->total_sectors_32 : bpb->total_sectors_16;
    u32 data_sectors = total_sectors - (bpb->reserved_sectors + bpb->num_fats * fat_size);
    fs->total_clusters = data_sectors / bpb->sectors_per_cluster;

    kprintf("[ [CFAT32 [W] Mount Success. Root Cluster: %d, Total Clusters: %d\n", 
            fs->root_cluster, fs->total_clusters);

    inode_t* root = kmalloc(sizeof(inode_t));
    memset(root, 0, sizeof(inode_t));
    strcpy(root->name, "/");
    root->flags = FS_DIRECTORY;
    root->ops = &fat32_ops;

    fat32_file_t* root_data = kmalloc(sizeof(fat32_file_t));
    memset(root_data, 0, sizeof(fat32_file_t));
    root_data->fs = fs;
    root_data->first_cluster = fs->root_cluster;
    root->ptr = root_data;

    kfree(buf);
    
    // Set up ops
    fat32_ops.read = fat32_read_file;
    fat32_ops.write = fat32_write_file;
    // Open and Close are not supported on FAT32
    fat32_ops.open = NULL;
    fat32_ops.close = NULL;
    fat32_ops.finddir = fat32_finddir;
    fat32_ops.create = fat32_create;
    fat32_ops.mkdir = fat32_mkdir;
    fat32_ops.unlink = fat32_unlink;
    fat32_ops.rmdir = fat32_rmdir;
    // FAT32 doesn't support symlink natively
    fat32_ops.symlink = NULL;

    return root;
}