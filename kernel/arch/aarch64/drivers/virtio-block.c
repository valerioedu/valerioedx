#include <virtio.h>
#include <vfs.h>

extern void debug();

u64 virtio_fs_read(inode_t* node, u64 offset, u64 size, u8* buffer) { // Added 'size' to match VFS signature
    u64 start_sector = offset / 512;
    u64 sectors_count = size / 512;
    
    debug();

    if (size % 512 != 0) sectors_count++; // Handle partial sectors if needed

    for (u64 i = 0; i < sectors_count; i++) {
        debug();
        
        if (virtio_blk_read(start_sector + i, buffer + (i * 512)) < 0) {
            return i * 512; // Return bytes read so far on error
        }
    }
    return sectors_count * 512;
}

u64 virtio_fs_write(inode_t* node, u64 size, u8* buffer) {
    u64 sector = size;

    if (virtio_blk_write(sector, buffer) == 0) {
        return 512;
    }
    return 0;
}