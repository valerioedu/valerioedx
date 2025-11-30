#ifndef HEAP_H
#define HEAP_H

#include <lib.h>

void heap_init(uintptr_t start, size_t size);
void* kmalloc(size_t size);
void* kcalloc(size_t num, size_t size);
void* krealloc(void* ptr, size_t new_size);
void kfree(void* ptr);

// Debugging
void heap_debug();

#endif