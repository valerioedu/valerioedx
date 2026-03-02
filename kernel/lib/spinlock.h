#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <lib.h>

typedef volatile u32 spinlock_t;

#ifdef ARM
static inline u32 spinlock_acquire_irqsave(spinlock_t *lock) {
    u32 flags;
    u32 tmp;
    u32 res;

    // Acquire Lock
    asm volatile(
        "1: ldaxr   %w0, [%2]\n"        // Load exclusive (acquire)
        "   cbnz    %w0, 1b\n"          // If locked (val != 0), retry
        "   stlxr    %w1, %w3, [%2]\n"  // Try to store 1
        "   cbnz    %w1, 1b\n"          // If store failed, retry
        : "=&r" (tmp), "=&r" (res)
        : "r" (lock), "r" (1)
        : "memory"
    );

    return flags;
}

static inline void spinlock_release_irqrestore(spinlock_t *lock, u32 flags) {
    asm volatile("stlr wzr, [%0]" :: "r"(lock) : "memory");
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