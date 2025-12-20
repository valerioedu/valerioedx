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
    if (!child) { /* TODO */}

    child->pid = pid_counter++;
    child->mm = mm_duplicate(parent->mm);
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

    strncpy(child->name, parent->name, 64);

    task_t *child_task = task_clone(current_task, child);

    trapframe_t *child_tf = (trapframe_t *)((u64)child_task->stack_page + 4096 - sizeof(trapframe_t));
    child_tf->x[0] = 0;
    
    return child->pid;
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