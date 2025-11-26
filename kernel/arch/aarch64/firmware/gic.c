#include <gic.h>
#include <uart.h>
#include <io.h>

void gic_init() {
    // Disables Distributor
    mmio_write32(GICD_CTLR, 0);

    // Enables CPU Interface
    mmio_write32(GICC_CTLR, 1);

    // Sets Priority Mask to allow all interrupts (0xFF)
    mmio_write32(GICC_PMR, 0xFF);

    // Enables Distributor
    mmio_write32(GICD_CTLR, 1);
}

void gic_enable_irq(u32 id) {
    // Enables register (1 bit per IRQ, 32 IRQs per register)
    u32 reg = GICD_ISENABLER + (id / 32) * 4;
    u32 bit = 1 << (id % 32);
    u32 val = mmio_read32(reg);
    mmio_write32(reg, val | bit);

    // 8 bits per IRQ, 4 IRQs per register
    if (id >= 32) {
        u32 target_reg = GICD_ITARGETS + (id / 4) * 4;
        u32 target_shift = (id % 4) * 8;
        u32 target_val = mmio_read32(target_reg);
        // Sets target to CPU 0 (bit 0 set -> 0x01)
        target_val |= (0x01 << target_shift);
        mmio_write32(target_reg, target_val);
    }
}

u32 gic_acknowledge_irq() {
    return mmio_read32(GICC_IAR);
}

void gic_end_irq(u32 id) {
    mmio_write32(GICC_EOIR, id);
}