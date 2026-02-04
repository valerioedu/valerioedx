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

i64 sys_setsid() {
    if (!current_task || !current_task->proc)
        return -1;

    process_t *p = current_task->proc;

    if (p->session_leader)
        return -1;

    if (p->pgid == p->pid)
        return -1;

    p->sid = p->pid;
    p->pgid = p->pid;
    p->session_leader = true;
    
    p->controlling_tty = NULL;

    return p->sid;
}

i64 sys_getsid(u64 pid) {
    if (pid == 0) {
        // pid 0 = calling process
        if (!current_task || !current_task->proc)
            return -1;

        return current_task->proc->sid;
    }

    process_t *target = find_process_by_pid(pid);
    if (!target)
        return -1;

    return target->sid;
}

i64 sys_getpgid(u64 pid) {
    process_t *target;

    if (pid == 0) {
        if (!current_task || !current_task->proc) 
            return -1;

        target = current_task->proc;
    } else target = find_process_by_pid(pid);

    if (!target) return -1;

    return target->pgid;
}

i64 sys_setpgid(u64 pid, u64 pgid) {
    if (!current_task || !current_task->proc) return -1;
    
    process_t *caller = current_task->proc;
    process_t *target;

    if (pid == 0 || pid == caller->pid)
        target = caller;
    
    else {
        target = find_process_by_pid(pid);
        if (!target) return -1;
        if (target->parent != caller) return -1;
    }
    
    if (target->session_leader) return -1;

    if (pgid == 0) pgid = target->pid;
    if (target->sid != caller->sid) return -1;

    target->pgid = pgid;
    return 0;
}

i64 sys_getpgrp() {
    if (!current_task || !current_task->proc)
        return -1;
    
    return current_task->proc->pgid;
}

i64 sys_getgroups(int size, gid_t *list) {
    if (!current_task || !current_task->proc)
        return -1;

    process_t *p = current_task->proc;

    if (size == 0) return p->ngroups;

    if (size < p->ngroups) return -1;

    if (copy_to_user(list, p->groups, p->ngroups * sizeof(gid_t)) < 0) 
        return -1;

    return p->ngroups;
}

i64 sys_setgroups(int size, const gid_t *list) {
    if (!current_task || !current_task->proc)
        return -1;

    process_t *p = current_task->proc;

    if (p->euid != 0) return -1;

    if (size < 0 || size > NGROUPS_MAX)
        return -1;

    gid_t new_groups[NGROUPS_MAX];
    if (copy_from_user(new_groups, list, size * sizeof(gid_t)) < 0)
        return -1;


    for (int i = 0; i < size; i++)
        p->groups[i] = new_groups[i];
    
    p->ngroups = size;

    return 0;
}