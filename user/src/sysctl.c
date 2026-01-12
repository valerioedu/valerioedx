#include <sys/sysctl.h>

int sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp, size_t newlen) {
    register int *x0 asm("x0") = name;
    register u_int x1 asm("x1") = namelen;
    register void *x2 asm("x2") = oldp;
    register size_t *x3 asm("x3") = oldlenp;
    register void *x4 asm("x4") = newp;
    register size_t x5 asm("x5") = newlen;
    register long x8 asm("x8") = 202;
    asm volatile("svc #0"
        : "+r"(x0) : "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5), "r"(x8)
        : "memory");
    return (int)x0;
}