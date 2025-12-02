#include <ramfb.h>
#include <fwcfg.h>
#include <stdarg.h>
#include <string.h>

// 8x8 bitmap font for ASCII
const u8 font8x8_basic[128][8] = {
    [' '] = {
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
    },
    ['!'] = {
        0b00011000,
        0b00011000,
        0b00011000,
        0b00011000,
        0b00011000,
        0b00000000,
        0b00011000,
        0b00000000,
    },
    ['"'] = { 0x24, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    ['#'] = { 0x24, 0x24, 0x7E, 0x24, 0x7E, 0x24, 0x24, 0x00 },
    ['$'] = { 0x10, 0x3C, 0x40, 0x38, 0x04, 0x3C, 0x10, 0x00 },
    ['%'] = { 0x62, 0x64, 0x08, 0x10, 0x20, 0x4C, 0x46, 0x00 },
    ['&'] = { 0x18, 0x24, 0x24, 0x18, 0x25, 0x42, 0x3D, 0x00 },
    ['\''] = { 0x18, 0x18, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00 },
    ['('] = { 0x0C, 0x10, 0x20, 0x20, 0x20, 0x10, 0x0C, 0x00 },
    [')'] = { 0x30, 0x08, 0x04, 0x04, 0x04, 0x08, 0x30, 0x00 },
    ['*'] = { 0x00, 0x10, 0x54, 0x38, 0x54, 0x10, 0x00, 0x00 },
    ['+'] = { 0x00, 0x10, 0x10, 0x7C, 0x10, 0x10, 0x00, 0x00 },
    [','] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x08 },
    ['-'] = { 0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00 },
    ['.'] = {
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00011000,
        0b00000000,
    },
    ['/'] = { 0x00, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x00 },
    ['0'] = { 0x3C, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3C, 0x00 },
    ['1'] = { 0x10, 0x30, 0x10, 0x10, 0x10, 0x10, 0x38, 0x00 },
    ['2'] = { 0x3C, 0x42, 0x02, 0x0C, 0x30, 0x40, 0x7E, 0x00 },
    ['3'] = { 0x3C, 0x42, 0x02, 0x1C, 0x02, 0x42, 0x3C, 0x00 },
    ['4'] = { 0x08, 0x18, 0x28, 0x48, 0x7E, 0x08, 0x08, 0x00 },
    ['5'] = { 0x7E, 0x40, 0x7C, 0x02, 0x02, 0x42, 0x3C, 0x00 },
    ['6'] = { 0x3C, 0x40, 0x7C, 0x42, 0x42, 0x42, 0x3C, 0x00 },
    ['7'] = { 0x7E, 0x02, 0x04, 0x08, 0x10, 0x20, 0x20, 0x00 },
    ['8'] = { 0x3C, 0x42, 0x42, 0x3C, 0x42, 0x42, 0x3C, 0x00 },
    ['9'] = { 0x3C, 0x42, 0x42, 0x3E, 0x02, 0x02, 0x3C, 0x00 },
    [':'] = { 0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00 },
    [';'] = { 0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x08, 0x00 },
    ['<'] = { 0x06, 0x18, 0x60, 0x60, 0x18, 0x06, 0x00, 0x00 },
    ['='] = { 0x00, 0x00, 0x7E, 0x00, 0x7E, 0x00, 0x00, 0x00 },
    ['>'] = { 0x60, 0x18, 0x06, 0x06, 0x18, 0x60, 0x00, 0x00 },
    ['?'] = { 0x3C, 0x42, 0x04, 0x08, 0x00, 0x00, 0x18, 0x00 },
    ['@'] = { 0x3C, 0x42, 0x5A, 0x52, 0x5E, 0x40, 0x3C, 0x00 },
    ['A'] = {
        0b00011000,
        0b00100100,
        0b01000010,
        0b01000010,
        0b01111110,
        0b01000010,
        0b01000010,
        0b00000000,
    },
    ['B'] = {
        0b01111100,
        0b01000010,
        0b01000010,
        0b01111100,
        0b01000010,
        0b01000010,
        0b01111100,
        0b00000000,
    },
    ['C'] = {
        0b00111100,
        0b01000010,
        0b01000000,
        0b01000000,
        0b01000000,
        0b01000010,
        0b00111100,
        0b00000000,
    },
    ['D'] = {
        0b01111000,
        0b01000100,
        0b01000010,
        0b01000010,
        0b01000010,
        0b01000100,
        0b01111000,
        0b00000000,
    },
    ['E'] = {
        0b01111110,
        0b01000000,
        0b01000000,
        0b01111100,
        0b01000000,
        0b01000000,
        0b01111110,
        0b00000000,
    },
    ['F'] = {
        0b01111110,
        0b01000000,
        0b01000000,
        0b01111100,
        0b01000000,
        0b01000000,
        0b01000000,
        0b00000000,
    },
    ['G'] = {
        0b00111100,
        0b01000010,
        0b01000000,
        0b01001110,
        0b01000010,
        0b01000010,
        0b00111100,
        0b00000000,
    },
    ['H'] = {
        0b01000010,
        0b01000010,
        0b01000010,
        0b01111110,
        0b01000010,
        0b01000010,
        0b01000010,
        0b00000000,
    },
    ['I'] = {
        0b00111100,
        0b00011000,
        0b00011000,
        0b00011000,
        0b00011000,
        0b00011000,
        0b00111100,
        0b00000000,
    },
    ['J'] = {
        0b00011110,
        0b00000100,
        0b00000100,
        0b00000100,
        0b01000100,
        0b01000100,
        0b00111000,
        0b00000000,
    },
    ['K'] = {
        0b01000010,
        0b01000100,
        0b01001000,
        0b01110000,
        0b01001000,
        0b01000100,
        0b01000010,
        0b00000000,
    },
    ['L'] = {
        0b01000000,
        0b01000000,
        0b01000000,
        0b01000000,
        0b01000000,
        0b01000000,
        0b01111110,
        0b00000000,
    },
    ['M'] = {
        0b01000010,
        0b01100110,
        0b01011010,
        0b01000010,
        0b01000010,
        0b01000010,
        0b01000010,
        0b00000000,
    },
    ['N'] = {
        0b01000010,
        0b01100010,
        0b01010010,
        0b01001010,
        0b01000110,
        0b01000010,
        0b01000010,
        0b00000000,
    },
    ['O'] = {
        0b00111100,
        0b01000010,
        0b01000010,
        0b01000010,
        0b01000010,
        0b01000010,
        0b00111100,
        0b00000000,
    },
    ['P'] = {
        0b01111100,
        0b01000010,
        0b01000010,
        0b01111100,
        0b01000000,
        0b01000000,
        0b01000000,
        0b00000000,
    },
    ['Q'] = {
        0b00111100,
        0b01000010,
        0b01000010,
        0b01000010,
        0b01001010,
        0b01000110,
        0b00111110,
        0b00000000,
    },
    ['R'] = {
        0b01111100,
        0b01000010,
        0b01000010,
        0b01111100,
        0b01001000,
        0b01000100,
        0b01000010,
        0b00000000,
    },
    ['S'] = {
        0b00111100,
        0b01000010,
        0b01000000,
        0b00111100,
        0b00000010,
        0b01000010,
        0b00111100,
        0b00000000,
    },
    ['T'] = {
        0b01111110,
        0b00011000,
        0b00011000,
        0b00011000,
        0b00011000,
        0b00011000,
        0b00011000,
        0b00000000,
    },
    ['U'] = {
        0b01000010,
        0b01000010,
        0b01000010,
        0b01000010,
        0b01000010,
        0b01000010,
        0b00111100,
        0b00000000,
    },
    ['V'] = {
        0b01000010,
        0b01000010,
        0b01000010,
        0b00100100,
        0b00100100,
        0b00011000,
        0b00011000,
        0b00000000,
    },
    ['W'] = {
        0b01000010,
        0b01000010,
        0b01000010,
        0b01011010,
        0b01011010,
        0b01100110,
        0b01000010,
        0b00000000,
    },
    ['X'] = {
        0b01000010,
        0b00100100,
        0b00011000,
        0b00011000,
        0b00011000,
        0b00100100,
        0b01000010,
        0b00000000,
    },
    ['Y'] = {
        0b01000010,
        0b00100100,
        0b00011000,
        0b00011000,
        0b00011000,
        0b00011000,
        0b00011000,
        0b00000000,
    },
    ['Z'] = {
        0b01111110,
        0b00000010,
        0b00000100,
        0b00001000,
        0b00010000,
        0b00100000,
        0b01111110,
        0b00000000,
    },
    ['['] = { 0x1C, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1C, 0x00 },
    ['\\'] = { 0x00, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x00 },
    [']'] = { 0x38, 0x08, 0x08, 0x08, 0x08, 0x08, 0x38, 0x00 },
    ['^'] = { 0x10, 0x28, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00 },
    ['_'] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF },
    ['`'] = { 0x10, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    ['a'] = { 0x00, 0x00, 0x3C, 0x02, 0x3E, 0x42, 0x3E, 0x00 },
    ['b'] = { 0x40, 0x40, 0x5C, 0x62, 0x42, 0x62, 0x5C, 0x00 },
    ['c'] = { 0x00, 0x00, 0x3C, 0x40, 0x40, 0x40, 0x3C, 0x00 },
    ['d'] = { 0x02, 0x02, 0x3A, 0x46, 0x42, 0x46, 0x3A, 0x00 },
    ['e'] = { 0x00, 0x00, 0x3C, 0x42, 0x7E, 0x40, 0x3C, 0x00 },
    ['f'] = { 0x0C, 0x12, 0x10, 0x3E, 0x10, 0x10, 0x10, 0x00 },
    ['g'] = { 0x00, 0x00, 0x3E, 0x42, 0x42, 0x3E, 0x02, 0x3C },
    ['h'] = { 0x40, 0x40, 0x5C, 0x62, 0x42, 0x62, 0x42, 0x00 },
    ['i'] = { 0x10, 0x00, 0x30, 0x10, 0x10, 0x10, 0x38, 0x00 },
    ['j'] = { 0x02, 0x00, 0x06, 0x02, 0x02, 0x02, 0x22, 0x1C },
    ['k'] = { 0x40, 0x40, 0x44, 0x48, 0x70, 0x48, 0x44, 0x00 },
    ['l'] = { 0x30, 0x10, 0x10, 0x10, 0x10, 0x10, 0x38, 0x00 },
    ['m'] = { 0x00, 0x00, 0x76, 0x49, 0x49, 0x49, 0x49, 0x00 },
    ['n'] = { 0x00, 0x00, 0x5C, 0x62, 0x42, 0x42, 0x42, 0x00 },
    ['o'] = { 0x00, 0x00, 0x3C, 0x42, 0x42, 0x42, 0x3C, 0x00 },
    ['p'] = { 0x00, 0x00, 0x5C, 0x62, 0x42, 0x62, 0x40, 0x40 },
    ['q'] = { 0x00, 0x00, 0x3A, 0x46, 0x42, 0x46, 0x02, 0x02 },
    ['r'] = { 0x00, 0x00, 0x5C, 0x62, 0x40, 0x40, 0x40, 0x00 },
    ['s'] = { 0x00, 0x00, 0x3C, 0x40, 0x3C, 0x02, 0x3C, 0x00 },
    ['t'] = { 0x10, 0x10, 0x3E, 0x10, 0x10, 0x12, 0x0C, 0x00 },
    ['u'] = { 0x00, 0x00, 0x42, 0x42, 0x42, 0x46, 0x3A, 0x00 },
    ['v'] = { 0x00, 0x00, 0x42, 0x42, 0x24, 0x24, 0x18, 0x00 },
    ['w'] = { 0x00, 0x00, 0x42, 0x42, 0x5A, 0x5A, 0x24, 0x00 },
    ['x'] = { 0x00, 0x00, 0x42, 0x24, 0x18, 0x24, 0x42, 0x00 },
    ['y'] = { 0x00, 0x00, 0x42, 0x42, 0x3E, 0x02, 0x22, 0x1C },
    ['z'] = { 0x00, 0x00, 0x3C, 0x04, 0x08, 0x10, 0x3C, 0x00 },
    ['{'] = { 0x0C, 0x10, 0x10, 0x20, 0x10, 0x10, 0x0C, 0x00 },
    ['|'] = { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00 },
    ['}'] = { 0x30, 0x08, 0x08, 0x04, 0x08, 0x08, 0x30, 0x00 },
    ['~'] = { 0x00, 0x00, 0x00, 0x4C, 0x28, 0x10, 0x00, 0x00 },
};

