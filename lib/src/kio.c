#include <kio.h>
#include <string.h>
#include <lib.h>
#include <stdarg.h>

#ifdef ARM
#include <uart.h>
#include <ramfb.h>
#endif

#ifdef X64
#include <vga.h>
#endif


// This is temporary, will be replaced by the fs/vfs
// TODO: Implement kprintf based on the write syscall
u8 current_kstdio = 255;

#ifdef ARM
void set_stdio(u8 device) {
    current_kstdio = device;
}

void kprintf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    switch (current_kstdio) {
        case RAMFB: ramfb_kprintf(fmt, args); break;
        default:
        case UART: uart_kprintf(fmt, args); break;
    }

    va_end(args);
}
#endif