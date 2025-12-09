#include <kernel.h>
#include <lib.h>
#include <kio.h>
#include <sched.h>
#include <vfs.h>
#include <heap.h>
#include <fat32.h>

void test_process() {
    kprintf("\n");
    heap_debug();
}

void test_kb_read() {
    char buffer[32];

    inode_t *virtio_kb = devfs_fetch_device("virtio-kb");
    if (virtio_kb) {
        kprintf("Test the keyboard: ");
        u64 bytes = vfs_read(virtio_kb, 0, 31, (u8*)buffer);
        buffer[bytes] = '\0';
        kprintf("Result: %s\n", buffer);
        task_create(test_process, LOW);
    } else {
        kprintf("virtio-kb device not found\n");
    }
}

void kmain() {
    vfs_init();
    devfs_init();

    inode_t* blk_dev = devfs_fetch_device("virtio-blk");

    if (blk_dev) {
        kprintf("[ [CKMAIN [W] Attempting to mount FAT32...\n");
        
        inode_t* root_fs = fat32_mount(blk_dev);
        
        if (root_fs) {
            vfs_mount_root(root_fs);
            
            inode_t* text_file = vfs_lookup("/TEST.TXT");
            if (text_file) {
                char buf[64];
                u64 bytes = vfs_read(text_file, 0, 63, (u8*)buf);
                buf[bytes] = 0;
                kprintf("[ [CKMAIN [W]Content of TEST.TXT: %s\n", buf);
            } else {
                kprintf("TEST.TXT not found (ensure disk is FAT32 formatted)\n");
            }
        } else {
            kprintf("Failed to mount FAT32.\n");
        }
    }

    task_create(test_kb_read, HIGH);
    while (true) asm volatile("wfi");
}