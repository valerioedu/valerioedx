#include <pmm.h>
#include <string.h>
#include <kio.h>
#include <spinlock.h>

static u32 *frame_stack = NULL;
static u32 stack_top = 0;      // Points to the next free slot (or count of free frames)
static u32 stack_capacity = 0;
static u32 used_frames = 0;
static spinlock_t pmm_lock = 0;

u64 phy_ram_size = 0;
u64 phy_ram_end = 0;
u32 total_frames = 0;

static inline u32 phys_to_index(uintptr_t addr) {
    if (addr < PHY_RAM_BASE || addr >= phy_ram_end) return (u32)-1;
    return (addr - PHY_RAM_BASE) >> PAGE_SHIFT;
}

static inline uintptr_t index_to_phys(u32 idx) {
    return PHY_RAM_BASE + ((uintptr_t)idx << PAGE_SHIFT);
}

void pmm_init(uintptr_t kernel_end, u64 ram_size) {
    phy_ram_size = ram_size;
    phy_ram_end = PHY_RAM_BASE + phy_ram_size;
    total_frames = phy_ram_size / PAGE_SIZE;

    uintptr_t stack_start = (kernel_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    
    size_t stack_size_bytes = total_frames * sizeof(u32);
    
    frame_stack = (u32*)stack_start;
    stack_capacity = total_frames;
    stack_top = 0;

    uintptr_t pmm_reserved_end = stack_start + stack_size_bytes;

    kprintf("[PMM] Initializing Page Stack Allocator...\n");
    kprintf("[PMM] Stack at 0x%x, Size: %d KB\n", frame_stack, stack_size_bytes / 1024);

    for (u32 i = 0; i < total_frames; i++) {
        uintptr_t addr = index_to_phys(i);

        // If used don't push to stack, else push
        if (addr >= PHY_RAM_BASE && addr < pmm_reserved_end) {
            used_frames++;
        } else {
            if (stack_top < stack_capacity)
                frame_stack[stack_top++] = i;
        }
    }

    kprintf("[PMM] Initialization complete. Free frames: %d\n", stack_top);
}

void pmm_mark_used_region(uintptr_t base, size_t size) {
    u32 start_idx = phys_to_index(base);
    u32 end_idx   = phys_to_index(base + size + PAGE_SIZE - 1);

    if (start_idx == (u32)-1) return;

    // This is linear with a stack, but is called just at the start of the kernel
    for (u32 target = start_idx; target < end_idx; target++) {
        for (u32 i = 0; i < stack_top; i++) {
            if (frame_stack[i] == target) {
                stack_top--;
                frame_stack[i] = frame_stack[stack_top];
                used_frames++;
                break; 
            }
        }
    }
}

uintptr_t pmm_alloc_frame() {
    u32 flags = spinlock_acquire_irqsave(&pmm_lock);

    // O(1) with a stack
    if (stack_top == 0) {
        kprintf("[PMM] CRITICAL: Out of Memory!\n");
        
        spinlock_release_irqrestore(&pmm_lock, flags);
        return 0;
    }

    // Pop from stack
    stack_top--;
    u32 idx = frame_stack[stack_top];
    used_frames++;

    uintptr_t release = index_to_phys(idx);
    
    spinlock_release_irqrestore(&pmm_lock, flags);
    return index_to_phys(idx);
}

void pmm_free_frame(uintptr_t addr) {
    u32 flags = spinlock_acquire_irqsave(&pmm_lock);

    u32 idx = phys_to_index(addr);
    
    if (idx == (u32)-1 || idx >= total_frames) return;

    // Safety check to not overflow the stack
    if (stack_top >= stack_capacity) {
        kprintf("[PMM] Error: Stack overflow on free!\n");
        return;
    }

    // Push to stack
    frame_stack[stack_top++] = idx;
    used_frames--;

    spinlock_release_irqrestore(&pmm_lock, flags);
}