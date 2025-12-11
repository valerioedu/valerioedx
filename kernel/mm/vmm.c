#include <vmm.h>
#include <kio.h>
#include <pmm.h>
#include <string.h>

// TODO: Implement VMA

#define MT_DEVICE_nGnRnE 0
#define MT_NORMAL        1

#define PT_AP_RW_EL1    (0ULL << 6)     // Read/Write, Kernel Only
#define PT_AP_RW_EL0    (1ULL << 6)     // Read/Write, User+Kernel
#define PT_AP_RO_EL1    (2ULL << 6)     // Read-Only,  Kernel Only
#define PT_AP_RO_EL0    (3ULL << 6)     // Read-Only,  User+Kernel

#define PT_PXN          (1ULL << 53)    // Privileged Execute Never
#define PT_UXN          (1ULL << 54)    // User Execute Never

static u64* root_table;

// Helper flag
static bool paging_enabled = false;

static void* safe_P2V(u64 phys) {
    if (paging_enabled) {
        return (void*)((uintptr_t)phys + PHYS_OFFSET);
    }
    return (void*)phys;
}

#ifdef ARM
static inline void isb() { asm volatile("isb" ::: "memory"); }
static inline void dsb() { asm volatile("dsb ish" ::: "memory"); }

static inline void tlb_flush() {
    asm volatile("tlbi vmalle1is"); 
    dsb();
    isb();
}

static inline void write_mair(u64 val) { asm volatile("msr mair_el1, %0" :: "r"(val) : "memory"); }
static inline void write_tcr(u64 val)  { asm volatile("msr tcr_el1, %0" :: "r"(val) : "memory"); }
static inline void write_ttbr0(u64 val){ asm volatile("msr ttbr0_el1, %0" :: "r"(val) : "memory"); }
static inline void write_ttbr1(u64 val){ asm volatile("msr ttbr1_el1, %0" :: "r"(val) : "memory"); }
static inline void write_sctlr(u64 val){ asm volatile("msr sctlr_el1, %0" :: "r"(val) : "memory"); }

static u64* get_next_table(u64* table, u64 idx) {
    if (table[idx] & PT_VALID) {
        // For 4KB granule, output address is bits [47:12]
        u64 phys = table[idx] & 0x0000FFFFFFFFF000ULL;
        return (u64*)safe_P2V(phys);
    }

    // Invalid
    u64 new_table_phys = pmm_alloc_frame();
    if (!new_table_phys) return NULL;   // Out of memory
    
    u64* new_table_virt = (u64*)safe_P2V(new_table_phys);

    memset((void*)new_table_virt, 0, PAGE_SIZE);
    
    table[idx] = new_table_phys | PT_TABLE | PT_VALID;

    return (u64*)new_table_virt;
}

// Gets the page table entry
static u64* vmm_get_pte(uintptr_t virt, bool allocate) {
    u64 l1_idx = (virt >> 30) & 0x1FF;
    u64 l2_idx = (virt >> 21) & 0x1FF;
    u64 l3_idx = (virt >> 12) & 0x1FF;

    if (!root_table) return NULL;

    u64* l2_table = get_next_table(root_table, l1_idx);
    if (!l2_table) return NULL;

    u64* l3_table = get_next_table(l2_table, l2_idx);
    if (!l3_table) return NULL;

    return &l3_table[l3_idx];
}
#endif

void dcache_clean_poc(void *addr, size_t size) {
#ifdef ARM
    uintptr_t start = (uintptr_t)addr & ~(64 - 1);
    uintptr_t end = (uintptr_t)addr + size;
    
    asm volatile("dsb sy" ::: "memory");
    while (start < end) {
        asm volatile("dc cvac, %0" :: "r"(start) : "memory");
        start += 64;
    }
    asm volatile("dsb sy" ::: "memory");
    asm volatile("isb" ::: "memory");
#endif
}

