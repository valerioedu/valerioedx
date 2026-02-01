#include <sys/ioctl.h>
#include <stdarg.h>

int ioctl(int fd, unsigned long request, ...) {
    register int x0 asm("x0") = fd;
    register unsigned long x1 asm("x1") = request;

    unsigned long arg = 0;
    va_list ap;
    va_start(ap, request);
    arg = va_arg(ap, unsigned long);
    va_end(ap);

    register unsigned long x2 asm("x2") = arg;
    register unsigned long x8 asm("x8") = 54;
    asm volatile("svc #0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x8) : "memory");
    return x0;
}