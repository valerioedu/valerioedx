#ifndef SCHED_H
#define SCHED_H

#include <lib.h>

typedef enum task_priority {
    IDLE = 0,
    LOW,
    NORMAL,
    HIGH,
    REALTIME,

    /* Automatically 5*/
    COUNT
} task_priority;

typedef enum task_state {
    TASK_RUNNING = 0,
    TASK_READY = 1,
    TASK_EXITED = 2,
    TASK_BLOCKED = 3
} task_state;

// Architecture-specific context (Callee-saved registers for AArch64)
// These are the registers that a function must preserve. We save them when switching.
struct cpu_context {
    u64 x19;
    u64 x20;
    u64 x21;
    u64 x22;
    u64 x23;
    u64 x24;
    u64 x25;
    u64 x26;
    u64 x27;
    u64 x28;
    u64 fp;  // x29
    u64 lr;  // x30
    u64 sp;  
};

typedef struct task {
    struct cpu_context context; // Must be at offset 0 for easier assembly
    u64 id;
    task_state state;
    task_priority priority;
    struct task* next;          // Linked list pointer
    struct task* next_wait;     // Pointer to 
    void* stack_page;           // Pointer to the allocated stack memory
} task_t;

typedef task_t* wait_queue_t;

void sched_init();
void task_create(void (*entry_point)(), task_priority priority);
void schedule();
void task_exit();
void cpu_switch_to(struct task* prev, struct task* next); // Assembly function

#endif