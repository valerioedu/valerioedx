#include <kernel.h>
#include <lib.h>
#include <kio.h>
#include <sched.h>
#include <ramfb.h>
#include <vfs.h>
#include <heap.h>

void test_process() {
    kprintf("\n");
    heap_debug();
}

void kmain() {
    vfs_init();
    devfs_init();
    vfs_write(&device_list[0], 16, "Hello, World!\n");
    task_create(test_process, LOW);
    while (true) asm volatile("wfi");
}