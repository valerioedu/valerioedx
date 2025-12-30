#include <syscalls.h>
#include <file.h>
#include <sched.h>

extern task_t *current_task;

#define SEEK_SET 0  // Set position to offset
#define SEEK_CUR 1  // Set position to current + offset
#define SEEK_END 2  // Set position to end + offset

i64 sys_dup(int oldfd) {
    if (!current_task || !(current_task->proc)) 
        return -1;

    file_t *file = fd_get(oldfd);
    if (!file) return -1;

    int newfd = fd_alloc();
    if (newfd < 0) return -1;

    file->ref_count++;

    current_task->proc->fd_table[newfd] = file;

    return newfd;
}

i64 sys_dup2(int oldfd, int newfd) {
    if (!current_task || !current_task->proc)
        return -1;

    if (newfd < 0 || newfd > MAX_FD)
        return -1;

    file_t *file = fd_get(oldfd);
    if (!file) return -1;

    if (oldfd == newfd) return newfd;

    file_t *existing = fd_get(newfd);
    if (existing) {
        file_close(existing);
        current_task->proc->fd_table[newfd] = NULL;
    }

    file->ref_count++;

    current_task->proc->fd_table[newfd] = file;
    
    return newfd;
}

i64 sys_lseek(int fd, i64 offset, int whence) {
    if (!current_task || !current_task->proc)
        return -1;

    file_t *file = fd_get(fd);
    if (!file) return -1;

    if (file->inode->flags & FS_CHARDEVICE)
        return -1;
    
    i64 new_off;

    switch (whence) {
        case SEEK_SET:
            new_off = offset;
            break;
        
        case SEEK_CUR:
            new_off = (i64)file->offset + offset;
            break;
        
        case SEEK_END:
            new_off = (i64)file->inode->size + offset;
            break;
        
        default: return -1;
    }

    if (new_off < 1) return -1;

    file->offset = new_off;
    return new_off;
}