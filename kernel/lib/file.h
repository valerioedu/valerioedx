#ifndef FILE_H
#define FILE_H

#include <vfs.h>

typedef struct file {
    inode_t *inode;
    u64 offset;
    u32 flags;
    int ref_count;
} file_t;

int fd_alloc();
void fd_free(int fd);
file_t* file_new(inode_t* inode, u32 flags);
void file_close(file_t* file);
file_t* fd_get(int fd);

#endif