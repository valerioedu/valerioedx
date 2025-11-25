#include <lib.h>
#include <string.h>
#include <ramfb.h>
#include <fwcfg.h>
#include <uart.h>

void main() {
    ramfb_init();
    kprintf("\033[2J");
    kprintf("\033[H");
    kprintf("Hello, World!\n");

    int level = -1;

    /* Gets the exception level, should be 1*/
    asm volatile("mrs %0, CurrentEL\n"
                 "lsr %0, %0, #2"
                 : "=r" (level) : :);
    level &= 0b11;
    kprintf("Exception Level: %d\n", level);

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
    while (1) asm volatile("wfi");
}