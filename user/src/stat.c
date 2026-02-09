#include <sys/stat.h>
#include <stdint.h>

int mkdir(const char *path, mode_t mode) {
    register const char *x0 asm("x0") = path;
    register mode_t x1 asm("x1") = mode;
    register unsigned long x8 asm("x8") = 136;
    asm volatile("svc #0"
        : "+r"(x0) : "r"(x1), "r"(x1) : "memory");
    return (int)(long)x0;
}

int stat(const char *path, struct stat *buf) {
    register const char *x0 asm("x0") = path;
    register struct stat *x1 asm("x1") = buf;
    register uint64_t x8 asm("x8") = 38;
    asm volatile("svc #0"
        : "+r"(x0) : "r"(x1), "r"(x8) : "memory");
    return (int)(long)x0;
}

int fstat(int fd, struct stat *buf) {
    register int x0 asm("x0") = fd;
    register struct stat *x1 asm("x1") = buf;
    register uint64_t x8 asm("x8") = 189;
    asm volatile("svc #0"
        : "+r"(x0) : "r"(x1), "r"(x8) : "memory");
    return (int)x0;
}

int lstat(const char *path, struct stat *buf) {
    register const char *x0 asm("x0") = path;
    register struct stat *x1 asm("x1") = buf;
    register uint64_t x8 asm("x8") = 40;
    asm volatile("svc #0"
        : "+r"(x0) : "r"(x1), "r"(x8) : "memory");
    return (int)(long)x0;
}

int chmod(const char *path, mode_t mode) {
    register const char *x0 asm("x0") = path;
    register mode_t x1 asm("x1") = mode;
    register int x8 asm("x8") = 15;
    asm volatile("svc #0" : "+r"(x0) : "r"(x1), "r"(x8) : "memory");
    return (int)(long)x0;
}

int fchmod(int fildes, mode_t mode) {
    register int x0 asm("x0") = fildes;
    register mode_t x1 asm("x1") = mode;
    register int x8 asm("x8") = 124;
    asm volatile("svc #0" : "+r"(x0) : "r"(x1), "r"(x8) : "memory");
    return x0;
}