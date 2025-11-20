#ifndef IDT_H
#define IDT_H

#include <lib.h>

struct idt_entry64 {
    u16 offset_low;
    u16 selector;
    u8  ist;
    u8  type_attr;
    u16 offset_mid;
    u32 offset_high;
    u32 zero;
} __attribute__((packed));

struct idt_ptr64 {
    u16 limit;
    u64 base;
} __attribute__((packed));

void load_idt64(void);

#endif