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
    while (true) asm volatile("wfi");
}