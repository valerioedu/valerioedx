#include "vfs.h"
#include <kio.h>
#include <heap.h>
#include <string.h>

#define MAX_MOUNTS 16

//TODO: Implement ref counting

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

void vfs_mount_root(inode_t *node) {
    vfs_root = node;
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
    if (node && node->ops && node->ops->open)
        node->ops->open(node);
}

void vfs_close(inode_t* node) {
    if (!node) return;

    // node->ref_count--;
    // if (node->ref_count == 0)
    if (node->flags & FS_TEMPORARY) {
        if (node->ops && node->ops->close) {
            node->ops->close(node); // Free private driver data (fat32_file_t)
        }
        kfree(node); // Free the VFS node itself
    }
}

inode_t* vfs_lookup(const char* path) {
    if (!vfs_root) return NULL;
    
    inode_t* current = vfs_root;

    if (current->mount_point) current = current->mount_point;
    
    char* path_copy = kmalloc(strlen(path) + 1);
    strcpy(path_copy, path);
    
    char *saveptr;
    char* token = strtok_r(path_copy, "/", &saveptr);
    
    while (token != NULL) {
        if (!current->ops || !current->ops->lookup) {
            vfs_close(current);
            kfree(path_copy);
            return NULL;
        }

        inode_t* next = current->ops->lookup(current, token);

        vfs_close(current);
        
        if (!next) {
            kfree(path_copy);
            return NULL;
        }
        
        current = next;

        for (int i = 0; i < MAX_MOUNTS; i++) {
        if (mount_table[i].target != NULL && mount_table[i].host_id == current->id) {
            inode_t* mounted_root = mount_table[i].target;
            kfree(current);
            current = mounted_root;
            break;
        }
    }

        token = strtok_r(NULL, "/", &saveptr);
    }
    
    kfree(path_copy);
    return current;
}