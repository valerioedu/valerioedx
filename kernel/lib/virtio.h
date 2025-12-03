#ifndef VIRTIO_H
#define VIRTIO_H

#include <lib.h>

// VirtIO MMIO Register Offsets
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
#define VIRTIO_MMIO_STATUS          0x070

// Magic value "virt"
#define VIRTIO_MAGIC 0x74726976

// Device IDs
#define VIRTIO_DEV_NET   1
#define VIRTIO_DEV_BLOCK 2
#define VIRTIO_DEV_GPU   16

// Status bits
#define VIRTIO_STATUS_ACKNOWLEDGE  1
#define VIRTIO_STATUS_DRIVER       2
#define VIRTIO_STATUS_FAILED       128
#define VIRTIO_STATUS_FEATURES_OK  8
#define VIRTIO_STATUS_DRIVER_OK    4

// There are 32 slots of 0x200 bytes each
#define VIRTIO_MMIO_BASE   0x0a000000
#define VIRTIO_MMIO_END    0x0a004000
#define VIRTIO_MMIO_INTERVAL 0x200

void virtio_init();

#endif