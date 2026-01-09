#include <kernel.h>
#include <lib.h>
#include <kio.h>
#include <sched.h>
#include <vfs.h>
#include <heap.h>
#include <fat32.h>
#include <tty.h>
#include <vma.h>
#include <syscalls.h>

extern task_t *current_task;
process_t *init_process = NULL;

void init_entry() {
    if (exec_init("BIN/INIT.ELF") == 0)
        kprintf("[ [RINIT [W] exec_init returned unexpectedly\n");
    
    while (true)
#ifdef ARM
        asm volatile("wfi");
#else
        asm volatile("hlt");
#endif
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
        } else {
            kprintf("[ [RKMAIN [W] Failed to mount FAT32\n");
        }
    }
    extern void ramfb_init();
    ramfb_init();
    init_process = process_create("init", init_entry, HIGH);

    if (!init_process) {
        kprintf("[ [RKMAIN [W] Failed to create init process!\n");
    }

    while (true) asm volatile("wfi");
}