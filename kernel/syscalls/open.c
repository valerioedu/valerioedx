#include <syscalls.h>
#include <vfs.h>
#include <file.h>
#include <sched.h>
#include <heap.h>
#include <kio.h>

extern task_t *current_task;

static int copy_string_from_user(char *kbuf, const char *user_str, size_t max_len) {
    for (size_t i = 0; i < max_len; i++) {
        if (copy_from_user(&kbuf[i], (void*)&user_str[i], 1) != 0)
            return -1;

        if (kbuf[i] == '\0')
            return 0;
    }
    
    return -1;
}

i64 sys_open(const char *path, int flags) {
    if (!path) return -1;

    char *kpath = kmalloc(256);
    if (!kpath) return -1;

    if (copy_string_from_user(kpath, path, 256) != 0) {
        kfree(kpath);
        return -1;
    }

    inode_t *inode = namei(kpath);
    if(!inode) return -1;

    int fd = fd_alloc();
    if (fd < 0) {
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