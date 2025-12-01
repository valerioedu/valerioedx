#ifndef RAMFB_H
#define RAMFB_H

#include <lib.h>

#define WIDTH  1366
#define HEIGHT 768

extern const u8 font8x8_basic[128][8];
extern u32 fb_buffer[WIDTH * HEIGHT];

void draw_string_bitmap(int x, int y, const char *s, u32 fg, u32 bg);
void ramfb_init();
void draw_loading_bar(u8 percentage);

#endif