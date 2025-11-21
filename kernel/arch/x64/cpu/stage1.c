#include <vga.h>
#include <pmm.h>
#include <multiboot.h>
#include <vmm.h>
#include <io.h>

extern u32 _kernel_end;

// Framebuffer State
uint32_t* fb_ptr = 0;
uint32_t fb_width = 0;
uint32_t fb_height = 0;
uint32_t fb_pitch = 0;
uint32_t fb_bpp = 0;

void put_pixel(int x, int y, uint32_t color) {
    // Calculate offset: y * pitch + x * bytes_per_pixel
    // Note: Pitch is in bytes, so we work with byte pointers
    uint32_t offset = (y * fb_pitch) + (x * (fb_bpp / 8));
    
    // Write the pixel
    uint32_t* pixel = (uint32_t*)((uint8_t*)fb_ptr + offset);
    *pixel = color;
}

int stage1(u32 magic, struct multiboot_info *mboot_info) {
    // 1. Basic Init
    vga_init(); // Still useful for debug logs before graphics takeover

    if (magic != 0x2BADB002) {
        kprintcolor("ERROR: INVALID MAGIC!\n", RED);
        return 1;
    }

    // 2. PMM Init
    u32 total_memory = (mboot_info->mem_upper + 1024) * 1024;
    pmm_init((u32)&_kernel_end, total_memory);
    parse_multiboot_mmap(mboot_info->mmap_addr, mboot_info->mmap_length);
    pmm_mark_used_region(0x0, 0x100000); // Protect BIOS area

    // 3. Enable Paging
    setup_paging();
    kprint("Paging Enabled.\n");

    // 4. Framebuffer Setup
    // Check bit 12 of flags (Framebuffer Info Available)
    if (mboot_info->flags & (1 << 12)) {
        kprint("Framebuffer Info Found!\n");
        
        fb_width = mboot_info->framebuffer_width;
        fb_height = mboot_info->framebuffer_height;
        fb_pitch = mboot_info->framebuffer_pitch;
        fb_bpp = mboot_info->framebuffer_bpp;
        uint32_t fb_phys_addr = (uint32_t)mboot_info->framebuffer_addr;
        
        // The Framebuffer is high in memory (e.g., 0xFD000000).
        // We must map it into our page directory to write to it.
        // We map enough 4MB pages to cover the whole screen.
        uint32_t fb_size = fb_height * fb_pitch;
        for (uint32_t i = 0; i < fb_size; i += 0x400000) {
            vmm_map_physical(fb_phys_addr + i, fb_phys_addr + i);
        }
        
        // Now we can use the address directly
        fb_ptr = (uint32_t*)fb_phys_addr;
        
        // TEST: Fill screen with Blue
        for (uint32_t y = 0; y < fb_height; y++) {
            for (uint32_t x = 0; x < fb_width; x++) {
                put_pixel(x, y, 0xFF0000FF); // ARGB: Alpha=FF, Red=00, Green=00, Blue=FF
            }
        }

        
        // Draw a Red Square in the middle
        for (int y = 300; y < 400; y++) {
            for (int x = 400; x < 500; x++) {
                put_pixel(x, y, 0xFFFF0000);
            }
        }
    } else {
        kprintf("Error: No Framebuffer provided by Bootloader: %d\n", mboot_info->flags);
    }

    while(1);
    return 0;
}