#include <sched.h>
#include <heap.h>
#include <kio.h>
#include <string.h>
#include <spinlock.h>
#include <sync.h>
#include <vmm.h>
#include <vma.h>
#include <file.h>

#define PID_HASH_SIZE 1024

extern void ret_from_fork();
extern void cpu_switch_to(struct task* prev, struct task* next);

task_t *runqueues[COUNT];
task_t *runqueues_tail[COUNT];

task_t *current_task = NULL;
u64 pid_counter = 1;
u64 tid_counter = 1;

spinlock_t sched_lock = 0;

static process_t *pid_hash[PID_HASH_SIZE];

// Store the kernel's root page table for TTBR1
static u64 kernel_ttbr1 = 0;

void sched_unlock_release() {
    spinlock_release_irqrestore(&sched_lock, 0); 
#ifdef ARM
    asm volatile("msr daifclr, #2");
#endif
}

void idle() {
    while (true) {
#ifdef ARM
        asm volatile("wfi");
#else
        asm volatile("hlt");
#endif
        schedule();
    }
}

void sched_init() {
    // Clears all queues
    for (int i = 0; i < COUNT; i++) {
        runqueues[i] = NULL;
        runqueues_tail[i] = NULL;
    }

#ifdef ARM
    // Store the kernel's TTBR1 value
    asm volatile("mrs %0, ttbr1_el1" : "=r"(kernel_ttbr1));
#endif

    task_create(idle, IDLE, NULL);

    current_task = runqueues[IDLE];

    kprintf("[ [CSCHED[W ] Multi-Queue Scheduler Initialized (%d Levels).\n", COUNT);
}

process_t *process_create(const char *name, void (*entry_point)(), task_priority priority) {
    process_t *proc = (process_t*)kmalloc(sizeof(process_t));
    if (!proc) {
        kprintf("[ [RSCHED [W] Failed to allocate memory for new process\n");
        return NULL;
    }
    
    memset(proc, 0, sizeof(process_t));

    proc->pid = pid_counter++;
    pid_hash_insert(proc);
    strncpy(proc->name, name, 63);

    extern int setup_standard_fds(process_t* proc);

    setup_standard_fds(proc);

    // Create address space
    proc->mm = mm_create();
    if (!proc->mm) {
        kfree(proc);
        kprintf("[ [RSCHED [W] Failed to create address space\n");
        return NULL;
    }

    task_create(entry_point, priority, proc);    
    proc->cwd = vfs_root;
    return proc;
}

void task_create(void (*entry_point)(), task_priority priority, struct process *proc) {
    if (priority >= COUNT) priority = NORMAL;

    task_t* t = (task_t*)kmalloc(sizeof(task_t));
    if (!t) return;
    memset(t, 0, sizeof(task_t));
    
    // 4 KB stack as of now
    t->stack_page = kmalloc(4096);
    if (!t->stack_page) {
        kprintf("[ [RSCHED[W ] Failed to allocate stack!\n");
        kfree(t);
        return;
    }

    t->id = tid_counter++;
    t->state = TASK_READY;
    t->priority = priority;
    t->next = NULL;
    t->next_wait = NULL;
    t->proc = proc;

    // Since the stack grows down SP will be at the end of the page.
    u64 stack_top = (u64)t->stack_page + 4096;
    
    t->context.sp = stack_top;
    t->context.lr  = (u64)ret_from_fork;
    t->context.x19 = (u64)entry_point;

    u32 flags = spinlock_acquire_irqsave(&sched_lock);
    
    if (runqueues[priority] == NULL) {
        runqueues[priority] = t;
        runqueues_tail[priority] = t;
    } else {
        runqueues_tail[priority]->next = t;
        runqueues_tail[priority] = t;
    }

    spinlock_release_irqrestore(&sched_lock, flags);
}

void sleep_on(wait_queue_t* queue, spinlock_t* release_lock) {
    u32 flags = spinlock_acquire_irqsave(&sched_lock);

    current_task->state = TASK_BLOCKED;

    current_task->next_wait = *queue;
    *queue = current_task;

    task_priority prio = current_task->priority;
    if (runqueues[prio] == current_task) {
        runqueues[prio] = current_task->next;
        if (runqueues[prio] == NULL) {
            runqueues_tail[prio] = NULL;
        }
    }
    
    if (release_lock) 
        __atomic_store_n(release_lock, 0, __ATOMIC_RELEASE);

    spinlock_release_irqrestore(&sched_lock, flags);
    schedule();
}

