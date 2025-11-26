#ifndef GIC_H
#define GIC_H

#include <lib.h>

#define GIC_DIST_BASE   0x08000000
#define GIC_CPU_BASE    0x08010000

#define GICD_CTLR       (GIC_DIST_BASE + 0x000)
#define GICD_ISENABLER  (GIC_DIST_BASE + 0x100)
#define GICD_IPRIORITY  (GIC_DIST_BASE + 0x400)
#define GICD_ITARGETS   (GIC_DIST_BASE + 0x800)

#define GICC_CTLR       (GIC_CPU_BASE + 0x0000)
#define GICC_PMR        (GIC_CPU_BASE + 0x0004)
#define GICC_IAR        (GIC_CPU_BASE + 0x000C)
#define GICC_EOIR       (GIC_CPU_BASE + 0x0010)

void gic_init();
void gic_enable_irq(u32 id);
u32 gic_acknowledge_irq();
void gic_end_irq(u32 id);

#endif