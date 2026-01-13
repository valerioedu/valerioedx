#include <syscalls.h>
#include <vfs.h>
#include <file.h>
#include <sched.h>
#include <heap.h>
#include <kio.h>
#include <string.h>

#define O_CREAT 0x100

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
    if (!inode && (flags & O_CREAT)) {
        char *parent_path = kmalloc(256);
        char *filename = kmalloc(256);
        
        char *last_slash = strrchr(kpath, '/');
        
        if (last_slash) {
            if (last_slash == kpath)
                strcpy(parent_path, "/");
            else {
                size_t len = last_slash - kpath;
                strncpy(parent_path, kpath, len);
                parent_path[len] = '\0';
            }
            
            strcpy(filename, last_slash + 1);
        } else {
            strcpy(parent_path, ".");
            strcpy(filename, kpath);
        }

        inode_t *parent = namei(parent_path);
        if (parent) {
            inode = vfs_create(parent, filename);
            vfs_close(parent);
        }
        
        kfree(parent_path);
        kfree(filename);

        goto success;
    }

    if (inode && (flags & O_CREAT)) {
        int fd = fd_alloc();
        kfree(kpath);
        return fd;
    }

    if(!inode) {
        kfree(kpath);
        return -1;
    }

success:
    int fd = fd_alloc();
    if (fd < 0) {
        vfs_close(inode);
        kfree(kpath);
        return -1;
    }

    file_t *file = file_new(inode, flags);
    if (!file) {
        fd_free(fd);
        vfs_close(inode);
        kfree(kpath);
        return -1;
    }

    current_task->proc->fd_table[fd] = file;

    vfs_close(inode);
    kfree(kpath);

    return fd;
}

i64 sys_close(int fd) {
    file_t *file = fd_get(fd);
    if (!file) return -1;

    fd_free(fd);

    file_close(file);

    return 0;
}