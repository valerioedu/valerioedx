#include <lib.h>
#include <uart.h>
#include <gic.h>
#include <timer.h>

void el1_sync_handler() {
    kprintf("[EXC] EL1 synchronous exception\n");
    while (1) asm volatile("wfe");
}

void el1_irq_handler() {
    u32 id = gic_acknowledge_irq();

    switch (id) {
        case 30: timer_handler(); break;
        default: kprintf("[ EXC ] Unknown IRQ ID\n"); break;
    }

    gic_end_irq(id);
}

void el1_fiq_handler() {
    kprintf("[EXC] EL1 FIQ\n");
    while (1) asm volatile("wfe");
}

void el1_serr_handler() {
    kprintf("[EXC] EL1 SError\n");
    while (1) asm volatile("wfe");
}