#include <syscalls.h>
#include <file.h>
#include <sched.h>

extern task_t *current_task;

i64 sys_ioctl(int fd, u64 request, u64 arg) {
    if (fd < 0 || fd >= MAX_FD) return -1;

    file_t *file = current_task->proc->fd_table[fd];
    if (!file) return -1;

    if (file->inode && file->inode->ops && file->inode->ops->ioctl)
        return file->inode->ops->ioctl(file->inode, request, arg);
    
    return -1;
}