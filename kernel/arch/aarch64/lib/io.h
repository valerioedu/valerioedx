#ifndef IO_H
#define IO_H

#include <lib.h>

static inline void mmio_write32(uintptr_t reg, u32 data) {
    *(volatile u32*)reg = data;
}

static inline u32 mmio_read32(uintptr_t reg) {
    return *(volatile u32*)reg;
}

static inline void mmio_write64(uintptr_t reg, u64 data) {
    *(volatile u64*)reg = data;
}

static inline u64 mmio_read64(uintptr_t reg) {
    return *(volatile u64*)reg;
}

#endif