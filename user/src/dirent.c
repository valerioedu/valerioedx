#include <dirent.h>
#include <sys/types.h>
#include <stdint.h>

ssize_t getdirentries(int fildes, char buf[], size_t nbytes, int64_t *basep) {
    register int x0 asm("x0") = fildes;
    register char *x1 asm("x1") = buf;
    register size_t x2 asm("x2") = nbytes;
    register int64_t *x3 asm("x3") = basep;
    register uint64_t x8 asm("x8") = 196;
    asm volatile("svc #0"
        : "+r"(x0) : "r"(x1), "r"(x2), "r"(x3) : "memory");
    return (ssize_t)x0;
}