/* 1366*768*4 = 4,196,000 bytes (~4.2MB) */
u32 fb_buffer[WIDTH * HEIGHT]; 

static int cursor_x = 0;
static int cursor_y = 0;
static u32 fg_color = 0x00FFFFFF; // White
static u32 bg_color = 0x00000000; // Black

#define CHAR_WIDTH 8
#define CHAR_HEIGHT 8
#define MAX_COLS (WIDTH / CHAR_WIDTH)
#define MAX_ROWS (HEIGHT / CHAR_HEIGHT)

static inline void put_pixel(int x, int y, u32 color) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
    fb_buffer[y * WIDTH + x] = color;
}

static void draw_char_bitmap(int x, int y, char c, u32 fg, u32 bg) {
    const int cw = 8;
    const int ch = 8;

    const u8 *glyph = font8x8_basic[(unsigned char)c];

    for (int row = 0; row < ch; row++) {
        u8 bits = glyph[row];
        for (int col = 0; col < cw; col++) {
            u32 color = (bits & (1 << (7 - col))) ? fg : bg;
            put_pixel(x + col, y + row, color);
        }
    }
}

void draw_string_bitmap(int x, int y, const char *s, u32 fg, u32 bg) {
    const int cw = 8;
    while (*s) {
        draw_char_bitmap(x, y, *s, fg, bg);
        x += cw;
        s++;
    }
}

