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
static u32 virtio_irq = 0;

static volatile virtq_desc* desc;
static volatile virtq_avail* avail;
static volatile virtq_used* used;

static u16 last_used_idx = 0;
static spinlock_t virtio_lock = 0;

static u16 free_desc_stack[QUEUE_SIZE];
static int free_desc_top = 0;

typedef struct {
    virtio_blk_req req;     // The Header (sent to device)
    volatile u8 status;     // The Status byte (written by device)
    wait_queue_t wait_q;    // Task waiting for this specific request
    bool active;
} req_slot_t;

static req_slot_t req_slots[QUEUE_SIZE];

static int alloc_desc() {
    if (free_desc_top > 0) {
        return free_desc_stack[--free_desc_top];
    }
    return -1;
}

static void free_desc(int idx) {
    if (free_desc_top < QUEUE_SIZE) {
        free_desc_stack[free_desc_top++] = idx;
    }
}

void virtio_blk_handler() {
    u32 status = mmio_read32(virtio_blk_base + VIRTIO_MMIO_INTERRUPT_STATUS);
    
    mmio_write32(virtio_blk_base + VIRTIO_MMIO_INTERRUPT_ACK, status);

    while (last_used_idx != used->idx) {
        // Barrier to ensure the index before the ring element is read
        asm volatile("dmb sy");

        virtq_used_elem* elem = (virtq_used_elem*)&used->ring[last_used_idx % QUEUE_SIZE];
        u16 head_idx = (u16)elem->id;

        req_slot_t* slot = &req_slots[head_idx];

        if (slot->active) {
            slot->active = false;
            // Wakes up the task waiting for this specific IO
            wake_up(&slot->wait_q);
        }

        // Returns descriptors to the free pool
        int curr = head_idx;
        while (desc[curr].flags & VRING_DESC_F_NEXT) {
            int next = desc[curr].next;
            free_desc(curr);
            curr = next;
        }

        free_desc(curr); // Free the last one

        last_used_idx++;
    }
}

void virtio_init() {
    kprintf("[ [CVirtIO [W] Scanning...\n");

    for (int i = 0; i < 32; i++) {
        u64 addr = 0x0a000000 + (i * 0x200);
        if (mmio_read32(addr + VIRTIO_MMIO_MAGIC_VALUE) != VIRTIO_MAGIC) continue;
        
        if (mmio_read32(addr + VIRTIO_MMIO_DEVICE_ID) == 2) {
            virtio_blk_base = addr;
            virtio_irq = 48 + i; // Default QEMU mapping
            kprintf("[ [CVirtIO [W] Found Block Device at 0x%llx (IRQ %d)\n", addr, virtio_irq);
            break;
        }
    }

    if (!virtio_blk_base) {
        kprintf("[ [CVirtIO [W] Not found.\n");
        return;
    }

    u64 base = virtio_blk_base;
    mmio_write32(base + VIRTIO_MMIO_STATUS, 0);
    
    u32 status = VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRV;
    mmio_write32(base + VIRTIO_MMIO_STATUS, status);
    
    mmio_write32(base + VIRTIO_MMIO_GUEST_PAGE_SIZE, 4096);

    // Setup Queue
    mmio_write32(base + VIRTIO_MMIO_QUEUE_SEL, 0);
    u32 max = mmio_read32(base + VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (max == 0) return;

    mmio_write32(base + VIRTIO_MMIO_QUEUE_NUM, QUEUE_SIZE);

    u64 page = pmm_alloc_frame();
    memset((void*)page, 0, 4096);

    desc  = (virtq_desc*) page;
    avail = (virtq_avail*) (page + QUEUE_SIZE * 16);
    used  = (virtq_used*) (page + 4096 - sizeof(virtq_used));

    mmio_write32(base + VIRTIO_MMIO_QUEUE_PFN, page >> 12);

    for (int i = 0; i < QUEUE_SIZE; i++) {
        free_desc_stack[i] = i;
        req_slots[i].active = false;
        req_slots[i].wait_q = NULL;
    }
    free_desc_top = QUEUE_SIZE;

    gic_enable_irq(virtio_irq);

    status |= VIRTIO_STATUS_FEAT_OK | VIRTIO_STATUS_DRV_OK;
    mmio_write32(base + VIRTIO_MMIO_STATUS, status);

    // Notify DevFS to mount it
    kprintf("[ [CVirtIO [W] Initialized.\n");
}

static int virtio_blk_rw(u64 sector, u8* buffer, int is_write) {
    if (!virtio_blk_base) return -1;

    u32 flags_irq = spinlock_acquire_irqsave(&virtio_lock);

    int idx_head = alloc_desc();
    int idx_buf  = alloc_desc();
    int idx_stat = alloc_desc();

    if (idx_head < 0 || idx_buf < 0 || idx_stat < 0) {
        kprintf("[ VirtIO ] [RError[W: Queue full!\n");
        spinlock_release_irqrestore(&virtio_lock, flags_irq);
        return -1;
    }

    req_slot_t* slot = &req_slots[idx_head];
    slot->req.type = is_write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
    slot->req.reserved = 0;
    slot->req.sector = sector;
    slot->status = 111; // Pending
    slot->active = true;
    slot->wait_q = NULL;

    // Header
    desc[idx_head].addr = (u64)&slot->req;
    desc[idx_head].len  = sizeof(virtio_blk_req);
    desc[idx_head].flags = VRING_DESC_F_NEXT;
    desc[idx_head].next = idx_buf;

    // Buffer
    desc[idx_buf].addr = (u64)buffer;
    desc[idx_buf].len  = 512;
    desc[idx_buf].flags = VRING_DESC_F_NEXT;
    if (!is_write) desc[idx_buf].flags |= VRING_DESC_F_WRITE;
    desc[idx_buf].next = idx_stat;

    // Status
    desc[idx_stat].addr = (u64)&slot->status;
    desc[idx_stat].len  = 1;
    desc[idx_stat].flags = VRING_DESC_F_WRITE;
    desc[idx_stat].next = 0;

    avail->ring[avail->idx % QUEUE_SIZE] = idx_head;
    
    asm volatile("dmb sy"); // Ensures descriptors are visible
    avail->idx++;
    asm volatile("dmb sy"); // Ensures index update is visible

    mmio_write32(virtio_blk_base + VIRTIO_MMIO_QUEUE_NOTIFY, 0);

    spinlock_release_irqrestore(&virtio_lock, flags_irq);
    
    // Blocks until the ISR wakes us up via this slot's wait queue
    while (slot->active) {
        sleep_on(&slot->wait_q);
    }

    return (slot->status == 0) ? 0 : -1;
}

int virtio_blk_read(u64 sector, u8* buffer) {
    return virtio_blk_rw(sector, buffer, 0);
}

int virtio_blk_write(u64 sector, u8* buffer) {
    return virtio_blk_rw(sector, buffer, 1);
}