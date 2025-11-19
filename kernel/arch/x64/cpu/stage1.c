#include <vga.h>
#include <pmm.h>
#include <multiboot.h>
#include <io.h>

extern u32 _kernel_end;

int stage1(u32 magic, struct multiboot_info *mboot_info) {
    vga_init();
    kprint("Hello, World!\n");

    if (magic != 0x2BADB002) {
        kprintcolor("ERROR: UNEXPECTED MULTIBOOT MAGIC NUMBER!\nSHUTTING DOWN", RED);
        return 1;
    }

    if (&_kernel_end < (u32*)0x100000) {
        kprintcolor("ERROR: KERNEL END BELOW 1MB!\n", RED);
        while(1) asm volatile("hlt");
    }

    u32 total_memory = (mboot_info->mem_upper + 1024) * 1024;

    pmm_init((u32)&_kernel_end, total_memory);
    kprint("PMM initialized\n");

    u32 mmap_addr = mboot_info->mmap_addr;
    u32 mmap_length = mboot_info->mmap_length;

    parse_multiboot_mmap(mmap_addr, mmap_length);

    // Mark the first MB as used due to MMIO
    pmm_mark_used_region(0x0, 0x100000);

    u32 bitmap_size = (total_memory / 0x1000) / 8;
    u32 kernel_end_addr = (u32)&_kernel_end;
    
    // Mark kernel memory as used
    pmm_mark_used_region(0x100000, (kernel_end_addr - 0x100000) + bitmap_size);

    uintptr_t a;
    if (a = pmm_alloc_frame()) kprintf("Succesfull allocation at address 0x%x\n", a);
    
    kprint("Test\n");

    while (true);
    return 0;
}