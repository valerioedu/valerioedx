#include <vfs.h>
#include <string.h>
#include <kio.h>
#include <tty.h>

#ifdef ARM
#include <uart.h>
#include <virtio.h>
#endif

static int dev_count = 0;
inode_t device_list[10];

void devfs_mount_device(char* name, inode_ops* ops) {
    if (dev_count >= 10) return;

    inode_t* node = &device_list[dev_count++];
    strcpy(node->name, name);
    node->flags = FS_CHARDEVICE;
    node->ops = ops;
    
    kprintf("[ [CDevFS [W] Mounted /dev/%s\n", name);
}

inode_t *devfs_fetch_device(const char* name) {
    for (int i = 0; i < dev_count; i++) {
        if (strcmp(device_list[i].name, name) == 0) {
            return &device_list[i];
        }
    }
    return NULL;
}

void devfs_init() {
#ifdef ARM
    devfs_mount_device("virtio-blk", &virtio_blk_ops);
    devfs_mount_device("uart", &uart_ops);
    devfs_mount_device("virtio-kb", &virtio_kb_ops);
    devfs_mount_device("tty0", &tty_console_ops);
    devfs_mount_device("ttS0", &tty_serial_ops);
    devfs_mount_device("stdin", &stdin_ops);
    devfs_mount_device("stdout", &stdout_ops);
    devfs_mount_device("stderr", &stderr_ops);
#endif
}