#include <virtio.h>
#include <kio.h>
#include <io.h>
#include <string.h>
#include <sched.h>
#include <pmm.h>
#include <gic.h>
#include <spinlock.h>
#include <vmm.h>

#define QUEUE_SIZE 16

static u64 virtio_blk_base = 0;
static virtq_desc *virtq_desc_base;
static virtq_avail *virtq_avail_base;
static virtq_used *virtq_used_base;
static u16 last_used_idx = 0;

static wait_queue_t blk_wait_queue = NULL;
static int virtio_async = 0;

static virtio_blk_req *req_headers;
static u8 *req_statuses;

void virtio_blk_init(u64 base) {
    virtio_blk_base = base;
    
    mmio_write32(base + VIRTIO_MMIO_STATUS, 0);
    
    // Acknowledge & Driver
    u32 status = VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRV;
    mmio_write32(base + VIRTIO_MMIO_STATUS, status);
    
    mmio_write32(base + VIRTIO_MMIO_GUEST_PAGE_SIZE, 4096);
    
    mmio_write32(base + VIRTIO_MMIO_QUEUE_SEL, 0);
    u32 max_queue = mmio_read32(base + VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (max_queue == 0) {
        kprintf("[VirtIO] Queue 0 not available\n");
        return;
    }
    
    mmio_write32(base + VIRTIO_MMIO_QUEUE_NUM, QUEUE_SIZE);
    mmio_write32(base + VIRTIO_MMIO_QUEUE_ALIGN, 16); 
    
    u64 page_phys = pmm_alloc_frame();
    u64 page_virt = (u64)P2V(page_phys);
    memset((void*)page_virt, 0, 4096);
    
    mmio_write32(base + VIRTIO_MMIO_QUEUE_PFN, page_phys >> 12);
    
    virtq_desc_base = (virtq_desc*)page_virt;
    virtq_avail_base = (virtq_avail*)(page_virt + QUEUE_SIZE * 16);
    
    u64 avail_end = (u64)virtq_avail_base + (4 + 2 * QUEUE_SIZE);
    u64 used_start = (avail_end + 15) & ~15;
    virtq_used_base = (virtq_used*)used_start;
    
    // Allocates memory for requests
    u64 req_page_phys = pmm_alloc_frame();
    u64 req_page_virt = (u64)P2V(req_page_phys);
    req_headers = (virtio_blk_req*)req_page_virt;
    req_statuses = (u8*)(req_page_virt + sizeof(virtio_blk_req) * QUEUE_SIZE);
    
    // Driver OK
    status |= VIRTIO_STATUS_DRV_OK;
    mmio_write32(base + VIRTIO_MMIO_STATUS, status);
    
    kprintf("[ [CVirtIO [W] Blk Initialized. Queue PFN: 0x%llx\n", page_phys >> 12);
}

void virtio_init() {
    kprintf("[ [CVirtIO [W] Scanning...\n");

    for (int i = 0; i < 32; i++) {
        u64 addr = 0x0a000000 + (i * 0x200);
        if (mmio_read32(addr + VIRTIO_MMIO_MAGIC_VALUE) != VIRTIO_MAGIC) 
            continue;

        u32 device_id = mmio_read32(addr + VIRTIO_MMIO_DEVICE_ID);

        if (device_id == 1) {
            kprintf("[ [CVirtIO [W] Found Network Card Device at 0x%llx\n", addr);
        } else if (device_id == 2) {
            kprintf("[ [CVirtIO [W] Found Block Device at 0x%llx\n", addr);
            virtio_blk_init(addr);
        } else if (device_id == 3) {
            kprintf("[ [CVirtIO [W] Found Console Device at 0x%llx\n", addr);
        }
    }
}

void virtio_set_async(int enable) {
    virtio_async = enable;
}

int virtio_blk_op(u64 sector, u8* buffer, int write) {
    if (virtio_blk_base == 0) return -1;

    int idx = 0; 
    
    virtio_blk_req *req = &req_headers[idx];
    req->type = write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
    req->reserved = 0;
    req->sector = sector;
    
    // Desc 0: Header
    virtq_desc_base[0].addr = V2P(req);
    virtq_desc_base[0].len = sizeof(virtio_blk_req);
    virtq_desc_base[0].flags = VRING_DESC_F_NEXT;
    virtq_desc_base[0].next = 1;
    
    // Desc 1: Buffer
    virtq_desc_base[1].addr = V2P(buffer);
    virtq_desc_base[1].len = 512; 
    virtq_desc_base[1].flags = VRING_DESC_F_NEXT | (write ? 0 : VRING_DESC_F_WRITE);
    virtq_desc_base[1].next = 2;
    
    // Desc 2: Status
    virtq_desc_base[2].addr = V2P(&req_statuses[idx]);
    virtq_desc_base[2].len = 1;
    virtq_desc_base[2].flags = VRING_DESC_F_WRITE;
    virtq_desc_base[2].next = 0;
    
    // Updates Avail Ring
    u16 avail_idx = virtq_avail_base->idx;
    virtq_avail_base->ring[avail_idx % QUEUE_SIZE] = 0;
    
    asm volatile("dmb sy");
    virtq_avail_base->idx = avail_idx + 1;
    asm volatile("dmb sy");
    
    mmio_write32(virtio_blk_base + VIRTIO_MMIO_QUEUE_NOTIFY, 0); 
    
    // Wait for completion
    while (last_used_idx == virtq_used_base->idx) {
        if (virtio_async) {
            sleep_on(&blk_wait_queue);
        } else {
            asm volatile("wfi");
        }
    }
    
    last_used_idx++;
    
    return req_statuses[idx] == 0 ? 0 : -1;
}

int virtio_blk_read(u64 sector, u8* buffer) {
    return virtio_blk_op(sector, buffer, 0);
}

int virtio_blk_write(u64 sector, u8* buffer) {
    return virtio_blk_op(sector, buffer, 1);
}

void virtio_blk_handler() {
    if (virtio_blk_base == 0) return;
    u32 status = mmio_read32(virtio_blk_base + VIRTIO_MMIO_INTERRUPT_STATUS);
    mmio_write32(virtio_blk_base + VIRTIO_MMIO_INTERRUPT_ACK, status);
    wake_up(&blk_wait_queue);
}