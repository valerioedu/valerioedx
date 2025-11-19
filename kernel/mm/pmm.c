#include <lib.h>
#include <vga.h>
#include <multiboot.h>
#include <io.h>
#include <spinlock.h>

#define VIRTUAL_OFFSET 0

#define P2V(a)          ((uintptr_t)(a) + VIRTUAL_OFFSET)
#define FRAME_SIZE      0x1000      // 4 KB
#define BITS_PER_BYTE   8

static u8 *pmm_bitmap = NULL;
static u32 max_frames = 0;
static u32 used_frames = 0;

static spinlock_t pmm_lock = 0;

static inline u32 addr_to_frame(uintptr_t addr) {
    return addr / FRAME_SIZE;
}

static inline u32 frame_to_byte(u32 frame) {
    return frame / BITS_PER_BYTE;
}

static inline uintptr_t frame_to_addr(u32 frame) {
    return (uintptr_t)frame * FRAME_SIZE;
}

// Marks a region as free
void pmm_init_region(uintptr_t base, u64 len) {
    u32 flags = spinlock_acquire_irqsave(&pmm_lock);

    u32 start_frame = addr_to_frame(base);
    u32 end_frame = addr_to_frame(base + len);

    if (end_frame > max_frames) {
        max_frames = end_frame;
    }

    for (u32 i = start_frame; i < end_frame; i++) {
        u32 byte_idx = frame_to_byte(i);
        u32 bit_idx = i % BITS_PER_BYTE;

        // Only clear if currently set (prevent double-decrement)
        if (pmm_bitmap[byte_idx] & (1 << bit_idx)) {
            pmm_bitmap[byte_idx] &= ~(1 << bit_idx);
            used_frames--;
        }
    }

    spinlock_release_irqrestore(&pmm_lock, flags);
}

// Marks a region as USED
void pmm_mark_used_region(uintptr_t base, u64 len) {
    u32 flags = spinlock_acquire_irqsave(&pmm_lock);

    u32 start_frame = addr_to_frame(base);
    u32 end_frame = addr_to_frame(base + len);

    for (u32 i = start_frame; i < end_frame; i++) {
        u32 byte_idx = frame_to_byte(i);
        u32 bit_idx = i % BITS_PER_BYTE;
        
        // Only set if currently clear (prevent double-increment)
        if (!(pmm_bitmap[byte_idx] & (1 << bit_idx))) {
            pmm_bitmap[byte_idx] |= (1 << bit_idx);
            used_frames++;
        }
    }

    spinlock_release_irqrestore(&pmm_lock, flags);
}

void parse_multiboot_mmap(u32 mmap_addr, u32 mmap_length) {
    struct multiboot_mmap_entry* entry = (struct multiboot_mmap_entry*)P2V(mmap_addr);
    uintptr_t map_end = P2V(mmap_addr + mmap_length);

    while ((uintptr_t)entry < map_end) {
        if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
            // This marks the memory as free
            pmm_init_region(entry->addr_low, entry->len_low);
        }
        entry = (struct multiboot_mmap_entry*) 
                ((uintptr_t)entry + entry->size + sizeof(entry->size));
    }
}

uintptr_t pmm_alloc_frame(void) {
    u32 flags = spinlock_acquire_irqsave(&pmm_lock);

    for (u32 i = 0; i < max_frames; i++) {
        u32 byte_idx = frame_to_byte(i);
        u32 bit_idx = i % BITS_PER_BYTE;
        
        // Check if bit is 0 (Free)
        if (!(pmm_bitmap[byte_idx] & (1 << bit_idx))) {
            // Found free frame, mark as used
            pmm_bitmap[byte_idx] |= (1 << bit_idx);
            used_frames++;

            spinlock_release_irqrestore(&pmm_lock, flags);
            return frame_to_addr(i);
        }
    }
    
    spinlock_release_irqrestore(&pmm_lock, flags);
    return 0; // Out of memory
}

void pmm_free_frame(uintptr_t addr) {
    u32 flags = spinlock_acquire_irqsave(&pmm_lock);

    u32 frame = addr_to_frame(addr);
    u32 byte_idx = frame_to_byte(frame);
    u32 bit_idx = frame % BITS_PER_BYTE;
    
    // Safety: check if it was actually used
    if (pmm_bitmap[byte_idx] & (1 << bit_idx)) {
        pmm_bitmap[byte_idx] &= ~(1 << bit_idx);
        used_frames--;
    }

    spinlock_release_irqrestore(&pmm_lock, flags);
}

void pmm_init(u32 kernel_end, u32 total_memory) {
    max_frames = total_memory / FRAME_SIZE;
    u32 bitmap_size = (max_frames + BITS_PER_BYTE - 1) / BITS_PER_BYTE;

    // Place bitmap at the end of the kernel
    uintptr_t bitmap_phys_start = kernel_end;
    pmm_bitmap = (u8*)P2V(bitmap_phys_start);

    for (u32 i = 0; i < bitmap_size; i++) {
        pmm_bitmap[i] = 0xFF;
    }
    used_frames = max_frames;
}