#include <virtio.h>
#include <string.h>

u64 virtio_blk_fs_read(inode_t* node, u64 offset, u64 size, u8* buffer) {
    if (size == 0) return 0;

    u64 start_sector = offset / VIRTIO_BLK_SECTOR_SIZE;
    u64 end_sector = (offset + size - 1) / VIRTIO_BLK_SECTOR_SIZE;
    u64 read_len = 0;
    u8 sector_buf[VIRTIO_BLK_SECTOR_SIZE];

    for (u64 i = start_sector; i <= end_sector; i++) {
        if (virtio_blk_read(i, sector_buf) != 0) {
            return read_len;
        }

        u64 sector_offset = 0;
        u64 chunk_size = VIRTIO_BLK_SECTOR_SIZE;

        // Handles offset within the first sector
        if (i == start_sector) {
            sector_offset = offset % VIRTIO_BLK_SECTOR_SIZE;
            chunk_size -= sector_offset;
        }

        // Handles size limit for the last sector
        if (read_len + chunk_size > size) {
            chunk_size = size - read_len;
        }

        memcpy(buffer + read_len, sector_buf + sector_offset, chunk_size);
        read_len += chunk_size;
    }
    return read_len;
}

u64 virtio_blk_fs_write(inode_t* node, u64 offset, u64 size, u8* buffer) {
    if (size == 0) return 0;

    u64 start_sector = offset / VIRTIO_BLK_SECTOR_SIZE;
    u64 end_sector = (offset + size - 1) / VIRTIO_BLK_SECTOR_SIZE;
    u64 written_len = 0;
    u8 sector_buf[VIRTIO_BLK_SECTOR_SIZE];

    for (u64 i = start_sector; i <= end_sector; i++) {
        u64 sector_offset = 0;
        u64 chunk_size = VIRTIO_BLK_SECTOR_SIZE;

        if (i == start_sector) {
            sector_offset = offset % VIRTIO_BLK_SECTOR_SIZE;
            chunk_size -= sector_offset;
        }

        if (written_len + chunk_size > size) {
            chunk_size = size - written_len;
        }

        if (chunk_size < VIRTIO_BLK_SECTOR_SIZE) {
            if (virtio_blk_read(i, sector_buf) != 0) {
                return written_len;
            }
        }

        memcpy(sector_buf + sector_offset, buffer + written_len, chunk_size);

        if (virtio_blk_write(i, sector_buf) != 0) {
            return written_len;
        }

        written_len += chunk_size;
    }

    return written_len;
}

inode_ops virtio_blk_ops = {
    .read = virtio_blk_fs_read,
    .write = virtio_blk_fs_write,
    .close = NULL,
    .open = NULL
};