void ramfb_clear() {
    for (int i = 0; i < WIDTH * HEIGHT; i++) {
        fb_buffer[i] = bg_color;
    }
    cursor_x = 0;
    cursor_y = 0;
}

void ramfb_scroll() {
    int total_pixels = WIDTH * HEIGHT;
    int scroll_pixels = WIDTH * CHAR_HEIGHT;
    
    // Shift content up
    for (int i = 0; i < total_pixels - scroll_pixels; i++) {
        fb_buffer[i] = fb_buffer[i + scroll_pixels];
    }

    // Clear the last row
    for (int i = total_pixels - scroll_pixels; i < total_pixels; i++) {
        fb_buffer[i] = bg_color;
    }
}

void ramfb_putc(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\t') {
        for (int i = 0; i < 4; i++) {
            ramfb_putc(' ');
        }
        return;
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
        } else if (cursor_y > 0) {
            cursor_y--;
            cursor_x = MAX_COLS - 1;
        }
        draw_char_bitmap(cursor_x * CHAR_WIDTH, cursor_y * CHAR_HEIGHT, ' ', fg_color, bg_color);
        return;
    } else {
        draw_char_bitmap(cursor_x * CHAR_WIDTH, cursor_y * CHAR_HEIGHT, c, fg_color, bg_color);
        cursor_x++;
    }

    if (cursor_x >= MAX_COLS) {
        cursor_x = 0;
        cursor_y++;
    }

    if (cursor_y >= MAX_ROWS) {
        ramfb_scroll();
        cursor_y = MAX_ROWS - 1;
    }
}

