#include <vga.h>
#include <pmm.h>
#include <multiboot.h>
#include <vmm.h>
#include <io.h>
#include <gdt.h>

extern u32 _kernel_end;

struct multiboot_module {
    u32 mod_start;
    u32 mod_end;
    u32 string;
    u32 reserved;
};

extern void enter_long_mode(u32 entry_point);

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

    if (mboot_info->mods_count == 0) {
        kprintcolor("ERROR: No 64-bit kernel module found!\n", RED);
        while(1) asm volatile("hlt");
    }

    // 2. Get the address of the first module
    struct multiboot_module* modules = (struct multiboot_module*)mboot_info->mods_addr;
    u32 kernel64_entry = modules[0].mod_start;

    kprintf("64-bit Kernel loaded at: 0x%x\n", kernel64_entry);

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

    setup_paging();
    setup_gdt();
    kprint("GDT initialized\n");
    enter_long_mode(kernel64_entry);

    return 0;
}