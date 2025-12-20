#include <syscalls.h>
#include <file.h>

i64 sys_write(u32 fd, const char *buf, size_t count) {
    file_t *f = fd_get(fd);
    if (!f) return -1;

    u64 bytes_written = vfs_write(f->inode, f->offset, (u64)count, (u8*)buf);
    f->offset += bytes_written;
    return bytes_written;
}

i64 sys_read(u32 fd, char *buf, size_t count) {
    file_t *f = fd_get(fd);
    if (!f) return -1;

    u64 bytes_read = vfs_read(f->inode, f->offset, count, (u8*)buf);
    f->offset += bytes_read;
    return bytes_read;
}