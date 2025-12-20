#include <vma.h>
#include <heap.h>
#include <pmm.h>
#include <string.h>
#include <kio.h>
#include <spinlock.h>
#include <vmm.h>

static spinlock_t vma_lock = 0;

mm_struct_t* mm_create() {
    mm_struct_t* mm = (mm_struct_t*)kmalloc(sizeof(mm_struct_t));
    if (!mm) return NULL;
    
    memset(mm, 0, sizeof(mm_struct_t));
    
    u64 pt_phys = pmm_alloc_frame();
    if (!pt_phys) {
        kfree(mm);
        return NULL;
    }
    
    memset(P2V(pt_phys), 0, PAGE_SIZE);
    
    mm->page_table = (u64*)pt_phys;
    mm->vma_list = NULL;
    mm->heap_start = USER_HEAP_START;
    mm->heap_end = USER_HEAP_START;
    mm->stack_start = USER_STACK_TOP;
    mm->mmap_base = USER_MMAP_BASE;
    
    vma_t* stack = vma_create(
        USER_STACK_TOP - USER_STACK_SIZE,
        USER_STACK_TOP,
        VMA_READ | VMA_WRITE | VMA_GROWSDN,
        VMA_ANONYMOUS
    );
    
    if (stack) {
        vma_insert(mm, stack);
    }
    
    kprintf("[ [CMM [W] Created address space: PT=0x%llx\n", pt_phys);
    return mm;
}

void mm_destroy(mm_struct_t* mm) {
    if (!mm) return;
    
    u32 flags = spinlock_acquire_irqsave(&vma_lock);
    
    vma_t* vma = mm->vma_list;
    while (vma) {
        vma_t* next = vma->vm_next;
        
        for (uintptr_t addr = vma->vm_start; addr < vma->vm_end; addr += PAGE_SIZE) {
            u64* pte = vmm_get_pte_from_table((u64*)P2V((uintptr_t)mm->page_table), addr);
            if (pte && (*pte & PT_VALID)) {
                u64 phys = *pte & 0x0000FFFFFFFFF000ULL;
                pmm_free_frame(phys);
                *pte = 0;
            }
        }
        
        kfree(vma);
        vma = next;
    }
    
    u64* l1_table = (u64*)P2V((uintptr_t)mm->page_table);
    for (int i = 0; i < 512; i++) {
        if (!(l1_table[i] & PT_VALID)) continue;
        
        u64* l2_table = (u64*)P2V(l1_table[i] & 0x0000FFFFFFFFF000ULL);
        for (int j = 0; j < 512; j++) {
            if (!(l2_table[j] & PT_VALID)) continue;
            
            u64* l3_table = (u64*)P2V(l2_table[j] & 0x0000FFFFFFFFF000ULL);
            pmm_free_frame(V2P(l3_table));
        }
        pmm_free_frame(V2P(l2_table));
    }
    pmm_free_frame(V2P(l1_table));
    
    spinlock_release_irqrestore(&vma_lock, flags);
    kfree(mm);
}

vma_t* vma_create(uintptr_t start, uintptr_t end, u32 flags, u8 type) {
    if (start >= end || start < USER_SPACE_START || end > USER_SPACE_END) {
        return NULL;
    }
    
    vma_t* vma = (vma_t*)kmalloc(sizeof(vma_t));
    if (!vma) return NULL;
    
    memset(vma, 0, sizeof(vma_t));
    vma->vm_start = start;
    vma->vm_end = end;
    vma->vm_flags = flags;
    vma->vm_type = type;
    vma->vm_next = NULL;
    vma->vm_file = NULL;
    vma->vm_pgoff = 0;
    
    return vma;
}

