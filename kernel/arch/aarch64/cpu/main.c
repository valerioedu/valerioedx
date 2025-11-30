#include <lib.h>
#include <string.h>
#include <ramfb.h>
#include <fwcfg.h>
#include <uart.h>
#include <gic.h>
#include <timer.h>
#include <irq.h>
#include <pmm.h>
#include <vmm.h>
#include <heap.h>
#include <dtb.h>

extern u64 _kernel_end;

void main() {
    dtb_init(0x40000000);
    ramfb_init();
    kprintf("\033[2J");
    kprintf("\033[H");
    kprintf("Hello, World!\n");

    kprintf("Kernel end: 0x%llx\n", &_kernel_end);
    pmm_init((uintptr_t)&_kernel_end);
    init_vmm();

    int level = -1;

    /* Gets the exception level, should be 1*/
    asm volatile("mrs %0, CurrentEL\n"
                 "lsr %0, %0, #2"
                 : "=r" (level) : :);
    level &= 0b11;
    kprintf("Exception Level: %d\n", level);

    heap_init(0x50000000, 8 * 1024 * 1024);

    u64 uart_addr = dtb_get_reg("pl011");
    u64 gic_addr  = dtb_get_reg("intc"); // Generic interrupt controller

    kprintf("Hardware Discovery:\n");
    kprintf("  UART PL011 Found at: 0x%llx\n", uart_addr);
    kprintf("  GIC Found at:        0x%llx\n", gic_addr);

    u32 bg_color = (0xFF << 24) | (0x00 << 16) | (0x00 << 8) | 0x20;
    for (int i = 0; i < WIDTH * HEIGHT; i++) {
        fb_buffer[i] = bg_color;
    }

    const char *msg = "VALERIOEDX";
    int msg_len = strlen(msg);


    const int char_w = 8;
    const int char_h = 8;

    int text_width  = msg_len * char_w;
    int text_height = char_h;

    int x0 = (WIDTH  - text_width)  / 2;
    int y0 = (HEIGHT - text_height) / 2;

    u32 fg_color = (0xFF << 24) | (0xFF << 16) | (0xFF << 8) | 0xFF; // white

    draw_string_bitmap(x0, y0, msg, fg_color, bg_color);
    draw_string_bitmap((WIDTH - (8 * strlen("LOADING KERNEL..."))) / 2, y0 + 16, "LOADING KERNEL...", fg_color, bg_color);

    gic_init();
    gic_enable_irq(30);
    timer_init(2000);
    irq_enable();
    while (1) asm volatile("wfi");
}