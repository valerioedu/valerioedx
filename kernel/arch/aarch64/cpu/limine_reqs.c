#include <limine.h>

__attribute__((used, section(".requests")))
static volatile LIMINE_BASE_REVISION(3);

__attribute__((used, section(".requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0
};