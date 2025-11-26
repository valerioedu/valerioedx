#ifndef IRQ_H
#define IRQ_H

static inline void irq_enable() {
    asm volatile("msr daifclr, #2");
}

static inline void irq_disable() {
    asm volatile("msr daifset, #2");
}

#endif