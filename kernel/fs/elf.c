#include <elf.h>
#include <vfs.h>
#include <heap.h>
#include <pmm.h>
#include <vmm.h>
#include <vma.h>
#include <string.h>
#include <kio.h>

int elf_validate(const u8* data, size_t size) {
    if (size < sizeof(elf64_ehdr_t)) {
        kprintf("[ [RELF [W] File too small for ELF header\n");
        return -1;
    }

    elf64_ehdr_t* ehdr = (elf64_ehdr_t*)data;

    if (*(u32*)ehdr->e_ident != ELF_MAGIC) {
        kprintf("[ [RELF [W] Invalid ELF magic: 0x%x\n", *(u32*)ehdr->e_ident);
        return -1;
    }

    // Check 64-bit
    if (ehdr->e_ident[4] != ELFCLASS64) {
        kprintf("[ [RELF [W] Not a 64-bit ELF\n");
        return -1;
    }

    // Check little endian
    if (ehdr->e_ident[5] != ELFDATA2LSB) {
        kprintf("[ [RELF [W] Not little-endian\n");
        return -1;
    }

    // Check executable or shared object (for PIE)
    if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) {
        kprintf("[ [RELF [W] Not an executable (type: %d)\n", ehdr->e_type);
        return -1;
    }

#ifdef ARM
    // Check architecture
    if (ehdr->e_machine != EM_AARCH64) {
        kprintf("[ [RELF [W] Wrong architecture: %d (expected AArch64)\n", ehdr->e_machine);
        return -1;
    }
#endif

    // Validate program header offset
    if (ehdr->e_phoff + ehdr->e_phnum * ehdr->e_phentsize > size) {
        kprintf("[ [RELF [W] Program headers extend beyond file\n");
        return -1;
    }

    return 0;
}

// Convert ELF flags to VMA flags
static u32 elf_to_vma_flags(u32 p_flags) {
    u32 flags = 0;

    if (p_flags & PF_R) flags |= VMA_READ;
    if (p_flags & PF_W) flags |= VMA_WRITE;
    if (p_flags & PF_X) flags |= VMA_EXEC;

    return flags;
}

