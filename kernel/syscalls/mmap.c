#include <vma.h>
#include <sched.h>
#include <file.h>
#include <vmm.h>
#include <pmm.h>
#include <string.h>
#include <kio.h>
#include <fat32.h>
#include <heap.h>

// mmap protection flags
#define PROT_NONE   0x0
#define PROT_READ   0x1
#define PROT_WRITE  0x2
#define PROT_EXEC   0x4

// mmap flags
#define MAP_SHARED      0x01
#define MAP_PRIVATE     0x02
#define MAP_FIXED       0x10
#define MAP_ANONYMOUS   0x20
#define MAP_ANON        MAP_ANONYMOUS

// Error codes
#define MAP_FAILED      ((void*)-1)
#define EINVAL          22
#define ENOMEM          12
#define EBADF           9
#define EACCES          13
#define ENODEV          19

extern task_t *current_task;

i64 sys_mmap(void *addr, size_t length, int prot, int flags, int fd, i64 offset) {
    if (!current_task || !current_task->proc || !current_task->proc->mm)
        return -EINVAL;
    
    mm_struct_t *mm = current_task->proc->mm;
    
    if (length == 0)
        return -EINVAL;
    
    // Align length to page boundary
    size_t aligned_length = (length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    
    // Validate offset alignment for file mappings
    if (!(flags & MAP_ANONYMOUS) && (offset & (PAGE_SIZE - 1)))
        return -EINVAL;
    
    // Must have either MAP_SHARED or MAP_PRIVATE, but not both
    if ((flags & MAP_SHARED) && (flags & MAP_PRIVATE))
        return -EINVAL;

    if (!(flags & MAP_SHARED) && !(flags & MAP_PRIVATE))
        return -EINVAL;
    
    // Convert protection flags to VMA flags
    u32 vma_flags = 0;
    if (prot & PROT_READ)
        vma_flags |= VMA_READ;
    
    if (prot & PROT_WRITE)
        vma_flags |= VMA_WRITE;
    
    if (prot & PROT_EXEC)
        vma_flags |= VMA_EXEC;
    
    if (flags & MAP_SHARED)
        vma_flags |= VMA_SHARED;
    
    uintptr_t map_addr = (uintptr_t)addr;
    
    if (flags & MAP_FIXED) {
        if (map_addr & (PAGE_SIZE - 1))
            return -EINVAL;
        
        if (map_addr < USER_SPACE_START || map_addr + aligned_length > USER_SPACE_END)
            return -EINVAL;
        
        vma_unmap(mm, map_addr, map_addr + aligned_length);
    } else {
        if (map_addr != 0) 
            map_addr = (map_addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    }
    
    if (flags & MAP_ANONYMOUS) {
        uintptr_t result = vma_allocate(mm, map_addr, aligned_length, vma_flags);
        if (result == 0)
            return -ENOMEM;

        return (i64)result;
    }
    
    if (fd < 0 || fd >= MAX_FD)
        return -EBADF;
    
    file_t *file = current_task->proc->fd_table[fd];
    if (!file || !file->inode)
        return -EBADF;
    
    // Check file permissions match requested protection
    if ((prot & PROT_READ) && !(file->flags & O_RDONLY) && !(file->flags & O_RDWR))
        return -EACCES;

    if ((prot & PROT_WRITE) && (flags & MAP_SHARED)) {
        if (!(file->flags & O_WRONLY) && !(file->flags & O_RDWR))
            return -EACCES;
    }
    
    uintptr_t result;
    if (flags & MAP_FIXED) {
        vma_t *new_vma = vma_create(map_addr, map_addr + aligned_length, vma_flags, VMA_FILE);
        if (!new_vma)
            return -ENOMEM;

        new_vma->vm_file = file->inode;
        new_vma->vm_pgoff = offset;
        
        if (vma_insert(mm, new_vma) < 0) {
            kfree(new_vma);
            return -ENOMEM;
        }

        result = map_addr;
    } else {
        result = vma_allocate_file(mm, map_addr, aligned_length, vma_flags, file->inode, offset);
        if (result == 0)
            return -ENOMEM;
    }
    
    return (i64)result;
}

i64 sys_munmap(void *addr, size_t length) {
    if (!current_task || !current_task->proc || !current_task->proc->mm)
        return -EINVAL;
    
    mm_struct_t *mm = current_task->proc->mm;
    uintptr_t start = (uintptr_t)addr;
    
    if (start & (PAGE_SIZE - 1))
        return -EINVAL;

    if (length == 0)
        return -EINVAL;
    
    // Align length up to page boundary
    size_t aligned_length = (length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    uintptr_t end = start + aligned_length;
    
    if (start < USER_SPACE_START || end > USER_SPACE_END || end < start)
        return -EINVAL;
    
    int result = vma_unmap(mm, start, end);
    
#ifdef ARM
    // Flush TLB for the unmapped range
    for (uintptr_t a = start; a < end; a += PAGE_SIZE)
        asm volatile("tlbi vaale1is, %0" :: "r"(a >> 12));

    asm volatile("dsb ish");
    asm volatile("isb");
#endif
    
    return result;
}