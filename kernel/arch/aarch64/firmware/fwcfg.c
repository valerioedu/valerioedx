#include <fwcfg.h>
#include <string.h>

static volatile u64* fw_cfg_dma_addr = NULL;

void fw_cfg_init(u64 base_addr) {
    fw_cfg_dma_addr = (u64*)(base_addr + FW_CFG_DMA_OFF);
}

void fw_cfg_dma(u32 control, void* data, u32 len) {
    volatile FWCfgDmaAccess access __attribute__((aligned(16)));
    
    access.control = bswap32(control);
    access.length = bswap32(len);
    access.address = bswap64((u64)data);

    /* Memory Barrier: Ensures 'access' is written to RAM before triggering DMA */
    asm volatile("dmb sy"); 

    /* Writes physical address of the struct to the device */
    *fw_cfg_dma_addr = bswap64((u64)&access);

    /* Wait for completion */
    while (bswap32(access.control) != 0)
        asm volatile("nop");
}

u16 find_ramfb_file() {
    u32 count;
    fw_cfg_dma(FW_CFG_DMA_CTL_READ | FW_CFG_DMA_CTL_SELECT | (FW_CFG_FILE_DIR << 16), &count, 4);
    count = bswap32(count);

    for (u32 i = 0; i < count; i++) {
        FWCfgFile file;
        fw_cfg_dma(FW_CFG_DMA_CTL_READ, &file, sizeof(FWCfgFile));

        if (strcmp(file.name, "etc/ramfb") == 0)
            return bswap16(file.select);
    }
    return 0;
}