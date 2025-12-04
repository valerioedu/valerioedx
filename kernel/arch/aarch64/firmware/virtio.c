#include <virtio.h>
#include <kio.h>
#include <io.h>
#include <string.h>
#include <sched.h>
#include <pmm.h>
#include <gic.h>
#include <spinlock.h>

#define QUEUE_SIZE 16

static u64 virtio_blk_base = 0;

void virtio_init() {
    kprintf("[ [CVirtIO [W] Scanning...\n");

    for (int i = 0; i < 32; i++) {
        u64 addr = 0x0a000000 + (i * 0x200);
        if (mmio_read32(addr + VIRTIO_MMIO_MAGIC_VALUE) != VIRTIO_MAGIC) 
            continue;

        if (mmio_read32(addr + VIRTIO_MMIO_DEVICE_ID) == 1) {
            kprintf("[ [CVirtIO [W] Found Network Card Device at 0x%llx\n", addr);
        } else if (mmio_read32(addr + VIRTIO_MMIO_DEVICE_ID) == 2) {
            virtio_blk_base = addr;
            kprintf("[ [CVirtIO [W] Found Block Device at 0x%llx\n", addr);
        } else if (mmio_read32(addr + VIRTIO_MMIO_DEVICE_ID) == 3) {
            kprintf("[ [CVirtIO [W] Found Console Device at 0x%llx\n", addr);
        }
    }
}