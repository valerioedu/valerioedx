#ifndef VFS_H
#define VFS_H

#include <lib.h>

#define FS_FILE        0x01
#define FS_DIRECTORY   0x02
#define FS_CHARDEVICE  0x03
#define FS_BLOCKDEVICE 0x04

struct vfs_node;

// Drivers implement these specific functions.
typedef struct {
    u64 (*read) (struct vfs_node* file, u64 format, u64 size, u8* buffer);
    u64 (*write)(struct vfs_node* file, u64 format, u64 size, u8* buffer);
    void (*open) (struct vfs_node* file);
    void (*close)(struct vfs_node* file);
    struct vfs_node* (*finddir)(struct vfs_node* file, const char *name);
} inode_ops;

typedef struct vfs_node {
    char name[32];
    u32 flags;
    u64 size;
    u64 id;
    inode_ops *ops;
    void *ptr;             // Private driver data
    struct vfs_node *mount_point;
} inode_t;

extern inode_t *vfs_root;

void vfs_init();
u64 vfs_read(inode_t* node, u64 offset, u64 size, u8* buffer);
u64 vfs_write(inode_t* node, u64 offset, u64 size, u8* buffer);
void vfs_open(inode_t* node);
void vfs_close(inode_t* node);

void vfs_mount_root(inode_t *node);
inode_t *vfs_lookup(const char *path);

inode_t* devfs_fetch_device(const char* name);
void devfs_mount_device(char* name, inode_ops* ops);
void devfs_init();

#endif