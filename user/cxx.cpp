#include <stdlib.h>
#include <new>

//Implement exceptions

namespace std {
    enum class align_val_t : size_t {};
}

void *operator new(size_t size) {
    return malloc(size);
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

void *operator new[](size_t size, std::align_val_t alignment) {
    void *ptr = nullptr;
    if (posix_memalign(&ptr, static_cast<size_t>(alignment), size) != 0) {
        return NULL;
    }

    return ptr;
}

void* operator new(size_t size, const std::nothrow_t&) noexcept {
    return malloc(size);
}

void* operator new[](size_t size, const std::nothrow_t&) noexcept {
    return malloc(size);
}

void operator delete(void* ptr, const std::nothrow_t&) noexcept {
    free(ptr);
}

void operator delete[](void* ptr, const std::nothrow_t&) noexcept {
    free(ptr);
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

extern "C" void __cxa_deleted_virtual() {
    exit(1);
}

extern "C" {
    int __cxa_guard_acquire(long *guard_object) {
        if (!*guard_object) {
            return 1;
        }
        return 0;
    }

    void __cxa_guard_release(long *guard_object) {
        *guard_object = 1;
    }

    void __cxa_guard_abort(long *guard_object) { }

    void *__dso_handle = nullptr;

    struct AtExitEntry {
        void (*destructor)(void *);
        void *arg;
        void *dso;
        AtExitEntry *next;
    };

    static AtExitEntry *atexit_funcs = nullptr;

    int __cxa_atexit(void (*f)(void *), void *objptr, void *dso) {
        AtExitEntry *entry = (AtExitEntry *)malloc(sizeof(AtExitEntry));
        if (!entry) {
            return -1;
        }

        entry->destructor = f;
        entry->arg = objptr;
        entry->dso = dso;
        entry->next = atexit_funcs;
        atexit_funcs = entry;
        return 0;
    }

    void __cxa_finalize(void *f) {
        AtExitEntry *current = atexit_funcs;
        AtExitEntry **prev = &atexit_funcs;

        while (current != nullptr) {
            if (f == nullptr || f == current->dso) {
                current->destructor(current->arg);
                *prev = current->next;
                AtExitEntry *to_delete = current;
                current = current->next;
                free(to_delete);
            } else {
                prev = &current->next;
                current = current->next;
            }
        }
    }
}