#include <uart.h>
#include <stdarg.h>

#define UART_BASE 0x09000000
#define UART_DR   (UART_BASE + 0x00)
#define UART_FR   (UART_BASE + 0x18)

#define UART_FR_RXFE (1 << 4)
#define UART_FR_TXFF (1 << 5)

void uart_putc(u8 c) {
    volatile u8 *uart = (volatile u8*)0x09000000;
    *uart = c;
}

void uart_puts(const char* str) {
    while (*str) uart_putc(*str++);
}

void kprint_num(int num) {
    char buf[16];
    int i = 0;
    bool is_negative = false;
    
    if (num < 0) {
        is_negative = true;
        num = -num;
    }
    
    do {
        buf[i++] = '0' + (num % 10);
        num /= 10;
    } while (num > 0 && i < 15);
    
    if (is_negative) {
        buf[i++] = '-';
    }
    
    buf[i] = '\0';
    
    for (int j = 0, k = i - 1; j < k; j++, k--) {
        char temp = buf[j];
        buf[j] = buf[k];
        buf[k] = temp;
    }
    
    uart_puts(buf);
}

void kprint_ull(unsigned long long num) {
    char buf[32];
    int i = 0;
    
    do {
        buf[i++] = '0' + (num % 10);
        num /= 10;
    } while (num > 0 && i < 31);
    
    buf[i] = '\0';
    
    for (int j = 0, k = i - 1; j < k; j++, k--) {
        char temp = buf[j];
        buf[j] = buf[k];
        buf[k] = temp;
    }
    
    uart_puts(buf);
}

void kprintf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    for (const char* p = format; *p != '\0'; p++) {
        if (*p == '%') {
            p++;
            if (*p == 'l') {
                p++;
                if (*p == 'l') {
                    p++;
                    if (*p == 'u') {
                        unsigned long long num = va_arg(args, unsigned long long);
                        kprint_ull(num);
                    } else if (*p == 'x') {
                        unsigned long long num = va_arg(args, unsigned long long);
                        char buf[32];
                        int i = 0;
                        
                        do {
                            int digit = num % 16;
                            buf[i++] = digit < 10 ? '0' + digit : 'A' + digit - 10;
                            num /= 16;
                        } while (num > 0 && i < 31);
                        
                        buf[i] = '\0';
                        
                        for (int j = 0, k = i - 1; j < k; j++, k--) {
                            char temp = buf[j];
                            buf[j] = buf[k];
                            buf[k] = temp;
                        }
                        
                        uart_puts(buf);
                    }
                }
            } else {
                switch (*p) {
                    case 'd': {
                        int num = va_arg(args, int);
                        kprint_num(num);
                        break;
                    } case 's': {
                        const char* str = va_arg(args, const char*);
                        uart_puts(str);
                        break;
                    } case 'c': {
                        char c = va_arg(args, int);
                        uart_putc(c);
                        break;
                    } case 'x': {
                        int num = va_arg(args, int);
                        char buf[16];
                        int i = 0;
                        int is_negative = 0;
                        
                        if (num < 0) {
                            is_negative = 1;
                            num = -num;
                        }
                        
                        do {
                            int digit = num % 16;
                            buf[i++] = digit < 10 ? '0' + digit : 'A' + digit - 10;
                            num /= 16;
                        } while (num > 0 && i < 15);
                        
                        if (is_negative) {
                            buf[i++] = '-';
                        }
                        
                        buf[i] = '\0';
                        
                        for (int j = 0, k = i - 1; j < k; j++, k--) {
                            char temp = buf[j];
                            buf[j] = buf[k];
                            buf[k] = temp;
                        }
                        
                        uart_puts(buf);
                        break;
                    } default:
                        uart_putc('%');
                        uart_putc(*p);
                        break;
                }
            }
        } else uart_putc(*p);
    }
    
    va_end(args);
}