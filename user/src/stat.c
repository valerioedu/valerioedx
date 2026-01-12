#include <sys/stat.h>

int mkdir(const char *path, mode_t mode) {
    register const char *x0 asm("x0") = path;
    register mode_t x1 asm("x1") = mode;
    register unsigned long x8 asm("x8") = 136;
    asm volatile("svc #0"
        : "+r"(x0) : "r"(x1), "r"(x1) : "memory");
    return (int)x0;
}