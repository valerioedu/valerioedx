#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <lib.h>

typedef volatile u32 spinlock_t;

static inline u32 read_eflags() {
    u32 eflags;
    asm volatile("pushf; pop %0" : "=r"(eflags));
    return eflags;
}

static inline void write_eflags(u32 eflags) {
    asm volatile("push %0; popf" : : "r"(eflags));
}

// Acquires lock and disables the interrupt
static inline u32 spinlock_acquire_irqsave(spinlock_t *lock) {
    u32 flags = read_eflags();

    asm volatile("cli");
    while (__sync_lock_test_and_set(lock, 1)) asm volatile("pause");

    return flags;
}

static inline void spinlock_release_irqrestore(spinlock_t *lock, u32 flags) {
    __sync_lock_release(lock);
    write_eflags(flags);
}

#endif