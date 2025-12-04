#include "virtio.h"
#include <kio.h>
#include <io.h>
#include <string.h>
#include <sched.h>
#include <pmm.h>
#include <gic.h>
#include <spinlock.h>

#define QUEUE_SIZE 16

static u64 virtio_blk_base = 0;

static volatile virtq_desc* desc;
static volatile virtq_avail* avail;
static volatile virtq_used* used;

static spinlock_t virtio_lock = 0;

// Simple descriptor allocator
static u16 free_desc_stack[QUEUE_SIZE];
static int free_desc_top = 0;

typedef struct {
    virtio_blk_req req;     
    volatile u8 status;     
} req_slot_t;

void virtio_init() {
    kprintf("[ [CVirtIO [W] Scanning...\n");

    for (int i = 0; i < 32; i++) {
        u64 addr = 0x0a000000 + (i * 0x200);
        if (mmio_read32(addr + VIRTIO_MMIO_MAGIC_VALUE) != VIRTIO_MAGIC) continue;
        if (mmio_read32(addr + VIRTIO_MMIO_DEVICE_ID) == 2) {
            virtio_blk_base = addr;
            kprintf("[ [CVirtIO [W] Found Block Device at 0x%llx\n", addr);
            break;
        }
    }

    if (!virtio_blk_base) return;

    u64 base = virtio_blk_base;
    mmio_write32(base + VIRTIO_MMIO_STATUS, 0); // Reset
    mmio_write32(base + VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRV);
    mmio_write32(base + VIRTIO_MMIO_GUEST_PAGE_SIZE, 4096);

    // Queue Setup
    mmio_write32(base + VIRTIO_MMIO_QUEUE_SEL, 0);
    if (mmio_read32(base + VIRTIO_MMIO_QUEUE_NUM_MAX) == 0) return;
    mmio_write32(base + VIRTIO_MMIO_QUEUE_NUM, QUEUE_SIZE);

    u64 page = pmm_alloc_frame();
    memset((void*)page, 0, 4096);

    desc  = (virtq_desc*) page;
    avail = (virtq_avail*) (page + QUEUE_SIZE * 16);
    used  = (virtq_used*) (page + 4096 - sizeof(virtq_used));

    mmio_write32(base + VIRTIO_MMIO_QUEUE_PFN, page >> 12);

    for (int i = 0; i < QUEUE_SIZE; i++) free_desc_stack[i] = i;
    free_desc_top = QUEUE_SIZE;

    mmio_write32(base + VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRV | VIRTIO_STATUS_FEAT_OK | VIRTIO_STATUS_DRV_OK);
    kprintf("[ [CVirtIO [W] Initialized (Polling Mode).\n");
}