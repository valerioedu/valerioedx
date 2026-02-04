#ifndef SCHED_H
#define SCHED_H

#include <lib.h>
#include <spinlock.h>
#include <file.h>

#define MAX_FD 1024
#define NSIG   32
#define NGROUPS_MAX 32

// Forward declarations
struct mm_struct;
typedef u32 sigset_t;
typedef void (*sighandler_t)(int);

typedef struct sigaction {
    sighandler_t sa_handler;    // Signal handler or SIG_DFL/SIG_IGN
    sigset_t sa_mask;           // Signals to block during handler
    int sa_flags;               // Flags
} sigaction_t;

typedef struct signal_struct {
    sigaction_t actions[NSIG];  // Signal handlers
    sigset_t pending;           // Pending signals
    sigset_t blocked;           // Blocked signals
} signal_struct_t;

// Signal frame for user-space signal delivery (saved on user stack)
typedef struct sigframe {
    u64 x[31];          // General purpose registers
    u64 sp;             // Stack pointer
    u64 pc;             // Program counter (return address)
    u64 pstate;         // Processor state
    sigset_t old_mask;  // Previous signal mask
    int sig;            // Signal number
    u64 retcode[2];     // sigreturn trampoline
} sigframe_t;

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
    TASK_BLOCKED = 3,
    TASK_STOPPED = 4
} task_state;

typedef enum process_state {
    PROCESS_ACTIVE = 0,
    PROCESS_ZOMBIE = 1,
    PROCESS_KILLED = 2,
    PROCESS_STOPPED = 3
} process_state;

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
    struct task* next_wait;     // Pointer to wait queue
    struct task* thread_next;
    void* stack_page;           // Pointer to the allocated stack memory
    struct process *proc;
} task_t;

typedef task_t* wait_queue_t;
typedef u32 uid_t;
typedef u32 gid_t;
struct tty;

// Process
typedef struct process {
    u64 pid;
    u64 sid;
    u64 pgid;
    bool session_leader;
    gid_t groups[NGROUPS_MAX];
    int ngroups;
    uid_t uid;
    gid_t gid;
    uid_t euid;
    gid_t egid;
    struct mm_struct *mm;       // Memory management struct
    struct file *fd_table[MAX_FD];
    char name[64];
    inode_t *cwd;
    struct process *parent;     // Pointer to parent process
    struct process *child;      // Head of the children processes
    struct process *sibling;    // Next sibling
    process_state state;
    int exit_code;
    wait_queue_t wait_queue;
    struct process *hash_next;
    struct signal_struct *signals;  // Signal handling state
    struct tty *controlling_tty;
    task_t *threads;
} process_t;

void sched_init();
void task_create(void (*entry_point)(), task_priority priority, struct process *proc);
void schedule();
void task_exit();
void cpu_switch_to(struct task* prev, struct task* next);
void sleep_on(wait_queue_t* queue, spinlock_t* release_lock);
void wake_up(wait_queue_t* queue);
void task_wake_up_process(process_t *proc);
process_t *process_create(const char *name, void (*entry_point)(), task_priority priority);
process_t *find_process_by_pid(u64 pid);
void pid_hash_insert(process_t *proc);
void pid_hash_remove(process_t *proc);

#endif