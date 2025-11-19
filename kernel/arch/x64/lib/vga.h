#ifndef VGA_H
#define VGA_H

#include <lib.h>
#include <string.h>

#define VGA_WIDTH  80
#define VGA_HEIGHT 25

#define VGA_ADDRESS 0xB8000

typedef enum Color {
    BLACK         = 0x0,
    BLUE          = 0x1,
    GREEN         = 0x2,
    CYAN          = 0x3,
    RED           = 0x4,
    MAGENTA       = 0x5,
    BROWN         = 0x6,
    LIGHT_GRAY    = 0x7,
    DARK_GRAY     = 0x8,
    LIGHT_BLUE    = 0x9,
    LIGHT_GREEN   = 0xA,
    LIGHT_CYAN    = 0xB,
    LIGHT_RED     = 0xC,
    LIGHT_MAGENTA = 0xD,
    LIGHT_BROWN   = 0xE,
    WHITE         = 0xF
} Color;

extern uint8_t color;

void vga_get_cursor(int *x, int *y);
void vga_move_cursor(int x, int y);
void vga_clear();
void vga_init();
void vga_putc(char c);
void set_color(Color new_color);
void kprintcolor(const char* str, Color new);
void rmline();
void kprint(const char* str);
void kprintf(const char* format, ...);

#endif