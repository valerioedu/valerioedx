#ifndef PMM_H
#define PMM_H

#include <lib.h>

#define PHY_RAM_BASE    0x40000000ULL          // Start of RAM (1GB)

extern u64 phy_ram_size;
extern u64 phy_ram_end;
extern u32 total_frames;

#define PAGE_SIZE       4096ULL
#define PAGE_SHIFT      12

void pmm_init(uintptr_t kernel_end, u64 ram_size);
uintptr_t pmm_alloc_frame();
void pmm_free_frame(uintptr_t addr);
void pmm_mark_used_region(uintptr_t base, size_t size);

#endif