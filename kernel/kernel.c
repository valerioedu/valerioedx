#include <kernel.h>
#include <lib.h>
#include <uart.h>
#include <sched.h>

void task_a() {
    while(1) {
        kprintf("A");
        for (volatile int i=0; i<5000000; i++); 
    }
}

void task_b() {
    while(1) {
        kprintf("B");
        for (volatile int i=0; i<5000000; i++);
    }
}

void kmain() {
    task_create(task_a);
    task_create(task_b);
    while (true) asm volatile("wfi");
}