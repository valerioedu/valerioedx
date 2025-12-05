#include <kernel.h>
#include <lib.h>
#include <kio.h>
#include <sched.h>
#include <ramfb.h>
#include <vfs.h>
#include <heap.h>
#include <gic.h>
#include <irq.h>

void test_process() {
    kprintf("\n");
    heap_debug();
}

void kmain() {
    vfs_init();
    devfs_init();
    
    task_create(test_process, LOW);
    while (true) asm volatile("wfi");
}