// Insert VMA into address space (sorted by start address)
int vma_insert(mm_struct_t* mm, vma_t* new_vma) {
    if (!mm || !new_vma) return -1;
    
    u32 flags = spinlock_acquire_irqsave(&vma_lock);
    
    // Check for overlaps
    vma_t* vma = mm->vma_list;
    while (vma) {
        if (!(new_vma->vm_end <= vma->vm_start || new_vma->vm_start >= vma->vm_end)) {
            spinlock_release_irqrestore(&vma_lock, flags);
            return -1;  // Overlap detected
        }
        
        vma = vma->vm_next;
    }
    
    // Insert in sorted order
    if (!mm->vma_list || new_vma->vm_start < mm->vma_list->vm_start) {
        new_vma->vm_next = mm->vma_list;
        mm->vma_list = new_vma;
    } else {
        vma_t* prev = mm->vma_list;

        while (prev->vm_next && prev->vm_next->vm_start < new_vma->vm_start)
            prev = prev->vm_next;

        new_vma->vm_next = prev->vm_next;
        prev->vm_next = new_vma;
    }
    
    spinlock_release_irqrestore(&vma_lock, flags);
    return 0;
}

// Find VMA containing address
vma_t* vma_find(mm_struct_t* mm, uintptr_t addr) {
    if (!mm) return NULL;
    
    u32 flags = spinlock_acquire_irqsave(&vma_lock);
    
    vma_t* vma = mm->vma_list;
    while (vma) {
        if (addr >= vma->vm_start && addr < vma->vm_end) {
            spinlock_release_irqrestore(&vma_lock, flags);
            return vma;
        }

        vma = vma->vm_next;
    }
    
    spinlock_release_irqrestore(&vma_lock, flags);
    return NULL;
}

int vma_unmap(mm_struct_t* mm, uintptr_t start, uintptr_t end) {
    if (!mm || start >= end) return -1;
    
    u32 flags = spinlock_acquire_irqsave(&vma_lock);
    
    vma_t* vma = mm->vma_list;
    vma_t* prev = NULL;
    
    while (vma) {
        vma_t* next = vma->vm_next;
        
        // Check if this VMA overlaps with the range to unmap
        if (vma->vm_end <= start || vma->vm_start >= end) {
            prev = vma;
            vma = next;
            continue;
        }
        
        // Case 1: Complete overlap - remove entire VMA
        if (start <= vma->vm_start && end >= vma->vm_end) {
            for (uintptr_t addr = vma->vm_start; addr < vma->vm_end; addr += PAGE_SIZE) {
                u64* pte = vmm_get_pte_from_table((u64*)P2V((uintptr_t)mm->page_table), addr);

                if (pte && (*pte & PT_VALID)) {
                    u64 phys = *pte & 0x0000FFFFFFFFF000ULL;
                    pmm_free_frame(phys);
                    *pte = 0;
                }
            }
            
            if (prev) prev->vm_next = next;
            else mm->vma_list = next;
            
            kfree(vma);
            vma = next;
            continue;
        }
        
        // Case 2: Partial overlap at start
        if (start <= vma->vm_start && end < vma->vm_end) {
            for (uintptr_t addr = vma->vm_start; addr < end; addr += PAGE_SIZE) {
                u64* pte = vmm_get_pte_from_table((u64*)P2V((uintptr_t)mm->page_table), addr);

                if (pte && (*pte & PT_VALID)) {
                    u64 phys = *pte & 0x0000FFFFFFFFF000ULL;
                    pmm_free_frame(phys);
                    *pte = 0;
                }
            }

            vma->vm_start = end;
        }
        
        // Case 3: Partial overlap at end
        else if (start > vma->vm_start && end >= vma->vm_end) {
            for (uintptr_t addr = start; addr < vma->vm_end; addr += PAGE_SIZE) {
                u64* pte = vmm_get_pte_from_table((u64*)P2V((uintptr_t)mm->page_table), addr);

                if (pte && (*pte & PT_VALID)) {
                    u64 phys = *pte & 0x0000FFFFFFFFF000ULL;
                    pmm_free_frame(phys);
                    *pte = 0;
                }
            }

            vma->vm_end = start;
        }
        
        // Case 4: Hole in middle - split VMA
        else if (start > vma->vm_start && end < vma->vm_end) {
            vma_t* new_vma = vma_create(end, vma->vm_end, vma->vm_flags, vma->vm_type);

            if (!new_vma) {
                spinlock_release_irqrestore(&vma_lock, flags);
                return -1;
            }
            
            for (uintptr_t addr = start; addr < end; addr += PAGE_SIZE) {
                u64* pte = vmm_get_pte_from_table((u64*)P2V((uintptr_t)mm->page_table), addr);

                if (pte && (*pte & PT_VALID)) {
                    u64 phys = *pte & 0x0000FFFFFFFFF000ULL;
                    pmm_free_frame(phys);
                    *pte = 0;
                }
            }
            
            vma->vm_end = start;
            
            new_vma->vm_next = vma->vm_next;
            vma->vm_next = new_vma;
        }
        
        prev = vma;
        vma = next;
    }
    
    spinlock_release_irqrestore(&vma_lock, flags);
    return 0;
}

