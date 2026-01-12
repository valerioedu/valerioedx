#include <sys/mman.h>

int munmap(void *addr, size_t len) {
    register void *x0 asm("x0") = addr;
    register size_t x1 asm("x1") = len;
    register long x8 asm("x8") = 73;
    asm volatile("svc #0"
        : "+r"(x0) : "r"(x1), "r"(x8) : "memory");
    return (int)x0;
}

void *mmap(void *addr, size_t len, int prot, int flags,
       int fildes, off_t off) {
    register void *x0 asm("x0") = addr;
    register size_t x1 asm("x1") = len;
    register int x2 asm("x2") = prot;
    register int x3 asm("x3") = flags;
    register int x4 asm("x4") = fildes;
    register off_t x5 asm("x5") = off;
    register long x8 asm("x8") = 197;
    asm volatile("svc #0"
        : "+r"(x0) : "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5), "r"(x8)
        : "memory");
    return x0;
}