#include <idt.h>

// Single dummy handler that just HLTs forever
static void default_isr(void *frame) {
    (void)frame;
    while (1) {
        __asm__ volatile("hlt");
    }
}

static struct idt_entry64 idt64[256];
static struct idt_ptr64   idt64_ptr;

static void set_idt_entry(int vec, void (*handler)(void*)) {
    u64 addr = (u64)handler;
    idt64[vec].offset_low  = (u16)(addr & 0xFFFF);
    idt64[vec].selector    = 0x08;          // kernel code segment
    idt64[vec].ist         = 0;
    idt64[vec].type_attr   = 0x8E;          // present, ring0, interrupt gate
    idt64[vec].offset_mid  = (u16)((addr >> 16) & 0xFFFF);
    idt64[vec].offset_high = (u32)((addr >> 32) & 0xFFFFFFFF);
    idt64[vec].zero        = 0;
}

void load_idt64(void) {
    for (int i = 0; i < 256; i++) {
        set_idt_entry(i, default_isr);
    }

    idt64_ptr.limit = sizeof(idt64) - 1;
    idt64_ptr.base  = (u64)&idt64;

    __asm__ volatile("lidt %0" : : "m"(idt64_ptr));
}