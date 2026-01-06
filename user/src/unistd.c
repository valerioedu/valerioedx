#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>

typedef uint64_t u64;
typedef int64_t i64;
typedef uint32_t u32;

//#ifdef ARM
ssize_t write(int fildes, const char *buf, u64 nbyte) {
    register int x0 asm("x0") = fildes;
    register const char *x1 asm("x1") = buf;
    register size_t x2 asm("x2") = nbyte;
    register u64 x8 asm("x8") = 4;
    asm volatile("svc #0"
                 : "+r"(x0)
                 : "r"(x1), "r"(x2), "r"(x8)
                 : "memory");
    return x0;
}

ssize_t read(int fildes, char *buf, size_t nbyte) {
    register int x0 asm("x0") = fildes;
    register char *x1 asm("x1") = buf;
    register size_t x2 asm("x2") = nbyte;
    register u64 x8 asm("x8") = 3;
    asm volatile("svc #0"
                 : "+r"(x0)
                 : "r"(x1), "r"(x2), "r"(x8)
                 : "memory");
    return x0;
}

char *getcwd(char *buf, u64 size) {
    register char *x0 asm("x0") = buf;
    register u64 x1 asm("x1") = size;
    register u64 x8 asm("x8") = 76;
    asm volatile("svc #0"
                 : "+r"(x0) 
                 : "r"(x1), "r"(x8) 
                 : "memory");

    if ((long)x0 < 0)
        return NULL; 

    return buf;
}

int close(int fildes) {
    register int x0 asm("x0") = fildes;
    register u64 x8 asm("x8") = 2;
    asm volatile("svc #0"
        : "+r"(x0) : "r"(x8) : "memory");
    return x0;
}

_Noreturn void _exit(int status) {
    register int x0 asm("x0") = status;
    register u64 x8 asm("x8") = 1;
    asm volatile("svc #0"
        :: "r"(x0), "r"(x8) : "memory");
}

pid_t getpid() {
    register pid_t x0 asm("x0") = 0;
    register u64 x8 asm("x8") = 20;
    asm volatile("svc #0"
        : "+r"(x0) : "r"(x8) : "memory");
    return x0;
}
//#endif