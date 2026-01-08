#include <sched.h>
#include <heap.h>
#include <syscalls.h>
#include <lib.h>
#include <string.h>
#include <kio.h>
#include <spinlock.h>
#include <vma.h>

extern task_t *current_task;
extern u64 pid_counter;
extern u64 tid_counter;
extern spinlock_t sched_lock;
extern task_t *runqueues[COUNT];
extern task_t *runqueues_tail[COUNT];

task_t *task_clone(task_t *task, process_t *process) {
    task_t *new = (task_t*)kmalloc(sizeof(task_t));
    if (!new) {
        kprintf("[ [RSCHED [W] Error allocating memory for fork task\n");
        return NULL;
    }

    memset(new, 0, sizeof(task_t));
    
    new->stack_page = kmalloc(4096);
    if (!new->stack_page) {
        kprintf("[ [RSCHED[W ] Failed to allocate stack!\n");
        kfree(new);
        return NULL;
    }

    memcpy(new->stack_page, task->stack_page, 4096);

    u32 flags = spinlock_acquire_irqsave(&sched_lock);

    new->context = task->context;
    new->state = task->state;
    new->priority = task->priority;
    new->next = NULL;
    new->next_wait = task->next_wait;
    new->id = tid_counter++;

    u64 stack_offset = (u64)task->context.sp - (u64)task->stack_page;
    new->context.sp = (u64)new->stack_page + stack_offset;

    new->proc = process;

    if (runqueues[new->priority] == NULL) {
        runqueues[new->priority] = new;
        runqueues_tail[new->priority] = new;
    } else {
        runqueues_tail[new->priority]->next = new;
        runqueues_tail[new->priority] = new;
    }

    spinlock_release_irqrestore(&sched_lock, flags);

    return new;
}

i64 sys_fork() {
    process_t *parent = current_task->proc;
    process_t *child = (process_t*)kmalloc(sizeof(process_t));
    if (!child) return -1;

    child->pid = pid_counter++;
    child->mm = mm_duplicate(parent->mm);
    if (!child->mm) {
        kfree(child);
        return -1;
    }
    
    child->parent = current_task->proc;
    child->sibling = parent->child;
    parent->child = child;

    child->child = NULL;

    for (int i = 0; i < MAX_FD; i++) {
        if (parent->fd_table[i]) {
            child->fd_table[i] = parent->fd_table[i];
            child->fd_table[i]->ref_count++;
        }
    }

    if (parent->cwd) {
        child->cwd = parent->cwd;
        vfs_retain(child->cwd);
    }

    strncpy(child->name, parent->name, 64);

    task_t *child_task = task_clone(current_task, child);
    if (!child_task) {
        mm_destroy(child->mm);
        kfree(child);
        return -1;
    }

    trapframe_t *child_tf = (trapframe_t *)child_task->context.sp;
    child_tf->x[0] = 0;
    
    return child->pid;
}

void sys_exit(int code) {
    u32 flags = spinlock_acquire_irqsave(&sched_lock);

    process_t *proc = current_task->proc;
    process_t *parent = proc->parent;

    proc->exit_code = code;
    proc->state = PROCESS_ZOMBIE;

    extern process_t *init_process;
    process_t *child = proc->child;
    while (child) {
        child->parent = init_process;
        child = child->sibling;
    }

    if (parent) {
        //TODO: Implement signals and send SIGCHILD
        wake_up(&parent->wait_queue);
    }

    if (proc->mm) {
        mm_destroy(proc->mm);
        proc->mm = NULL;
    }

    spinlock_release_irqrestore(&sched_lock, flags);
    task_exit();
}

i64 sys_wait(int *status) {
    while (true) {
        u32 flags = spinlock_acquire_irqsave(&sched_lock);

        process_t *proc = current_task->proc;
        process_t *child = proc->child;
        process_t *prev = NULL;
        bool has_children = false;

        while (child) {
            has_children = true;

            if (child->state == PROCESS_ZOMBIE) {
                if (status) *status = child->exit_code;

                u64 zombie_pid = child->pid;

                if (prev) prev->sibling = child->sibling;
                else proc->child = child->sibling;

                kfree(child);
                spinlock_release_irqrestore(&sched_lock, flags);
                
                return zombie_pid;
            }

            prev = child;
            child = child->sibling;
        }

        if (!has_children) {
            spinlock_release_irqrestore(&sched_lock, flags);
            return -1;  //ECHILD
        }
        
        sleep_on(&proc->wait_queue, &sched_lock);
    }
}

i64 sys_getpid() {
    if (!current_task->proc) return -1;

    return current_task->proc->pid;
}

i64 sys_getppid() {
    if (!current_task->proc || !current_task->proc->parent)
        return -1;

    return current_task->proc->parent->pid;
}