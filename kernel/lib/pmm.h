#ifndef PMM_H
#define PMM_H

#include <lib.h>

void parse_multiboot_mmap(uint32_t mmap_addr, uint32_t mmap_length);
void pmm_init(uint32_t kernel_end, uint32_t total_memory);
uintptr_t pmm_alloc_frame();
void pmm_mark_used_region(uintptr_t base, uint64_t len);

#endif