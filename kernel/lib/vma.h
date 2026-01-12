#ifndef VMA_H
#define VMA_H

#include <lib.h>
#include <vfs.h>

#define VMA_READ    (1 << 0)
#define VMA_WRITE   (1 << 1)
#define VMA_EXEC    (1 << 2)
#define VMA_SHARED  (1 << 3)  // Shared mapping (vs private)
#define VMA_FIXED   (1 << 4)  // Fixed address mapping
#define VMA_GROWSUP (1 << 5)  // Stack grows up
#define VMA_GROWSDN (1 << 6)  // Stack grows down

#define VMA_ANONYMOUS 0  // Anonymous memory (heap, stack)
#define VMA_FILE      1  // File-backed mapping
#define VMA_DEVICE    2  // Device mapping

typedef struct vma_struct {
    uintptr_t vm_start;        // Start address (inclusive)
    uintptr_t vm_end;          // End address (exclusive)
    u32 vm_flags;              // Protection and flags
    u8 vm_type;                // VMA type
    
    // For file-backed mappings
    struct vfs_node* vm_file;  // File backing this VMA
    u64 vm_pgoff;              // Offset in file (in pages)
    
    struct vma_struct* vm_next;
} vma_t;

// Address space structure
typedef struct mm_struct {
    u64* page_table;           // Root page table (physical address for TTBR)
    vma_t* vma_list;           // List of VMAs
    uintptr_t heap_start;      // Start of heap
    uintptr_t heap_end;        // Current heap end
    uintptr_t stack_start;     // Stack top
    uintptr_t mmap_base;       // Base for mmap allocations
} mm_struct_t;

// User space layout (39-bit address space)
#define USER_SPACE_START  0x0000000000000000ULL
#define USER_SPACE_END    0x0000004000000000ULL  // 256GB user space
#define USER_STACK_TOP    0x0000003FFFFF0000ULL  // Stack at top of user space
#define USER_STACK_SIZE   (8 * 1024 * 1024)      // 8MB default stack
#define USER_HEAP_START   0x0000000010000000ULL  // 256MB
#define USER_MMAP_BASE    0x0000002000000000ULL  // 128GB

mm_struct_t* mm_create();
void mm_destroy(mm_struct_t* mm);
vma_t* vma_create(uintptr_t start, uintptr_t end, u32 flags, u8 type);
int vma_insert(mm_struct_t* mm, vma_t* vma);
vma_t* vma_find(mm_struct_t* mm, uintptr_t addr);
int vma_unmap(mm_struct_t* mm, uintptr_t start, uintptr_t end);
uintptr_t vma_allocate(mm_struct_t* mm, uintptr_t addr, size_t length, u32 flags);
uintptr_t vma_allocate_file(mm_struct_t* mm, uintptr_t addr, size_t length, u32 flags, void* file, u64 offset);
int vma_page_fault(mm_struct_t* mm, uintptr_t addr, bool is_write);
int vma_expand_stack(mm_struct_t* mm, uintptr_t addr);
mm_struct_t* mm_duplicate(mm_struct_t* old_mm);

#endif