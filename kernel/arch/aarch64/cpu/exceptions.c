#include <lib.h>
#include <uart.h>
#include <gic.h>
#include <timer.h>

void el1_sync_handler() {
    u64 esr, elr, far;
    asm volatile("mrs %0, esr_el1" : "=r"(esr));
    asm volatile("mrs %0, elr_el1" : "=r"(elr));
    asm volatile("mrs %0, far_el1" : "=r"(far));

    u32 ec = (esr >> 26) & 0x3F;

    switch (ec) {
        case 0x25:
            kprintf("\n[PANIC] SYNC EXCEPTION\n");
            kprintf("  ESR: 0x%llx (EC: 0x%x)\n", esr, ec);
            kprintf("  ELR: 0x%llx (PC)\n", elr);
            kprintf("  FAR: 0x%llx (Addr)\n", far);
            break;
        /* TODO: Implement syscalls later on */
        case 0x15: return; break;
        default:
            kprintf("\n[PANIC] SYNC EXCEPTION\n");
            kprintf("  ESR: 0x%llx (EC: 0x%x)\n", esr, ec);
            kprintf("  ELR: 0x%llx (PC)\n", elr);
            kprintf("  FAR: 0x%llx (Addr)\n", far);
            break;
    }

    kprintf("[EXC] EL1 synchronous exception\n");
    while (1) asm volatile("wfe");
}

void el1_irq_handler() {
    u32 id = gic_acknowledge_irq();

    gic_end_irq(id);
    
    switch (id) {
        case 30: timer_handler(); break;
        default: kprintf("[ EXC ] Unknown IRQ ID\n"); break;
    }
}

void el1_fiq_handler() {
    kprintf("[EXC] EL1 FIQ\n");
    while (1) asm volatile("wfe");
}

void el1_serr_handler() {
    kprintf("[EXC] EL1 SError\n");
    while (1) asm volatile("wfe");
}