#include <syscalls.h>
#include <vfs.h>
#include <file.h>
#include <sched.h>

extern task_t *current_task;

i64 sys_open(const char *path, int flags) {
    if (!path) return -1;

    inode_t *inode = namei(path);
    if(!inode) return -1;

    int fd = fd_alloc();
    if (fd < 3) {
        vfs_close(inode);
        return -1;
    }

    file_t *file = file_new(inode, flags);
    if (!file) {
        fd_free(fd);
        vfs_close(inode);
        return -1;
    }

    current_task->proc->fd_table[fd] = file;

    vfs_close(inode);

    return fd;
}

i64 sys_close(int fd) {
    file_t *file = fd_get(fd);
    if (!file) return -1;

    fd_free(fd);

    file_close(file);

    return 0;
}