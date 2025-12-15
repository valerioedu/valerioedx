#ifndef SCHED_H
#define SCHED_H

#include <lib.h>
#include <spinlock.h>
#include <file.h>

#define MAX_FD 1024

typedef enum task_priority {
    IDLE = 0,
    LOW,
    NORMAL,
    HIGH,
    REALTIME,

    /* Automatically 5 */
    COUNT
} task_priority;

typedef enum task_state {
    TASK_RUNNING = 0,
    TASK_READY = 1,
    TASK_EXITED = 2,
    TASK_BLOCKED = 3
} task_state;

// Architecture-specific context (Callee-saved registers for AArch64)
// These are the registers that a function must preserve.
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

    __uint128_t q8;
    __uint128_t q9;
    __uint128_t q10;
    __uint128_t q11;
    __uint128_t q12;
    __uint128_t q13;
    __uint128_t q14;
    __uint128_t q15;
};

struct process;

// Thread
typedef struct task {
    struct cpu_context context; // Must be at offset 0 for easier assembly
    u64 id;
    task_state state;
    task_priority priority;
    struct task* next;          // Linked list pointer
    struct task* next_wait;     // Pointer to 
    void* stack_page;           // Pointer to the allocated stack memory
    struct process *proc;
} task_t;

// Process
typedef struct process {
    u64 pid;
    u64 *page_table;
    struct file *fd_table[MAX_FD];
    char name[64];
} process_t;

typedef task_t* wait_queue_t;

void sched_init();
void task_create(void (*entry_point)(), task_priority priority, struct process *proc);
void schedule();
void task_exit();
void cpu_switch_to(struct task* prev, struct task* next);
void sleep_on(wait_queue_t* queue, spinlock_t* release_lock);
void wake_up(wait_queue_t* queue);

#endif