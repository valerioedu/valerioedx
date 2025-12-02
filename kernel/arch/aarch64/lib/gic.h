#ifndef GIC_H
#define GIC_H

#include <lib.h>

void gic_init();
void gic_enable_irq(u64 id);
u64 gic_acknowledge_irq();
void gic_end_irq(u64 id);

#endif