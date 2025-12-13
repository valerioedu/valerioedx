#include "vfs.h"
#include <kio.h>
#include <heap.h>
#include <string.h>

inode_t *vfs_root = NULL;

//TODO: Implement page cache

void vfs_init() {
    vfs_root = NULL;
    kprintf("[ [CVFS [W] Virtual File System Initialized\n");
}

void vfs_mount_root(inode_t *node) {
    vfs_root = node;
    kprintf("[ [CVFS[W ] Root mounted: %s\n", node->name);
}

int vfs_mount(inode_t* mountpoint, inode_t* fs_root) {
    if (!mountpoint || !fs_root) return 0;
    mountpoint->mount_point = fs_root;
    return 1;
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
    if (node && node->ops && node->ops->open)
        node->ops->open(node);
}

void vfs_close(inode_t* node) {
    if (node && node->ops && node->ops->close)
        node->ops->close(node);
}

inode_t* vfs_lookup(const char* path) {
    if (!vfs_root) return NULL;
    
    inode_t* current = vfs_root;

    if (current->mount_point) current = current->mount_point;
    
    char* path_copy = kmalloc(strlen(path) + 1);
    strcpy(path_copy, path);
    
    char* token = strtok(path_copy, "/");
    
    while (token != NULL) {
        if (!current->ops || !current->ops->finddir) {
            kfree(path_copy);
            return NULL;
        }

        inode_t* next = current->ops->finddir(current, token);
        
        if (!next) {
            kfree(path_copy);
            return NULL;
        }
        
        current = next;

        if (current->mount_point) {
            current = current->mount_point;
        }

        token = strtok(NULL, "/");
    }
    
    kfree(path_copy);
    return current;
}