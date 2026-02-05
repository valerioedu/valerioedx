#include <fb.h>
#include <ramfb.h>
#include <vmm.h>
#include <pmm.h>
#include <vma.h>
#include <sched.h>
#include <string.h>
#include <kio.h>
#include <heap.h>

extern u32 fb_buffer[WIDTH * HEIGHT];

static fb_fix_screeninfo_t fb_fix_info;
static fb_var_screeninfo_t fb_var_info;
static u64 fb_phys_addr = 0;

static u64 fb_read(inode_t *node, u64 offset, u64 size, u8 *buffer) {
    u64 fb_size = WIDTH * HEIGHT * 4;
    
    if (offset >= fb_size)
        return 0;
    
    if (offset + size > fb_size)
        size = fb_size - offset;
    
    memcpy(buffer, (u8*)fb_buffer + offset, size);
    return size;
}

static u64 fb_write(inode_t *node, u64 offset, u64 size, u8 *buffer) {
    u64 fb_size = WIDTH * HEIGHT * 4;
    
    if (offset >= fb_size)
        return 0;
    
    if (offset + size > fb_size)
        size = fb_size - offset;
    
    memcpy((u8*)fb_buffer + offset, buffer, size);
    return size;
}

static int fb_ioctl(inode_t *node, u64 request, u64 arg) {
    switch (request) {
        case FBIOGET_FSCREENINFO:
            if (!arg) return -1;
            memcpy((void*)arg, &fb_fix_info, sizeof(fb_fix_screeninfo_t));
            return 0;
        
        case FBIOGET_VSCREENINFO:
            if (!arg) return -1;
            memcpy((void*)arg, &fb_var_info, sizeof(fb_var_screeninfo_t));
            return 0;
        
        case FBIOPUT_VSCREENINFO:
            return 0;
        
        case FBIOPAN_DISPLAY:
            return 0;
        
        default:
            return -1;
    }
}

static i64 fb_mmap(inode_t *node, u64 vaddr, u64 length, int prot, int flags, u64 offset) {
    extern task_t *current_task;
    
    if (!current_task || !current_task->proc || !current_task->proc->mm) {
        kprintf("[FB] mmap: no mm\n");
        return -1;
    }
    
    mm_struct_t *mm = current_task->proc->mm;
    u64 fb_size = WIDTH * HEIGHT * 4;
    
    if (offset >= fb_size) {
        kprintf("[FB] mmap: offset too large\n");
        return -1;
    }
    
    if (length > fb_size - offset)
        length = fb_size - offset;
    
    length = (length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    
    if (vaddr == 0) {
        vaddr = mm->mmap_base;
        vma_t *vma = mm->vma_list;
        while (vma) {
            if (vaddr + length <= vma->vm_start)
                break;
            
            if (vaddr < vma->vm_end)
                vaddr = vma->vm_end;
            
            vma = vma->vm_next;
        }
    }
    
    vaddr = vaddr & ~(PAGE_SIZE - 1);
    
    // Create VMA
    u32 vma_flags = VMA_READ;
    if (prot & 0x2)
        vma_flags |= VMA_WRITE;
    
    if (flags & 0x01)
        vma_flags |= VMA_SHARED;
    
    vma_t *new_vma = vma_create(vaddr, vaddr + length, vma_flags, VMA_DEVICE);
    if (!new_vma) {
        kprintf("[FB] mmap: vma_create failed\n");
        return -1;
    }
    
    if (vma_insert(mm, new_vma) < 0) {
        kprintf("[FB] mmap: vma_insert failed\n");
        kfree(new_vma);
        return -1;
    }
    
    u64 *user_pt = (u64*)P2V((uintptr_t)mm->page_table);
    
    for (u64 off = 0; off < length; off += PAGE_SIZE) {
        u64 phys = fb_phys_addr + offset + off;
        u64 virt = vaddr + off;
        
        u64 *pte = vmm_get_pte_from_table_alloc(user_pt, virt);
        if (!pte) {
            kprintf("[FB] mmap: pte alloc failed at virt=0x%lx\n", virt);
            return -1;
        }
        
        u64 entry = (phys & 0x0000FFFFFFFFF000ULL);
        entry |= PT_VALID | PT_PAGE | PT_AF;
        entry |= PT_SH_INNER;
        entry |= (1ULL << 2);
        entry |= PT_AP_RW_EL0;
        entry |= PT_UXN | PT_PXN;
        
        *pte = entry;   
    }
    
    return (i64)vaddr;
}

inode_ops fb_ops = { 0 };

void fb_device_init(void) {
    if ((u64)fb_buffer >= PHYS_OFFSET)
        fb_phys_addr = V2P((uintptr_t)fb_buffer);

    else fb_phys_addr = (u64)fb_buffer;
    
    
    strncpy(fb_fix_info.id, "ramfb", 15);
    fb_fix_info.smem_start = fb_phys_addr;
    fb_fix_info.smem_len = WIDTH * HEIGHT * 4;
    fb_fix_info.type = FB_TYPE_PACKED_PIXELS;
    fb_fix_info.visual = FB_VISUAL_TRUECOLOR;
    fb_fix_info.line_length = WIDTH * 4;
    
    fb_var_info.xres = WIDTH;
    fb_var_info.yres = HEIGHT;
    fb_var_info.xres_virtual = WIDTH;
    fb_var_info.yres_virtual = HEIGHT;
    fb_var_info.bits_per_pixel = 32;
    
    fb_var_info.blue.offset = 0;
    fb_var_info.blue.length = 8;
    fb_var_info.green.offset = 8;
    fb_var_info.green.length = 8;
    fb_var_info.red.offset = 16;
    fb_var_info.red.length = 8;
    fb_var_info.transp.offset = 24;
    fb_var_info.transp.length = 8;

    fb_ops.read = fb_read;
    fb_ops.write = fb_write;
    fb_ops.ioctl = fb_ioctl;
    fb_ops.mmap = fb_mmap;
}