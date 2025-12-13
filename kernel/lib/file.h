#ifndef FILE_H
#define FILE_H

#include <vfs.h>

typedef struct file {
    inode_t *inode;
    u64 offset;
    u32 flags;
    int ref_count;
} file_t;

#endif