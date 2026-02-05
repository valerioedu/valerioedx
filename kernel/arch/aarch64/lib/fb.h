#ifndef FB_H
#define FB_H

#include <lib.h>
#include <vfs.h>

// Framebuffer ioctl commands
#define FBIOGET_VSCREENINFO  0x4600
#define FBIOPUT_VSCREENINFO  0x4601
#define FBIOGET_FSCREENINFO  0x4602
#define FBIOPAN_DISPLAY      0x4606

#define FB_TYPE_PACKED_PIXELS  0
#define FB_VISUAL_TRUECOLOR    2

typedef struct fb_fix_screeninfo {
    char id[16];
    u64 smem_start;        // Physical address of framebuffer
    u32 smem_len;          // Length of framebuffer memory
    u32 type;              // FB_TYPE_*
    u32 visual;            // FB_VISUAL_*
    u32 line_length;
} fb_fix_screeninfo_t;

typedef struct fb_var_screeninfo {
    u32 xres;
    u32 yres;
    u32 xres_virtual;
    u32 yres_virtual;
    u32 xoffset;
    u32 yoffset;
    u32 bits_per_pixel;
    u32 grayscale;         // 0 = color, 1 = grayscale
    
    struct {
        u32 offset;        // Bit offset
        u32 length;        // Bit length
        u32 msb_right;     // MSB is right
    } red, green, blue, transp;
} fb_var_screeninfo_t;

extern inode_ops fb_ops;

void fb_device_init(void);

#endif