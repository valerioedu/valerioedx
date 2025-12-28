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

void ls(const char* path) {
    inode_t* dir = vfs_lookup(path);
    if (!dir) return;

    char name[32];
    int is_dir;
    int index = 0;

    // Loop until readdir returns 0
    while (dir->ops->readdir(dir, index, name, 32, &is_dir)) {
        kprintf("%s%s\n", name, is_dir ? "/" : "");
        index++;
    }
}

void init_entry() {
    // Try to exec /bin/init or /init
    if (exec_init("BIN/INIT.ELF") == 0) {
        // Should not return
        kprintf("[ [RINIT [W] exec_init returned unexpectedly\n");
    }
    
    // Stay alive as init process
    while (true) {
#ifdef ARM
        asm volatile("wfi");
#else
        asm volatile("hlt");
#endif
    }
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

    ls("/");
    ls("/bin");

    init_process = process_create("init", init_entry, HIGH);

    if (!init_process) {
        kprintf("[ [RKMAIN [W] Failed to create init process!\n");
    } else {
        kprintf("[ [CKMAIN [W] Init process created with PID %d\n", init_process->pid);
    }

    while (true) asm volatile("wfi");
}