void wake_up(wait_queue_t* queue) {
    u32 flags = spinlock_acquire_irqsave(&sched_lock);
    if (*queue == NULL) {
        spinlock_release_irqrestore(&sched_lock, flags);
        return;
    }

    task_t* t = *queue;
    *queue = t->next_wait;

    t->next_wait = NULL;

    t->state = TASK_READY;

    task_priority prio = t->priority;
    t->next = NULL;
    
    if (runqueues[prio] == NULL) {
        runqueues[prio] = t;
        runqueues_tail[prio] = t;
    } else {
        runqueues_tail[prio]->next = t;
        runqueues_tail[prio] = t;
    }
    spinlock_release_irqrestore(&sched_lock, flags);
}

void task_exit() {
    current_task->state = TASK_EXITED;
    schedule();
}

void rotate_queue(task_priority priority) {
    task_t *head = runqueues[priority];
    if (!head || !head->next) return;

    runqueues[priority] = head->next;
    head->next = NULL;

    runqueues_tail[priority]->next = head;
    runqueues_tail[priority] = head;
}

void schedule() {
    u32 flags = spinlock_acquire_irqsave(&sched_lock);

    // Round Robin
    task_t* prev_task = current_task;
    task_t* next_task = NULL;

    // Scan priorities
    for (int i = COUNT - 1; i >= 0; i--) {
        if (i == IDLE) continue;

        task_t* curr = runqueues[i];

        while (curr && curr->state == TASK_EXITED) {
            runqueues[i] = curr->next;
            
            // Tail clean up if queue becomes empty
            if (runqueues[i] == NULL) {
                runqueues_tail[i] = NULL;
            }

            curr = runqueues[i];
        }

        if (runqueues[i] && prev_task && 
            prev_task->priority == i && 
            prev_task->state == TASK_RUNNING) {
            rotate_queue(i);
        }

        curr = runqueues[i];
        while (curr) {
            if (curr->state == TASK_READY || curr->state == TASK_RUNNING) {
                next_task = curr;
                break;
            }
            curr = curr->next;
        }

        if (next_task) break;
    }

    if (!next_task) {
        next_task = runqueues[IDLE];
    }

    if (next_task != prev_task) {
        current_task = next_task;

        if (current_task->state == TASK_READY) {
            current_task->state = TASK_RUNNING;
        }

        mm_struct_t *next_mm = (next_task->proc) ? next_task->proc->mm : NULL;

        cpu_switch_to(prev_task, next_task);

        if (current_task->proc && current_task->proc->mm) {
            asm volatile(
                "msr ttbr0_el1, %0\n"
                "isb\n"
                "tlbi vmalle1is\n"
                "dsb ish\n"
                "isb\n"
                :: "r"((u64)current_task->proc->mm->page_table)
                : "memory"
            );
        }
    }

    spinlock_release_irqrestore(&sched_lock, flags);
}

void switch_to_child_mm() {
    if (current_task && current_task->proc && current_task->proc->mm) {
        u64 ttbr = (u64)current_task->proc->mm->page_table;
        asm volatile(
            "msr ttbr0_el1, %0\n"
            "isb\n"
            "tlbi vmalle1is\n"
            "dsb ish\n"
            "isb\n"
            :: "r"(ttbr)
            : "memory"
        );
    }
}

void pid_hash_insert(process_t *proc) {
    u64 idx = proc->pid % PID_HASH_SIZE;
    proc->hash_next = pid_hash[idx];
    pid_hash[idx] = proc;
}

void pid_hash_remove(process_t *proc) {
    u64 idx = proc->pid % PID_HASH_SIZE;
    process_t **curr = &pid_hash[idx];

    while (*curr) {
        if (*curr == proc) {
            *curr = proc->hash_next;
            proc->hash_next = NULL; 
            return;
        }

        curr = &(*curr)->hash_next;
    }
}

process_t *find_process_by_pid(u64 pid) {
    u64 idx = pid % PID_HASH_SIZE;

    u32 flags = spinlock_acquire_irqsave(&sched_lock);
    
    process_t *proc = pid_hash[idx];
    while (proc) {
        if (proc->pid == pid) {
            spinlock_release_irqrestore(&sched_lock, flags);
            return proc;
        }
        
        proc = proc->hash_next;
    }

    spinlock_release_irqrestore(&sched_lock, flags);
    return NULL;
}