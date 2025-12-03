#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <lib.h>

typedef volatile u32 spinlock_t;

#ifdef ARM
static inline u32 spinlock_acquire_irqsave(spinlock_t *lock) {
    u32 flags;
    u32 tmp;
    u32 res;
    
    // Save PSTATE.DAIF (Interrupt Status)
    asm volatile("mrs %0, daif" : "=r"(flags));
    
    // daifset #2 masks the 'I' (IRQ) bit.
    asm volatile("msr daifset, #2");

    // Acquire Lock
    asm volatile(
        "1: ldaxr   %w0, [%2]\n"      // Load exclusive (acquire)
        "   cbnz    %w0, 1b\n"        // If locked (val != 0), retry
        "   stxr    %w1, %w3, [%2]\n" // Try to store 1
        "   cbnz    %w1, 1b\n"        // If store failed, retry
        : "=&r" (tmp), "=&r" (res)
        : "r" (lock), "r" (1)
        : "memory"
    );

    return flags;
}

static inline void spinlock_release_irqrestore(spinlock_t *lock, u32 flags) {
    __atomic_store_n(lock, 0, __ATOMIC_RELEASE);
    asm volatile("msr daif, %0" :: "r"(flags));
}

#else
static inline u32 spinlock_acquire_irqsave(spinlock_t *lock) {
    u32 eflags;
    asm volatile("pushf; pop %0" : "=r"(eflags));
    asm volatile("cli");
    
    while (__sync_lock_test_and_set(lock, 1)) {
        asm volatile("pause");
    }
    return eflags;
}

static inline void spinlock_release_irqrestore(spinlock_t *lock, u32 flags) {
    __sync_lock_release(lock);
    asm volatile("push %0; popf" : : "r"(flags));
}
#endif

#endif