// mmap primitive
uintptr_t vma_allocate(mm_struct_t* mm, uintptr_t addr, size_t length, u32 flags) {
    if (!mm || length == 0) return 0;
    
    // Align length to page boundary
    length = (length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    
    u32 lock_flags = spinlock_acquire_irqsave(&vma_lock);
    
    // If addr is 0, find a suitable location
    if (addr == 0) {
        addr = mm->mmap_base;
        
        // Find free space
        vma_t* vma = mm->vma_list;
        while (vma) {
            if (addr + length <= vma->vm_start)
                break;

            if (addr < vma->vm_end)
                addr = vma->vm_end;

            vma = vma->vm_next;
        }
        
        // Check if ran out of space
        if (addr + length > USER_SPACE_END) {
            spinlock_release_irqrestore(&vma_lock, lock_flags);
            return 0;
        }
    }
    
    spinlock_release_irqrestore(&vma_lock, lock_flags);
    
    vma_t* new_vma = vma_create(addr, addr + length, flags, VMA_ANONYMOUS);
    if (!new_vma) return 0;
    
    if (vma_insert(mm, new_vma) < 0) {
        kfree(new_vma);
        return 0;
    }
    
    return addr;
}

// Handle page fault in user space
int vma_page_fault(mm_struct_t* mm, uintptr_t addr, bool is_write) {
    if (!mm) return -1;
    
    vma_t* vma = vma_find(mm, addr);
    if (!vma) {
        // Check if this could be stack expansion
        if (addr < USER_STACK_TOP && addr >= USER_STACK_TOP - USER_STACK_SIZE * 2)
            return vma_expand_stack(mm, addr);

        return -1;  // No VMA covers this address
    }
    
    if (is_write && !(vma->vm_flags & VMA_WRITE))
        return -1;  // Write to read-only memory
    
    u64* pte = vmm_get_pte_from_table_alloc((u64*)P2V((uintptr_t)mm->page_table), addr);
    if (!pte) return -1;
    
    if (!(*pte & PT_VALID)) {
        u64 phys = pmm_alloc_frame();
        if (!phys) return -1;
        
        memset(P2V(phys), 0, PAGE_SIZE);
        
        u64 entry = phys | PT_VALID | PT_AF | PT_PAGE | PT_SH_INNER;
        entry |= (MT_NORMAL << 2);  // Normal memory
        entry |= PT_AP_RW_EL0;      // User accessible
        
        if (!(vma->vm_flags & VMA_WRITE)) {
            entry &= ~(1ULL << 7);  // Clear AP[2] to make read-only
            entry |= PT_AP_RO_EL0;
        }
        
        if (!(vma->vm_flags & VMA_EXEC))
            entry |= PT_UXN;  // User execute never
        
        *pte = entry;
        
        asm volatile("tlbi vaale1is, %0" :: "r"(addr >> 12));
        asm volatile("dsb ish");
        asm volatile("isb");
        
        return 0;
    }
    
    if (is_write && (*pte & PT_SW_COW)) {
        u64 old_phys = *pte & 0x0000FFFFFFFFF000ULL;
        u64 new_phys = pmm_alloc_frame();
        if (!new_phys) return -1;
        
        memcpy(P2V(new_phys), P2V(old_phys), PAGE_SIZE);
        
        u64 entry = new_phys | PT_VALID | PT_AF | PT_PAGE | PT_SH_INNER;
        entry |= (MT_NORMAL << 2);  // Normal memory
        entry |= PT_AP_RW_EL0;      // User accessible, writable
        
        if (!(vma->vm_flags & VMA_EXEC))
            entry |= PT_UXN;  // User execute never
        
        *pte = entry;
        
        pmm_free_frame(old_phys);
        
        asm volatile("tlbi vaale1is, %0" :: "r"(addr >> 12));
        asm volatile("dsb ish");
        asm volatile("isb");
        
        return 0;
    }
    
    return -1;
}

// Expand stack downward
int vma_expand_stack(mm_struct_t* mm, uintptr_t addr) {
    if (!mm) return -1;
    
    vma_t* stack_vma = NULL;
    vma_t* vma = mm->vma_list;
    while (vma) {
        if (vma->vm_flags & VMA_GROWSDN) {
            stack_vma = vma;
            break;
        }
        vma = vma->vm_next;
    }
    
    if (!stack_vma) return -1;
    
    // Align address down to page boundary
    uintptr_t new_start = addr & ~(PAGE_SIZE - 1);
    
    // Prevent stack overflow into other VMAs
    if (new_start < USER_STACK_TOP - USER_STACK_SIZE * 2)
        return -1;
    
    u32 flags = spinlock_acquire_irqsave(&vma_lock);
    
    // Expand the VMA
    stack_vma->vm_start = new_start;
    
    spinlock_release_irqrestore(&vma_lock, flags);
    
    kprintf("[ [CMM [W] Stack expanded to 0x%llx\n", new_start);
    return 0;
}

mm_struct_t* mm_duplicate(mm_struct_t* old_mm) {
    if (!old_mm) return NULL;

    mm_struct_t* new_mm = (mm_struct_t*)kmalloc(sizeof(mm_struct_t));
    if (!new_mm) return NULL;
    memset(new_mm, 0, sizeof(mm_struct_t));

    u64 pt_phys = pmm_alloc_frame();
    if (!pt_phys) {
        kfree(new_mm);
        return NULL;
    }

    memset(P2V(pt_phys), 0, PAGE_SIZE);

    new_mm->page_table = (u64*)pt_phys;
    new_mm->heap_start = old_mm->heap_start;
    new_mm->heap_end   = old_mm->heap_end;
    new_mm->stack_start = old_mm->stack_start;
    new_mm->mmap_base  = old_mm->mmap_base;

    // Iterates through parent's VMAs and clone them
    vma_t* old_vma = old_mm->vma_list;
    while (old_vma) {
        vma_t* new_vma = vma_create(old_vma->vm_start, old_vma->vm_end, old_vma->vm_flags, old_vma->vm_type);
        if (!new_vma) {
            // TODO: clean everything up
            return NULL; 
        }

        // Clone metadata
        new_vma->vm_file = old_vma->vm_file;
        new_vma->vm_pgoff = old_vma->vm_pgoff;
        
        vma_insert(new_mm, new_vma);

        for (uintptr_t addr = old_vma->vm_start; addr < old_vma->vm_end; addr += PAGE_SIZE) {
            u64* old_pte = vmm_get_pte_from_table((u64*)P2V((uintptr_t)old_mm->page_table), addr);

            if (old_pte && (*old_pte & PT_VALID)) {
                u64* new_pte = vmm_get_pte_from_table_alloc((u64*)P2V((uintptr_t)new_mm->page_table), addr);
                if (!new_pte) continue;

                u64 phys = *old_pte & 0x0000FFFFFFFFF000ULL;

                if (!(old_vma->vm_flags & VMA_SHARED) && 
                   ((*old_pte & PT_AP_RW_EL0) || (*old_pte & PT_AP_RW_EL1))) {
                    
                    // Set Read-Only and COW flag in parent
                    *old_pte &= ~(3ULL << 6); // Clear AP bits
                    *old_pte |= PT_AP_RO_EL0; // Set to User Read-Only
                    *old_pte |= PT_SW_COW;    // Set our software COW marker
                }

                *new_pte = *old_pte;
                
                pmm_inc_ref(phys);
            }
        }
        old_vma = old_vma->vm_next;
    }

    asm volatile("tlbi vmalle1is"); 
    asm volatile("dsb ish");
    asm volatile("isb");

    return new_mm;
}