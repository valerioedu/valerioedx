#include <sched.h>
#include <heap.h>
#include <kio.h>
#include <string.h>
#include <spinlock.h>

//TODO: Make a reaper for zombie tasks 

extern void ret_from_fork();
extern void cpu_switch_to(struct task* prev, struct task* next);

static task_t *runqueues[COUNT];
static task_t *runqueues_tail[COUNT];

static task_t *current_task = NULL;
static task_t *zombie_task = NULL;
static u64 pid_counter = 0;

static spinlock_t sched_lock = 0;

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

void sched_init() {
    // Clears all queues
    for (int i = 0; i < COUNT; i++) {
        runqueues[i] = NULL;
        runqueues_tail[i] = NULL;
    }

    task_create(idle, IDLE);

    current_task = runqueues[IDLE];

    kprintf("[ [CSCHED[W ] Multi-Queue Scheduler Initialized (%d Levels).\n", COUNT);
}

void task_create(void (*entry_point)(), task_priority priority) {
    if (priority >= COUNT) priority = NORMAL;

    task_t* t = (task_t*)kmalloc(sizeof(task_t));
    memset(t, 0, sizeof(task_t));
    
    // 4 KB stack as of now
    t->stack_page = kmalloc(4096);
    if (!t->stack_page) {
        kprintf("[ [CSCHED[W ] [RFailed to allocate stack![W\n");
        kfree(t);
        return;
    }

    t->id = pid_counter++;
    t->state = TASK_READY;
    t->priority = priority;
    t->next = NULL;
    t->next_wait = NULL;

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
    kprintf("[ [CSCHED[W ] Created Process:\tID: %d\tPriority: %d\n", t->id, priority);
}

void sleep_on(wait_queue_t* queue) {
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

    if (zombie_task) {
        kfree(zombie_task->stack_page);
        kfree(zombie_task);
        zombie_task = NULL;
    }
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

        if (runqueues[i] != NULL) {
            next_task = runqueues[i];
            
            // Round Robin Rotation logic
            if (prev_task && prev_task->priority == i && prev_task->state == TASK_RUNNING) {
                rotate_queue(i);
                next_task = runqueues[i];
            }
            break; 
        }
    }

    if (!next_task) {
        next_task = runqueues[IDLE];
    }

    if (next_task != prev_task) {
        if (prev_task->state == TASK_EXITED) zombie_task = prev_task;
        
        current_task = next_task;

        if (current_task->state == TASK_READY) {
            current_task->state = TASK_RUNNING;
        }

        cpu_switch_to(prev_task, next_task);
    }

    spinlock_release_irqrestore(&sched_lock, flags);
}