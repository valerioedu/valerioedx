#include <vmm.h>
#include <pmm.h>
#include <vga.h>
#include <io.h>

uint32_t pml4_physical_addr = 0;

void setup_paging() {
    uint32_t pml4_addr = (uint32_t)pmm_alloc_frame();
    uint32_t pdpt_addr = (uint32_t)pmm_alloc_frame();
    uint32_t pd_addr   = (uint32_t)pmm_alloc_frame();

    if (!pml4_addr || !pdpt_addr || !pd_addr) {
        kprintcolor("CRITICAL: Failed to allocate page tables.\n", RED);
        while(1) asm volatile("hlt");
    }

    // Zeroes out the tables
    uint64_t* pml4 = (uint64_t*)pml4_addr;
    uint64_t* pdpt = (uint64_t*)pdpt_addr;
    uint64_t* pd   = (uint64_t*)pd_addr;

    for(int i=0; i<512; i++) {
        pml4[i] = 0;
        pdpt[i] = 0;
        pd[i]   = 0;
    }

    // -------------------------------------------------------
    // Identity Map (Low Half)
    // Virtual 0x00000000 -> Physical 0x00000000
    // -------------------------------------------------------
    
    // Link PML4[0] -> PDPT
    pml4[0] = (uint64_t)pdpt_addr | PAGE_PRESENT | PAGE_WRITE;

    // Link PDPT[0] -> PD
    pdpt[0] = (uint64_t)pd_addr | PAGE_PRESENT | PAGE_WRITE;

    // Link PD entries to identity map a larger low physical range using 2MB pages.
    // This ensures the 64-bit kernel module loaded by Multiboot (kernel64.elf)
    // is mapped in long mode even if it is placed above 2MB.
    for (int i = 0; i < 512; i++) {
        uint64_t phys = (uint64_t)i * 0x200000ULL; // 2MB per entry
        pd[i] = phys | PAGE_PRESENT | PAGE_WRITE | PAGE_HUGE;
    }

    // -------------------------------------------------------
    // 4. Higher Half Map (High Half)
    // Virtual 0xFFFFFFFF80000000 -> Physical 0x00000000
    // -------------------------------------------------------
    
    // The virtual address 0xFFFFFFFF80000000 corresponds to:
    // PML4 Index: 511  (The very last entry)
    // PDPT Index: 510  (The second to last entry)
    // PD Index:   0    (The first entry)

    // Link PML4[511] -> The SAME PDPT as before
    pml4[511] = (uint64_t)pdpt_addr | PAGE_PRESENT | PAGE_WRITE;

    // Link PDPT[510] -> The SAME PD as before
    pdpt[510] = (uint64_t)pd_addr | PAGE_PRESENT | PAGE_WRITE;

    // PD[0] is already set to map to Physical 0x0.
    // So accessing high memory will lead to the same physical RAM.

    pml4_physical_addr = pml4_addr;
    
    kprint("Page Tables Built Successfully.\n");
}