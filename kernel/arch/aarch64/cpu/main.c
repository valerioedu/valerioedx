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
u64 memory_size = 0;
u8 *uart = (u8*)0x09000000;
u64 *gic = (u64*)0x08000000;
u64 *fwcfg = (u64*)0x09020000;
u64 *virtio = (u64*)0x0A000000;

void signature() {
    kprintf("[ [CVirtIO Test [W] Checking data persistence...\n");
    u8 *buf = (u8*)kmalloc(512);
    if (!buf) {
        kprintf("[ [RVirtIO Test [W] Failed to allocate buffer\n");
        return;
    }

    virtio_blk_read(1, buf);
    
    const char *signature = "VALERIOEDX_PERSISTENT_DATA";

    int match = 1;
    for (int i = 0; signature[i] != 0; i++) {
        if (buf[i] != signature[i]) {
            match = 0;
            break;
        }
    }

    if (match) {
        kprintf("[ [GVirtIO Test [W] SUCCESS: Data persisted from previous boot!\n");
        kprintf("[ [CVirtIO Test [W] Content: [G%s[W\n", buf);
    } else {
        kprintf("[ [RVirtIO Test [W] No signature found (First run or clean disk).\n");
        kprintf("[ [RVirtIO Test [W] Writing signature to sector 1...\n");
        
        memset(buf, 0, 512);
        
        int i = 0;
        while(signature[i]) { buf[i] = signature[i]; i++; }
        buf[i] = 0;

        virtio_blk_write(1, buf);
    }

    kfree(buf);
}

void main() {
    dtb_init(0x40000000);
#ifndef DEBUG
    kprintf("\033[2J");
    kprintf("\033[H");
#endif    
    kprintf("Hello, World!\n");

    kprintf("Kernel end: 0x%llx\n", &_kernel_end);

    u64 memory_size = dtb_get_memory_size();
    kprintf("MEMORY SIZE: %lluB\n             %lluMB\n             %lluGB\n",
         memory_size, memory_size / 1024 / 1024, memory_size / 1024 / 1024 / 1024);

    pmm_init((uintptr_t)&_kernel_end, memory_size);
    init_vmm();

    int level = -1;

    /* Gets the exception level, should be 1*/
    asm volatile("mrs %0, CurrentEL\n"
                 "lsr %0, %0, #2"
                 : "=r" (level) : :);
    level &= 0b11;
    kprintf("Exception Level: %d\n", level);

    heap_init(PHYS_OFFSET + 0x50000000, 8 * 1024 * 1024);

    uart = (u8*)dtb_get_reg("pl011");
    gic  = (u64*)dtb_get_reg("intc");
    fwcfg = (u64*)dtb_get_reg("fw-cfg");
    virtio = (u64*)dtb_get_reg("mmio");
    
    fw_cfg_init((u64)fwcfg);

    kprintf("Hardware Discovery:\n");
    kprintf("  UART PL011 Found at: 0x%llx\n", uart);
    kprintf("  GIC Found at:        0x%llx\n", gic);
    kprintf("  FWCFG Found at:      0x%llx\n", fwcfg);
    kprintf("  VIRTIO Found at:     0x%llx\n", virtio);
    
    ramfb_init();

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

    signature();

    kmain();
    while (1) asm volatile("wfi");
}