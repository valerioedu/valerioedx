#include <virtio.h>
#include <vfs.h>

u64 virtio_fs_read(inode_t* node, u64 offset, u8* buffer) {
    u64 sector = offset / 512;
    // TODO: FS layer handles the buffering if offset isn't aligned.
    return virtio_blk_read(sector, buffer);
}

u64 virtio_fs_write(inode_t* node, u64 size, u8* buffer) {
    u64 sector = size;

    if (virtio_blk_write(sector, buffer) == 0) {
        return 512;
    }
    return 0;
}