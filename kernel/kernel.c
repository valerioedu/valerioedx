#include <kernel.h>
#include <lib.h>
#include <uart.h>
#include <sched.h>
#include <ramfb.h>

void task_a() {
    for (int i = 0; i < 10; i++){
        kprintf("A");
    }
}

void task_b() {
    for (int i = 0; i < 10; i++) {
        kprintf("B");
    }
}

void kmain() {
    task_create(task_a);
    task_create(task_b);
    while (true) asm volatile("wfi");
}