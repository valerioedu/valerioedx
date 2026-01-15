#include <fcntl.h>

int open(const char *path, int oflag) {
    register const char *x0 asm("x0") = path;
    register int x1 asm("x1") = oflag;
    register unsigned short x8 asm("x8") = 5;
    asm volatile("svc #0"
        : "+r"(x0) : "r"(x1), "r"(x8) : "memory");
    return (int)(long)x0;
}