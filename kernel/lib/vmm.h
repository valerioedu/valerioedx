#ifndef VMM_H
#define VMM_H

#include <lib.h>

/* * AArch64 Translation Table Descriptors (Level 1 / Block Descriptor)
 * We use 1GB blocks for simplicity right now.
 */

// Descriptor Types
#define PT_TABLE        0b11    // Points to next level table
#define PT_BLOCK        0b01    // Points to physical memory (1GB or 2MB)
#define PT_PAGE         0b11    // Points to physical memory (4KB)

// Access Permissions
#define PT_AF           (1ULL << 10) // Access Flag (Must be 1)
#define PT_SH_INNER     (3ULL << 8)  // Inner Shareable
#define PT_UXN          (1ULL << 54) // User Execute Never
#define PT_PXN          (1ULL << 53) // Privileged Execute Never

/* MAIR Indices (matches setup in vmm.c) */
#define MT_DEVICE       0x0
#define MT_NORMAL       0x1

/*
 * Combined Attributes for our mappings
 */

// DEVICE: UART, GIC (No Cache, Ordered)
#define VMM_DEVICE  (PT_BLOCK | PT_AF | (MT_DEVICE << 2) | PT_UXN | PT_PXN)

// NORMAL: RAM (Cacheable)
#define VMM_KERNEL  (PT_BLOCK | PT_AF | PT_SH_INNER | (MT_NORMAL << 2))

void init_vmm();

#endif