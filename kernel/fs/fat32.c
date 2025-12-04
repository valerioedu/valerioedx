#include <fat32.h>
#include <vfs.h>
#include <kio.h>
#include <string.h>
#include <heap.h>

// Internal FS Context (Private Data for inode_t)
typedef struct {
    inode_t* disk;
    u32 partition_start; // Sector offset if partitions exist
    u32 fat_start;       // Sector
    u32 data_start;      // Sector
    u32 sectors_per_cluster;
    u32 bytes_per_cluster;
    u32 root_cluster;
} fat32_fs_t;

// Forward Declarations
u64 fat32_read(inode_t* node, u64 offset, u64 size, u8* buffer);
inode_t* fat32_finddir(inode_t* node, const char* name);

static inode_ops fat32_ops = {
    .read = fat32_read,
    .write = NULL, // Implement write later
    .open = NULL,
    .close = NULL,
    .finddir = fat32_finddir
};

// --------------------------------------------------------
// FAT Helpers
// --------------------------------------------------------

static u32 cluster_to_sector(fat32_fs_t* fs, u32 cluster) {
    return fs->data_start + ((cluster - 2) * fs->sectors_per_cluster);
}

// Reads the FAT table to find the next cluster in the chain
static u32 get_next_cluster(fat32_fs_t* fs, u32 current_cluster) {
    u32 fat_sector = fs->fat_start + (current_cluster * 4) / 512;
    u32 fat_offset = (current_cluster * 4) % 512;

    u8 buffer[512];
    // Read the specific sector of the FAT
    vfs_read(fs->disk, fat_sector * 512, 512, buffer);

    u32 table_value = *(u32*)&buffer[fat_offset];
    return table_value & 0x0FFFFFFF; // Ignore high 4 bits
}

// --------------------------------------------------------
// Driver Implementation
// --------------------------------------------------------

// Reads data by following the cluster chain
u64 fat32_read(inode_t* node, u64 offset, u64 size, u8* buffer) {
    fat32_fs_t* fs = (fat32_fs_t*)node->ptr;
    u32 current_cluster = (u32)node->id;
    
    // 1. Advance to the cluster containing 'offset'
    u32 cluster_skip_count = offset / fs->bytes_per_cluster;
    u32 cluster_offset     = offset % fs->bytes_per_cluster;

    for (u32 i = 0; i < cluster_skip_count; i++) {
        current_cluster = get_next_cluster(fs, current_cluster);
        if (current_cluster >= 0x0FFFFFF8) return 0; // EOF
    }

    u64 bytes_read = 0;
    u8* cluster_buf = kmalloc(fs->bytes_per_cluster);

    while (bytes_read < size) {
        if (current_cluster >= 0x0FFFFFF8) break; // End of Chain

        // Read entire cluster
        u32 sector = cluster_to_sector(fs, current_cluster);
        // We read N sectors manually because vfs_read might take byte offset
        // Assuming underlying disk uses byte offset in vfs_read:
        vfs_read(fs->disk, (u64)sector * 512, fs->bytes_per_cluster, cluster_buf);

        // Copy relevant part
        u64 available = fs->bytes_per_cluster - cluster_offset;
        u64 to_copy   = (size - bytes_read < available) ? (size - bytes_read) : available;

        memcpy(buffer + bytes_read, cluster_buf + cluster_offset, to_copy);

        bytes_read += to_copy;
        cluster_offset = 0; // Next clusters start at 0
        
        current_cluster = get_next_cluster(fs, current_cluster);
    }

    kfree(cluster_buf);
    return bytes_read;
}

// Helper to convert "FILE    TXT" to "file.txt"
void fat_name_to_str(char* dest, char* src) {
    int i;
    // Name
    for (i = 0; i < 8 && src[i] != ' '; i++) *dest++ = src[i];
    
    if (src[8] != ' ') {
        *dest++ = '.';
        // Extension
        for (i = 8; i < 11 && src[i] != ' '; i++) *dest++ = src[i];
    }
    *dest = '\0';
}

inode_t* fat32_finddir(inode_t* node, const char* name) {
    fat32_fs_t* fs = (fat32_fs_t*)node->ptr;
    u32 current_cluster = (u32)node->id;
    
    u8* buffer = kmalloc(fs->bytes_per_cluster);
    fat_dir_entry_t* dir;

    while (current_cluster < 0x0FFFFFF8) {
        u32 sector = cluster_to_sector(fs, current_cluster);
        vfs_read(fs->disk, (u64)sector * 512, fs->bytes_per_cluster, buffer);

        // Iterate entries in this cluster
        int entries = fs->bytes_per_cluster / sizeof(fat_dir_entry_t);
        
        for (int i = 0; i < entries; i++) {
            dir = (fat_dir_entry_t*)(buffer + (i * sizeof(fat_dir_entry_t)));

            if (dir->name[0] == 0x00) break; // End of directory
            if (dir->name[0] == 0xE5) continue; // Deleted
            if (dir->attr == 0x0F) continue; // Skip LFN for now

            char fname[13];
            fat_name_to_str(fname, dir->name);

            if (strcmp(fname, name) == 0) {
                // Found! Create inode
                inode_t* child = kmalloc(sizeof(inode_t));
                strcpy(child->name, fname);
                child->flags = (dir->attr & ATTR_DIRECTORY) ? FS_DIRECTORY : FS_FILE;
                child->id = (u32)((dir->cluster_high << 16) | dir->cluster_low);
                child->size = dir->size;
                child->ops = &fat32_ops;
                child->ptr = fs; // Share the FS context
                
                kfree(buffer);
                return child;
            }
        }
        current_cluster = get_next_cluster(fs, current_cluster);
    }

    kfree(buffer);
    return NULL;
}

void debug() {
    static int x = 0;
    kprintf("%d\n", x++);
}

inode_t* fat32_init(inode_t* block_device) {
    kprintf("[ [CFAT32 [W] Reading BPB...\n");
    
    u8* bpb_buf = kmalloc(512);
    debug();
    vfs_read(block_device, 0, 512, bpb_buf);

    debug();
    
    fat_bpb_t* bpb = (fat_bpb_t*)bpb_buf;

    debug();

    if (bpb->boot_signature != 0x29 && bpb->boot_signature != 0x28) {
        // FAT32 usually has 0x29 extended signature
        // Note: Check bytes 510/511 too (0xAA55)
    }

    debug();

    fat32_fs_t* fs = kmalloc(sizeof(fat32_fs_t));
    fs->disk = block_device;
    fs->sectors_per_cluster = bpb->sectors_per_cluster;
    fs->bytes_per_cluster = bpb->sectors_per_cluster * bpb->bytes_per_sector;
    fs->root_cluster = bpb->root_cluster;
    
    u32 fat_sz = bpb->sectors_per_fat_32;
    fs->fat_start = bpb->reserved_sectors;
    fs->data_start = fs->fat_start + (bpb->fat_count * fat_sz);

    kprintf("[ [CFAT32 [W] Cluster Size: %d, Root Cluster: %d\n", fs->bytes_per_cluster, fs->root_cluster);

    inode_t* root = kmalloc(sizeof(inode_t));
    strcpy(root->name, "/");
    root->flags = FS_DIRECTORY;
    root->id = fs->root_cluster;
    root->ops = &fat32_ops;
    root->ptr = fs;

    kfree(bpb_buf);
    return root;
}