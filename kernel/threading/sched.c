#include <sched.h>
#include <heap.h>
#include <kio.h>
#include <string.h>
#include <spinlock.h>
#include <sync.h>
#include <vmm.h>
#include <vma.h>
#include <file.h>
#include <signal.h>
#include <tty.h>

#define PID_HASH_SIZE 1024

extern void ret_from_fork();
extern void cpu_switch_to(struct task* prev, struct task* next);

task_t *runqueues[COUNT];
task_t *runqueues_tail[COUNT];
task_t *sleep_queue = NULL;

task_t *current_task = NULL;
u64 pid_counter = 1;
u64 tid_counter = 1;

spinlock_t sched_lock = 0;
u32 active_priorities = 0; // Bitmask for priorities

static process_t *pid_hash[PID_HASH_SIZE];

// Store the kernel's root page table for TTBR1
static u64 kernel_ttbr1 = 0;

void sched_enqueue_task(task_t *t) {
    task_priority prio = t->priority;
    t->next = NULL;
    t->prev = NULL;
    
    if (runqueues[prio] == NULL) {
        runqueues[prio] = t;
        runqueues_tail[prio] = t;
    } else {
        t->prev = runqueues_tail[prio];
        runqueues_tail[prio]->next = t;
        runqueues_tail[prio] = t;
    }
    
    active_priorities |= (1 << prio);
}

void sched_dequeue_task(task_t *t) {
    task_priority prio = t->priority;
    
    if (t->prev) {
        t->prev->next = t->next;
    } else {
        if (runqueues[prio] == t)
            runqueues[prio] = t->next; 
    }

    if (t->next) {
        t->next->prev = t->prev;
    } else {
        if (runqueues_tail[prio] == t)
            runqueues_tail[prio] = t->prev; 
    }

    t->next = NULL;
    t->prev = NULL;

    if (runqueues[prio] == NULL) {
        active_priorities &= ~(1 << prio);
    }
}

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
    strncpy(proc->name, name, 511);

    extern int setup_standard_fds(process_t* proc);

    setup_standard_fds(proc);

    // Create address space
    proc->mm = mm_create();
    if (!proc->mm) {
        kfree(proc);
        kprintf("[ [RSCHED [W] Failed to create address space\n");
        return NULL;
    }

    // Initialize signal handling
    proc->signals = signal_create();
    if (!proc->signals) {
        mm_destroy(proc->mm);
        kfree(proc);
        kprintf("[ [RSCHED [W] Failed to create signal struct\n");
        return NULL;
    }

    task_create(entry_point, priority, proc);    
    proc->cwd = vfs_root;
    proc->gid = 0;
    proc->uid = 0;
    proc->pgid = proc->pid;
    proc->sid = proc->pid;
    proc->session_leader = true;
    
    // first process gets the console
    proc->controlling_tty = &tty_console;
    
    u32 flags = spinlock_acquire_irqsave(&tty_console.lock);
    tty_console.session = proc;
    tty_console.session_id = proc->sid;
    tty_console.pgrp = proc->pgid;
    spinlock_release_irqrestore(&tty_console.lock, flags);

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
    t->prev = NULL;
    t->next_wait = NULL;
    t->proc = proc;

    // Since the stack grows down SP will be at the end of the page.
    u64 stack_top = (u64)t->stack_page + 4096;
    
    t->context.sp = stack_top;
    t->context.lr  = (u64)ret_from_fork;
    t->context.x19 = (u64)entry_point;

    u32 flags = spinlock_acquire_irqsave(&sched_lock);

    if (proc) {
        t->thread_next = proc->threads;
        proc->threads = t;
    }
    
    sched_enqueue_task(t);

    spinlock_release_irqrestore(&sched_lock, flags);
}

void sleep_on(wait_queue_t* queue, spinlock_t* release_lock) {
    u32 flags = spinlock_acquire_irqsave(&sched_lock);

    current_task->state = TASK_BLOCKED;

    current_task->next_wait = *queue;
    *queue = current_task;

    sched_dequeue_task(current_task);
    
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

    sched_enqueue_task(t);

    spinlock_release_irqrestore(&sched_lock, flags);
}

