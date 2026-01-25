#include <vfs.h>
#include <sched.h>
#include <syscalls.h>

extern task_t *current_task;

i64 sys_getuid() {
    if (!current_task || !current_task->proc)
        return -1;

    return current_task->proc->uid;
}

i64 sys_setuid(u32 uid) {
    if (!current_task || !current_task->proc)
        return -1;

    if (current_task->proc->euid != 0)
        return -1;
    
    current_task->proc->uid = uid;
    current_task->proc->euid = uid;
    return 0;
}

i64 sys_seteuid(u32 euid) {
    if (!current_task || !current_task->proc)
        return -1;

    if (current_task->proc->euid != 0 && euid != current_task->proc->uid)
        return -1;

    current_task->proc->euid = euid;
    return 0;
}

i64 sys_getgid() {
    if (!current_task || !current_task->proc)
        return -1;

    return current_task->proc->gid;
}

i64 sys_setgid(u32 gid) {
    if (!current_task || !current_task->proc)
        return -1;

    if (current_task->proc->euid != 0)
        return -1;
    
    current_task->proc->gid = gid;
    current_task->proc->egid = gid;
    return 0;
}

i64 sys_setegid(u32 egid) {
    if (!current_task || !current_task->proc)
        return -1;

    if (current_task->proc->euid != 0 && egid != current_task->proc->gid)
        return -1;

    current_task->proc->egid = egid;
    return 0;
}

i64 sys_geteuid() {
    return current_task && current_task->proc ? current_task->proc->euid : -1;
}

i64 sys_getegid() {
    return current_task && current_task->proc ? current_task->proc->egid : -1;
}