#include <pmm.h>
#include <string.h>
#include <uart.h>

static u8 *bitmap = NULL;
static u32 used_frames = 0;

static inline u32 phys_to_index(uintptr_t addr) {
    if (addr < PHY_RAM_BASE || addr >= PHY_RAM_END) return (u32)-1;
    return (addr - PHY_RAM_BASE) >> PAGE_SHIFT;
}

static inline uintptr_t index_to_phys(u32 idx) {
    return PHY_RAM_BASE + ((uintptr_t)idx << PAGE_SHIFT);
}

// Helpers for bit manipulation
static inline void bitmap_set(u32 bit) {
    bitmap[bit / 8] |= (1 << (bit % 8));
}

static inline void bitmap_unset(u32 bit) {
    bitmap[bit / 8] &= ~(1 << (bit % 8));
}

static inline bool bitmap_test(u32 bit) {
    return bitmap[bit / 8] & (1 << (bit % 8));
}

void pmm_init(uintptr_t kernel_end) {
    // Page aligned after kernel end
    uintptr_t bitmap_start = (kernel_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    bitmap = (u8*)bitmap_start;
    
    memset(bitmap, 0, BITMAP_SIZE);
    
    // Total used memory
    uintptr_t used_end = bitmap_start + BITMAP_SIZE;
    size_t used_size   = used_end - PHY_RAM_BASE;
    pmm_mark_used_region(PHY_RAM_BASE, used_size);

    kprintf("[PMM] Initialized. Bitmap at 0x%x, Size: %d KB\n", bitmap, BITMAP_SIZE / 1024);
    kprintf("[PMM] Managing RAM: 0x%llx - 0x%llx\n", PHY_RAM_BASE, PHY_RAM_END);
}

void pmm_mark_used_region(uintptr_t base, size_t size) {
    u32 start_idx = phys_to_index(base);
    u32 end_idx   = phys_to_index(base + size + PAGE_SIZE - 1); // Round up

    if (start_idx == (u32)-1) return; // Address out of range

    for (u32 i = start_idx; i < end_idx; i++) {
        if (i < TOTAL_FRAMES && !bitmap_test(i)) {
            bitmap_set(i);
            used_frames++;
        }
    }
}

uintptr_t pmm_alloc_frame() {
    // Linear search
    for (u32 i = 0; i < TOTAL_FRAMES; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            used_frames++;
            return index_to_phys(i);
        }
    }
    
    kprintf("[PMM] CRITICAL: Out of Memory!\n");
    return 0;
}

void pmm_free_frame(uintptr_t addr) {
    u32 idx = phys_to_index(addr);
    
    if (idx != (u32)-1 && idx < TOTAL_FRAMES) {
        if (bitmap_test(idx)) {
            bitmap_unset(idx);
            used_frames--;
        }
    }
}