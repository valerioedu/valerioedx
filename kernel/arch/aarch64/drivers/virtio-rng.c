#include <lib.h>
#include <virtio.h>
#include <vfs.h>
#include <kio.h>
#include <io.h>
#include <string.h>
#include <sched.h>
#include <pmm.h>
#include <gic.h>
#include <vmm.h>
#include <sync.h>

//TODO: implement entropy accounting for /dev/random

#define ENTROPY_POOL_SIZE 256
#define RNG_QUEUE_SIZE 16

static u64 virtio_rng_base = 0;

static virtq_desc  *rng_desc;
static virtq_avail *rng_avail;
static virtq_used  *rng_used;
static u16 rng_last_used = 0;

static wait_queue_t rng_wait_queue = NULL;
static mutex_t rng_mutex;

static u8 entropy_pool[ENTROPY_POOL_SIZE];
static volatile int entropy_available = 0;

inode_ops virtio_rng_ops = { };

u64 virtio_rng_fs_read(inode_t *node, u64 offset, u64 size, u8 *buffer);

void virtio_rng_init(u64 base) {
    virtio_rng_base = base;
    
    mmio_write32(base + VIRTIO_MMIO_STATUS, 0);
    
    u32 status = VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRV;
    mmio_write32(base + VIRTIO_MMIO_STATUS, status);
    
    u32 features = mmio_read32(base + VIRTIO_MMIO_DEVICE_FEATURES);
    features &= ~(1 << 29);
    mmio_write32(base + VIRTIO_MMIO_DRIVER_FEATURES, features);
    
    status |= VIRTIO_STATUS_FEAT_OK;
    mmio_write32(base + VIRTIO_MMIO_STATUS, status);
    
    status = mmio_read32(base + VIRTIO_MMIO_STATUS);
    if (!(status & VIRTIO_STATUS_FEAT_OK)) {
        kprintf("[VirtIO RNG] Feature negotiation FAILED\n");
        return;
    }
    
    mmio_write32(base + VIRTIO_MMIO_GUEST_PAGE_SIZE, 4096);
    
    mmio_write32(base + VIRTIO_MMIO_QUEUE_SEL, 0);
    
    u32 max_queue = mmio_read32(base + VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (max_queue == 0) {
        kprintf("[ [RVirtIO RNG [W] Queue 0 not available\n");
        return;
    }
    
    mmio_write32(base + VIRTIO_MMIO_QUEUE_NUM, RNG_QUEUE_SIZE);
    mmio_write32(base + VIRTIO_MMIO_QUEUE_ALIGN, 16);
    
    u64 page_phys = pmm_alloc_frame();
    u64 page_virt = (u64)P2V(page_phys);
    memset((void*)page_virt, 0, 4096);
    
    mmio_write32(base + VIRTIO_MMIO_QUEUE_PFN, page_phys >> 12);
    
    rng_desc  = (virtq_desc*)page_virt;
    rng_avail = (virtq_avail*)(page_virt + RNG_QUEUE_SIZE * 16);
    
    u64 avail_end = (u64)rng_avail + (4 + 2 * RNG_QUEUE_SIZE);
    u64 used_start = (avail_end + 15) & ~15;
    rng_used = (virtq_used*)used_start;
    
    mutex_init(&rng_mutex);
    
    status = mmio_read32(base + VIRTIO_MMIO_STATUS);
    status |= VIRTIO_STATUS_DRV_OK;
    mmio_write32(base + VIRTIO_MMIO_STATUS, status);

    virtio_rng_ops.read = virtio_rng_fs_read;
    
    kprintf("[ [CVirtIO [W] RNG Initialized at 0x%llx\n", base);
}

static int virtio_rng_request(u8 *buffer, u32 len) {
    if (virtio_rng_base == 0 || len == 0) return -1;
    if (len > 4096) len = 4096;
    
    // Single descriptor: device-writable buffer
    rng_desc[0].addr = V2P(buffer);
    rng_desc[0].len = len;
    rng_desc[0].flags = VRING_DESC_F_WRITE;
    rng_desc[0].next = 0;
    
    u16 avail_idx = rng_avail->idx;
    rng_avail->ring[avail_idx % RNG_QUEUE_SIZE] = 0;
    
    asm volatile("dmb sy");
    rng_avail->idx = avail_idx + 1;
    asm volatile("dmb sy");
    
    mmio_write32(virtio_rng_base + VIRTIO_MMIO_QUEUE_NOTIFY, 0);
    
    while (rng_last_used == rng_used->idx) {
        asm volatile("dmb sy");
        asm volatile("yield");
    }
    
    u32 used_len = rng_used->ring[rng_last_used % RNG_QUEUE_SIZE].len;
    rng_last_used++;
    
    return (int)used_len;
}

// Fill the internal entropy pool
static void virtio_rng_fill_pool() {
    int got = virtio_rng_request(entropy_pool, ENTROPY_POOL_SIZE);
    if (got > 0) entropy_available = got;
}

u64 virtio_rng_read(u8 *buffer, u64 size) {
    if (virtio_rng_base == 0) return 0;
    
    mutex_acquire(&rng_mutex);
    
    u64 total = 0;
    
    while (total < size) {
        u64 chunk = size - total;
        if (chunk > ENTROPY_POOL_SIZE) chunk = ENTROPY_POOL_SIZE;
        
        int got = virtio_rng_request(buffer + total, (u32)chunk);
        if (got <= 0) break;
        
        total += got;
    }
    
    mutex_release(&rng_mutex);
    return total;
}

u64 virtio_rng_fs_read(inode_t *node, u64 offset, u64 size, u8 *buffer) {
    return virtio_rng_read(buffer, size);
}

void virtio_rng_handler() {
    if (virtio_rng_base == 0) return;
    u32 status = mmio_read32(virtio_rng_base + VIRTIO_MMIO_INTERRUPT_STATUS);
    mmio_write32(virtio_rng_base + VIRTIO_MMIO_INTERRUPT_ACK, status);
    wake_up(&rng_wait_queue);
}