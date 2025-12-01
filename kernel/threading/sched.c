#include <sched.h>
#include <heap.h>
#include <uart.h>
#include <string.h>

extern void ret_from_fork();
extern void cpu_switch_to(struct task* prev, struct task* next);

static task_t* current_task = NULL;
static task_t* task_list_head = NULL;
static task_t* task_list_tail = NULL;
static u64 pid_counter = 0;

void sched_init() {
    task_t* main_task = (task_t*)kmalloc(sizeof(task_t));
    memset(main_task, 0, sizeof(task_t));
    
    main_task->id = pid_counter++;
    main_task->state = TASK_RUNNING;
    main_task->next = NULL;

    current_task = main_task;
    task_list_head = main_task;
    task_list_tail = main_task;

    kprintf("[SCHED] Initialized. Main Task ID: %d\n", main_task->id);
}

void task_create(void (*entry_point)()) {
    task_t* t = (task_t*)kmalloc(sizeof(task_t));
    memset(t, 0, sizeof(task_t));
    
    // 4 KB stack as of now
    t->stack_page = kmalloc(4096);
    if (!t->stack_page) {
        kprintf("[SCHED] Failed to allocate stack!\n");
        kfree(t);
        return;
    }

    t->id = pid_counter++;
    t->state = TASK_READY;
    t->next = NULL;

    // Since the stack grows down SP will be at the end of the page.
    u64 stack_top = (u64)t->stack_page + 4096;
    
    t->context.sp = stack_top;
    t->context.lr  = (u64)ret_from_fork;
    t->context.x19 = (u64)entry_point;
    
    if (task_list_tail) {
        task_list_tail->next = t;
        task_list_tail = t;
    } else {
        task_list_head = t;
        task_list_tail = t;
    }

    kprintf("[SCHED] Created Task %d\n", t->id);
}

void task_exit() {
    current_task->state = TASK_EXITED;
    kprintf("[SCHED] Task %d exited.\n", current_task->id);

    schedule();
}

void schedule() {
    // Round Robin
    task_t* next_task = current_task->next;

    while (1) {
        if (!next_task)
            next_task = task_list_head;

        if (next_task == current_task) {
            if (next_task->state == TASK_EXITED) {
                kprintf("[SCHED] All tasks dead! Halting.\n");
                while(1) asm volatile("wfi");
            }

            // Only task left running. Continue running.
            return;
        }

        if (next_task->state != TASK_EXITED) {
            break;
        }

        next_task = next_task->next;
    }

    // Switch
    task_t* prev_task = current_task;
    current_task = next_task;

    // kprintf("[SCHED] Switch %d -> %d\n", prev_task->id, next_task->id);
    cpu_switch_to(prev_task, next_task);
}