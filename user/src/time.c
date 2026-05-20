#include <time.h>
#include <sys/time.h>

int nanosleep(const struct timespec *req, struct timespec *rem) {
    register const struct timespec *x0 asm("x0") = req;
    register struct timespec *x1 asm("x1") = rem;
    register int x8 asm("x8") = 240;
    asm volatile("svc #0" : "+r"(x0) : "r"(x1), "r"(x8) : "memory");
    return (int)(long)x0;
}

int gettimeofday(struct timeval *restrict tv, struct timezone *restrict tz) {
    register struct timeval *restrict x0 asm("x0") = tv;
    register struct timezone *restrict x1 asm("x1") = tz;
    register long x8 asm("x8") = 116;
    asm volatile("svc #0" : "+r"(x0) : "r"(x1), "r"(x8) : "memory");
    return (int)(long)x0;
}

int settimeofday(const struct timeval *tv, const struct timezone *tz) {
    register const struct timeval *restrict x0 asm("x0") = tv;
    register const struct timezone *restrict x1 asm("x1") = tz;
    register long x8 asm("x8") = 122;
    asm volatile("svc #0" : "+r"(x0) : "r"(x1), "r"(x8) : "memory");
    return (int)(long)x0;
}