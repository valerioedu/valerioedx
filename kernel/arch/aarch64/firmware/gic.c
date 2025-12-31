#include <gic.h>
#include <kio.h>
#include <io.h>
#include <vmm.h>

#define GIC_DIST_BASE   (u64)gic + PHYS_OFFSET
#define GIC_CPU_BASE    GIC_DIST_BASE + 0x10000ULL

#define GICD_CTLR       (GIC_DIST_BASE + 0x000)
#define GICD_ISENABLER  (GIC_DIST_BASE + 0x100)
#define GICD_IPRIORITY  (GIC_DIST_BASE + 0x400)
#define GICD_ITARGETS   (GIC_DIST_BASE + 0x800)

#define GICC_CTLR       (GIC_CPU_BASE + 0x0000)
#define GICC_PMR        (GIC_CPU_BASE + 0x0004)
#define GICC_IAR        (GIC_CPU_BASE + 0x000C)
#define GICC_EOIR       (GIC_CPU_BASE + 0x0010)

extern u64 *gic;

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

void gic_enable_irq(u64 id) {
    // Enables register (1 bit per IRQ, 32 IRQs per register)
    u64 reg = GICD_ISENABLER + (id / 32) * 4;
    u64 bit = 1 << (id % 32);
    u64 val = mmio_read32(reg);
    mmio_write32(reg, val | bit);

    // 8 bits per IRQ, 4 IRQs per register
    if (id >= 32) {
        u64 target_reg = GICD_ITARGETS + (id / 4) * 4;
        u64 target_shift = (id % 4) * 8;
        u64 target_val = mmio_read32(target_reg);
        // Sets target to CPU 0 (bit 0 set -> 0x01)
        target_val |= (0x01 << target_shift);
        mmio_write32(target_reg, target_val);
    }
}

u64 gic_acknowledge_irq() {
    return mmio_read32(GICC_IAR);
}

void gic_end_irq(u64 id) {
    mmio_write32(GICC_EOIR, id);
}