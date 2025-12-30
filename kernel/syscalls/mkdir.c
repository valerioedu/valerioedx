#include <syscalls.h>
#include <vfs.h>
#include <file.h>
#include <sched.h>
#include <heap.h>
#include <string.h>
#include <kio.h>

extern task_t *current_task;

// Linux dirent structure for getdirentries
typedef struct dirent {
    u64 d_ino;          // Inode number
    u64 d_off;          // Offset to next dirent
    u16 d_reclen;       // Length of this record
    u8  d_type;         // Type of file
    char d_name[256];   // Filename (null-terminated)
} dirent_t;

// Directory entry types
#define DT_UNKNOWN  0
#define DT_FIFO     1
#define DT_CHR      2
#define DT_DIR      4
#define DT_BLK      6
#define DT_REG      8
#define DT_LNK      10
#define DT_SOCK     12

i64 sys_mkdir(const char *path, mode_t mode) {
    if (!path || !current_task->proc) return -1;

    char *path_copy = kmalloc(strlen(path) + 1);
    if (!path_copy) return -1;
    strcpy(path_copy, path);

    char *last_slash = strrchr(path_copy, '/');
    char *dirname;
    inode_t *parent;

    if (last_slash == path_copy) {
        dirname = last_slash + 1;
        parent = vfs_root;
        vfs_retain(parent);
    } else if (last_slash) {
        *last_slash = '\0';
        dirname = last_slash + 1;
        parent = namei(path_copy);
    } else {
        dirname = path_copy;
        if (current_task->proc->cwd) {
            parent = current_task->proc->cwd;
            vfs_retain(parent);
        } else {
            parent = vfs_root;
            vfs_retain(parent);
        }
    }

    if (!parent) {
        kfree(path_copy);
        return -1;
    }

    if (!(parent->flags & FS_DIRECTORY)) {
        vfs_close(parent);
        kfree(path_copy);
        return -1;
    }

    if (!parent->ops || !parent->ops->mkdir) {
        vfs_close(parent);
        kfree(path_copy);
        return -1;
    }

    inode_t *new_dir = parent->ops->mkdir(parent, dirname);
    
    vfs_close(parent);
    kfree(path_copy);

    if (!new_dir) return -1;

    vfs_close(new_dir);
    return 0;
}

i64 sys_rmdir(const char *path) {
    if (!path || !current_task->proc) return -1;

    char *path_copy = kmalloc(strlen(path) + 1);
    if (!path_copy) return -1;
    strcpy(path_copy, path);

    char *last_slash = strrchr(path_copy, '/');
    char *dirname;
    inode_t *parent;

    if (last_slash == path_copy) {
        dirname = last_slash + 1;
        parent = vfs_root;
        vfs_retain(parent);
    } else if (last_slash) {
        *last_slash = '\0';
        dirname = last_slash + 1;
        parent = namei(path_copy);
    } else {
        dirname = path_copy;
        if (current_task->proc->cwd) {
            parent = current_task->proc->cwd;
            vfs_retain(parent);
        } else {
            parent = vfs_root;
            vfs_retain(parent);
        }
    }

    if (!parent) {
        kfree(path_copy);
        return -1;
    }

    if (!(parent->flags & FS_DIRECTORY)) {
        vfs_close(parent);
        kfree(path_copy);
        return -1;
    }

    if (!parent->ops || !parent->ops->rmdir) {
        vfs_close(parent);
        kfree(path_copy);
        return -1;
    }

    if (strcmp(dirname, ".") == 0 || strcmp(dirname, "..") == 0) {
        vfs_close(parent);
        kfree(path_copy);
        return -1;
    }

    int result = parent->ops->rmdir(parent, dirname);
    
    vfs_close(parent);
    kfree(path_copy);

    return result;
}

i64 sys_getdirentries(int fd, char *buf, size_t nbytes, i64 *basep) {
    if (!buf || nbytes == 0) return -1;
    if (!current_task || !current_task->proc) return -1;

    file_t *file = fd_get(fd);
    if (!file || !file->inode) return -1;

    inode_t *dir = file->inode;
    
    if (!(dir->flags & FS_DIRECTORY)) return -1;
    
    if (!dir->ops || !dir->ops->readdir) return -1;

    // Get starting position
    int index = (int)file->offset;
    if (basep)
        index = (int)(*basep);

    size_t bytes_written = 0;
    char namebuf[256];
    int is_dir;

    while (bytes_written < nbytes) {
        int result = dir->ops->readdir(dir, index, namebuf, sizeof(namebuf), &is_dir);
        
        if (result == 0) break;

        size_t name_len = strlen(namebuf) + 1;
        size_t reclen = sizeof(u64) * 2 + sizeof(u16) + sizeof(u8) + name_len;
        reclen = (reclen + 7) & ~7;  // Align to 8 bytes

        if (bytes_written + reclen > nbytes)
            break;

        dirent_t *entry = (dirent_t *)(buf + bytes_written);
        entry->d_ino = dir->id + index + 1;
        entry->d_off = index + 1;
        entry->d_reclen = (u16)reclen;
        entry->d_type = is_dir ? DT_DIR : DT_REG;
        strncpy(entry->d_name, namebuf, 255);
        entry->d_name[255] = '\0';

        bytes_written += reclen;
        index++;
    }

    file->offset = index;
    
    if (basep)
        *basep = index;

    return (i64)bytes_written;
}