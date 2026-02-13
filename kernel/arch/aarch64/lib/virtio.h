#ifndef VIRTIO_H
#define VIRTIO_H

#include <lib.h>
#include <vfs.h>
#include <sched.h>

#define VIRTIO_MMIO_MAGIC_VALUE     0x000
#define VIRTIO_MMIO_VERSION         0x004
#define VIRTIO_MMIO_DEVICE_ID       0x008
#define VIRTIO_MMIO_VENDOR_ID       0x00c
#define VIRTIO_MMIO_DEVICE_FEATURES 0x010
#define VIRTIO_MMIO_DRIVER_FEATURES 0x020
#define VIRTIO_MMIO_GUEST_PAGE_SIZE 0x028
#define VIRTIO_MMIO_QUEUE_SEL       0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX   0x034
#define VIRTIO_MMIO_QUEUE_NUM       0x038
#define VIRTIO_MMIO_QUEUE_ALIGN     0x03c
#define VIRTIO_MMIO_QUEUE_PFN       0x040
#define VIRTIO_MMIO_QUEUE_READY     0x044
#define VIRTIO_MMIO_QUEUE_NOTIFY    0x050 
#define VIRTIO_MMIO_INTERRUPT_STATUS 0x060
#define VIRTIO_MMIO_INTERRUPT_ACK    0x064
#define VIRTIO_MMIO_STATUS          0x070

#define VIRTIO_MAGIC 0x74726976

#define VIRTIO_STATUS_ACK       1
#define VIRTIO_STATUS_DRV       2
#define VIRTIO_STATUS_DRV_OK    4
#define VIRTIO_STATUS_FEAT_OK   8

#define VRING_DESC_F_NEXT       1
#define VRING_DESC_F_WRITE      2
#define VRING_DESC_F_INDIRECT   4

#define EV_SYN  0x00
#define EV_KEY  0x01
#define EV_REL  0x02
#define EV_ABS  0x03

#define VIRTIO_BLK_T_IN         0
#define VIRTIO_BLK_T_OUT        1

#define VIRTIO_BLK_SECTOR_SIZE  512
#define KB_BUFFER_SIZE 1024

#define KEY_LEFTSHIFT  42
#define KEY_RIGHTSHIFT 54
#define KEY_LEFTCTRL   29
#define KEY_RIGHTCTRL  97

// Mouse event types and codes
#define EV_SYN   0x00
#define EV_KEY   0x01
#define EV_REL   0x02
#define EV_ABS   0x03
#define SYN_REPORT 0

#define REL_X    0x00
#define REL_Y    0x01

#define BTN_LEFT    0x110
#define BTN_RIGHT   0x111
#define BTN_MIDDLE  0x112

typedef struct {
    u16 type;
    u16 code;
    u32 value;
} __attribute__((packed)) virtio_input_event;

typedef struct {
    u64 addr;
    u32 len;
    u16 flags;
    u16 next;
} __attribute__((packed)) virtq_desc;

typedef struct {
    u16 flags;
    u16 idx;
    u16 ring[16]; 
} __attribute__((packed)) virtq_avail;

typedef struct {
    u32 id;
    u32 len;
} __attribute__((packed)) virtq_used_elem;

typedef struct {
    u16 flags;
    u16 idx;
    virtq_used_elem ring[16];
} __attribute__((packed)) virtq_used;

typedef struct {
    u32 type;
    u32 reserved;
    u64 sector;
} __attribute__((packed)) virtio_blk_req;

void virtio_init();
int virtio_blk_read(u64 sector, u8* buffer);
int virtio_blk_write(u64 sector, u8* buffer);
void virtio_blk_handler();
void virtio_input_handler();
u64 virtio_blk_fs_read(inode_t* node, u64 offset, u64 size, u8* buffer);
u64 virtio_blk_fs_write(inode_t* node, u64 offset, u64 size, u8* buffer);
u64 virtio_kb_fs_read(inode_t *node, u64 offset, u64 size, u8 *buffer);
void virtio_blk_ops_init();
void virtio_rng_init(u64 base);
void virtio_rng_handler();
void virtio_mouse_handle_event(virtio_input_event *evt);

extern inode_ops virtio_blk_ops;
extern inode_ops virtio_kb_ops;
extern inode_ops virtio_mouse_ops;

extern inode_ops virtio_rng_ops;

#endif