#include <syscalls.h>
#include <elf.h>
#include <vfs.h>
#include <vma.h>
#include <sched.h>
#include <heap.h>
#include <string.h>
#include <kio.h>
#include <pmm.h>
#include <vmm.h>

extern task_t *current_task;

// Set up user stack with arguments and auxiliary vector
static u64 setup_user_stack(mm_struct_t* mm, const char* argv[], const char* envp[], 
                            elf_load_result_t* elf_result) {
    // Stack top
    u64 sp = USER_STACK_TOP;
    
    // Count arguments and environment
    int argc = 0;
    int envc = 0;
    
    if (argv) {
        while (argv[argc]) argc++;
    }
    if (envp) {
        while (envp[envc]) envc++;
    }

    // Get the stack VMA and ensure pages are mapped
    vma_t* stack_vma = vma_find(mm, USER_STACK_TOP - PAGE_SIZE);
    if (!stack_vma) {
        kprintf("[ [REXEC [W] Stack VMA not found\n");
        return 0;
    }

    // Allocate initial stack pages (4 pages = 16KB)
    for (int i = 0; i < 4; i++) {
        u64 addr = USER_STACK_TOP - (i + 1) * PAGE_SIZE;
        
        u64* pte = vmm_get_pte_from_table_alloc((u64*)P2V((uintptr_t)mm->page_table), addr);
        if (!pte) {
            kprintf("[ [REXEC [W] Failed to get PTE for stack\n");
            return 0;
        }

        if (!(*pte & PT_VALID)) {
            u64 phys = pmm_alloc_frame();
            if (!phys) {
                kprintf("[ [REXEC [W] Failed to allocate stack page\n");
                return 0;
            }
            memset(P2V(phys), 0, PAGE_SIZE);

            u64 entry = phys | PT_VALID | PT_AF | PT_PAGE | PT_SH_INNER;
            entry |= (MT_NORMAL << 2);
            entry |= PT_AP_RW_EL0;  // User read/write
            entry |= PT_UXN;        // No execute on stack

            *pte = entry;
        }
    }

    // Helper to write to user stack (using physical addresses since we haven't switched yet)
    #define STACK_PUSH(val) do { \
        sp -= sizeof(u64); \
        u64* pte = vmm_get_pte_from_table((u64*)P2V((uintptr_t)mm->page_table), sp & ~(PAGE_SIZE-1)); \
        if (pte && (*pte & PT_VALID)) { \
            u64 phys = *pte & 0x0000FFFFFFFFF000ULL; \
            u64 offset = sp & (PAGE_SIZE - 1); \
            *(u64*)((u8*)P2V(phys) + offset) = (val); \
        } \
    } while(0)

    // Copy strings to stack and save pointers
    u64 string_area_start = sp - 4096;  // Reserve 4KB for strings
    u64 string_ptr = string_area_start;
    u64 argv_ptrs[32] = {0};
    u64 envp_ptrs[32] = {0};

    // Helper to copy string to user stack
    #define COPY_STRING(str) ({ \
        u64 len = strlen(str) + 1; \
        u64 dest = string_ptr; \
        string_ptr += len; \
        for (u64 i = 0; i < len; i++) { \
            u64 addr = dest + i; \
            u64* pte = vmm_get_pte_from_table((u64*)P2V((uintptr_t)mm->page_table), addr & ~(PAGE_SIZE-1)); \
            if (pte && (*pte & PT_VALID)) { \
                u64 phys = *pte & 0x0000FFFFFFFFF000ULL; \
                u64 offset = addr & (PAGE_SIZE - 1); \
                *((u8*)P2V(phys) + offset) = str[i]; \
            } \
        } \
        dest; \
    })

    // Copy argv strings
    for (int i = 0; i < argc && i < 32; i++) {
        argv_ptrs[i] = COPY_STRING(argv[i]);
    }

    // Copy envp strings
    for (int i = 0; i < envc && i < 32; i++) {
        envp_ptrs[i] = COPY_STRING(envp[i]);
    }

    // Align string pointer
    string_ptr = (string_ptr + 15) & ~15;

    // Now build the stack (grows down)
    // Stack layout (from high to low):
    //   strings (argv[n], envp[n]...)
    //   padding for alignment
    //   auxv[n] = {AT_NULL, 0}
    //   auxv[...]
    //   envp[envc] = NULL
    //   envp[...]
    //   argv[argc] = NULL
    //   argv[...]
    //   argc

    sp = string_area_start;
    sp = sp & ~15;  // Align to 16 bytes

    // Auxiliary vector (end with AT_NULL)
    STACK_PUSH(0);              // AT_NULL value
    STACK_PUSH(AT_NULL);        // AT_NULL type

    STACK_PUSH(PAGE_SIZE);      // AT_PAGESZ value
    STACK_PUSH(AT_PAGESZ);      // AT_PAGESZ type

    STACK_PUSH(elf_result->entry_point);  // AT_ENTRY value
    STACK_PUSH(AT_ENTRY);       // AT_ENTRY type

    STACK_PUSH(elf_result->phdr_count);   // AT_PHNUM value
    STACK_PUSH(AT_PHNUM);       // AT_PHNUM type

    STACK_PUSH(sizeof(elf64_phdr_t));     // AT_PHENT value
    STACK_PUSH(AT_PHENT);       // AT_PHENT type

    // NULL terminator for envp
    STACK_PUSH(0);

    // envp pointers (in reverse order)
    for (int i = envc - 1; i >= 0; i--) {
        STACK_PUSH(envp_ptrs[i]);
    }

    // NULL terminator for argv
    STACK_PUSH(0);

    // argv pointers (in reverse order)
    for (int i = argc - 1; i >= 0; i--) {
        STACK_PUSH(argv_ptrs[i]);
    }

    // argc
    STACK_PUSH(argc);

    return sp;
}

