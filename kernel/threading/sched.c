#include <sched.h>
#include <heap.h>
#include <kio.h>
#include <string.h>

extern void ret_from_fork();
extern void cpu_switch_to(struct task* prev, struct task* next);

static task_t *runqueues[COUNT];
static task_t *runqueues_tail[COUNT];

static task_t *current_task = NULL;
static u64 pid_counter = 0;

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
    
    if (runqueues[priority] == NULL) {
        runqueues[priority] = t;
        runqueues_tail[priority] = t;
    } else {
        runqueues_tail[priority]->next = t;
        runqueues_tail[priority] = t;
    }

    kprintf("[ [CSCHED[W ] Created Process:\tID: %d\tPriority: %d\n", t->id, priority);
}

void sleep_on(wait_queue_t* queue) {
    current_task->state = TASK_BLOCKED;

    current_task->next_wait = *queue;
    *queue = current_task;

    schedule();
}

void wake_up(wait_queue_t* queue) {
    if (*queue == NULL) return;

    task_t* t = *queue;
    *queue = t->next_wait;

    t->next_wait = NULL;

    t->state = TASK_READY;
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
    // Round Robin
    task_t* prev_task = current_task;
    task_t* next_task = NULL;

    // Scan from Highest Priority down to Lowest
    // Enums are integers, so this loop works perfectly
    for (int i = COUNT - 1; i >= 0; i--) {
        // Skip IDLE queue in the main scan to avoid rotating it unnecessarily
        if (i == IDLE) continue;

        if (runqueues[i] != NULL) {
            next_task = runqueues[i];
            
            // If we are still running the same priority class, rotate 
            // to ensure fairness among tasks of equal importance.
            if (prev_task && prev_task->priority == i && prev_task->state == TASK_RUNNING) {
                rotate_queue(i);
                next_task = runqueues[i];
            }
            break; 
        }
    }

    // Fallback to idle if everything else is blocked/empty
    if (!next_task) {
        next_task = runqueues[IDLE];
        // Do not rotate the idle queue
    }

    if (next_task != prev_task) {
        current_task = next_task;
        current_task->state = TASK_RUNNING;
        cpu_switch_to(prev_task, next_task);
    }
}