void task_wake_up_process(process_t *proc) {
    if (!proc) return;

    u32 flags = spinlock_acquire_irqsave(&sched_lock);

    task_t *t = proc->threads;
    while (t) {
        if (t->state == TASK_STOPPED) {
            t->state = TASK_READY;
            sched_enqueue_task(t);
        }

        t = t->thread_next;
    }

    spinlock_release_irqrestore(&sched_lock, flags);
}

void task_exit() {
    u32 flags = spinlock_acquire_irqsave(&sched_lock);
    current_task->state = TASK_EXITED;
    sched_dequeue_task(current_task);
    spinlock_release_irqrestore(&sched_lock, flags);
    
    schedule();
}

void rotate_queue(task_priority priority) {
    task_t *head = runqueues[priority];
    if (!head || !head->next) return;

    sched_dequeue_task(head);
    sched_enqueue_task(head);
}

void schedule() {
    u32 flags = spinlock_acquire_irqsave(&sched_lock);

    // Round Robin
    task_t* prev_task = current_task;
    task_t* next_task = NULL;

    if (prev_task && prev_task->state == TASK_RUNNING)
        rotate_queue(prev_task->priority);


    if (active_priorities == 0)
        next_task = runqueues[IDLE];
    
    else {
        int highest_prio = 31 - __builtin_clz(active_priorities);
        if (highest_prio >= COUNT) highest_prio = COUNT - 1;
        next_task = runqueues[highest_prio];
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

int signal_send_group(u64 pgid, int sig) {
    if (sig < 0 || sig > NSIG) return -1;

    int count = 0;
    u32 flags = spinlock_acquire_irqsave(&sched_lock);

    for (int i = 0; i < PID_HASH_SIZE; i++) {
        process_t *proc = pid_hash[i];

        while (proc) {
            if (proc->pgid == pgid) {
                if (signal_send(proc, sig) == 0)
                    count++;
            }

            proc = proc->hash_next;
        }
    }

    spinlock_release_irqrestore(&sched_lock, flags);
    
    return (count > 0) ? 0 : -1;
}

void sched_check_sleeping_tasks(u64 now) {
    u32 flags = spinlock_acquire_irqsave(&sched_lock);

    task_t **curr = &sleep_queue;
    while (*curr) {
        task_t *t = *curr;
        if (now >= t->wake_tick) {
            *curr = t->next_wait; 
            t->next_wait = NULL;

            t->state = TASK_READY;
            sched_enqueue_task(t);
        } else curr = &(*curr)->next_wait;
    }

    spinlock_release_irqrestore(&sched_lock, flags);
}

void task_sleep_ticks(u64 ticks) {
    u32 flags = spinlock_acquire_irqsave(&sched_lock);

    extern volatile u64 jiffies;
    current_task->wake_tick = jiffies + ticks;
    current_task->state = TASK_SLEEPING;

    sched_dequeue_task(current_task);

    current_task->next_wait = sleep_queue;
    sleep_queue = current_task;

    spinlock_release_irqrestore(&sched_lock, flags);
    
    schedule();
}

// Returns: 0 if woken by wake_up(), 1 if timeout expired
int sleep_on_timeout(wait_queue_t* queue, u64 timeout_ticks) {
    if (timeout_ticks == 0) return 1;
    
    u32 flags = spinlock_acquire_irqsave(&sched_lock);

    extern volatile u64 jiffies;
    current_task->wake_tick = jiffies + timeout_ticks;
    current_task->state = TASK_BLOCKED;
    current_task->flags |= TASK_TIMEOUT;

    current_task->next_wait = *queue;
    *queue = current_task;

    current_task->sleep_next = sleep_queue;
    sleep_queue = current_task;

    sched_dequeue_task(current_task);

    spinlock_release_irqrestore(&sched_lock, flags);
    schedule();

    flags = spinlock_acquire_irqsave(&sched_lock);
    int timed_out = (current_task->flags & TASK_TIMEDOUT) != 0;
    current_task->flags &= ~(TASK_TIMEOUT | TASK_TIMEDOUT);
    spinlock_release_irqrestore(&sched_lock, flags);

    return timed_out;
}