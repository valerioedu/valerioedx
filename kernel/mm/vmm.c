#include <vmm.h>
#include <kio.h>
#include <pmm.h>
#include <string.h>

#define MT_DEVICE_nGnRnE 0
#define MT_NORMAL        1

#define PT_AP_RW_EL1    (0ULL << 6)     // Read/Write, Kernel Only
#define PT_AP_RW_EL0    (1ULL << 6)     // Read/Write, User+Kernel
#define PT_AP_RO_EL1    (2ULL << 6)     // Read-Only,  Kernel Only
#define PT_AP_RO_EL0    (3ULL << 6)     // Read-Only,  User+Kernel

#define PT_PXN          (1ULL << 53)    // Privileged Execute Never
#define PT_UXN          (1ULL << 54)    // User Execute Never

static u64* root_table;

static inline void write_mair(u64 val) { asm volatile("msr mair_el1, %0" :: "r"(val)); }
static inline void write_tcr(u64 val)  { asm volatile("msr tcr_el1, %0" :: "r"(val)); }
static inline void write_ttbr0(u64 val){ asm volatile("msr ttbr0_el1, %0" :: "r"(val)); }
static inline void write_ttbr1(u64 val){ asm volatile("msr ttbr1_el1, %0" :: "r"(val)); }
static inline void write_sctlr(u64 val){ asm volatile("msr sctlr_el1, %0" :: "r"(val)); }

static inline void tlb_flush() {
    asm volatile("tlbi vmalle1is"); 
    asm volatile("dsb ish");
    asm volatile("isb");
}

static u64* get_next_table(u64* table, u64 idx) {
    if (table[idx] & PT_VALID) {
        // For 4KB granule, output address is bits [47:12]
        u64 phys = table[idx] & 0x0000FFFFFFFFF000ULL;
        return (u64*)phys;
    }
    
    // Invalid
    u64 new_table_phys = pmm_alloc_frame();
    if (!new_table_phys) return NULL;   // Out of memory
    
    memset((void*)new_table_phys, 0, PAGE_SIZE);
    
    table[idx] = new_table_phys | PT_TABLE | PT_VALID;
    
    return (u64*)new_table_phys;
}

// Maps a single 4KB page
void vmm_map_page(uintptr_t virt, uintptr_t phys, u64 flags) {
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
        entry |= (MT_DEVICE_nGnRnE << 2);
        entry |= PT_PXN | PT_UXN;
    } else {
        entry |= (MT_NORMAL << 2);
    }

    if (flags & VM_USER) {
        entry |= (flags & VM_WRITABLE) ? PT_AP_RW_EL0 : PT_AP_RO_EL0;
        if (flags & VM_NO_EXEC) entry |= PT_UXN;
    } else {
        entry |= (flags & VM_WRITABLE) ? PT_AP_RW_EL1 : PT_AP_RO_EL1;
        entry |= PT_UXN;        // User cannot execute kernel pages
        if (flags & VM_NO_EXEC) entry |= PT_PXN;
    }

    l3_table[l3_idx] = entry;
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
    root_table = (u64*)pmm_alloc_frame();
    memset(root_table, 0, PAGE_SIZE);

    // Index 0: Device-nGnRnE (Strict order, no cache)
    // Index 1: Normal Memory (Outer/Inner Write-Back Cacheable)
    u64 mair = (0x00UL << (8 * MT_DEVICE_nGnRnE)) | 
                    (0xFFUL << (8 * MT_NORMAL));
    write_mair(mair);

    // Setup Translation Control (TCR)
    // T0SZ=25 (User 512GB), T1SZ=25 (Kernel 512GB)
    // TG0=4KB, TG1=4KB
    // IPS=32-bit PA
    u64 tcr = (25UL << 0) | (0UL << 14) | (0UL << 32) | (25UL << 16) | (2UL << 30);
    write_tcr(tcr);
    
    // Kernel map
    u64 kernel_size = (u64)&_kernel_end - 0x40000000;
    vmm_map_region(0x40000000, 0x40000000, kernel_size, VM_WRITABLE);

    // Heap map
    vmm_map_region(0x50000000, 0x50000000, 8 * 1024 * 1024, VM_WRITABLE);

    // MMIO Peripherals map
    vmm_map_region((uintptr_t)gic, (uintptr_t)gic, 0x00100000, VM_DEVICE | VM_WRITABLE); // GIC
    vmm_map_region((uintptr_t)uart, (uintptr_t)uart, 0x00100000, VM_DEVICE | VM_WRITABLE); // UART
    vmm_map_region((uintptr_t)fwcfg, (uintptr_t)fwcfg, 0x00010000, VM_DEVICE | VM_WRITABLE); // FW_CFG
    vmm_map_region((uintptr_t)virtio, (uintptr_t)virtio, 0x00100000, VM_DEVICE | VM_WRITABLE); // VirtIO

    // Activates Paging
    write_ttbr0((u64)root_table); // User space
    write_ttbr1((u64)root_table); // Kernel space (Temporary sharing)

    tlb_flush();

    // Enables MMU and Caches
    u64 sctlr;
    asm volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    sctlr |= (1 << 0) | (1 << 2) | (1 << 12);
    write_sctlr(sctlr);

    kprintf("[VMM] 4KB Paging Engine Initialized.\n");
}