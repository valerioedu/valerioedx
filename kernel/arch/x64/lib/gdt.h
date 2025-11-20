#ifndef GDT_H
#define GDT_H

#include <lib.h>

// Standard GDT Entry (8 bytes)
struct gdt_entry {
    u16 limit_low;
    u16 base_low;
    u8  base_middle;
    u8  access;
    u8  granularity;
    u8  base_high;
} __attribute__((packed));

// GDTR
struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

void setup_gdt();

extern struct gdt_ptr gdt64_pointer;

#endif