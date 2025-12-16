#include <kernel.h>
#include <lib.h>
#include <kio.h>
#include <sched.h>
#include <vfs.h>
#include <heap.h>
#include <fat32.h>
#include <tty.h>
#include <vma.h>

extern task_t *current_task;

// Test user process
void test_user_process() {
    kprintf("[ [CTEST [W] User process started (TID: %d)\n", current_task->id);
    
    mm_struct_t *mm = current_task->proc->mm;
    
    // Test 1: Allocate some memory
    kprintf("[ [CTEST [W] Test 1: Allocating 8KB of memory\n");
    uintptr_t addr = vma_allocate(mm, 0, 8192, VMA_READ | VMA_WRITE);
    if (addr) {
        kprintf("[ [GTEST [W] Allocated at 0x%llx\n", addr);
    } else {
        kprintf("[ [RTEST [W] Allocation failed\n");
    }
    
    // Test 2: Find the VMA
    kprintf("[ [CTEST [W] Test 2: Looking up VMA\n");
    vma_t *vma = vma_find(mm, addr);
    if (vma) {
        kprintf("[ [GTEST [W] Found VMA: 0x%llx - 0x%llx (flags: 0x%x)\n",
                vma->vm_start, vma->vm_end, vma->vm_flags);
    } else {
        kprintf("[ [RTEST [W] VMA not found\n");
    }
    
    // Test 3: Allocate more memory
    kprintf("[ [CTEST [W] Test 3: Allocating another 4KB\n");
    uintptr_t addr2 = vma_allocate(mm, 0, 4096, VMA_READ | VMA_WRITE);
    if (addr2 && addr2 != addr) {
        kprintf("[ [GTEST [W] Allocated at 0x%llx\n", addr2);
    } else {
        kprintf("[ [RTEST [W] Second allocation failed\n");
    }
    
    // Test 4: List all VMAs
    kprintf("[ [CTEST [W] Test 4: Listing all VMAs\n");
    vma_t *current = mm->vma_list;
    int count = 0;
    while (current) {
        kprintf("  VMA %d: 0x%llx - 0x%llx (flags: 0x%x, type: %d)\n",
                ++count, current->vm_start, current->vm_end, 
                current->vm_flags, current->vm_type);
        current = current->vm_next;
    }
    
    // Test 5: Unmap first allocation
    kprintf("[ [CTEST [W] Test 5: Unmapping first allocation\n");
    if (vma_unmap(mm, addr, addr + 8192) == 0) {
        kprintf("[ [GTEST [W] Successfully unmapped\n");
    } else {
        kprintf("[ [RTEST [W] Unmap failed\n");
    }
    
    // Test 6: Verify it's gone
    kprintf("[ [CTEST [W] Test 6: Verifying unmap\n");
    vma = vma_find(mm, addr);
    if (!vma) {
        kprintf("[ [GTEST [W] VMA correctly removed\n");
    } else {
        kprintf("[ [RTEST [W] VMA still exists\n");
    }
    
    // Test 7: Stack info
    kprintf("[ [CTEST [W] Test 7: Stack information\n");
    kprintf("  Stack top: 0x%llx\n", mm->stack_start);
    kprintf("  Heap start: 0x%llx\n", mm->heap_start);
    kprintf("  Heap end: 0x%llx\n", mm->heap_end);
    kprintf("  Mmap base: 0x%llx\n", mm->mmap_base);
    
    kprintf("[ [CTEST [W] All tests completed\n");
    
    while (true) asm volatile("wfi");
}

void kmain() {
    tty_init();
    vfs_init();
    devfs_init();

    inode_t* blk_dev = devfs_fetch_device("virtio-blk");

    if (blk_dev) {
        kprintf("[ [CKMAIN [W] Attempting to mount FAT32...\n");
        
        inode_t* root_fs = fat32_mount(blk_dev);
        
        if (root_fs) {
            vfs_mount_root(root_fs);

            inode_t* dev_dir = vfs_lookup("/dev");

            if (!dev_dir) {
                 if (root_fs->ops && root_fs->ops->mkdir) {
                    kprintf("[ [CKMAIN [W] /dev not detected\n");
                    dev_dir = root_fs->ops->mkdir(root_fs, "dev");
                 }
            }

            if (dev_dir) {
                vfs_mount(dev_dir, devfs_get_root());
                kprintf("[ [CKMAIN [W] Mounted devfs on /dev\n");
            } else {
                kprintf("[ [RKMAIN [W] Failed to find or create /dev directory\n");
            }
            
            inode_t* text_file = vfs_lookup("/TEST.TXT");
            if (text_file) {
                char buf[64];
                u64 bytes = vfs_read(text_file, 0, 63, (u8*)buf);
                buf[bytes] = 0;
                kprintf("[ [CKMAIN [W] Content of TEST.TXT: %s\n", buf);
            } else {
                kprintf("[ [RKMAIN [W] TEST.TXT not found\n");
            }
        } else {
            kprintf("[ [RKMAIN [W] Failed to mount FAT32\n");
        }
    }

    heap_debug();

    // Create a test user process
    kprintf("\n[ [CKMAIN [W] === VMA System Test ===\n");
    process_create("test_process", test_user_process, NORMAL);
    while (true) asm volatile("wfi");
}