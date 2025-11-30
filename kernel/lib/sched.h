#ifndef SCHED_H
#define SCHED_H

#include <lib.h>

#define TASK_RUNNING 0
#define TASK_READY   1
#define TASK_EXITED  2

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
    u64 state;
    struct task* next;          // Linked list pointer
    void* stack_page;           // Pointer to the allocated stack memory
} task_t;

void sched_init();
void task_create(void (*entry_point)());
void schedule();
void cpu_switch_to(struct task* prev, struct task* next); // Assembly function

#endif