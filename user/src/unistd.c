#include <unistd.h>
#include <stdint.h>

typedef uint64_t u64;
typedef uint32_t u32;

//#ifdef ARM
u64 write(u32 fd, const char *buf, u64 size) {
    register u64 x0 asm("x0") = fd;
    register const char *x1 asm("x1") = buf;
    register u64 x2 asm("x2") = size;
    register u64 x8 asm("x8") = 4;
    asm volatile("svc #0"
                 : "+r"(x0)
                 : "r"(x1), "r"(x2), "r"(x8)
                 : "memory");
    return x0;
}

u64 read(u32 fd, char *buf, u64 size) {
    register u64 x0 asm("x0") = fd;
    register char *x1 asm("x1") = buf;
    register u64 x2 asm("x2") = size;
    register u64 x8 asm("x8") = 3;
    asm volatile("svc #0"
                 : "+r"(x0)
                 : "r"(x1), "r"(x2), "r"(x8)
                 : "memory");
    return x0;
}

// Temporary return type until char* implemented
void getcwd(char *buf, u64 size) {
    register char *x0 asm("x0") = buf;
    register u64 x1 asm("x1") = size;
    register u64 x8 asm("x8") = 76;
    asm volatile("svc #0"
        :: "r"(x0), "r"(x1), "r"(x8) : "memory");
}
//#endif