#include <elf.h>
#include <vfs.h>
#include <kio.h>
#include <vmm.h>
#include <vma.h>
#include <pmm.h>
#include <string.h>
#include <sched.h>
#include <heap.h>

extern task_t *current_task;

static bool elf_check_header(Elf64_Ehdr *hdr) {
    if (*(u32*)hdr->e_ident != ELF_MAGIC) return false;
    if (hdr->e_machine != EM_AARCH64) return false;
    if (hdr->e_type != ET_EXEC && hdr->e_type != ET_DYN) return false;
    return true;
}

// Helper to map a segment into the current process's page table
// We manually construct PTEs here because vmm_map_page() might use the kernel root_table
static bool map_segment(u64 vaddr, u64 paddr, u64 size, u32 flags) {
    if (!current_task || !current_task->proc || !current_task->proc->mm) return false;

    u64 *pgdir = current_task->proc->mm->page_table;
    u64 pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    for (u64 i = 0; i < pages; i++) {
        u64 virt = vaddr + (i * PAGE_SIZE);
        u64 phys = paddr + (i * PAGE_SIZE);

        u64 *pte = vmm_get_pte_from_table_alloc(pgdir, virt);
        if (!pte) return false;

        // Construct Attribute Flags
        u64 attr = phys | PT_PAGE | PT_VALID | PT_AF | PT_SH_INNER;
        
        // User Accessible
        attr |= VM_USER; 
        
        // Permissions
        if (flags & PF_W) attr |= PT_AP_RW_EL0; // RW for User
        else attr |= PT_AP_RO_EL0;              // RO for User
        
        // Execute Never (If X flag is NOT set, set UXN)
        if (!(flags & PF_X)) attr |= PT_UXN;

        // Access Flag (AF)
        attr |= PT_AF;
        
        // Normal Memory
        attr |= (MT_NORMAL << 2);

        *pte = attr;
    }
    
    // Invalidate TLB for this range
    // Note: Since we are likely modifying the ACTIVE table, we must flush
    asm volatile("tlbi vmalle1is"); 
    asm volatile("dsb ish");
    asm volatile("isb");
    
    return true;
}

// Loads an ELF file into the CURRENT process's address space.
// Returns the entry point address on success, or 0 on failure.
u64 elf_load(const char *path) {
    inode_t *file = vfs_lookup(path);
    if (!file) {
        kprintf("[ELF] File not found: %s\n", path);
        return 0;
    }

    vfs_open(file);

    Elf64_Ehdr hdr;
    if (vfs_read(file, 0, sizeof(Elf64_Ehdr), (u8*)&hdr) != sizeof(Elf64_Ehdr)) {
        kprintf("[ELF] Failed to read header\n");
        vfs_close(file);
        return 0;
    }

    if (!elf_check_header(&hdr)) {
        kprintf("[ELF] Invalid ELF header\n");
        vfs_close(file);
        return 0;
    }

    // Iterate Program Headers
    Elf64_Phdr phdr;
    for (int i = 0; i < hdr.e_phnum; i++) {
        u64 offset = hdr.e_phoff + (i * hdr.e_phentsize);
        
        if (vfs_read(file, offset, sizeof(Elf64_Phdr), (u8*)&phdr) != sizeof(Elf64_Phdr)) {
            kprintf("[ELF] Failed to read PHDR %d\n", i);
            break;
        }

        if (phdr.p_type == PT_LOAD) {
            u64 mem_size = phdr.p_memsz;
            u64 file_size = phdr.p_filesz;
            u64 vaddr = phdr.p_vaddr;
            u64 file_offset = phdr.p_offset;
            
            // Align start address to page boundary
            u64 vaddr_aligned = vaddr & ~(PAGE_SIZE - 1);
            u64 offset_diff = vaddr - vaddr_aligned;
            u64 total_size = mem_size + offset_diff;
            u64 pages_needed = (total_size + PAGE_SIZE - 1) / PAGE_SIZE;

            // Allocate Physical Memory
            // In a real OS, you might lazily map this (Demand Paging), 
            // but for now, we eagerly load.
            u64 phys_base = 0;
            
            // Allocate contiguous pages (Simplification: ideally alloc page by page)
            // Since we don't have pmm_alloc_contiguous, we alloc individually 
            // and map individually in a loop.
            
            for (u64 p = 0; p < pages_needed; p++) {
                u64 frame = pmm_alloc_frame();
                if (!frame) {
                    kprintf("[ELF] OOM loading segment\n");
                    vfs_close(file);
                    return 0;
                }
                
                // Temporary map the frame to Kernel space to copy data?
                // Or assume P2V works.
                void *kaddr = (void*)((uintptr_t)frame + PHYS_OFFSET);
                memset(kaddr, 0, PAGE_SIZE); // Clear for BSS safety

                // Map into User Space
                u64 page_vaddr = vaddr_aligned + (p * PAGE_SIZE);
                if (!map_segment(page_vaddr, frame, PAGE_SIZE, phdr.p_flags)) {
                    kprintf("[ELF] Failed to map page 0x%llx\n", page_vaddr);
                    vfs_close(file);
                    return 0;
                }
                
                // Calculate what part of the file goes into this page
                u64 page_offset_in_segment = (p * PAGE_SIZE) - offset_diff;
                
                // If this page contains file data
                if (p * PAGE_SIZE >= offset_diff && page_offset_in_segment < file_size) {
                    u64 read_sz = PAGE_SIZE;
                    u64 dest_offset = 0;

                    // Handle first page alignment
                    if (p == 0) {
                        dest_offset = offset_diff;
                        read_sz = PAGE_SIZE - offset_diff;
                        page_offset_in_segment = 0;
                    }
                    
                    // Handle last page (truncation)
                    if (page_offset_in_segment + read_sz > file_size) {
                        read_sz = file_size - page_offset_in_segment;
                    }

                    // Read from file directly into the physical frame (via kernel mapping)
                    vfs_read(file, 
                             file_offset + page_offset_in_segment, 
                             read_sz, 
                             (u8*)kaddr + dest_offset);
                }
            }
        }
    }

    vfs_close(file);
    kprintf("[ELF] Loaded %s entry: 0x%llx\n", path, hdr.e_entry);
    return hdr.e_entry;
}