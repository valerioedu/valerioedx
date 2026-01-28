#include <vfs.h>
#include <string.h>
#include <kio.h>
#include <tty.h>
#include <heap.h>

#define DEVFS_ROOT_ID 0xDE7F5

#ifdef ARM
#include <uart.h>
#include <virtio.h>
#endif

typedef struct device_driver {
    char name[32];
    u32 id;
    int type;
    inode_ops *ops;
    struct device_driver *next;
} device_driver_t;

static device_driver_t *device_head = NULL;
static int dev_count = 0;
static unsigned long devfs_next_id = 0x80000000ULL;

static inode_t *create_device_inode(device_driver_t *device) {
    if (!device) goto done;

    inode_t *node = (inode_t*)kmalloc(sizeof(inode_t));
    if (!node) goto done;

    memset(node, 0, sizeof(inode_t));
    strncpy(node->name, device->name, 31);

    node->flags = device->type | FS_TEMPORARY;
    node->ops = device->ops;

    node->id = device->id;
    node->dev = DEVFS_ROOT_ID;
    node->rdev = device->id;
    node->size = 0;

    if (device->type == FS_BLOCKDEVICE) {
        node->mode = S_IFBLK | 0600;
    } else {
        node->mode = S_IFCHR | 0600;
    }

    node->nlink = 1;
    node->ptr = NULL; 
    node->uid = 0;
    node->gid = 0;
    node->size = 0;
    node->blksize = 0;
    node->blocks = 0;

    return node;


done:
    kprintf("[ [RDEVFS [W] Failed to create device node");
    return NULL;
}

inode_t *defvs_lookup(inode_t *node, const char *name) {
    return devfs_fetch_device(name);
}

void devfs_open(inode_t *node) {

}

int devfs_readdir(inode_t* node, int index, char* namebuf, int buflen, int* is_dir) {
    if (index < 0) return 0;

    if (index == 0) {
        strncpy(namebuf, ".", buflen);
        if (is_dir) *is_dir = 1;

        return 1;
    }

    if (index == 1) {
        strncpy(namebuf, "..", buflen);
        if (is_dir) *is_dir = 1;

        return 1;
    }

    int target_idx = index - 2;
    device_driver_t *current = device_head;
    int current_idx = 0;

    while (current) {
        if (current_idx == target_idx) {
            strncpy(namebuf, current->name, buflen);
            if (is_dir) *is_dir = 0;

            return 1;
        }

        current = current->next;
        current_idx++;
    }

    return 0;
}

inode_ops devfs_root_ops = { 0 };

inode_t devfs_root_node = {
    .name = "DEV",
    .flags = FS_DIRECTORY,
    .mode = S_IFDIR | 755,
    .ops = NULL,
    .id = 0,
    .size = 0,
    .mount_point = NULL
};

inode_t *devfs_get_root() {
    return &devfs_root_node;
}

void devfs_mount_device(char* name, inode_ops* ops, int type) {
    device_driver_t *drv = kmalloc(sizeof(device_driver_t));

    if (!drv) {
        kprintf("[DevFS] Failed to allocate driver for %s\n", name);
        return;
    }

    strncpy(drv->name, name, 31);
    drv->ops = ops;
    drv->type = type;
    drv->id = ++dev_count;
    drv->next = device_head;
    device_head = drv;

    kprintf("[ [CDevFS [W] Registered device: %s\n", name);
}

inode_t *devfs_fetch_device(const char* name) {
    device_driver_t *current = device_head;

    while (current) {
        if (strcmp(current->name, name) == 0)
            return create_device_inode(current);

        current = current->next;
    }

    return NULL;
}

void devfs_init() {
    devfs_root_ops.open = devfs_open;
    devfs_root_ops.lookup = defvs_lookup;
    devfs_root_ops.readdir = devfs_readdir;
    devfs_root_node.ops = &devfs_root_ops;
    device_head = NULL;
    dev_count = 0;
#ifdef ARM
    devfs_mount_device("virtio-blk", &virtio_blk_ops, FS_BLOCKDEVICE);
    devfs_mount_device("uart", &uart_ops, FS_CHARDEVICE);
    devfs_mount_device("virtio-kb", &virtio_kb_ops, FS_CHARDEVICE);
    devfs_mount_device("tty0", &tty_console_ops, FS_CHARDEVICE);
    devfs_mount_device("ttS0", &tty_serial_ops, FS_CHARDEVICE);
    devfs_mount_device("stdin", &stdin_ops, FS_CHARDEVICE);
    devfs_mount_device("stdout", &stdout_ops, FS_CHARDEVICE);
    devfs_mount_device("stderr", &stderr_ops, FS_CHARDEVICE);
#endif
}