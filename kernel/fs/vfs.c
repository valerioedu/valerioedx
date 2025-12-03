#include "vfs.h"
#include <kio.h>

void vfs_init() {
    kprintf("[ [CVFS [W] Virtual File System Initialized\n");
}

u64 vfs_read(inode_t* node, u64 size, u8* buffer) {
    if (node && node->ops && node->ops->read)
        return node->ops->read(node, size, buffer);
    return 0;
}

u64 vfs_write(inode_t* node, u64 size, u8* buffer) {
    if (node && node->ops && node->ops->write)
        return node->ops->write(node, size, buffer);
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