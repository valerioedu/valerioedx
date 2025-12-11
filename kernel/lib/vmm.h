#ifndef VMM_H
#define VMM_H

#include <lib.h>

#define PAGE_SIZE   4096ULL
#define PAGE_SHIFT  12

#define PHYS_OFFSET 0xFFFFFF8000000000ULL
#define V2P(x) ((uintptr_t)(x) >= PHYS_OFFSET ? (uintptr_t)(x) - PHYS_OFFSET : (uintptr_t)(x))
#define P2V(x) ((void*)((uintptr_t)(x) + PHYS_OFFSET))

#ifdef ARM
// Standard AArch64 Page Table Flags
#define PT_VALID    (1ULL << 0)
#define PT_TABLE    (1ULL << 1)
#define PT_PAGE     (1ULL << 1)
#define PT_BLOCK    (0ULL << 1)

#define PT_AF       (1ULL << 10) // Access Flag
#define PT_SH_INNER (3ULL << 8)  // Inner Shareable
#define PT_SW_COW   (1ULL << 55)
#endif

#define VM_WRITABLE  (1ULL << 0)
#define VM_USER      (1ULL << 1)
#define VM_NO_EXEC   (1ULL << 2)
#define VM_DEVICE    (1ULL << 3) // Memory Mapped I/O (nGnRnE)

void init_vmm();
void vmm_map_page(uintptr_t virt, uintptr_t phys, u64 flags);
void vmm_map_region(uintptr_t virt, uintptr_t phys, size_t size, u64 flags);
void dcache_clean_poc(void *addr, size_t size);
int vmm_handle_page_fault(uintptr_t virt, bool is_write);

#endif