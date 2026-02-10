#include <pl031.h>
#include <dtb.h>
#include <io.h>
#include <vmm.h>
#include <kio.h>
#include <io.h>

extern u64 *pl031;

u32 pl031_get_time() {
    if (!pl031) return 0;
    
    return mmio_read32((uintptr_t)pl031 + PL031_DR);
}

void pl031_set_time(u32 timestamp) {
    if (!pl031) return;

    mmio_write32((uintptr_t)pl031 + PL031_LR, timestamp);
}