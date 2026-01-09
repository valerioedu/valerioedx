#include <virtio.h>
#include <kio.h>
#include <io.h>
#include <string.h>
#include <sched.h>
#include <pmm.h>
#include <gic.h>
#include <spinlock.h>
#include <vmm.h>
#include <sync.h>
#include <tty.h>

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

u8 virtio_blk_irq_id = 0;
u8 virtio_key_irq_id = 0;

static u64 virtio_input_base = 0;
static virtq_desc *input_desc;
static virtq_avail *input_avail;
static virtq_used *input_used;
static virtio_input_event *input_events;
static u16 input_last_used_idx = 0;

// Keyboard circular buffer
char kb_buffer[KB_BUFFER_SIZE];
volatile int kb_head = 0;
volatile int kb_tail = 0;
wait_queue_t kb_wait_queue = NULL;

static bool shifting = false;

static mutex_t blk_mutex;

static char key_map[] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

// Shifted characters map
static char key_map_shift[] = {
    0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|',
    'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' '
};

static char virtio_scancode_to_ascii(u16 code) {
    if (code == 1) return 0; // ESC
    if (code >= 2 && code < sizeof(key_map)) {
        return shifting ? key_map_shift[code] : key_map[code];
    }
    return 0;
}

void virtio_input_init(u64 base) {
    virtio_input_base = base;

    mmio_write32(base + VIRTIO_MMIO_STATUS, 0);

    // Acknowledge & Driver
    u32 status = VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRV;
    mmio_write32(base + VIRTIO_MMIO_STATUS, status);

    // Feature negotiation
    u32 features = mmio_read32(base + VIRTIO_MMIO_DEVICE_FEATURES);
    // Mask VIRTIO_F_RING_EVENT_IDX feature, which suppresses irqs
    features &= ~(1 << 29);
    mmio_write32(base + VIRTIO_MMIO_DRIVER_FEATURES, features);
    
    status |= VIRTIO_STATUS_FEAT_OK;
    mmio_write32(base + VIRTIO_MMIO_STATUS, status);
    
    // Verify features accepted
    status = mmio_read32(base + VIRTIO_MMIO_STATUS);
    if (!(status & VIRTIO_STATUS_FEAT_OK)) {
        kprintf("[VirtIO Input] Feature negotiation FAILED\n");
        return;
    }

    // Queue Setup (Queue 0 is Event Queue)
    mmio_write32(base + VIRTIO_MMIO_GUEST_PAGE_SIZE, 4096);
    mmio_write32(base + VIRTIO_MMIO_QUEUE_SEL, 0); // Select Event Queue

    u32 max_queue = mmio_read32(base + VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (max_queue == 0) {
        kprintf("[ [RVirtIO Input [W] Queue 0 not available\n");
        return;
    }
    kprintf("[ [CVirtIO [W] Max input queue size: %d\n", max_queue);

    mmio_write32(base + VIRTIO_MMIO_QUEUE_NUM, QUEUE_SIZE);
    mmio_write32(base + VIRTIO_MMIO_QUEUE_ALIGN, 16);

    u64 page_phys = pmm_alloc_frame();
    u64 page_virt = (u64)P2V(page_phys);
    memset((void*)page_virt, 0, 4096);

    mmio_write32(base + VIRTIO_MMIO_QUEUE_PFN, page_phys >> 12);

    input_desc  = (virtq_desc*)page_virt;
    input_avail = (virtq_avail*)(page_virt + QUEUE_SIZE * 16);
    
    u64 avail_end = (u64)input_avail + (4 + 2 * QUEUE_SIZE);
    u64 used_start = (avail_end + 15) & ~15;
    input_used = (virtq_used*)used_start;

    // Allocate buffers for events
    u64 event_page_phys = pmm_alloc_frame();
    input_events = (virtio_input_event*)P2V(event_page_phys);
    memset(input_events, 0, 4096);

    // Fill Descriptors & Available Ring
    for (int i = 0; i < QUEUE_SIZE; i++) {
        input_desc[i].addr = V2P(&input_events[i]);
        input_desc[i].len  = sizeof(virtio_input_event);
        input_desc[i].flags = VRING_DESC_F_WRITE; // Device writes to this
        input_desc[i].next  = 0;

        input_avail->ring[i] = i;
    }
    
    asm volatile("dmb sy");
    input_avail->idx = QUEUE_SIZE; // All available
    asm volatile("dmb sy");

    mmio_write32(base + VIRTIO_MMIO_QUEUE_NOTIFY, 0); // Kick queue 0

    // Driver OK
    status |= VIRTIO_STATUS_DRV_OK;
    mmio_write32(base + VIRTIO_MMIO_STATUS, status);

    // Verify status was accepted
    u32 final_status = mmio_read32(base + VIRTIO_MMIO_STATUS);
    kprintf("[ [CVirtIO [W] Input Initialized at 0x%llx, status=0x%x\n", base, final_status);
}

void virtio_input_handler() {
    if (virtio_input_base == 0) return;

    // Acknowledge Interrupt
    u32 status = mmio_read32(virtio_input_base + VIRTIO_MMIO_INTERRUPT_STATUS);
    mmio_write32(virtio_input_base + VIRTIO_MMIO_INTERRUPT_ACK, status);

    // Check Used Ring for new events
    while (input_last_used_idx != input_used->idx) {
        u32 id = input_used->ring[input_last_used_idx % QUEUE_SIZE].id;
        virtio_input_event *evt = &input_events[id];

        if (evt->type == EV_KEY) {
            // Handle shift key press/release
            if (evt->code == KEY_LEFTSHIFT || evt->code == KEY_RIGHTSHIFT) {
                shifting = (evt->value == 1); // 1 = press, 0 = release
            } else if (evt->value == 1) { // Key press
                char c = virtio_scancode_to_ascii(evt->code);
                if (c) {
                    // Add to circular buffer
                    int next_head = (kb_head + 1) % KB_BUFFER_SIZE;
                    if (next_head != kb_tail) { // Buffer not full
                        kb_buffer[kb_head] = c;
                        kb_head = next_head;
                        wake_up(&kb_wait_queue);
                    }
                }
                tty_push_char(c, &tty_console);
            }
        }

        // Return buffer to Available Ring
        input_avail->ring[input_avail->idx % QUEUE_SIZE] = id;
        asm volatile("dmb sy");
        input_avail->idx++;
        asm volatile("dmb sy");
        
        input_last_used_idx++;
    }
    
    mmio_write32(virtio_input_base + VIRTIO_MMIO_QUEUE_NOTIFY, 0);
}

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
        kprintf("[ [CVirtIO [W] Queue 0 not available\n");
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

    mutex_init(&blk_mutex);
    
    kprintf("[ [CVirtIO [W] Blk Initialized. Queue PFN: 0x%llx\n", page_phys >> 12);
}

