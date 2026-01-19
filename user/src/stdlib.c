#include <sys/mman.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define ALIGNMENT 16
#define PAGE_SIZE 4096

#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))
#define BLOCK_HEADER_SIZE sizeof(block_meta_t)

typedef struct block_meta {
    size_t size;
    struct block_meta *next;
    struct block_meta *prev;
    int free;
    int magic;
} block_meta_t;

#define MAGIC 0xDEADBEEF

static block_meta_t *global_base = NULL;

static block_meta_t *request_space(block_meta_t *last, size_t size) {
    block_meta_t *block;
    size_t alloc_size = size + BLOCK_HEADER_SIZE;
    
    // Round up to nearest page multiple
    size_t num_pages = (alloc_size + PAGE_SIZE - 1) / PAGE_SIZE;
    alloc_size = num_pages * PAGE_SIZE;

    block = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, 
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    if (block == MAP_FAILED)
        return NULL;

    if (last) last->next = block;

    block->size = alloc_size - BLOCK_HEADER_SIZE;
    block->next = NULL;
    block->prev = last;
    block->free = 1;
    block->magic = MAGIC;

    return block;
}

static block_meta_t *find_free_block(block_meta_t **last, size_t size) {
    block_meta_t *current = global_base;
    while (current && !(current->free && current->size >= size)) {
        *last = current;
        current = current->next;
    }

    return current;
}

static void split_block(block_meta_t *block, size_t size) {
    // Only split if the remaining space fits a header + some data (alignment size)
    if (block->size >= size + BLOCK_HEADER_SIZE + ALIGNMENT) {
        block_meta_t *new_block = (block_meta_t *)((char *)block + BLOCK_HEADER_SIZE + size);
        
        new_block->size = block->size - size - BLOCK_HEADER_SIZE;
        new_block->next = block->next;
        new_block->prev = block;
        new_block->free = 1;
        new_block->magic = MAGIC;
        
        if (new_block->next)
            new_block->next->prev = new_block;
        
        block->size = size;
        block->next = new_block;
    }
}

void *malloc(size_t size) {
    if (size <= 0) return NULL;

    size_t aligned_size = ALIGN(size);
    block_meta_t *block;

    if (!global_base) {
        block = request_space(NULL, aligned_size);
        if (!block) return NULL;
        global_base = block;
    } else {
        block_meta_t *last = global_base;
        block = find_free_block(&last, aligned_size);
        
        if (!block) {
            block = request_space(last, aligned_size);
            if (!block) return NULL;
        } else {
            split_block(block, aligned_size);
            block->free = 0;
        }
    }
    
    if (block->free) {
        split_block(block, aligned_size);
        block->free = 0;
    }

    return (void *)(block + 1);
}

void free(void *ptr) {
    if (!ptr) return;

    block_meta_t *block = (block_meta_t *)ptr - 1;
    
    // Pointer invalid or corruption detected
    if (block->magic != MAGIC) return;

    block->free = 1;

    // Coalesce with next block if it is free and physically adjacent
    if (block->next && block->next->free) {
        if ((char *)block + BLOCK_HEADER_SIZE + block->size == (char *)block->next) {
            block->size += BLOCK_HEADER_SIZE + block->next->size;
            block->next = block->next->next;
            if (block->next)
                block->next->prev = block;
        }
    }

    // Coalesce with previous block if it is free and physically adjacent
    if (block->prev && block->prev->free) {
        if ((char *)block->prev + BLOCK_HEADER_SIZE + block->prev->size == (char *)block) {
            block->prev->size += BLOCK_HEADER_SIZE + block->size;
            block->prev->next = block->next;
            if (block->next)
                block->next->prev = block->prev;
        }
    }
}

void *calloc(size_t nelem, size_t elsize) {
    size_t size = nelem * elsize;
    void *ptr = malloc(size);
    if (ptr) memset(ptr, 0, size);
    return ptr;
}

void *realloc(void *ptr, size_t size) {
    if (!ptr) return malloc(size);
    if (size == 0) {
        free(ptr);
        return NULL;
    }

    block_meta_t *block = (block_meta_t *)ptr - 1;
    if (block->size >= size)
        return ptr;

    void *new_ptr = malloc(size);
    if (!new_ptr) return NULL;

    memcpy(new_ptr, ptr, block->size);
    free(ptr);
    return new_ptr;
}

int atoi(const char *str) {
    if (!str) return 0;
    
    while (*str == ' ' || *str == '\t' ||
        *str == '\n' || *str == '\v' || 
        *str == '\f' || *str == '\r')
        str++;

    int sign = 1;
    if (*str == '+' || *str == '-') {
        if (*str == '-') sign = -1;
        str++;
    }

    long result = 0;
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }

    return (int)(result * sign);
}