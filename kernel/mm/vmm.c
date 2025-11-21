#include <vmm.h>
#include <pmm.h>
#include <vga.h>
#include <io.h>

// 32-bit Page Table Entry Flags
#define PAGE_PRESENT    (1 << 0)
#define PAGE_WRITE      (1 << 1)
#define PAGE_USER       (1 << 2)
#define PAGE_HUGE       (1 << 7) // 4MB Page (PSE)

// The Page Directory (1024 entries * 4 bytes = 4KB)
uint32_t* page_directory = 0;

void setup_paging() {
    // 1. Allocate one frame for the Page Directory
    // We cast to uint32_t* because we are in 32-bit mode
    page_directory = (uint32_t*)pmm_alloc_frame();

    // 2. Clear it (Not present)
    for (int i = 0; i < 1024; i++) {
        page_directory[i] = 0 | PAGE_WRITE; 
    }

    // 3. Identity Map the first 4MB (Virtual 0 -> Physical 0)
    // Index 0 covers 0x00000000 to 0x00400000
    page_directory[0] = 0x00000000 | PAGE_PRESENT | PAGE_WRITE | PAGE_HUGE;

    // 4. Load CR3
    asm volatile("mov %0, %%cr3" :: "r"(page_directory));

    // 5. Enable PSE (CR4 Bit 4)
    uint32_t cr4;
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1 << 4);
    asm volatile("mov %0, %%cr4" :: "r"(cr4));

    // 6. Enable Paging (CR0 Bit 31)
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= (1 << 31);
    asm volatile("mov %0, %%cr0" :: "r"(cr0));

    kprint("32-bit Paging Enabled (PSE).\n");
}

// Map a physical address to a virtual address (using 4MB pages)
void vmm_map_physical(uint32_t phys_addr, uint32_t virt_addr) {
    uint32_t pd_index = virt_addr >> 22;
    uint32_t entry = (phys_addr & 0xFFC00000) | PAGE_PRESENT | PAGE_WRITE | PAGE_HUGE;
    page_directory[pd_index] = entry;
    
    // RELOAD CR3 to flush everything (Brute force fix)
    asm volatile("mov %0, %%cr3" :: "r"(page_directory) : "memory");
}