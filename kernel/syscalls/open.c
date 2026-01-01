#include <syscalls.h>
#include <vfs.h>
#include <file.h>
#include <sched.h>
#include <kio.h>
#include <heap.h>

extern task_t *current_task;

i64 sys_open(const char *path, int flags) {
    if (!path) return -1;

    char *kpath = kmalloc(256);
    if (copy_from_user(kpath, path, 256) != 0)
        goto fail;

    inode_t *inode = namei(kpath);
    if(!inode) goto fail;

    int fd = fd_alloc();
    if (fd < 0) {
        vfs_close(inode);
        goto fail;
    }

    file_t *file = file_new(inode, flags);
    if (!file) {
        fd_free(fd);
        vfs_close(inode);
        goto fail;
    }

    current_task->proc->fd_table[fd] = file;

    vfs_close(inode);
    kfree(kpath);
    return fd;
fail:
    kfree(kpath);
    return -1;
}

i64 sys_close(int fd) {
    file_t *file = fd_get(fd);
    if (!file) return -1;

    fd_free(fd);

    file_close(file);

    return 0;
}