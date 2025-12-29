#include <syscalls.h>
#include <file.h>
#include <sched.h>
#include <string.h>

extern task_t *current_task;

//TODO: Implement return failure for EACCESS
//TODO: Implement chmod
i64 sys_chdir(const char *path) {
    if (!path || !current_task->proc) return -1;
    
    inode_t *node = namei(path);
    if (!node) return -1;
    
    if (!(node->flags & FS_DIRECTORY)) {
        vfs_close(node);
        return -1;
    }
    
    if (current_task->proc->cwd)
        vfs_close(current_task->proc->cwd);
    
    current_task->proc->cwd = node;
    return 0;
}

i64 sys_fchdir(int fd) {
    if (!current_task->proc) return -1;
    if (fd < 0 || fd >= MAX_FD) return -1;
    
    file_t *fd_file = current_task->proc->fd_table[fd];
    if (!fd_file || !fd_file->inode) return -1;
    
    if (!(fd_file->inode->flags & FS_DIRECTORY))
        return -1;
    
    vfs_retain(fd_file->inode);
    
    if (current_task->proc->cwd)
        vfs_close(current_task->proc->cwd);
    
    current_task->proc->cwd = fd_file->inode;
    return 0;
}