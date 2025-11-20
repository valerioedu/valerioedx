#ifndef VMM_H
#define VMM_H

#include <lib.h> 

// x64 Page Table Flags
#define PAGE_PRESENT    (1 << 0)
#define PAGE_WRITE      (1 << 1)
#define PAGE_USER       (1 << 2)
#define PAGE_PWT        (1 << 3) // Write-Through
#define PAGE_PCD        (1 << 4) // Cache Disable
#define PAGE_ACCESSED   (1 << 5)
#define PAGE_DIRTY      (1 << 6)
#define PAGE_HUGE       (1 << 7) // 2MB Page
#define PAGE_GLOBAL     (1 << 8)

void setup_paging(void);

extern uint32_t pml4_physical_addr;

#endif