void ramfb_kprint(const char* str) {
    for (int i = 0; i < strlen(str); i++) {
        ramfb_putc(str[i]);
    }
}

void ramfb_kprint_num(int num) {
    char buf[16];
    int i = 0;
    int is_negative = 0;
    
    if (num < 0) {
        is_negative = 1;
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
    
    ramfb_kprint(buf);
}

void ramfb_kprintf(const char* format, va_list args) {
    for (const char* p = format; *p != '\0'; p++) {
        if (*p == '%') {
            p++;
            switch (*p) {
                case 'd': {
                    int num = va_arg(args, int);
                    ramfb_kprint_num(num);
                    break;
                }
                case 's': {
                    const char* str = va_arg(args, const char*);
                    ramfb_kprint(str);
                    break;
                }
                case 'c': {
                    char c = va_arg(args, int);
                    ramfb_putc(c);
                    break;
                }
                case 'x': {
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
                    
                    ramfb_kprint(buf);
                    break;
                }
                case 'f': {
                    double num = va_arg(args, double);
                    int int_part = (int)num;
                    double fractional_part = num - int_part;
                    ramfb_kprint_num(int_part);
                    ramfb_putc('.');
                    for (int i = 0; i < 6; i++) {
                        fractional_part *= 10;
                        int digit = (int)fractional_part;
                        ramfb_kprint_num(digit);
                        fractional_part -= digit;
                    }
                    break;
                }
                default:
                    ramfb_putc('%');
                    ramfb_putc(*p);
                    break;
            }
        } else if (*p == '[') {
            p++;
            switch (*p) {
                case 'W': set_fg_color(0x00FFFFFF); break;
                case 'G': set_fg_color(0x0000FF00); break;
                case 'C': set_fg_color(0x0000FFFF); break;
                case 'R': set_fg_color(0x00FF0000); break;
                case 'B': set_fg_color(0x000000FF); break;
                default:
                    ramfb_putc('[');
                    p--;
            }
        } else {
            ramfb_putc(*p);
        }
    }
}

void set_fg_color(u32 color) {
    fg_color = color;
}

void set_bg_color(u32 color) {
    bg_color = color;
}

void ramfb_init() {
    u16 select = find_ramfb_file();
    if (select == 0) return; 

    RamfbCfg cfg;
    cfg.addr = bswap64((u64)fb_buffer);
    
    /* XR24 (XRGB8888) is the most standard format for QEMU. */
    /* 'X','R','2','4' -> 0x34325258 (Little Endian) */
    cfg.fourcc = bswap32(0x34325258); 
    
    cfg.flags = bswap32(0);
    cfg.width = bswap32(WIDTH);
    cfg.height = bswap32(HEIGHT);
    cfg.stride = bswap32(WIDTH * 4);

    fw_cfg_dma(FW_CFG_DMA_CTL_WRITE | FW_CFG_DMA_CTL_SELECT | (select << 16), &cfg, sizeof(cfg));
    
    ramfb_clear();
}

void draw_loading_bar(u8 percentage) {
    if (percentage > 100) percentage = 100;

    const int bar_width = 400;
    const int bar_height = 20;
    const int start_x = (WIDTH - bar_width) / 2;
    const int start_y = (HEIGHT - bar_height) / 2;
    
    int filled_width = (percentage * bar_width) / 100;
    u32 white = 0x00FFFFFF;
    u32 black = 0x00000000;

    for (int y = 0; y < bar_height; y++) {
        for (int x = 0; x < bar_width; x++) {
            put_pixel(start_x + x, start_y + y, x < filled_width ? white : black);
        }
    }
}