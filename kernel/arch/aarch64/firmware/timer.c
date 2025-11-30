#include <timer.h>
#include <gic.h>
#include <uart.h>
#include <sched.h>

#define TIMER_IRQ_ID 30 // EL1 Physical Timer

static u32 current_interval = 0;

void timer_init(u32 interval_ms) {
    current_interval = interval_ms;
    
    // Gets Frequency
    u64 frq;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(frq));

    u64 ticks = (frq * interval_ms) / 1000;
    
    // Sets timer value
    asm volatile("msr cntp_tval_el0, %0" : : "r"(ticks));
    
    // Enables timer (Bit 0: enable, Bit 1: mask)
    asm volatile("msr cntp_ctl_el0, %0" : : "r"(1));
    gic_enable_irq(TIMER_IRQ_ID);
}

void timer_handler() {    
    // Reset timer
    u64 frq;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(frq));

    u64 ticks = (frq * current_interval) / 1000;
    asm volatile("msr cntp_tval_el0, %0" : : "r"(ticks));

    schedule();
}