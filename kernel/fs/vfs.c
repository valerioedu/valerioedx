#include "vfs.h"
#include <kio.h>
#include <heap.h>
#include <string.h>
#include <sched.h>

#define MAX_MOUNTS 16

extern task_t *current_task;

struct mount_entry {
    u64 host_id;
    inode_t *target;
} mount_table[MAX_MOUNTS];

inode_t *vfs_root = NULL;

//TODO: Implement page cache

void vfs_init() {
    vfs_root = NULL;
    memset(mount_table, 0, sizeof(mount_table));
    kprintf("[ [CVFS [W] Virtual File System Initialized\n");
}

void vfs_retain(inode_t *node) {
    if (node) node->ref_count++;
}

void vfs_mount_root(inode_t *node) {
    vfs_root = node;
    vfs_root->parent = NULL;
    vfs_retain(vfs_root);
    kprintf("[ [CVFS[W ] Root mounted: %s\n", node->name);
}

int vfs_mount(inode_t* mountpoint, inode_t* fs_root) {
    if (!mountpoint || !fs_root) return 0;

    for (int i = 0; i < MAX_MOUNTS; i++) {
        if (mount_table[i].target == NULL) {
            mount_table[i].host_id = mountpoint->id; // Bind by ID, not pointer
            mount_table[i].target = fs_root;
            return 1;
        }
    }

    return 0;
}

u64 vfs_read(inode_t* node, u64 offset, u64 size, u8* buffer) {
    if (node && node->ops && node->ops->read)
        return node->ops->read(node, offset, size, buffer);
    return 0;
}

u64 vfs_write(inode_t* node, u64 offset, u64 size, u8* buffer) {
    if (node && node->ops && node->ops->write)
        return node->ops->write(node, offset, size, buffer);
    return 0;
}

void vfs_open(inode_t* node) {
    if (!node) return;

    node->ref_count++;
    
    if (node && node->ops && node->ops->open)
        node->ops->open(node);
}

void vfs_close(inode_t* node) {
    if (!node) return;

    node->ref_count--;
    if (node->ref_count <= 0) {
        if (node->flags & FS_TEMPORARY && node->ops->close) {
            if (node->ops && node->ops->close)
                node->ops->close(node); // Free private driver data (fat32_file_t)
            
            if (node->flags & FS_TEMPORARY)
                kfree(node); // Free the VFS node itself
        }
    }
}

inode_t *vfs_create(inode_t *node, const char *name) {
    if (!node) return NULL;

    if (node && node->ops->create)
        return node->ops->create(node, name);

    return NULL;
}

int vfs_unlink(struct vfs_node *parent, const char *name) {
    if (!parent || !name)
        return -1;

    if (parent && parent->ops->unlink)
        return parent->ops->unlink(parent, name);

    return -1;
}

inode_t *namei(const char *path) {
    if (!vfs_root || !path) return NULL;

    inode_t *current;
    
    if (path[0] == '/') current = vfs_root;
    else {
        if (current_task && current_task->proc && current_task->proc->cwd) {
            current = current_task->proc->cwd;
        } else {
            current = vfs_root; 
        }
    }

    vfs_retain(current);

    if (current->mount_point) {
        inode_t *next = current->mount_point;
        vfs_retain(next);
        vfs_close(current);
        current = next;
    }

    char *copy = (char*)kmalloc(strlen(path) + 1);
    strcpy(copy, path);

    char *saveptr;
    char *token = strtok_r(copy, "/", &saveptr);

    while (token) {
        if (strcmp(token, ".") == 0) {
            token = strtok_r(NULL, "/", &saveptr);
            continue;
        }

        if (strcmp(token, "..") == 0) {
            if (current->parent) {
                inode_t *parent = current->parent;
                vfs_retain(parent);
                vfs_close(current);
                current = parent;
            }

            token = strtok_r(NULL, "/", &saveptr);
            continue;
        }

        inode_t* next = current->ops->lookup(current, token);

        if (!next) {
            vfs_close(current);
            kfree(copy);
            return NULL;
        }

        next->parent = current;
        
        vfs_retain(current);
        vfs_close(current);

        current = next;

        for (int i = 0; i < MAX_MOUNTS; i++) {
            if (mount_table[i].target != NULL && \
                mount_table[i].host_id == current->id) {
                inode_t *mounted_root = mount_table[i].target;

                vfs_retain(mounted_root);

                vfs_close(current);

                current = mounted_root;
                break;
            }
        }

        token = strtok_r(NULL, "/", &saveptr);
    }

    kfree(copy);
    return current;
}