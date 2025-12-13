#include <vfs.h>
#include <string.h>
#include <kio.h>
#include <tty.h>

#ifdef ARM
#include <uart.h>
#include <virtio.h>
#endif

static int dev_count = 0;
// TODO: Implement a list
inode_t device_list[32];

inode_t *defvs_finddir(inode_t *node, const char *name) {
    return devfs_fetch_device(name);
}

static inode_ops devfs_root_ops = {
    .finddir = defvs_finddir
};

static inode_t devfs_root_node = {
    .name = "devfs",
    .flags = FS_DIRECTORY,
    .ops = &devfs_root_ops,
    .id = 0,
    .size = 0,
    .mount_point = NULL
};

inode_t *devfs_get_root() {
    return &devfs_root_node;
}

void devfs_mount_device(char* name, inode_ops* ops) {
    if (dev_count >= 32) return;

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