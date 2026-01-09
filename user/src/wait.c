#include <sys/wait.h>

pid_t wait(int *stat_loc) {
    register int *x0 asm("x0") = stat_loc;
    register uint64_t x8 asm("x8") = 7;
    asm volatile("svc #0"
        : "+r"(x0) : "r"(x8) : "memory");
    return (pid_t)x0;
}