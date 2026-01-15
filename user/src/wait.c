#include <sys/wait.h>

pid_t waitpid(pid_t pid, int *stat_loc, int options) {
    register pid_t x0 asm("x0") = pid;
    register int *x1 asm("x1") = stat_loc;
    register int x2 asm("x2") = options;
    register uint64_t x8 asm("x8") = 7;
    asm volatile("svc #0"
        : "+r"(x0) : "r"(x1), "r"(x2), "r"(x8) : "memory");
    return (pid_t)x0;
}

pid_t wait(int *stat_loc) {
    return waitpid(-1, stat_loc, 0);
}