#include <lib.h>
#include <string.h>
#include <ramfb.h>
#include <fwcfg.h>
#include <kio.h>
#include <gic.h>
#include <timer.h>
#include <irq.h>
#include <pmm.h>
#include <vmm.h>
#include <heap.h>
#include <dtb.h>
#include <kernel.h>
#include <sched.h>
#include <virtio.h>
#include <uart.h>

extern u64 _kernel_end;
u8 *uart = (u8*)0x09000000;
u64 *gic = (u64*)0x08000000;

void main() {
    dtb_init(0x40000000);
    ramfb_init();
#ifndef DEBUG
    kprintf("\033[2J");
    kprintf("\033[H");
#endif    
    kprintf("Hello, World!\n");

    kprintf("Kernel end: 0x%llx\n", &_kernel_end);
    pmm_init((uintptr_t)&_kernel_end);
    init_vmm();

    int level = -1;

    /* Gets the exception level, should be 1*/
    asm volatile("mrs %0, CurrentEL\n"
                 "lsr %0, %0, #2"
                 : "=r" (level) : :);
    level &= 0b11;
    kprintf("Exception Level: %d\n", level);

    heap_init(0x50000000, 8 * 1024 * 1024);

    uart = (u8*)dtb_get_reg("pl011");
    gic  = (u64*)dtb_get_reg("intc");

    kprintf("Hardware Discovery:\n");
    kprintf("  UART PL011 Found at: 0x%llx\n", uart);
    kprintf("  GIC Found at:        0x%llx\n", gic);
    
    kprintf("Now switching to the graphical interface...\n");
    set_stdio(RAMFB);

    sched_init();

    gic_init();
    uart_init();
    virtio_init();
    gic_enable_irq(30);
#ifdef DEBUG
    timer_init(1000);
#else
    timer_init(1);
#endif
    irq_enable();

    kmain();
    while (1) asm volatile("wfi");
}