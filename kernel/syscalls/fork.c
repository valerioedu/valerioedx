#include <sched.h>
#include <heap.h>
#include <syscalls.h>
#include <lib.h>
#include <string.h>
#include <kio.h>
#include <spinlock.h>
#include <file.h>
#include <vma.h>
#include <signal.h>

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

i64 sys_fork(trapframe_t *tf) {
    process_t *parent = current_task->proc;
    process_t *child = (process_t*)kmalloc(sizeof(process_t));
    if (!child) return -1;

    memset(child, 0, sizeof(process_t));

    child->pid = pid_counter++;
    pid_hash_insert(child);
    child->state = PROCESS_ACTIVE;
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

    if (parent->signals) {
        child->signals = signal_copy(parent->signals);
        if (!child->signals) {
            mm_destroy(child->mm);
            kfree(child);
            return -1;
        }
    }

    strncpy(child->name, parent->name, 64);
    child->uid = parent->uid;
    child->gid = parent->gid;

    task_t *child_task = task_clone(current_task, child);
    if (!child_task) {
        if (child->signals)
            signal_destroy(child->signals);

        mm_destroy(child->mm);
        kfree(child);
        return -1;
    }

    trapframe_t *child_tf = (trapframe_t*)((u64)child_task->stack_page + 4096 - sizeof(trapframe_t));
    *child_tf = *tf;
    child_tf->x[0] = 0;
    
    child_task->context.sp = (u64)child_tf;
    child_task->context.x19 = 0;
    extern void ret_from_fork_child();
    child_task->context.lr = (u64)ret_from_fork_child;

    return child->pid;
}

void sys_exit(int code) {
    process_t *proc = current_task->proc;
    process_t *parent = proc->parent;

    for (int i = 0; i < MAX_FD; i++) {
        if (proc->fd_table[i]) {
            proc->fd_table[i]->ref_count--;

            if (proc->fd_table[i]->ref_count == 0)
                fd_free(i);
        }
    }

    if (proc->cwd) {
         vfs_close(proc->cwd);
         proc->cwd = NULL;
    }

    if (proc->signals) {
        signal_destroy(proc->signals);
        proc->signals = NULL;
    }

    u32 flags = spinlock_acquire_irqsave(&sched_lock);

    proc->exit_code = code;
    proc->state = PROCESS_ZOMBIE;
    current_task->state = TASK_EXITED;

    extern process_t *init_process;
    process_t *child = proc->child;
    while (child) {
        process_t *next = child->sibling;
        child->parent = init_process;
        
        child->sibling = init_process->child;
        init_process->child = child;
        child = next;
    }

    proc->child = NULL;

    spinlock_release_irqrestore(&sched_lock, flags);

    if (parent) {
        // Send SIGCHLD to parent
        signal_send(parent, SIGCHLD);
        wake_up(&parent->wait_queue);
    }

    schedule();
    while(1);
}

// waitpid options flags
#define WNOHANG    0x00000001  // Don't block if no child has exited
#define WUNTRACED  0x00000002  // Also return for stopped children
#define WCONTINUED 0x00000008  // Also return for continued children

#define __W_EXITCODE(ret, sig) ((ret) << 8 | (sig))
#define __WIFEXITED(status)    (((status) & 0x7f) == 0)
#define __WEXITSTATUS(status)  (((status) & 0xff00) >> 8)

i64 sys_wait3(i64 pid, int *status, int options) {
    process_t *proc = current_task->proc;
    if (!proc) return -1;

    while (true) {
        u32 flags = spinlock_acquire_irqsave(&sched_lock);

        process_t *child = proc->child;
        process_t *prev = NULL;
        process_t *target = NULL;
        bool has_matching_children = false;

        while (child) {
            bool matches = false;

            if (pid > 0)
                matches = (child->pid == (u64)pid);
            
            else if (pid == -1)
                matches = true;
            
            //TODO: Implement process groups
            else if (pid == 0)
                matches = true;
            
            else if (pid < -1)
                matches = true;

            if (matches) {
                has_matching_children = true;

                if (child->state == PROCESS_ZOMBIE) {
                    target = child;
                    break;
                }
            }

            prev = child;
            child = child->sibling;
        }

        if (target) {
            if (status)
                *status = __W_EXITCODE(target->exit_code, 0);

            u64 zombie_pid = target->pid;

            if (prev) {
                process_t *p = proc->child;
                process_t *pp = NULL;
                while (p && p != target) {
                    pp = p;
                    p = p->sibling;
                }

                if (pp) pp->sibling = target->sibling;
                else proc->child = target->sibling;
            } else {
                proc->child = target->sibling;
            }

            spinlock_release_irqrestore(&sched_lock, flags);
            
            // Clean up the zombie
            if (target->mm) mm_destroy(target->mm);
            if (target->signals) signal_destroy(target->signals);
            pid_hash_remove(target);
            kfree(target);

            return zombie_pid;
        }

        if (!has_matching_children) {
            spinlock_release_irqrestore(&sched_lock, flags);
            return -1;  // -ECHILD
        }

        // WNOHANG: return immediately if no child has exited
        if (options & WNOHANG) {
            spinlock_release_irqrestore(&sched_lock, flags);
            return 0;  // No child has exited yet
        }

        spinlock_release_irqrestore(&sched_lock, flags);
        sleep_on(&proc->wait_queue, NULL);
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