#include <time.h>

int nanosleep(const struct timespec *req, struct timespec *rem) {
    register const struct timespec *x0 asm("x0") = req;
    register struct timespec *x1 asm("x1") = rem;
    register int x8 asm("x8") = 240;
    asm volatile("svc #0" : "+r"(x0) : "r"(x1), "r"(x8) : "memory");
    return (int)(long)x0;
}