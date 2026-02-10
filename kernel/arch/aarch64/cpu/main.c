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
#include <pl031.h>
#include <io.h>
#include <uart.h>

extern u64 _kernel_end;
u64 memory_size = 0;
u8 *uart = (u8*)0x09000000;
u64 *gic = (u64*)0x08000000;
u64 *fwcfg = (u64*)0x09020000;
u64 *virtio = (u64*)0x0A000000;
u64 *pl031 = (u64*)0x09010000;

u64 boot_time = 0;
bool sgntr = false;

void signature() {
    u8 *buf = (u8*)kmalloc(512);
    if (!buf) {
        kprintf("[ [RVirtIO [W] Failed to allocate buffer to check data persistency\n");
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
        sgntr = true;
        kprintf("[ [GVirtIO [W] Disk signature found\n");
    } else {
        kprintf("[ [RVirtIO [W] No signature found (First run or clean disk).\n");
        
        memset(buf, 0, 512);
        
        int i = 0;
        while(signature[i]) { buf[i] = signature[i]; i++; }
        buf[i] = 0;

        virtio_blk_write(1, buf);
    }

    kfree(buf);
}

void main() {
    asm volatile("mrs %0, CNTPCT_EL0" : "=r"(boot_time));
    dtb_init(0x40000000);
    pl031 = (u64*)dtb_get_reg("pl031");
    mmio_write32((uintptr_t)pl031 + PL031_CR, 1);
    pl031_init_time();
#ifndef DEBUG
    kprintf("\033[2J");
    kprintf("\033[H");
#endif    
    kprintf("Hello, World!\n");

    kprintf("Kernel end: 0x%llx\n", &_kernel_end);

    u64 memory_size = dtb_get_memory_size();
    kprintf("MEMORY SIZE: %lluB\n             %lluMB\n             %lluGB\n",
         memory_size, memory_size / 1024 / 1024, memory_size / 1024 / 1024 / 1024);
    
    fw_cfg_init((u64)fwcfg);

    pmm_init((uintptr_t)&_kernel_end, memory_size);
    init_vmm();

    // Switch execution to Higher Half now
    // otherwise the jump to main would bring the instructions
    // back to low mapped addresses
    asm volatile(
        "mov x0, %0         \n"     // Loads PHYS_OFFSET into x0
        "add sp, sp, x0     \n"     // Adds the offset to the SP
        "add x29, x29, x0   \n"     // then to the frame pointer
        "adr x1, 1f         \n"
        "add x1, x1, x0     \n"     // Calculates high address
        "br x1              \n"     // Jump to high address 
        "1:                 \n"
        : : "r"(PHYS_OFFSET) : "x0", "x1", "memory"
    );

    u64 vbar_low;
    asm volatile("mrs %0, VBAR_EL1" : "=r"(vbar_low));
    u64 vbar_high = vbar_low + PHYS_OFFSET;
    asm volatile("msr VBAR_EL1, %0" :: "r"(vbar_high));
    asm volatile("isb");

    extern u32 *frame_stack;
    extern u8 *ref_counts;
    extern u64* root_table;
    extern u64 root_phys;

    frame_stack = (u32*)((uintptr_t)frame_stack + PHYS_OFFSET);
    ref_counts = (u8*)((uintptr_t)ref_counts + PHYS_OFFSET);

    root_table = (u64*)((uintptr_t)root_phys + PHYS_OFFSET);

    int level = -1;

    /* Gets the exception level, should be 1*/
    asm volatile("mrs %0, CurrentEL\n"
                 "lsr %0, %0, #2"
                 : "=r" (level) : :);
    level &= 0b11;
    kprintf("[MAIN] Exception Level: %d\n", level);
    virtio_blk_ops_init();

    heap_init(PHYS_OFFSET + 0x50000000, 8 * 1024 * 1024);

    uart = (u8*)dtb_get_reg("pl011");
    gic  = (u64*)dtb_get_reg("intc");
    fwcfg = (u64*)dtb_get_reg("fw-cfg");
    virtio = (u64*)dtb_get_reg("mmio");

    kprintf("Now switching to the graphical interface...\n");
    ramfb_init();
    set_stdio(RAMFB);

    kprintf("[ [CMAIN [W] Hardware Discovery:\n");
    kprintf("             UART PL011 Found at: 0x%llx\n", uart);
    kprintf("             GIC Found at:        0x%llx\n", gic);
    kprintf("             FWCFG Found at:      0x%llx\n", fwcfg);
    kprintf("             VIRTIO Found at:     0x%llx\n\n", virtio);
    
    sched_init();

    gic_init();
    uart_init();
    virtio_init();
    gic_enable_irq(30);
    uart += PHYS_OFFSET;
    pl031 += PHYS_OFFSET;
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