#include <kernel.h>
#include <lib.h>
#include <kio.h>
#include <sched.h>
#include <ramfb.h>

void test_process() {
    kprintf("\n\nTest process...\n\n");
}

void kmain() {
    task_create(test_process, LOW);
    while (true) asm volatile("wfi");
}