#include <sched.h>
#include <heap.h>
#include <kio.h>
#include <string.h>
#include <spinlock.h>
#include <sync.h>
#include <vmm.h>

extern void ret_from_fork();
extern void cpu_switch_to(struct task* prev, struct task* next);

static task_t *runqueues[COUNT];
static task_t *runqueues_tail[COUNT];

task_t *current_task = NULL;
static u64 pid_counter = 1;
static u64 tid_counter = 1;

static spinlock_t sched_lock = 0;

static task_t *zombie_head = NULL;
static wait_queue_t reaper_wq = NULL;

void sched_unlock_release() {
    spinlock_release_irqrestore(&sched_lock, 0); 
#ifdef ARM
    asm volatile("msr daifclr, #2");
#endif
}

void idle() {
    while (true) {
        asm volatile("wfi");
        schedule();
    }
}

// The Reaper Thread cleans up zombie tasks
void reaper_thread() {
    while (true) {
        u32 flags = spinlock_acquire_irqsave(&sched_lock);

        // If no zombies, sleep until one arrives
        if (zombie_head == NULL) {
            spinlock_release_irqrestore(&sched_lock, flags);
            sleep_on(&reaper_wq, NULL);
            continue;
        }

        // Detach the list of zombies to process them without holding the lock
        task_t *zombies_to_free = zombie_head;
        zombie_head = NULL;

        spinlock_release_irqrestore(&sched_lock, flags);

        // Free resources
        while (zombies_to_free) {
            task_t *next = zombies_to_free->next;
            
            kprintf("[ [CReaper [W] Cleaning up PID %d\n", zombies_to_free->id);
            kfree(zombies_to_free->stack_page);
            kfree(zombies_to_free);
            
            zombies_to_free = next;
        }
    }
}

void sched_init() {
    // Clears all queues
    for (int i = 0; i < COUNT; i++) {
        runqueues[i] = NULL;
        runqueues_tail[i] = NULL;
    }

    task_create(idle, IDLE, NULL);

    current_task = runqueues[IDLE];

    task_create(reaper_thread, HIGH, NULL);

    kprintf("[ [CSCHED[W ] Multi-Queue Scheduler Initialized (%d Levels).\n", COUNT);
    kprintf("[ [CSCHED[W ] Reaper Thread started.\n");
}

process_t *process_create(const char *name, void (*entry_point)(), task_priority priority) {
    process_t *proc = (process_t*)kmalloc(sizeof(process_t));
    if (!proc) {
        kprintf("[ [CSCHED [W] [RFailed to allocate memory for new process[W");
        return NULL;
    }
    
    memset(proc, 0, sizeof(process_t));

    proc->pid = pid_counter++;
    strncpy(proc->name, name, 63);

    //TODO: implement vmm for user space
    proc->page_table = NULL;

    task_create(entry_point, priority, proc);
    kprintf("[ [CSCHED[W ] Created Process:\tPID: %d\tName: %s\n", proc->pid, proc->name);
}

void task_create(void (*entry_point)(), task_priority priority, struct process *proc) {
    if (priority >= COUNT) priority = NORMAL;

    task_t* t = (task_t*)kmalloc(sizeof(task_t));
    if (!t) return;
    memset(t, 0, sizeof(task_t));
    
    // 4 KB stack as of now
    t->stack_page = kmalloc(4096);
    if (!t->stack_page) {
        kprintf("[ [CSCHED[W ] [RFailed to allocate stack![W\n");
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
    kprintf("[ [CSCHED[W ] Created Kernel Thread:\tID: %d\tPriority: %d\n", t->id, priority);
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
    kprintf("[ [CSCHED[W ] Task %d exited.\n", current_task->id);

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
        
        if (prev_task->state == TASK_EXITED) {
            // Add to zombie list
            prev_task->next = zombie_head;
            zombie_head = prev_task;

            // Wake up Reaper if it is sleeping
            if (reaper_wq) {
                task_t *reaper = reaper_wq;
                reaper_wq = reaper->next_wait;
                
                reaper->next_wait = NULL;
                reaper->state = TASK_READY;
                reaper->next = NULL;

                task_priority p = reaper->priority;
                if (runqueues[p] == NULL) {
                    runqueues[p] = reaper;
                    runqueues_tail[p] = reaper;
                } else {
                    runqueues_tail[p]->next = reaper;
                    runqueues_tail[p] = reaper;
                }
            }
        }
        
        current_task = next_task;

        if (current_task->state == TASK_READY) {
            current_task->state = TASK_RUNNING;
        }

        u64 *next_pt = (next_task->proc) ? next_task->proc->page_table : NULL;
        u64 *prev_pt = (prev_task->proc) ? prev_task->proc->page_table : NULL;

        if (next_pt && next_pt != prev_pt) {
#ifdef ARM
            // Physical address
            asm volatile("msr ttbr0_el1, %0" :: "r"(V2P(next_pt)));
            asm volatile("tlbi vmalle1is"); 
            asm volatile("dsb ish");
            asm volatile("isb");
#endif
        }

        cpu_switch_to(prev_task, next_task);
    }

    spinlock_release_irqrestore(&sched_lock, flags);
}