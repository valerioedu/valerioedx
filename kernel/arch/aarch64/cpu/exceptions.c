#include <lib.h>
#include <kio.h>
#include <gic.h>
#include <timer.h>
#include <uart.h>
#include <virtio.h>
#include <vmm.h>
#include <syscalls.h>

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

void el1_sync_handler(trapframe_t *tf) {
    u64 esr, elr, far;
    asm volatile("mrs %0, esr_el1" : "=r"(esr));

    u32 ec = (esr >> 26) & 0x3F;
    u64 iss = esr & 0x1FFFFFF;

    switch (ec) {
        case 0x20:  // Instruction Abort (Higher EL)
        case 0x21:  // Data Abort   (Higher EL)
        case 0x24:  // Instruction Abort (Lower EL)
        case 0x25:  // Data Abort   (Lower EL)
            u8 fsc = iss & 0x3F;
            
            // Extract Write flag (Bit 6 of ISS for Data Aborts)
            bool is_write = (iss >> 6) & 1; 

            if ((fsc & 0x3C) == 0x04) {
                if (vmm_handle_page_fault(far, false) == 0) return;
            }

            else if ((fsc & 0x3C) == 0x0C) {
                if (is_write && vmm_handle_page_fault(far, true) == 0) return;
            }

            // TODO: if in userland kill the process
            kprintf("\n[PANIC] UNHANDLED SYNC EXCEPTION\n");
            kprintf("  Type: %s Abort\n", (ec & 0x1) ? "Data" : "Instruction");
            kprintf("  ESR: 0x%llx (FSC: 0x%x, Write: %d)\n", esr, fsc, is_write);
            kprintf("  FAR: 0x%llx\n", far);
            dump_stack();
            while(1) asm volatile("wfe");
            break;

        case 0x15: {
            u64 syscall_num = tf->x[8];
            
            u64 ret = syscall_handler(
                syscall_num,
                tf->x[0], tf->x[1], tf->x[2], 
                tf->x[3], tf->x[4], tf->x[5]
            );

            tf->x[0] = ret;

            // Advances PC past the SVC instruction (4 bytes)
            tf->elr += 4; 
            break;
        }
        
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
    extern u8 virtio_blk_irq_id;
    extern u8 virtio_key_irq_id;

    switch (id) {
        case 30: timer_handler(); break;
        case 33: uart_irq_handler(); break;
        default:
            if (id == virtio_blk_irq_id) {
                virtio_blk_handler();
            } else if (id == virtio_key_irq_id) {
                virtio_input_handler();
            } else {
                kprintf("[ EXC ] Unknown IRQ ID: %d\n", id);
            }
            break;
    }
    gic_end_irq(id);
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