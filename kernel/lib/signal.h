#ifndef SIGNAL_H
#define SIGNAL_H

#include <lib.h>
#include <syscalls.h>
#include <sched.h>

// Standard POSIX signals
#define SIGHUP      1   // Hangup
#define SIGINT      2   // Interrupt (Ctrl+C)
#define SIGQUIT     3   // Quit (Ctrl+\)
#define SIGILL      4   // Illegal instruction
#define SIGTRAP     5   // Trace/breakpoint trap
#define SIGABRT     6   // Abort
#define SIGBUS      7   // Bus error
#define SIGFPE      8   // Floating point exception
#define SIGKILL     9   // Kill (cannot be caught)
#define SIGUSR1     10  // User defined 1
#define SIGSEGV     11  // Segmentation fault
#define SIGUSR2     12  // User defined 2
#define SIGPIPE     13  // Broken pipe
#define SIGALRM     14  // Alarm clock
#define SIGTERM     15  // Termination
#define SIGSTKFLT   16  // Stack fault
#define SIGCHLD     17  // Child status changed
#define SIGCONT     18  // Continue if stopped
#define SIGSTOP     19  // Stop (cannot be caught)
#define SIGTSTP     20  // Terminal stop (Ctrl+Z)
#define SIGTTIN     21  // Background read from tty
#define SIGTTOU     22  // Background write to tty
#define SIGURG      23  // Urgent data on socket
#define SIGXCPU     24  // CPU time limit exceeded
#define SIGXFSZ     25  // File size limit exceeded
#define SIGVTALRM   26  // Virtual timer expired
#define SIGPROF     27  // Profiling timer expired
#define SIGWINCH    28  // Window size change
#define SIGIO       29  // I/O possible
#define SIGPWR      30  // Power failure
#define SIGSYS      31  // Bad system call

// Signal actions
#define SIG_DFL     ((sighandler_t)0)   // Default action
#define SIG_IGN     ((sighandler_t)1)   // Ignore signal
#define SIG_ERR     ((sighandler_t)-1)  // Error return

// sigaction flags
#define SA_NOCLDSTOP    0x00000001  // Don't send SIGCHLD when children stop
#define SA_NOCLDWAIT    0x00000002  // Don't create zombie on child death
#define SA_SIGINFO      0x00000004  // Use sa_sigaction instead of sa_handler
#define SA_ONSTACK      0x08000000  // Use alternate signal stack
#define SA_RESTART      0x10000000  // Restart syscall on signal return
#define SA_NODEFER      0x40000000  // Don't block signal while handling
#define SA_RESETHAND    0x80000000  // Reset to SIG_DFL on entry

// sigprocmask how values
#define SIG_BLOCK       0   // Block signals in set
#define SIG_UNBLOCK     1   // Unblock signals in set
#define SIG_SETMASK     2   // Set mask to set

// Signal utility macros
#define sigmask(sig)            (1U << ((sig) - 1))
#define sigaddset(set, sig)     (*(set) |= sigmask(sig))
#define sigdelset(set, sig)     (*(set) &= ~sigmask(sig))
#define sigemptyset(set)        (*(set) = 0)
#define sigfillset(set)         (*(set) = ~0U)
#define sigismember(set, sig)   ((*(set) & sigmask(sig)) != 0)

// Uncatchable/unblockable signals
#define SIG_KERNEL_ONLY_MASK (sigmask(SIGKILL) | sigmask(SIGSTOP))

// Default action types
#define SIG_ACTION_TERM     0   // Terminate process
#define SIG_ACTION_IGN      1   // Ignore
#define SIG_ACTION_CORE     2   // Terminate + core dump
#define SIG_ACTION_STOP     3   // Stop process
#define SIG_ACTION_CONT     4   // Continue process

signal_struct_t* signal_create(void);
void signal_destroy(signal_struct_t* sig);
signal_struct_t* signal_copy(signal_struct_t* old);
int signal_send(process_t *proc, int sig);
int signal_send_pid(u64 pid, int sig);
void signal_check_pending(trapframe_t* tf);
int signal_get_default_action(int sig);

#endif