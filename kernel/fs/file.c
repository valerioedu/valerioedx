#include <file.h>
#include <heap.h>
#include <sched.h>
#include <kio.h>

extern task_t *current_task;

int fd_alloc() {
    if (!current_task) return -1;

    for (int i = 0; i < MAX_FD; i++) {
        if (current_task->fd_table[i] == NULL) {
            return i;
        }
    }
    return -1;
}

// Does not close the file itself
void fd_free(int fd) {
    if (!current_task) return;
    if (fd < 0 || fd >= MAX_FD) return;

    current_task->fd_table[fd] = NULL;
}

file_t* file_new(inode_t* inode, u32 flags) {
    file_t* file = (file_t*)kmalloc(sizeof(file_t));
    if (!file) return NULL;

    file->inode = inode;
    file->offset = 0;
    file->flags = flags;
    file->ref_count = 1;

    // Trigger VFS open hook if it exists
    vfs_open(inode);
    
    return file;
}

void file_close(file_t* file) {
    if (!file) return;

    file->ref_count--;
    
    if (file->ref_count <= 0) {
        // Trigger VFS close hook
        vfs_close(file->inode);
        kfree(file);
    }
}

file_t* fd_get(int fd) {
    if (!current_task) return NULL;
    if (fd < 0 || fd >= MAX_FD) return NULL;

    return current_task->fd_table[fd];
}