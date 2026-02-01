#include <signal.h>
#include <unistd.h>

int kill(pid_t pid, int sig) {
    register pid_t x0 asm("x0") = pid;
    register int x1 asm("x1") = sig;
    register long x8 asm("x8") = 37;
    asm volatile("svc #0" : "+r"(x0) : "r"(x1), "r"(x8) : "memory");
    return (int)x0;
}

int sigaction(int sig, const struct sigaction *act, struct sigaction *oldact) {
    register int x0 asm("x0") = sig;
    register const struct sigaction *x1 asm("x1") = act;
    register struct sigaction *x2 asm("x2") = oldact;
    register long x8 asm("x8") = 46;
    asm volatile("svc #0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x8) : "memory");
    return (int)x0;
}

int sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
    register int x0 asm("x0") = how;
    register const sigset_t *x1 asm("x1") = set;
    register sigset_t *x2 asm("x2") = oldset;
    register long x8 asm("x8") = 48;
    asm volatile("svc #0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x8) : "memory");
    return x0;
}
int sigpending(sigset_t *set) {
    register sigset_t *x0 asm("x0") = set;
    register long x8 asm("x8") = 52;
    asm volatile("svc #0" : "+r"(x0) : "r"(x8) : "memory");
    return (int)(long)x0;
}

int raise(int sig) {
    return kill(getpid(), sig);
}

sighandler_t signal(int sig, sighandler_t handler) {
    struct sigaction sa, old;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(sig, &sa, &old) < 0)
        return (sighandler_t)SIG_ERR;
    
    return old.sa_handler;
}