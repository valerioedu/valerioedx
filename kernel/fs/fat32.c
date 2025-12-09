#include <fat32.h>
#include <kio.h>
#include <heap.h>
#include <string.h>

//TODO: Implement more ops

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

// Helper to convert "NAME    EXT" to "name.ext" for comparison
static void fat_get_name(char* dest, const u8* src) {
    int i, j = 0;
    // Copy Name (up to 8 chars)
    for (i = 0; i < 8 && src[i] != ' '; i++) dest[j++] = src[i];
    
    // Copy Extension (up to 3 chars)
    if (src[8] != ' ') {
        dest[j++] = '.';
        for (i = 8; i < 11 && src[i] != ' '; i++) dest[j++] = src[i];
    }
    dest[j] = '\0';
    to_upper(dest);
}

// Reads a cluster from the underlying device
static int read_cluster(fat32_fs_t* fs, u32 cluster, u8* buffer) {
    u32 lba = fs->cluster_begin_lba + (cluster - 2) * fs->sectors_per_cluster;
    return vfs_read(fs->device, lba * 512, fs->bytes_per_cluster, buffer);
}

// Lookup the next cluster in the chain via the File Allocation Table
static u32 get_next_cluster(fat32_fs_t* fs, u32 current_cluster) {
    u32 fat_offset = current_cluster * 4;
    u32 fat_sector = fs->fat_begin_lba + (fat_offset / 512);
    u32 ent_offset = fat_offset % 512;

    u8 sector_buf[512];
    vfs_read(fs->device, fat_sector * 512, 512, sector_buf);

    u32 next_cluster = *(u32*)&sector_buf[ent_offset];
    return next_cluster & 0x0FFFFFFF; // Mask out top 4 bits
}

u64 fat32_read_file(inode_t* node, u64 offset, u64 size, u8* buffer) {
    fat32_file_t* file = (fat32_file_t*)node->ptr;
    fat32_fs_t* fs = file->fs;

    if (offset + size > node->size) size = node->size - offset;
    if (size == 0) return 0;

    u32 cluster = file->first_cluster;
    u32 cluster_size = fs->bytes_per_cluster;
    
    // Skip clusters to reach the offset
    u32 clusters_to_skip = offset / cluster_size;
    u32 chunk_offset = offset % cluster_size;

    for (u32 i = 0; i < clusters_to_skip; i++) {
        cluster = get_next_cluster(fs, cluster);
        if (cluster >= 0x0FFFFFF8) return 0; // End of chain unexpectedly
    }

    u64 read = 0;
    u8* cl_buf = kmalloc(cluster_size);
    if (!cl_buf) return 0;

    while (read < size) {
        if (cluster >= 0x0FFFFFF8) break; // End of chain

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

inode_t* fat32_finddir(inode_t* node, const char* name) {
    fat32_file_t* dir_info = (fat32_file_t*)node->ptr;
    fat32_fs_t* fs = dir_info->fs;

    u32 cluster = dir_info->first_cluster;
    u32 cluster_size = fs->bytes_per_cluster;
    u8* buf = kmalloc(cluster_size);
    
    char search_name[13];
    strncpy(search_name, name, 12);
    to_upper(search_name);

    inode_t* result = NULL;

    while (cluster < 0x0FFFFFF8) {
        read_cluster(fs, cluster, buf);

        fat_dir_entry_t* entries = (fat_dir_entry_t*)buf;
        int entries_per_cluster = cluster_size / sizeof(fat_dir_entry_t);

        for (int i = 0; i < entries_per_cluster; i++) {
            fat_dir_entry_t* entry = &entries[i];

            if (entry->name[0] == 0x00) goto done; // End of directory
            if (entry->name[0] == 0xE5) continue;  // Deleted entry

            // Skips Long File Names for now
            if (entry->attr == FAT_ATTR_LFN) continue;

            char entry_name[13];
            fat_get_name(entry_name, entry->name);

            // Match
            if (strcmp(search_name, entry_name) == 0) {
                result = kmalloc(sizeof(inode_t));
                memset(result, 0, sizeof(inode_t));
                strcpy(result->name, entry_name);
                
                result->flags = (entry->attr & FAT_ATTR_DIRECTORY) ? FS_DIRECTORY : FS_FILE;
                result->size = entry->file_size;
                result->ops = &fat32_ops;

                // Setup private data
                fat32_file_t* file_data = kmalloc(sizeof(fat32_file_t));
                file_data->fs = fs;
                file_data->first_cluster = (entry->fst_clus_hi << 16) | entry->fst_clus_lo;
                
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

inode_t* fat32_mount(inode_t* device) {
    if (!device) return NULL;

    u8* buf = kmalloc(512);
    if (!buf) return NULL;

    // Read Sector 0
    if (vfs_read(device, 0, 512, buf) != 512) {
        kprintf("[FAT32] Failed to read Boot Sector\n");
        kfree(buf);
        return NULL;
    }
    
    fat32_bpb_t* bpb = (fat32_bpb_t*)buf;
    u32 partition_offset = 0;

    // Check for 0x55AA signature
    if (buf[510] != 0x55 || buf[511] != 0xAA) {
        kprintf("[FAT32] Invalid Boot Signature at Sector 0\n");
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
            kprintf("[FAT32] Found Partition at Sector %d\n", partition_offset);
            
            // Read the VBR from the partition start
            vfs_read(device, partition_offset * 512, 512, buf);
        } else {
            kprintf("[FAT32] No FAT32 partition found (Type 0x%x)\n", part1->type);
            kfree(buf);
            return NULL;
        }
    }

    // Validate the VBR/BPB
    if (bpb->bytes_per_sector != 512) {
        kprintf("[FAT32] Unsupported sector size: %d\n", bpb->bytes_per_sector);
        kfree(buf);
        return NULL;
    }

    // Filesystem Initialization
    fat32_fs_t* fs = kmalloc(sizeof(fat32_fs_t));
    fs->device = device;
    fs->sectors_per_cluster = bpb->sectors_per_cluster;
    fs->bytes_per_cluster = bpb->sectors_per_cluster * 512;
    fs->root_cluster = bpb->root_cluster;
    
    u32 fat_size = bpb->sectors_per_fat_32;
    if (fat_size == 0) fat_size = bpb->sectors_per_fat_16;

    fs->fat_begin_lba = partition_offset + bpb->reserved_sectors;
    fs->cluster_begin_lba = partition_offset + bpb->reserved_sectors + (bpb->num_fats * fat_size);

    kprintf("[ [GFAT32 [W] Mount Success. Root Cluster: %d\n", fs->root_cluster);

    inode_t* root = kmalloc(sizeof(inode_t));
    memset(root, 0, sizeof(inode_t));
    strcpy(root->name, "/");
    root->flags = FS_DIRECTORY;
    root->ops = &fat32_ops;

    fat32_file_t* root_data = kmalloc(sizeof(fat32_file_t));
    root_data->fs = fs;
    root_data->first_cluster = fs->root_cluster;
    root->ptr = root_data;

    kfree(buf);
    
    // Set up ops
    fat32_ops.read = fat32_read_file;
    fat32_ops.finddir = fat32_finddir;

    return root;
}