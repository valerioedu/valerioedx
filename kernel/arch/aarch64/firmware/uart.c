#include <kio.h>
#include <stdarg.h>
#include <sched.h>
#include <gic.h>

#define UART_BASE uart
#define UART_DR   (UART_BASE + 0x00)
#define UART_FR   (UART_BASE + 0x18)
#define UART_IMSC (UART_BASE + 0x38)
#define UART_MIS  (UART_BASE + 0x40)
#define UART_ICR  (UART_BASE + 0x44)

#define UART_FR_RXFE (1 << 4)
#define UART_FR_TXFF (1 << 5)
#define UART_RX_IRQ  (1 << 4)

#define RX_BUF_SIZE 512 // For the circular buffer

extern u8 *uart;

// Circular buffer
static u8 rx_buffer[RX_BUF_SIZE];
static volatile int rx_head = 0;
static volatile int rx_tail = 0;
static wait_queue_t rx_wait_queue = NULL;

void uart_putc(u8 c) {
    *uart = c;
}

u8 uart_getc() {
    while (rx_head == rx_tail) sleep_on(&rx_wait_queue);

    u8 c = rx_buffer[rx_tail];
    rx_tail = (rx_tail + 1) % RX_BUF_SIZE;
    return c;
}

void uart_irq_handler() {
    volatile u32 *mis = (u32*)UART_MIS;
    volatile u32 *dr = (u32*)UART_DR;
    volatile u32 *icr = (u32*)UART_ICR;
    volatile u32 *fr = (u32*)UART_FR;

    if (*mis & UART_RX_IRQ) {
        while (!(*fr & UART_FR_RXFE)) {
            u8 c = (u8)(*dr & 0xFF);
                
            int next = (rx_head + 1) % RX_BUF_SIZE;
            if (next != rx_tail) {
                rx_buffer[rx_head] = c;
                rx_head = next;
            }
        }

        wake_up(&rx_wait_queue);

        *icr = UART_RX_IRQ;
    }

    *icr = 0x7FF;
}

void uart_init() {
    volatile u32 *imsc = (u32*)UART_IMSC;
    *imsc |= (1 << 4) | (1 << 6); 

    gic_enable_irq(33);
}

void uart_puts(const char* str) {
    while (*str) uart_putc(*str++);
}

void uart_kprint_num(int num) {
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

void uart_kprint_ull(unsigned long long num) {
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

void uart_kprintf(const char* format, va_list args) {
    for (const char* p = format; *p != '\0'; p++) {
        if (*p == '%') {
            p++;
            if (*p == 'l') {
                p++;
                if (*p == 'l') {
                    p++;
                    if (*p == 'u') {
                        unsigned long long num = va_arg(args, unsigned long long);
                        uart_kprint_ull(num);
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
                        uart_kprint_num(num);
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
}