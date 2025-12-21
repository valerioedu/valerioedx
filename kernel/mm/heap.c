#include <heap.h>
#include <string.h>
#include <kio.h>
#include <spinlock.h>

//TODO: Implement a slab allocator

#define HEAP_MAGIC 0xCAFEBABE
#define MIN_ALLOC  8

typedef struct heap_block {
    struct heap_block* next;
    struct heap_block* prev;
    size_t size;     // Size of the data region
    bool is_free;
    u32 magic;
} heap_block_t;

static heap_block_t* heap_head = NULL;

static spinlock_t heap_lock = 0;

// Helper: Align a number up to nearest multiple of 8 (64-bit safe)
static inline size_t align_up(size_t n) {
    return (n + 7) & ~7;
}

// Merges argument block with its next neighbor if it's free
static void coalesce(heap_block_t* block) {
    if (!block) return;

    // Merges Right (Next)
    if (block->next && block->next->is_free) {
        heap_block_t* next_block = block->next;
        
        block->size += sizeof(heap_block_t) + next_block->size;
        
        block->next = next_block->next;
        if (block->next) {
            block->next->prev = block;
        }
    }

    // Merges Left (prev)
    if (block->prev && block->prev->is_free) {
        heap_block_t* prev_block = block->prev;
        
        prev_block->size += sizeof(heap_block_t) + block->size;
        
        prev_block->next = block->next;
        if (prev_block->next) {
            prev_block->next->prev = prev_block;
        }
    }
}

void heap_init(uintptr_t start, size_t size) {
    uintptr_t aligned_start = align_up(start);
    size_t adjustment = aligned_start - start;
    
    if (size < adjustment + sizeof(heap_block_t) + MIN_ALLOC) {
        kprintf("[ [CHEAP [W] Error: Heap too small\n");
        return;
    }
    size -= adjustment;

    heap_head = (heap_block_t*)aligned_start;
    heap_head->size = size - sizeof(heap_block_t);
    heap_head->is_free = true;
    heap_head->next = NULL;
    heap_head->prev = NULL;
    heap_head->magic = HEAP_MAGIC;

    kprintf("[ [CHEAP [W] Initialized: 0x%llx - 0x%llx (%d MB)\n", 
            aligned_start, aligned_start + size, size / 1024 / 1024);
}

void* kmalloc(size_t size) {
    if (size == 0) return NULL;

    u32 flags = spinlock_acquire_irqsave(&heap_lock);

    size = align_up(size); // Force 8-byte alignment

    heap_block_t* curr = heap_head;
    while (curr) {
        if (curr->magic != HEAP_MAGIC) {
            kprintf("[ [CHEAP [W] Corruption detected at 0x%llx!\n", curr);

            spinlock_release_irqrestore(&heap_lock, flags);
            return NULL;
        }

        if (curr->is_free && curr->size >= size) {
            // Fit found
            
            // Needs enough space for: Request + New Header + Min Data
            if (curr->size >= size + sizeof(heap_block_t) + MIN_ALLOC) {
                
                // Create the new block at offset
                heap_block_t* new_block = (heap_block_t*)((uintptr_t)curr + sizeof(heap_block_t) + size);
                
                new_block->magic = HEAP_MAGIC;
                new_block->is_free = true;
                new_block->size = curr->size - size - sizeof(heap_block_t);
                
                new_block->next = curr->next;
                new_block->prev = curr;
                
                if (new_block->next) {
                    new_block->next->prev = new_block;
                }

                // Update current block
                curr->next = new_block;
                curr->size = size;
            }

            curr->is_free = false;
            
            spinlock_release_irqrestore(&heap_lock, flags);

            return (void*)((uintptr_t)curr + sizeof(heap_block_t));
        }
        curr = curr->next;
    }

    spinlock_release_irqrestore(&heap_lock, flags);
    
    kprintf("[ [CHEAP [W] Out of Memory (Requested %d bytes)\n", size);
    return NULL;
}

void kfree(void* ptr) {
    if (!ptr) return;

    u32 flags = spinlock_acquire_irqsave(&heap_lock);

    heap_block_t* block = (heap_block_t*)((uintptr_t)ptr - sizeof(heap_block_t));

    if (block->magic != HEAP_MAGIC) {
        kprintf("[ [CHEAP [W] Double free or corruption at 0x%llx\n", block);
        
        spinlock_release_irqrestore(&heap_lock, flags);
        return;
    }
    
    block->is_free = true;
    
    // Coalesce with neighbors to reduce fragmentation
    coalesce(block);

    spinlock_release_irqrestore(&heap_lock, flags);
}

void* kcalloc(size_t num, size_t size) {
    size_t total = num * size;
    void* ptr = kmalloc(total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

void* krealloc(void* ptr, size_t new_size) {
    if (!ptr) return kmalloc(new_size);
    if (new_size == 0) {
        kfree(ptr);
        return NULL;
    }

    heap_block_t* block = (heap_block_t*)((uintptr_t)ptr - sizeof(heap_block_t));
    if (block->size >= new_size) return ptr; // Already big enough

    void* new_ptr = kmalloc(new_size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, block->size);
        kfree(ptr);
    }
    return new_ptr;
}

void heap_debug() {
    u32 flags = spinlock_acquire_irqsave(&heap_lock);
    
    heap_block_t* curr = heap_head;
    kprintf("--- Heap Status ---\n");
    while (curr) {
        if (curr->is_free) kprintf("[ [GFREE[W ] Size: %d, Addr: 0x%llx\n", curr->size, curr);
        else kprintf("[ [BUSED[W ] Size: %d, Addr: 0x%llx\n", curr->size, curr);
        curr = curr->next;
    }
    kprintf("-------------------\n");
    
    spinlock_release_irqrestore(&heap_lock, flags);
}