void virtio_init() {
    kprintf("[ [CVirtIO [W] Scanning...\n");

    for (int i = 0; i < 32; i++) {
        u64 addr = 0x0a000000 + PHYS_OFFSET+ (i * 0x200);
        if (mmio_read32(addr + VIRTIO_MMIO_MAGIC_VALUE) != VIRTIO_MAGIC) 
            continue;

        u32 device_id = mmio_read32(addr + VIRTIO_MMIO_DEVICE_ID);

        if (device_id == 1) {
            kprintf("[ [CVirtIO [W] Found Network Card Device at 0x%llx\n", addr);
        } else if (device_id == 2) {
            kprintf("[ [CVirtIO [W] Found Block Device at 0x%llx\n", addr);
            virtio_blk_init(addr);
            virtio_blk_irq_id = 48 + i;
            gic_enable_irq(virtio_blk_irq_id);
        } else if (device_id == 3) {
            kprintf("[ [CVirtIO [W] Found Console Device at 0x%llx\n", addr);
        } else if (device_id == 18) {
            kprintf("[ [CVirtIO [W] Found Input Device at 0x%llx\n", addr);
            virtio_input_init(addr);
            virtio_key_irq_id = 48 + i;
            gic_enable_irq(virtio_key_irq_id);
        }
    }
}

void virtio_set_async(int enable) {
    virtio_async = enable;
}

int virtio_blk_op(u64 sector, u8* buffer, int write) {
    if (virtio_blk_base == 0) return -1;

    mutex_acquire(&blk_mutex);

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
        asm volatile("dmb sy");  // Memory barrier to re-read from memory
        asm volatile("yield");
    }
    
    last_used_idx++;
    
    mutex_release(&blk_mutex);
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