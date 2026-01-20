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
#include <file.h>

extern task_t *current_task;

static inline u64 get_phys_from_pte(u64 *pte) {
    return (*pte) & 0x0000FFFFFFFFF000ULL;
}

static int ensure_user_page(mm_struct_t* mm, u64 va) {
    u64 page_base = va & ~(PAGE_SIZE - 1);
    u64 *pte = vmm_get_pte_from_table_alloc((u64*)P2V((uintptr_t)mm->page_table), page_base);

    if (!pte) return -1;
    if (!(*pte & PT_VALID)) {
        u64 phys = pmm_alloc_frame();
        if (!phys) return -1;
        memset(P2V(phys), 0, PAGE_SIZE);
        u64 entry = phys | PT_VALID | PT_AF | PT_PAGE | PT_SH_INNER;
        entry |= (MT_NORMAL << 2);
        entry |= PT_AP_RW_EL0;
        entry |= PT_UXN;
        *pte = entry;
    }

    return 0;
}

static u64 copy_string_to_user(mm_struct_t* mm, const char* src, u64 dest) {
    u64 len = strlen(src) + 1;

    for (u64 i = 0; i < len; i++) {
        u64 addr = dest + i;

        if (ensure_user_page(mm, addr) != 0)
            return 0;

        u64 *pte = vmm_get_pte_from_table((u64*)P2V((uintptr_t)mm->page_table), addr & ~(PAGE_SIZE - 1));
        if (!pte || !(*pte & PT_VALID)) 
            return 0;

        u64 phys = get_phys_from_pte(pte);
        u64 offset = addr & (PAGE_SIZE - 1);
        *((u8*)P2V(phys) + offset) = ((const u8*)src)[i];
    }

    return dest;
}

// Set up user stack with arguments and auxiliary vector
static u64 setup_user_stack(mm_struct_t* mm, const char* argv[], const char* envp[], 
                            elf_load_result_t* elf_result) {
    u64 sp = USER_STACK_TOP;
    
    int argc = 0;
    int envc = 0;
    
    if (argv) 
        while (argv[argc]) argc++;

    if (envp)
        while (envp[envc]) envc++;

    vma_t* stack_vma = vma_find(mm, USER_STACK_TOP - PAGE_SIZE);
    if (!stack_vma) {
        kprintf("[ [REXEC [W] Stack VMA not found\n");
        return 0;
    }

    // 4 pages = 16KB
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

    // Helper to write to user stack using physical addresses
    #define STACK_PUSH(val) do { \
        sp -= sizeof(u64); \
        u64* pte = vmm_get_pte_from_table((u64*)P2V((uintptr_t)mm->page_table), sp & ~(PAGE_SIZE-1)); \
        if (pte && (*pte & PT_VALID)) { \
            u64 phys = *pte & 0x0000FFFFFFFFF000ULL; \
            u64 offset = sp & (PAGE_SIZE - 1); \
            *(u64*)((u8*)P2V(phys) + offset) = (val); \
        } \
    } while(0)

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
        argv_ptrs[i] = copy_string_to_user(mm, argv[i], string_ptr);
        if (!argv_ptrs[i]) {
            kprintf("[ [REXEC [W] Failed to copy argv string\n");
            return 0;
        }

        string_ptr += strlen(argv[i]) + 1;
    }

    // Copy envp strings
    for (int i = 0; i < envc && i < 32; i++) {
        envp_ptrs[i] = copy_string_to_user(mm, envp[i], string_ptr);
        if (!envp_ptrs[i]) {
            kprintf("[ [REXEC [W] Failed to copy envp string\n");
            return 0;
        }

        string_ptr += strlen(envp[i]) + 1;
    }

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
    for (int i = envc - 1; i >= 0; i--)
        STACK_PUSH(envp_ptrs[i]);

    // NULL terminator for argv
    STACK_PUSH(0);

    // argv pointers (in reverse order)
    for (int i = argc - 1; i >= 0; i--)
        STACK_PUSH(argv_ptrs[i]);

    // argc
    STACK_PUSH(argc);

    return sp;
}