// Maps a single 4KB page
void vmm_map_page(uintptr_t virt, uintptr_t phys, u64 flags) {
#ifdef ARM
    // Indices for 39-bit Virtual Address Space
    // L1: bits [38:30]
    // L2: bits [29:21]
    // L3: bits [20:12]
    
    u64 l1_idx = (virt >> 30) & 0x1FF;
    u64 l2_idx = (virt >> 21) & 0x1FF;
    u64 l3_idx = (virt >> 12) & 0x1FF;

    u64* l2_table = get_next_table(root_table, l1_idx);
    if (!l2_table) {
        kprintf("[VMM] Failed to allocate L2 table for 0x%llx\n", virt);
        return;
    }

    // Walk L2 -> L3
    u64* l3_table = get_next_table(l2_table, l2_idx);
    if (!l3_table) {
        kprintf("[VMM] Failed to allocate L3 table for 0x%llx\n", virt);
        return;
    }

    u64 entry = phys | PT_PAGE | PT_VALID | PT_AF | PT_SH_INNER;
    
    if (flags & VM_DEVICE) {
        entry |= PT_PXN | PT_UXN;
    } else {
        entry |= (MT_NORMAL << 2);
    }

    if (flags & VM_USER) {
        if (flags & VM_NO_EXEC) entry |= PT_UXN;
        
        if (flags & PT_SW_COW) {
            entry |= PT_AP_RO_EL0;   // Read-only for user
            entry |= PT_SW_COW;      // Preserve the COW marker
        } else if (flags & VM_WRITABLE) {
            entry |= PT_AP_RW_EL0;   // Read-Write for user
        } else {
            entry |= PT_AP_RO_EL0;   // Read-only for user
        }
    } else {
        entry |= PT_UXN;
        if (flags & VM_WRITABLE) {
            entry |= PT_AP_RW_EL1;
        } else {
            entry |= PT_AP_RO_EL1;
        }
    }

    l3_table[l3_idx] = entry;
#endif
}

// Maps a contiguous range of physical memory
void vmm_map_region(uintptr_t virt, uintptr_t phys, size_t size, u64 flags) {
    size_t mapped = 0;
    while (mapped < size) {
        vmm_map_page(virt + mapped, phys + mapped, flags);
        mapped += PAGE_SIZE;
    }
}

extern u64 _kernel_end;
extern u8 *uart;
extern u64 *gic;
extern u64 *fwcfg;
extern u64 *virtio;

void init_vmm() {
    u64 root_phys = pmm_alloc_frame();
    memset((void*)root_phys, 0, PAGE_SIZE);

    root_table = (u64*)root_phys;

#ifdef ARM
    // Index 0: Device-nGnRnE (Strict order, no cache)
    // Index 1: Normal Memory (Outer/Inner Write-Back Cacheable)
    u64 mair = (0x00UL << (8 * MT_DEVICE_nGnRnE)) | 
                    (0xFFUL << (8 * MT_NORMAL));
    write_mair(mair);

    // Setup Translation Control (TCR)
    // T0SZ=25 (User 512GB), T1SZ=25 (Kernel 512GB)
    // TG0=4KB, TG1=4KB
    // IPS=40-bit PA
    u64 tcr = (25UL << 0) | (0UL << 14) | (2UL << 32) | (25UL << 16) | (2UL << 30);
    write_tcr(tcr);
#endif

    // Kernel map
    u64 kernel_size = (u64)&_kernel_end - 0x40000000;
    vmm_map_region(0x40000000, 0x40000000, kernel_size, VM_WRITABLE);

    vmm_map_region(PHYS_OFFSET + PHY_RAM_BASE, PHY_RAM_BASE, phy_ram_size, VM_WRITABLE);

    // Heap map
    vmm_map_region(0x50000000, 0x50000000, 8 * 1024 * 1024, VM_WRITABLE);

#ifdef ARM
    // MMIO Peripherals map
    vmm_map_region(PHYS_OFFSET + (uintptr_t)gic, (uintptr_t)gic, 0x00100000, VM_DEVICE | VM_WRITABLE); // GIC
    vmm_map_region(PHYS_OFFSET + (uintptr_t)uart, (uintptr_t)uart, 0x00100000, VM_DEVICE | VM_WRITABLE); // UART
    vmm_map_region(PHYS_OFFSET + (uintptr_t)fwcfg, (uintptr_t)fwcfg, 0x00010000, VM_DEVICE | VM_WRITABLE); // FW_CFG
    vmm_map_region(PHYS_OFFSET + (uintptr_t)virtio, (uintptr_t)virtio, 0x00100000, VM_DEVICE | VM_WRITABLE); // VirtIO

    // Activates Paging
    write_ttbr0(root_phys); // User space
    write_ttbr1(root_phys); // Kernel space (Temporary sharing)

    tlb_flush();

    // Enables MMU and Caches
    u64 sctlr;
    asm volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    sctlr |= (1 << 0) | (1 << 2) | (1 << 12);
    write_sctlr(sctlr);

    isb();

    uart = (u8*)((uintptr_t)uart + PHYS_OFFSET);
    gic = (u64*)((uintptr_t)gic + PHYS_OFFSET);
    fwcfg = (u64*)((uintptr_t)fwcfg + PHYS_OFFSET);
    virtio = (u64*)((uintptr_t)virtio + PHYS_OFFSET);
#endif

    paging_enabled = true;
    
    root_table = (u64*)((uintptr_t)root_phys + PHYS_OFFSET);
    kprintf("[VMM] Higher Half Kernel Initialized.\n");
#ifdef ARM
    // Switches execution to Higher Half
    asm volatile(
        "mov x0, %0         \n"     // Loads PHYS_OFFSET into x0
        "add sp, sp, x0     \n"     // Adds the offset to the SP
        "add x29, x29, x0   \n"     // then to the frame pointer
        "adr x1, 1f         \n"
        "add x1, x1, x0     \n"     // Calculates high address
        "br x1              \n"     // Jump to high address 
        "1:                 \n"
        : : "r"(PHYS_OFFSET) : "x0", "x1", "memory"
    );
#endif
}

