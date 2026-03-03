#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <lib.h>

typedef volatile u32 spinlock_t;

#ifdef ARM
static inline void spinlock_acquire(spinlock_t *lock) {
    u32 tmp, res;
    asm volatile(
        "1: ldaxr   %w0, [%2]\n"        // Load exclusive (acquire)
        "   cbnz    %w0, 1b\n"          // If locked (val != 0), retry
        "   stlxr   %w1, %w3, [%2]\n"  // Try to store 1
        "   cbnz    %w1, 1b\n"          // If store failed, retry
        : "=&r" (tmp), "=&r" (res)
        : "r" (lock), "r" (1)
        : "memory"
    );
}

static inline void spinlock_release(spinlock_t *lock) {
    asm volatile("stlr wzr, [%0]" :: "r"(lock) : "memory");
}

static inline u32 spinlock_acquire_irqsave(spinlock_t *lock) {
    u32 flags, tmp, res;
    asm volatile(
        "mrs %0, daif\n"
        "msr daifset, #2\n"
        "1: ldaxr   %w1, [%3]\n"
        "   cbnz    %w1, 1b\n"
        "   stlxr   %w2, %w4, [%3]\n"
        "   cbnz    %w2, 1b\n"
        : "=&r" (flags), "=&r" (tmp), "=&r" (res)
        : "r" (lock), "r" (1)
        : "memory"
    );
    return flags;
}

static inline void spinlock_release_irqrestore(spinlock_t *lock, u32 flags) {
    asm volatile(
        "stlr wzr, [%1]\n"
        "msr daif, %0\n"
        :: "r" (flags), "r" (lock)
        : "memory"
    );
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