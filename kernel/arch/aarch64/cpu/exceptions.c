#include <lib.h>
#include <kio.h>
#include <gic.h>
#include <timer.h>
#include <uart.h>
#include <virtio.h>

void dump_stack() {
    uint64_t fp;
    
    asm volatile("mov %0, x29" : "=r"(fp));

    kprintf("\n--- Stack Trace ---\n");
    
    int depth = 0;
    while (fp != 0 && depth < 20) {
        if (fp < 0x40000000) break; 

        uint64_t lr = *(uint64_t*)(fp + 8);
        uint64_t prev_fp = *(uint64_t*)fp;

        kprintf("[%d] PC: 0x%llx\n", depth, lr);

        fp = prev_fp;
        depth++;
    }
    kprintf("-------------------\n");
}

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
            dump_stack();
            break;
        /* TODO: Implement syscalls later on */
        case 0x15: return; break;
        default:
            kprintf("\n[PANIC] SYNC EXCEPTION\n");
            kprintf("  ESR: 0x%llx (EC: 0x%x)\n", esr, ec);
            kprintf("  ELR: 0x%llx (PC)\n", elr);
            kprintf("  FAR: 0x%llx (Addr)\n", far);
            dump_stack();
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
        case 33: uart_irq_handler(); break;
        default:
            // TODO: Standardize this
            if (id >= 48 && id < 48+32) {
                virtio_blk_handler();
            } else {
                kprintf("[ EXC ] Unknown IRQ ID: %d\n", id);
            }
            break;
    }
}

void el1_fiq_handler() {
    kprintf("[EXC] EL1 FIQ\n");
    dump_stack();
    while (1) asm volatile("wfe");
}

void el1_serr_handler() {
    dump_stack();
    kprintf("[EXC] EL1 SError\n");
    while (1) asm volatile("wfe");
}