#ifdef ARM
// Returns 0 on success, -1 on unrecoverable error (If in userland kill the process)
int vmm_handle_page_fault(uintptr_t virt, bool is_write) {
    u64* pte = vmm_get_pte(virt, false);
    if (!pte) return -1; // Tables missing, fatal for now (to implement large range demand paging)

    u64 entry = *pte;

    // Case 1: demand paging
    // Page is not valid (Bit 0 is 0)
    if (!(entry & PT_VALID)) {
        // TODO: check VMA structures here.
        
        u64 new_frame = pmm_alloc_frame();
        if (!new_frame) {
            kprintf("[VMM] OOM during demand paging\n");
            return -1;
        }

        memset((void*)P2V(new_frame), 0, PAGE_SIZE);

        // Later check VMA  permissions
        u64 new_entry = new_frame | PT_PAGE | PT_VALID | PT_AF | PT_SH_INNER;
        new_entry |= (MT_NORMAL << 2);  // Normal memory
        new_entry |= PT_AP_RW_EL0;      // User read/write
        new_entry |= PT_UXN;            // No user execute (data page)
        
        *pte = new_entry;
        
        // Invalidate TLB for this address
        asm volatile("tlbi vaale1is, %0" :: "r"(virt >> 12));
        asm volatile("dsb ish");
        asm volatile("isb");
        
        return 0;
    }

    // Case 2: copy and write
    // Page is Valid, Write Access attempted, and SW_COW bit is set
    if ((entry & PT_VALID) && is_write && (entry & PT_SW_COW)) {
        u64 old_phys = entry & 0x0000FFFFFFFFF000ULL;
        
        // Allocate new frame
        u64 new_phys = pmm_alloc_frame();
        if (!new_phys) return -1;

        memcpy((void*)P2V(new_phys), (void*)P2V(old_phys), PAGE_SIZE);

        u64 new_entry = entry;
        new_entry &= ~0x0000FFFFFFFFF000ULL; // Clear old address
        new_entry |= new_phys;               // Set new address
        new_entry &= ~PT_SW_COW;             // Clear COW flag
        new_entry &= ~(1ULL << 7);           // Clear AP[2] (ReadOnly bit) to make it RW

        *pte = new_entry;

        pmm_free_frame(old_phys); 

        // Flush TLB
        asm volatile("tlbi vaale1is, %0" :: "r"(virt >> 12));
        asm volatile("dsb ish");
        asm volatile("isb");

        return 0;
    }

    // Segfault
    return -1;
}
#endif