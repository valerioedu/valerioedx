#include <lib.h>
#include <uart.h>

void el1_sync_handler(void) {
    kprintf("[EXC] EL1 synchronous exception\n");
    while (1) asm volatile("wfe");
}

void el1_irq_handler(void) {
    
}

void el1_fiq_handler(void) {
    kprintf("[EXC] EL1 FIQ\n");
    while (1) asm volatile("wfe");
}

void el1_serr_handler(void) {
    kprintf("[EXC] EL1 SError\n");
    while (1) asm volatile("wfe");
}