#include <syscalls.h>
#include <file.h>
#include <sched.h>
#include <string.h>
#include <heap.h>
#include <vma.h>
#include <vmm.h>

extern task_t *current_task;

i64 sys_chdir(const char *path) {
    if (!path || !current_task->proc) return -1;
    
    char *kbuf = kmalloc(256);
    if (!kbuf) return -1;
    
    if (copy_from_user(kbuf, path, 256) != 0) {
        kfree(kbuf);
        return -1;
    }

    inode_t *node = namei(path);
    if (!node) return -1;
    
    if (!(node->flags & FS_DIRECTORY)) {
        vfs_close(node);
        return -1;
    }
    
    if (current_task->proc->cwd)
        vfs_close(current_task->proc->cwd);
    
    current_task->proc->cwd = node;

    kfree(kbuf);
    return 0;
}

i64 sys_fchdir(int fd) {
    if (!current_task->proc) return -1;
    if (fd < 0 || fd >= MAX_FD) return -1;
    
    file_t *fd_file = current_task->proc->fd_table[fd];
    if (!fd_file || !fd_file->inode) return -1;
    
    if (!(fd_file->inode->flags & FS_DIRECTORY))
        return -1;
    
    vfs_retain(fd_file->inode);
    
    if (current_task->proc->cwd)
        vfs_close(current_task->proc->cwd);
    
    current_task->proc->cwd = fd_file->inode;
    return 0;
}

static int build_path_recursive(inode_t *node, char *buf, size_t size, size_t *pos) {
    if (!node) return 0;

    if (node->parent == NULL || node == vfs_root) {
        if (*pos < size)
            buf[(*pos)++] = '/';

        return 0;
    }

    build_path_recursive(node->parent, buf, size, pos);

    size_t name_len = strlen(node->name);
    if (*pos + name_len + 1 < size) {
        memcpy(buf + *pos, node->name, name_len);
        *pos += name_len;
        buf[(*pos)++] = '/';
    }

    return 0;
}

i64 sys_getcwd(char *buf, size_t size) {
    if (!buf || size == 0) return 0;
    if (!current_task || !current_task->proc) return 0;

    inode_t *cwd = current_task->proc->cwd;
    
    // If no cwd set, use root
    if (!cwd) cwd = vfs_root;

    if (!cwd) return 0;

    char *kbuf = kmalloc(size);
    if (!kbuf) return 0;

    // Special case: at root
    if (cwd == vfs_root || cwd->parent == NULL) {
        if (size >= 2) {
            kbuf[0] = '/';
            kbuf[1] = '\0';
            
            if (copy_to_user(buf, kbuf, 2) != 0) {
                kfree(kbuf);
                return 0;
            }
            
            kfree(kbuf);
            return (i64)buf;
        }

        kfree(kbuf);
        return 0;
    }

    size_t total_len = 0;
    int depth = 0;
    inode_t *node = cwd;
    
    while (node && node != vfs_root && node->parent != NULL) {
        total_len += strlen(node->name) + 1;  // +1 for '/'
        depth++;
        node = node->parent;
    }

    total_len++;  // For leading '/'

    if (total_len >= size) {
        kfree(kbuf);
        return 0;
    }

    char **components = kmalloc(depth * sizeof(char*));
    if (!components) {
        kfree(kbuf);
        return 0;
    }

    node = cwd;
    int i = depth - 1;
    while (node && node != vfs_root && node->parent != NULL && i >= 0) {
        components[i] = node->name;
        i--;
        node = node->parent;
    }

    // Now build the path
    size_t pos = 0;
    kbuf[pos++] = '/';
    
    for (i = 0; i < depth; i++) {
        size_t len = strlen(components[i]);
        memcpy(kbuf + pos, components[i], len);
        pos += len;
        if (i < depth - 1)
            kbuf[pos++] = '/';
    }

    kbuf[pos] = '\0';
    kfree(components);

    if (copy_to_user(buf, kbuf, pos + 1) != 0) {
        kfree(kbuf);
        return 0;
    }

    kfree(kbuf);
    return (i64)buf;
}