// Execute a new program (replaces current process)
i64 sys_execve(const char* path, const char* argv[], const char* envp[]) {
    if (!path) return -1;

    kprintf("[ [CEXEC [W] Executing: %s\n", path);

    // Look up the file
    inode_t* file = namei(path);
    if (!file) {
        kprintf("[ [REXEC [W] File not found: %s\n", path);
        return -1;
    }

    process_t* proc = current_task->proc;
    if (!proc) {
        vfs_close(file);
        kprintf("[ [REXEC [W] No process context\n");
        return -1;
    }

    // Create new address space
    mm_struct_t* new_mm = mm_create();
    if (!new_mm) {
        vfs_close(file);
        kprintf("[ [REXEC [W] Failed to create address space\n");
        return -1;
    }

    // Load ELF
    elf_load_result_t elf_result;
    if (elf_load_from_file(new_mm, file, &elf_result) != 0) {
        mm_destroy(new_mm);
        vfs_close(file);
        kprintf("[ [REXEC [W] Failed to load ELF\n");
        return -1;
    }

    vfs_close(file);

    // Set up user stack
    const char* default_argv[] = { path, NULL };
    const char* default_envp[] = { "PATH=/bin", NULL };

    u64 user_sp = setup_user_stack(new_mm, 
                                    argv ? argv : default_argv,
                                    envp ? envp : default_envp,
                                    &elf_result);
    if (user_sp == 0) {
        mm_destroy(new_mm);
        kprintf("[ [REXEC [W] Failed to setup user stack\n");
        return -1;
    }

    // Switch to new address space
    mm_struct_t* old_mm = proc->mm;
    proc->mm = new_mm;

#ifdef ARM
    // Switch page table
    asm volatile("msr ttbr0_el1, %0" :: "r"((u64)new_mm->page_table));
    asm volatile("tlbi vmalle1is");
    asm volatile("dsb ish");
    asm volatile("isb");
#endif

    // Destroy old address space
    if (old_mm) {
        mm_destroy(old_mm);
    }

    // Update process name
    const char* name = strrchr(path, '/');
    if (name) name++;
    else name = path;
    strncpy(proc->name, name, 63);

    kprintf("[ [CEXEC [W] Jumping to user mode: entry=0x%llx sp=0x%llx\n",
            elf_result.entry_point, user_sp);

    // Jump to user mode using eret
    // This is for syscall context where we can modify the trapframe
#ifdef ARM
    asm volatile(
        "msr sp_el0, %0\n"          // Set user stack pointer
        "msr elr_el1, %1\n"         // Set return address (entry point)
        "mov x0, #0\n"              // Clear x0 (return value)
        "msr spsr_el1, xzr\n"       // SPSR = 0 (EL0, interrupts enabled)
        "eret\n"                     // Return to EL0
        :
        : "r"(user_sp), "r"(elf_result.entry_point)
        : "x0", "memory"
    );
#endif

    // Never reached
    return 0;
}

// Assembly helper to enter user mode (defined in switch.S)
extern void enter_usermode(u64 entry, u64 sp);

// Kernel function to start init process - called from kernel thread
int exec_init(const char* path) {
    kprintf("[ [CINIT [W] Loading init: %s\n", path);

    // Look up the file
    inode_t* file = namei(path);
    if (!file) {
        kprintf("[ [RINIT [W] Init not found: %s\n", path);
        return -1;
    }

    // Get current process
    process_t* proc = current_task->proc;
    if (!proc || !proc->mm) {
        vfs_close(file);
        kprintf("[ [RINIT [W] No process or mm context\n");
        return -1;
    }

    // Load ELF
    elf_load_result_t elf_result;
    if (elf_load_from_file(proc->mm, file, &elf_result) != 0) {
        vfs_close(file);
        kprintf("[ [RINIT [W] Failed to load init ELF\n");
        return -1;
    }

    vfs_close(file);

    // Set up user stack
    const char* argv[] = { path, NULL };
    const char* envp[] = { "PATH=/bin", "HOME=/", NULL };

    u64 user_sp = setup_user_stack(proc->mm, argv, envp, &elf_result);
    if (user_sp == 0) {
        kprintf("[ [RINIT [W] Failed to setup user stack\n");
        return -1;
    }

    kprintf("[ [CINIT [W] Switching to user page table: 0x%llx\n", (u64)proc->mm->page_table);

#ifdef ARM
    // Switch to user page table
    asm volatile(
        "msr ttbr0_el1, %0\n"
        "isb\n"
        "tlbi vmalle1is\n"
        "dsb ish\n"
        "isb\n"
        :: "r"((u64)proc->mm->page_table)
        : "memory"
    );
#endif

    kprintf("[ [CINIT [W] Entering user mode: entry=0x%llx sp=0x%llx\n",
            elf_result.entry_point, user_sp);

#ifdef ARM
    // Enter user mode using assembly function
    enter_usermode(elf_result.entry_point, user_sp);
#endif

    // Never reached
    return 0;
}