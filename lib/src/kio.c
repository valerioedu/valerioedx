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

int snprintf(char* buf, size_t size, const char* fmt, ...) {
    if (size == 0) return 0;
    
    va_list args;
    va_start(args, fmt);
    
    int pos = 0;
    
    while (*fmt && pos < (int)size - 1) {
        if (*fmt != '%') {
            buf[pos++] = *fmt++;
            continue;
        }
        
        fmt++;
        
        switch (*fmt) {
            case 's': {
                const char* s = va_arg(args, const char*);
                while (*s && pos < (int)size - 1) buf[pos++] = *s++;
                break;
            }
            case 'd': {
                int n = va_arg(args, int);
                if (n < 0) {
                    buf[pos++] = '-';
                    n = -n;
                }
                char tmp[12];
                int i = 0;
                do {
                    tmp[i++] = '0' + (n % 10);
                    n /= 10;
                } while (n && i < 11);
                while (i-- > 0 && pos < (int)size - 1) buf[pos++] = tmp[i];
                break;
            }
            case 'x': {
                unsigned int n = va_arg(args, unsigned int);
                char tmp[9];
                int i = 0;
                do {
                    int d = n & 0xF;
                    tmp[i++] = d < 10 ? '0' + d : 'a' + d - 10;
                    n >>= 4;
                } while (n && i < 8);
                while (i-- > 0 && pos < (int)size - 1) buf[pos++] = tmp[i];
                break;
            }
            case '%':
                buf[pos++] = '%';
                break;
            default:
                buf[pos++] = *fmt;
                break;
        }
        fmt++;
    }
    
    buf[pos] = '\0';
    va_end(args);
    return pos;
}