// Set up standard file descriptors for a process
int setup_standard_fds(process_t* proc) {
    // stdin (fd 0) - console input
    inode_t* stdin_node = devfs_fetch_device("stdin");
    if (stdin_node) {
        file_t* stdin_file = file_new(stdin_node, 0);  // Read-only
        if (stdin_file)
            proc->fd_table[0] = stdin_file;
    }

    // stdout (fd 1) - console output
    inode_t* stdout_node = devfs_fetch_device("stdout");
    if (stdout_node) {
        file_t* stdout_file = file_new(stdout_node, 1);  // Write-only
        if (stdout_file)
            proc->fd_table[1] = stdout_file;
    }

    // stderr (fd 2) - console output (same as stdout for now)
    inode_t* stderr_node = devfs_fetch_device("stderr");
    if (stderr_node) {
        file_t* stderr_file = file_new(stderr_node, 1);  // Write-only
        if (stderr_file)
            proc->fd_table[2] = stderr_file;
    }

    if (!proc->fd_table[0] || !proc->fd_table[1] || !proc->fd_table[2]) {
        kprintf("[ [REXEC [W] Warning: Failed to set up some standard fds\n");
        return -1;
    }

    return 0;
}

i64 sys_execve(const char* path, const char* argv[], const char* envp[]) {
    if (!path) return -1;

    const char **kargv = kmalloc(2048);
    if (!kargv) return -1;

    if (copy_from_user(kargv, argv, 2048) != 0) {
        kfree(kargv);
        return -1;
    }

    const char **kenvp = kmalloc(2048);
    if (!kenvp) return -1;

    if (copy_from_user(kenvp, envp, 2048) != 0) {
        kfree(kenvp);
        return -1;
    }

    char *kpath = kmalloc(256);
    if (!kpath) return -1;
    
    if (copy_from_user(kpath, path, 256) != 0) {
        kfree(kpath);
        return -1;
    }

    inode_t* file = namei(kpath);
    if (!file)
        return -1;

    process_t* proc = current_task->proc;
    if (!proc) {
        vfs_close(file);
        kprintf("[ [REXEC [W] No process context\n");
        return -1;
    }

    mm_struct_t* new_mm = mm_create();
    if (!new_mm) {
        vfs_close(file);
        kprintf("[ [REXEC [W] Failed to create address space\n");
        return -1;
    }

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
                                    kargv ? kargv : default_argv,
                                    kenvp ? kenvp : default_envp,
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

    char *kargv_strings[32] = {0};
    int argc = 0;
    if (kargv) {
        for (int i = 0; i < 31 && kargv[i]; i++) {
            kargv_strings[i] = kmalloc(256);
            copy_from_user(kargv_strings[i], kargv[i], 256);
            argc++;
        }
    }

    if (old_mm) mm_destroy(old_mm);

    // Update process name
    const char* name = strrchr(kpath, '/');
    if (name) name++;
    else name = kpath;

    strncpy(proc->name, name, 63);

    // Jump to user mode using eret
    extern void enter_usermode(u64 entry, u64 sp);
    enter_usermode(elf_result.entry_point, user_sp);

    // Never reached
    return 0;
}

extern void enter_usermode(u64 entry, u64 sp);

// Kernel function to start init process
int exec_init(const char* path) {
    inode_t* file = namei(path);
    if (!file) {
        kprintf("[ [RINIT [W] Init not found: %s\n", path);
        return -1;
    }

    process_t* proc = current_task->proc;
    if (!proc || !proc->mm) {
        vfs_close(file);
        kprintf("[ [RINIT [W] No process or mm context\n");
        return -1;
    }

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

    enter_usermode(elf_result.entry_point, user_sp);

    // Never reached
    return 0;
}