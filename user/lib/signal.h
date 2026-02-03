#ifndef SIGNAL_H
#define SIGNAL_H

#include <sys/types.h>
#include <stdint.h>

typedef uint32_t sigset_t;
typedef void (*sighandler_t)(int);
typedef volatile int sig_atomic_t;

#define SIGHUP      1
#define SIGINT      2
#define SIGQUIT     3
#define SIGILL      4
#define SIGTRAP     5
#define SIGABRT     6
#define SIGBUS      7
#define SIGFPE      8
#define SIGKILL     9
#define SIGUSR1     10
#define SIGSEGV     11
#define SIGUSR2     12
#define SIGPIPE     13
#define SIGALRM     14
#define SIGTERM     15
#define SIGSTKFLT   16
#define SIGCHLD     17
#define SIGCONT     18
#define SIGSTOP     19
#define SIGTSTP     20
#define SIGTTIN     21
#define SIGTTOU     22
#define SIGURG      23
#define SIGXCPU     24
#define SIGXFSZ     25
#define SIGVTALRM   26
#define SIGPROF     27
#define SIGWINCH    28
#define SIGIO       29
#define SIGPWR      30
#define SIGSYS      31

#define SIG_DFL     ((sighandler_t)0)
#define SIG_IGN     ((sighandler_t)1)
#define SIG_ERR     ((sighandler_t)-1)

#define SA_NOCLDSTOP    0x00000001
#define SA_NOCLDWAIT    0x00000002
#define SA_SIGINFO      0x00000004
#define SA_ONSTACK      0x08000000
#define SA_RESTART      0x10000000
#define SA_NODEFER      0x40000000
#define SA_RESETHAND    0x80000000

#define SIG_BLOCK       0
#define SIG_UNBLOCK     1
#define SIG_SETMASK     2

#define sigmask(sig)            (1U << ((sig) - 1))
#define sigaddset(set, sig)     (*(set) |= sigmask(sig))
#define sigdelset(set, sig)     (*(set) &= ~sigmask(sig))
#define sigemptyset(set)        (*(set) = 0)
#define sigfillset(set)         (*(set) = ~0U)
#define sigismember(set, sig)   ((*(set) & sigmask(sig)) != 0)

union sigval {
    int sival_int;
    void *sival_ptr;
};

typedef struct siginfo_t {
    int si_signo;
    int si_code;
    int si_errno;
    pid_t si_pid;
    uid_t si_uid;
    void *si_addr;
    int si_status;
    union sigval si_value;
} siginfo_t;

struct sigaction {
    sighandler_t sa_handler;
    sigset_t sa_mask;
    int sa_flags;
    void (*sa_sigaction)(int, siginfo_t*, void*);
};

struct sigevent {
    int sigev_notify;
    int sigev_signo;
    union sigval sigev_value;
    void (*sigev_notify_function)(union sigval);
};

int kill(pid_t pid, int sig);
int sigaction(int sig, const struct sigaction *act, struct sigaction *oldact);
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
int sigpending(sigset_t *set);
int raise(int sig);
sighandler_t signal(int sig, sighandler_t handler);

#endif