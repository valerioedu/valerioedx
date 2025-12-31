#include <syscalls.h>
#include <file.h>
#include <sched.h>
#include <vmm.h>
#include <vma.h>
#include <heap.h>
#include <kio.h>

extern task_t *current_task;

int copy_from_user(void *kernel_dst, const void *user_src, size_t size) {
    
    if (!current_task || !current_task->proc || !current_task->proc->mm)
        return -1;
    
    mm_struct_t *mm = current_task->proc->mm;
    
    u8 *dst = (u8 *)kernel_dst;
    u64 src_addr = (u64)user_src;
    
    // Just try the first byte
    u64 page_addr = src_addr & ~(PAGE_SIZE - 1);
    u64 offset = src_addr & (PAGE_SIZE - 1);
    
    
    u64 *pte = vmm_get_pte_from_table((u64 *)P2V((uintptr_t)mm->page_table), page_addr);
    
    if (!pte) return -1;    
    
    if (!(*pte & PT_VALID)) return -1;
    
    u64 phys = (*pte & 0x0000FFFFFFFFF000ULL) + offset;
    
    u8 first_byte = *(u8 *)P2V(phys);
    
    for (size_t i = 0; i < size; i++) {
        u64 addr = src_addr + i;
        page_addr = addr & ~(PAGE_SIZE - 1);
        offset = addr & (PAGE_SIZE - 1);
        
        pte = vmm_get_pte_from_table((u64 *)P2V((uintptr_t)mm->page_table), page_addr);
        if (!pte || !(*pte & PT_VALID))
            return -1;
        
        phys = (*pte & 0x0000FFFFFFFFF000ULL) + offset;
        dst[i] = *(u8 *)P2V(phys);
    }
    
    return 0;
}

i64 sys_write(u32 fd, const char *buf, size_t count) {
    file_t *f = fd_get(fd);
    if (!f) return -1;
    
    char *kbuf = kmalloc(count);
    if (!kbuf) return -1;
    
    if (copy_from_user(kbuf, buf, count) != 0) {
        kfree(kbuf);
        return -1;
    }
    
    u64 bytes_written = vfs_write(f->inode, f->offset, (u64)count, (u8 *)kbuf);
    f->offset += bytes_written;
    
    kfree(kbuf);
    return bytes_written;
}

int copy_to_user(void *user_dst, const void *kernel_src, size_t size) {
    if (!current_task || !current_task->proc || !current_task->proc->mm)
        return -1;
    
    mm_struct_t *mm = current_task->proc->mm;
    
    const u8 *src = (const u8 *)kernel_src;
    u64 dst_addr = (u64)user_dst;
    
    for (size_t i = 0; i < size; i++) {
        u64 addr = dst_addr + i;
        u64 page_addr = addr & ~(PAGE_SIZE - 1);
        u64 offset = addr & (PAGE_SIZE - 1);
        
        u64 *pte = vmm_get_pte_from_table((u64 *)P2V((uintptr_t)mm->page_table), page_addr);
        if (!pte || !(*pte & PT_VALID))
            return -1;
        
        u64 phys = (*pte & 0x0000FFFFFFFFF000ULL) + offset;
        *(u8 *)P2V(phys) = src[i];
    }
    
    return 0;
}


i64 sys_read(u32 fd, char *buf, size_t count) {
    file_t *f = fd_get(fd);
    if (!f) return -1;

    char *kbuf = kmalloc(count);
    if (!kbuf) return -1;

    u64 bytes_read = vfs_read(f->inode, f->offset, count, (u8*)kbuf);
    f->offset += bytes_read;

    if (bytes_read > 0) {
        if (copy_to_user(buf, kbuf, bytes_read) != 0) {
            kfree(kbuf);
            return -1;
        }
    }

    kfree(kbuf);
    return bytes_read;
}