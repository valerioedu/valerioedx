#include <virtio.h>
#include <kio.h>
#include <io.h>

static u64 virtio_blk_base = 0;

void virtio_init() {
    kprintf("[ [CVirtIO [W] Scanning for devices...\n");

    // Scan the 32 possible VirtIO slots
    for (u64 addr = VIRTIO_MMIO_BASE; addr < VIRTIO_MMIO_END; addr += VIRTIO_MMIO_INTERVAL) {
        
        u32 magic = mmio_read32(addr + VIRTIO_MMIO_MAGIC_VALUE);
        
        if (magic != VIRTIO_MAGIC) {
            continue;
        }

        u32 device_id = mmio_read32(addr + VIRTIO_MMIO_DEVICE_ID);
        u32 vendor_id = mmio_read32(addr + VIRTIO_MMIO_VENDOR_ID);

        if (device_id == 0) continue; // Placeholder

        kprintf("[ [CVirtIO [W] Found Device at 0x%llx", addr);

        if (device_id == VIRTIO_DEV_BLOCK) {
            virtio_blk_base = addr;
            kprintf("| ID: %d | Vendor: 0x%x | Device: Block Device\n", device_id, vendor_id);
            // TODO: Initialize the specific driver here
        }
    }

    if (virtio_blk_base == 0) {
        kprintf("[VirtIO] Warning: No Block Device found.\n");
    }
}