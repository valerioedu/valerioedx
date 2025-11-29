#include <vmm.h>
#include <uart.h>

// Global L1 Page Table (Translation Table)
// 512 entries * 8 bytes = 4096 bytes. Must be 4KB aligned.
__attribute__((aligned(4096))) 
uint64_t l1_table[512]; 

// Helpers to write system registers
static inline void write_mair(uint64_t val) { asm volatile("msr mair_el1, %0" :: "r"(val)); }
static inline void write_tcr(uint64_t val)  { asm volatile("msr tcr_el1, %0" :: "r"(val)); }
static inline void write_ttbr0(uint64_t val){ asm volatile("msr ttbr0_el1, %0" :: "r"(val)); }
static inline void write_ttbr1(uint64_t val){ asm volatile("msr ttbr1_el1, %0" :: "r"(val)); }
static inline void write_sctlr(uint64_t val){ asm volatile("msr sctlr_el1, %0" :: "r"(val)); }

static inline void tlb_flush() {
    asm volatile("tlbi vmalle1is"); // Invalidate all TLB entries
    asm volatile("dsb ish");        // Data Synchronization Barrier
    asm volatile("isb");            // Instruction Synchronization Barrier
}

void init_vmm() {
    // 1. Setup MAIR (Memory Attribute Indirection Register)
    // Defines what "Type 0" and "Type 1" memory mean.
    // Index 0: Device-nGnRnE (Strictly ordered, no cache) -> For UART/GIC
    // Index 1: Normal Memory (Write-Back Cacheable)       -> For RAM
    uint64_t mair = (0x00UL << (8 * MT_DEVICE)) | 
                    (0xFFUL << (8 * MT_NORMAL));
    write_mair(mair);

    // 2. Setup TCR (Translation Control Register)
    // T0SZ=25  -> Use 39-bit Virtual Address space (512 GB)
    // TG0=00   -> 4KB Granule size
    // IPS=32   -> 32-bit Physical Address size (4GB RAM limit)
    uint64_t tcr = (25UL << 0) | (0UL << 14) | (0UL << 32);
    write_tcr(tcr);

    // 3. Populate Identity Map (Virtual = Physical)
    for (int i = 0; i < 512; i++) l1_table[i] = 0;

    // Map 0x00000000 -> 1GB DEVICE (covers UART 0x09000000, GIC 0x08000000)
    l1_table[0] = 0x00000000 | VMM_DEVICE;

    // Map 0x40000000 -> 1GB RAM (Kernel + Start of RAM)
    l1_table[1] = 0x40000000 | VMM_KERNEL;

    // Map 0x80000000 -> 1GB RAM (Upper RAM)
    l1_table[2] = 0x80000000 | VMM_KERNEL;

    // 4. Set Translation Table Base Registers
    write_ttbr0((uint64_t)l1_table); // User / Lower Half
    write_ttbr1((uint64_t)l1_table); // Kernel / Upper Half

    // 5. Enable MMU and Caches
    // Read current SCTLR (System Control Register)
    uint64_t sctlr;
    asm volatile("mrs %0, sctlr_el1" : "=r"(sctlr));

    // Set bits: M (MMU Enable), C (Data Cache), I (Instruction Cache)
    sctlr |= (1 << 0) | (1 << 2) | (1 << 12);

    tlb_flush();       // Flush old TLB entries
    write_sctlr(sctlr); // WRITE SCTLR -> MMU IS NOW ON!
    asm volatile("isb");

    kprintf("[VMM] MMU Enabled. Caches ON.\n");
}