// Load ELF into address space
int elf_load(mm_struct_t* mm, const u8* data, size_t size, elf_load_result_t* result) {
    if (!mm || !data || !result) return -1;

    if (elf_validate(data, size) != 0) return -1;

    elf64_ehdr_t* ehdr = (elf64_ehdr_t*)data;
    elf64_phdr_t* phdrs = (elf64_phdr_t*)(data + ehdr->e_phoff);

    memset(result, 0, sizeof(elf_load_result_t));
    result->entry_point = ehdr->e_entry;
    result->phdr_count = ehdr->e_phnum;

    u64 max_addr = 0;
    u64 min_addr = (u64)-1;

    // Calculate address range and validate
    for (u16 i = 0; i < ehdr->e_phnum; i++) {
        elf64_phdr_t* phdr = &phdrs[i];

        if (phdr->p_type != PT_LOAD) continue;

        // Validate segment is within file
        if (phdr->p_offset + phdr->p_filesz > size) {
            kprintf("[ [RELF [W] Segment %d extends beyond file\n", i);
            return -1;
        }

        // Validate addresses are in user space
        if (phdr->p_vaddr < USER_SPACE_START || 
            phdr->p_vaddr + phdr->p_memsz > USER_SPACE_END) {
            kprintf("[ [RELF [W] Segment %d address out of user space: 0x%llx\n", 
                    i, phdr->p_vaddr);
            return -1;
        }

        if (phdr->p_vaddr < min_addr) min_addr = phdr->p_vaddr;
        if (phdr->p_vaddr + phdr->p_memsz > max_addr) 
            max_addr = phdr->p_vaddr + phdr->p_memsz;
    }

    result->base_addr = min_addr;

    // Load segments
    for (u16 i = 0; i < ehdr->e_phnum; i++) {
        elf64_phdr_t* phdr = &phdrs[i];

        if (phdr->p_type != PT_LOAD) continue;
        if (phdr->p_memsz == 0) continue;

        u64 vaddr_start = phdr->p_vaddr & ~(PAGE_SIZE - 1);
        u64 vaddr_end = (phdr->p_vaddr + phdr->p_memsz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

        u32 vma_flags = elf_to_vma_flags(phdr->p_flags);

        // Create VMA for this segment
        vma_t* vma = vma_create(vaddr_start, vaddr_end, vma_flags, VMA_FILE);
        if (!vma) {
            kprintf("[ [RELF [W] Failed to create VMA for segment %d\n", i);
            return -1;
        }

        if (vma_insert(mm, vma) < 0) {
            kfree(vma);
            kprintf("[ [RELF [W] Failed to insert VMA for segment %d\n", i);
            return -1;
        }

        // Allocate and map pages
        for (u64 addr = vaddr_start; addr < vaddr_end; addr += PAGE_SIZE) {
            u64 phys = pmm_alloc_frame();
            if (!phys) {
                kprintf("[ [RELF [W] Out of memory loading segment %d\n", i);
                return -1;
            }

            memset(P2V(phys), 0, PAGE_SIZE);

            u64* pte = vmm_get_pte_from_table_alloc((u64*)P2V((uintptr_t)mm->page_table), addr);
            if (!pte) {
                pmm_free_frame(phys);
                kprintf("[ [RELF [W] Failed to get PTE for 0x%llx\n", addr);
                return -1;
            }

            u64 entry = phys | PT_VALID | PT_AF | PT_PAGE | PT_SH_INNER;
            entry |= (MT_NORMAL << 2);  // Normal memory
            entry |= PT_AP_RW_EL0;      // User accessible

            if (!(vma_flags & VMA_EXEC))
                entry |= PT_UXN;  // User execute never

            *pte = entry;
        }

        // Copy file data to segment
        if (phdr->p_filesz > 0) {
            u64 bytes_copied = 0;
            u64 file_offset = phdr->p_offset;

            while (bytes_copied < phdr->p_filesz) {
                u64 current_vaddr = phdr->p_vaddr + bytes_copied;
                u64 page_vaddr = current_vaddr & ~(PAGE_SIZE - 1);
                u64 page_offset = current_vaddr & (PAGE_SIZE - 1);
                u64 bytes_this_page = PAGE_SIZE - page_offset;

                if (bytes_copied + bytes_this_page > phdr->p_filesz)
                    bytes_this_page = phdr->p_filesz - bytes_copied;

                // Get physical address of this page
                u64* pte = vmm_get_pte_from_table((u64*)P2V((uintptr_t)mm->page_table), page_vaddr);
                if (!pte || !(*pte & PT_VALID)) {
                    kprintf("[ [RELF [W] Page not mapped at 0x%llx\n", page_vaddr);
                    return -1;
                }

                u64 phys = *pte & 0x0000FFFFFFFFF000ULL;
                u8* dest = (u8*)P2V(phys) + page_offset;

                memcpy(dest, data + file_offset + bytes_copied, bytes_this_page);
                bytes_copied += bytes_this_page;
            }
        }

        // Set proper protection
        if (!(vma_flags & VMA_WRITE)) {
            for (u64 addr = vaddr_start; addr < vaddr_end; addr += PAGE_SIZE) {
                u64* pte = vmm_get_pte_from_table((u64*)P2V((uintptr_t)mm->page_table), addr);

                if (pte && (*pte & PT_VALID)) {
                    *pte &= ~(3ULL << 6);  // Clear AP bits
                    *pte |= PT_AP_RO_EL0;   // Set read-only for user
                }
            }
        }
    }

    // Set up heap after the loaded segments
    result->brk = (max_addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    mm->heap_start = result->brk;
    mm->heap_end = result->brk;

    // TODO: copy phdrs to user space
    result->phdr_addr = min_addr;
    return 0;
}

int elf_load_from_file(mm_struct_t* mm, inode_t* file, elf_load_result_t* result) {
    if (!mm || !file || !result) return -1;

    u64 file_size = file->size;
    if (file_size == 0 || file_size > 64 * 1024 * 1024) {  // Max 64MB
        kprintf("[ [RELF [W] Invalid file size: %llu\n", file_size);
        return -1;
    }

    u8* buffer = (u8*)kmalloc(file_size);
    if (!buffer) {
        kprintf("[ [RELF [W] Failed to allocate buffer for ELF file\n");
        return -1;
    }

    u64 bytes_read = vfs_read(file, 0, file_size, buffer);
    if (bytes_read != file_size) {
        kprintf("[ [RELF [W] Failed to read complete file: %llu/%llu\n", 
                bytes_read, file_size);
        
        kfree(buffer);
        return -1;
    }

    int ret = elf_load(mm, buffer, file_size, result);

    kfree(buffer);
    return ret;
}