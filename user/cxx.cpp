#include <stdlib.h>

//Implement exceptions

namespace std {
    enum class align_val_t : size_t {};
}

void *operator new(size_t size) {
    return malloc(size);
}

void* operator new(size_t, void* p) noexcept {
    return p;
}

void *operator new[](size_t size) {
    return malloc(size);
}

void *operator new(size_t size, std::align_val_t alignment) {
    void *ptr = nullptr;
    if (posix_memalign(&ptr, static_cast<size_t>(alignment), size) != 0) {
        return NULL;
    }

    return ptr;
}

void* operator new[](size_t, void* p) noexcept {
    return p;
}

void *operator new[](size_t size, std::align_val_t alignment) {
    void *ptr = nullptr;
    if (posix_memalign(&ptr, static_cast<size_t>(alignment), size) != 0) {
        return NULL;
    }

    return ptr;
}

void operator delete(void *ptr) noexcept {
    free(ptr);
}

void operator delete(void *ptr, size_t size) noexcept {
    free(ptr);
}

void operator delete(void *ptr, std::align_val_t) noexcept {
    free(ptr);
}

void operator delete(void *ptr, size_t size, std::align_val_t) noexcept {
    free(ptr);
}

inline void  operator delete(void*, void*) noexcept { }

void operator delete[](void *ptr) noexcept {
    free(ptr);
}

void operator delete[](void *ptr, size_t size) noexcept {
    free(ptr);
}

void operator delete[](void *ptr, std::align_val_t) noexcept {
    free(ptr);
}

void operator delete[](void *ptr, size_t size, std::align_val_t) noexcept {
    free(ptr);
}

inline void  operator delete[](void*, void*) noexcept { }

extern "C" void __cxa_pure_virtual() {
    exit(1); 
}