#ifndef PMM_H
#define PMM_H

#include <lib.h>

#define PHY_RAM_BASE    0x40000000ULL          // Start of RAM (1GB)
#define PHY_RAM_SIZE    (2ULL * 1024 * 1024 * 1024) // 2GB
#define PHY_RAM_END     (PHY_RAM_BASE + PHY_RAM_SIZE)

#define PAGE_SIZE       4096ULL
#define PAGE_SHIFT      12

// Calculates how many frames need to be tracked
#define TOTAL_FRAMES    (PHY_RAM_SIZE / PAGE_SIZE)
#define BITMAP_SIZE     (TOTAL_FRAMES / 8)

void pmm_init(uintptr_t kernel_end);
uintptr_t pmm_alloc_frame();
void pmm_free_frame(uintptr_t addr);
void pmm_mark_used_region(uintptr_t base, size_t size);

#endif