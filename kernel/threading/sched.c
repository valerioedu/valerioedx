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
    // 1. Create a task struct for the *currently running* kernel (Task 0)
    // We do NOT allocate a new stack for it, we assume we are already on one.
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
    
    // 1. Allocate a 4KB Stack for the new task
    t->stack_page = kmalloc(4096);
    if (!t->stack_page) {
        kprintf("[SCHED] Failed to allocate stack!\n");
        kfree(t);
        return;
    }

    t->id = pid_counter++;
    t->state = TASK_READY;
    t->next = NULL;

    // 2. Setup Context
    // Stack grows DOWN. Point SP to the END of the page.
    u64 stack_top = (u64)t->stack_page + 4096;
    
    t->context.sp = stack_top;
    t->context.lr  = (u64)ret_from_fork;
    t->context.lr = (u64)entry_point; // When 'ret' executes in switch.S, jump here
    
    // 3. Add to Linked List
    if (task_list_tail) {
        task_list_tail->next = t;
        task_list_tail = t;
    } else {
        task_list_head = t;
        task_list_tail = t;
    }

    kprintf("[SCHED] Created Task %d\n", t->id);
}

void schedule() {
    // 1. Get next task (Round Robin)
    task_t* next_task = current_task->next;
    
    // If end of list, loop back to head
    if (!next_task) {
        next_task = task_list_head;
    }

    // Don't switch if it's the same task
    if (next_task == current_task) return;

    // 2. Perform Switch
    task_t* prev_task = current_task;
    current_task = next_task;

    // kprintf("[SCHED] Switch %d -> %d\n", prev_task->id, next_task->id);
    cpu_switch_to(prev_task, next_task);
}