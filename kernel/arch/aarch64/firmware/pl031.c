#include <pl031.h>
#include <dtb.h>
#include <io.h>
#include <vmm.h>
#include <kio.h>
#include <io.h>

extern u64 *pl031;
extern u64 boot_time;

static u64 boot_epoch = 0;

void pl031_init_time() {
    if (pl031) boot_epoch = mmio_read32((uintptr_t)pl031 + PL031_DR);
}

u32 pl031_get_time() {
    if (!pl031) return 0;
    
    return mmio_read32((uintptr_t)pl031 + PL031_DR);
}

void pl031_set_time(u32 timestamp) {
    if (!pl031) return;

    mmio_write32((uintptr_t)pl031 + PL031_LR, timestamp);
}

void get_monotonic_time(u64 *sec, u64 *nsec) {
    u64 now, frq;
    asm volatile("mrs %0, cntpct_el0" : "=r"(now));
    asm volatile("mrs %0, cntfrq_el0" : "=r"(frq));

    u64 elapsed = now - boot_time;
    *sec  = elapsed / frq;
    *nsec = ((elapsed % frq) * 1000000000ULL) / frq;
}

void get_realtime(u64 *sec, u64 *nsec) {
    u64 mono_sec, mono_nsec;
    get_monotonic_time(&mono_sec, &mono_nsec);
    *sec  = boot_epoch + mono_sec;
    *nsec = mono_nsec;
}