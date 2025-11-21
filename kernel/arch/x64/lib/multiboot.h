#include <lib.h>

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

/* The types of memory in the memory map */
#define MULTIBOOT_MEMORY_AVAILABLE        1
#define MULTIBOOT_MEMORY_RESERVED         2
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE 3
#define MULTIBOOT_MEMORY_NVS              4
#define MULTIBOOT_MEMORY_BADRAM           5

struct multiboot_aout_symbol_table {
    uint32_t tabsize;
    uint32_t strsize;
    uint32_t addr;
    uint32_t reserved;
};

struct multiboot_elf_section_header_table {
    uint32_t num;
    uint32_t size;
    uint32_t addr;
    uint32_t shndx;
};

struct multiboot_info {
    /* Bitmask: tells you which of the following fields are valid */
    uint32_t flags;

    /* Basic memory limits (KB) - Unreliable, use mmap instead */
    uint32_t mem_lower;
    uint32_t mem_upper;

    /* Which disk/partition we booted from */
    uint32_t boot_device;

    /* Kernel command line (e.g., "kernel.bin debug=on") */
    uint32_t cmdline;

    /* Boot Modules (initrd, drivers loaded by GRUB) */
    uint32_t mods_count;
    uint32_t mods_addr;

    /* Symbol tables (useful for stack tracing) */
    union {
        struct multiboot_aout_symbol_table aout_sym;
        struct multiboot_elf_section_header_table elf_sec;
    } u;

    /* THE MEMORY MAP (BIOS E820) */
    uint32_t mmap_length;
    uint32_t mmap_addr;

    /* Drive Info (mostly legacy CHS stuff) */
    uint32_t drives_length;
    uint32_t drives_addr;

    /* ROM Configuration Table */
    uint32_t config_table;

    /* Bootloader Name (e.g., "GNU GRUB 2.04") */
    uint32_t boot_loader_name;

    /* Advanced Power Management (Legacy) */
    uint32_t apm_table;

    /* Video Information (VBE/GOP) - Crucial for Graphics */
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;

    /* Framebuffer info (added in later Multiboot specs) */
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
};

/* The Memory Map Buffer Entry */
struct multiboot_mmap_entry {
    uint32_t size;      // Size of this entry structure
    u32 addr_low;      // Base address of the memory region
    u32 addr_high;
    u32 len_low;       // Length of the region
    u32 len_high;
    uint32_t type;      // 1=Available, anything else=Reserved
} __attribute__((packed));