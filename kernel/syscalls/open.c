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

    if (inode && (flags & O_CREAT))
        goto success;

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

i64 sys_unlink(const char *path) {
    if (!path) return -1;

    char *kpath = kmalloc(256);
    if (!kpath) return -1;

    if (copy_string_from_user(kpath, path, 256) != 0) {
        kfree(kpath);
        return -1;
    }

    char *parent_path = kmalloc(256);
    char *filename = kmalloc(256);
    if (!parent_path || !filename) {
        kfree(kpath);
        kfree(parent_path);
        kfree(filename);
        return -1;
    }

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
    if (!parent) {
        kfree(kpath);
        kfree(parent_path);
        kfree(filename);
        return -1;
    }

    i64 ret = vfs_unlink(parent, filename);

    vfs_close(parent);
    kfree(kpath);
    kfree(parent_path);
    kfree(filename);
    return ret;
}

i64 sys_symlink(const char *path1, const char *path2) {
    if (!path1 || !path2) return -1;

    char *kpath1 = (char*)kmalloc(256);
    if (!kpath1) return -1;

    if (copy_string_from_user(kpath1, path1, 256) != 0) {
        kfree(kpath1);
        return -1;
    }

    char *kpath2 = (char*)kmalloc(256);
    if (!kpath2) {
        kfree(kpath1);
        return -1;
    }

    if (copy_string_from_user(kpath2, path2, 256) != 0) {
        kfree(kpath1);
        kfree(kpath2);
        return -1;
    }

    char *parent_path = kmalloc(256);
    char *filename = kmalloc(256);
    if (!parent_path || !filename) {
        kfree(kpath1);
        kfree(kpath2);
        kfree(parent_path);
        kfree(filename);
        return -1;
    }

    char *last_slash = strrchr(kpath2, '/');
    if (last_slash) {
        if (last_slash == kpath2)
            strcpy(parent_path, "/");
        else {
            size_t len = last_slash - kpath2;
            strncpy(parent_path, kpath2, len);
            parent_path[len] = '\0';
        }
        strcpy(filename, last_slash + 1);
    } else {
        strcpy(parent_path, ".");
        strcpy(filename, kpath2);
    }

    inode_t *parent = namei(parent_path);
    if (!parent) {
        kfree(kpath1);
        kfree(kpath2);
        kfree(parent_path);
        kfree(filename);
        return -1;
    }

    inode_t *created = vfs_symlink(parent, filename, kpath1);
    if (created) vfs_close(created);

    vfs_close(parent);
    kfree(kpath1);
    kfree(kpath2);
    kfree(parent_path);
    kfree(filename);

    return created ? 0 : -1;
}

static int resolve_parent_and_name(const char* path, inode_t** parent_out, char** name_out, char** buffer_to_free) {
    *buffer_to_free = (char*)kmalloc(strlen(path) + 1);
    strcpy(*buffer_to_free, path);
    
    char *last_slash = strrchr(*buffer_to_free, '/');
    inode_t *parent = NULL;

    if (last_slash) {
        *last_slash = 0;
        *name_out = last_slash + 1;
        
        if (strlen(*buffer_to_free) == 0) {
            parent = vfs_root;
            vfs_retain(parent);
        } else parent = namei(*buffer_to_free);
    } else {
        *name_out = *buffer_to_free;
        extern task_t *current_task;
        if (current_task && current_task->proc && current_task->proc->cwd)
            parent = current_task->proc->cwd;
        
        else parent = vfs_root;

        vfs_retain(parent);
    }

    if (!parent) {
        kfree(*buffer_to_free);
        return -1;
    }

    *parent_out = parent;
    return 0;
}

i64 sys_link(const char *oldpath, const char *newpath) {
    inode_t *target = namei(oldpath);
    if (!target) return -1;

    inode_t *parent = NULL;
    char *name = NULL;
    char *path_buf = NULL;

    if (resolve_parent_and_name(newpath, &parent, &name, &path_buf) != 0) {
        vfs_close(target);
        return -1;
    }

    int ret = vfs_link(parent, name, target);

    vfs_close(parent);
    vfs_close(target);
    kfree(path_buf);
    return ret;
}

i64 sys_mknod(const char *path, int mode, int dev) {
    inode_t *parent = NULL;
    char *name = NULL;
    char *path_buf = NULL;

    if (resolve_parent_and_name(path, &parent, &name, &path_buf) != 0)
        return -1;

    inode_t *node = vfs_mknod(parent, name, mode, dev);
    int ret = node ? 0 : -1;
    if (node) vfs_close(node);

    vfs_close(parent);
    kfree(path_buf);
    return ret;
}