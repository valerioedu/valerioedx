#include <gdt.h>

struct gdt_entry gdt32[3];
struct gdt_ptr gdt32_pointer;

void set_gdt_entry(int index, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt32[index].base_low    = (base & 0xFFFF);
    gdt32[index].base_middle = (base >> 16) & 0xFF;
    gdt32[index].base_high   = (base >> 24) & 0xFF;

    gdt32[index].limit_low   = (limit & 0xFFFF);
    gdt32[index].granularity = (limit >> 16) & 0x0F;

    gdt32[index].granularity |= gran & 0xF0;
    gdt32[index].access      = access;
}

void setup_gdt() {
    // 0. Null
    set_gdt_entry(0, 0, 0, 0, 0);

    // 1. Kernel Code (0x08)
    // Base=0, Limit=4GB, Access=0x9A (Exec/Read), Gran=0xCF (4KB blocks, 32-bit)
    set_gdt_entry(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);

    // 2. Kernel Data (0x10)
    // Base=0, Limit=4GB, Access=0x92 (Read/Write), Gran=0xCF
    set_gdt_entry(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    gdt32_pointer.limit = sizeof(gdt32) - 1;
    gdt32_pointer.base  = (uint32_t)&gdt32;
    
    // Load it
    asm volatile("lgdt %0" : : "m"(gdt32_pointer));
    
    // Flush Segments
    asm volatile(
        "jmp $0x08, $.flush \n"
        ".flush: \n"
        "mov $0x10, %ax \n"
        "mov %ax, %ds \n"
        "mov %ax, %es \n"
        "mov %ax, %fs \n"
        "mov %ax, %gs \n"
        "mov %ax, %ss \n"
    );
}