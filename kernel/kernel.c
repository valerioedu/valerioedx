#include <kernel.h>
#include <lib.h>
#include <kio.h>
#include <sched.h>
#include <ramfb.h>
#include <vfs.h>
#include <heap.h>
#include <gic.h>
#include <irq.h>
#include <fat32.h>

void mount() {
    // 2. Open Raw Disk
    inode_t* disk = devfs_fetch_device("vda");
    if (!disk) {
        kprintf("PANIC: Disk not found!\n");
        while(1);
    }

    // 3. Mount FAT32
    inode_t* root_fs = fat32_init(disk);
    vfs_mount_root(root_fs);

    // 4. Test File Read
    kprintf("Looking for TEST.TXT...\n");
    inode_t* file = vfs_lookup("TEST.TXT"); // Case sensitive 8.3 usually uppercase
    
    if (file) {
        char buf[128];
        u64 bytes = vfs_read(file, 0, 127, (u8*)buf);
        buf[bytes] = '\0';
        kprintf("File Contents: %s\n", buf);
    } else {
        kprintf("File not found.\n");
    }
}

void test_process() {
    kprintf("\n");
    heap_debug();
}

void kmain() {
    vfs_init();
    devfs_init();

    task_create(mount, HIGH);
    
    task_create(test_process, LOW);
    while (true) asm volatile("wfi");
}