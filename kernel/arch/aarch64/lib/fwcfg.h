#ifndef FWCFG_H
#define FWCFG_H

#include <lib.h>

/* QEMU virt Machine Hardware Constants */
#define FW_CFG_CTL_OFF          0x08
#define FW_CFG_DATA_OFF         0x00
#define FW_CFG_DMA_OFF          0x10

#define FW_CFG_SIGNATURE        0x00
#define FW_CFG_ID               0x01
#define FW_CFG_FILE_DIR         0x19

#define FW_CFG_DMA_CTL_ERROR    0x01
#define FW_CFG_DMA_CTL_READ     0x02
#define FW_CFG_DMA_CTL_SKIP     0x04
#define FW_CFG_DMA_CTL_SELECT   0x08
#define FW_CFG_DMA_CTL_WRITE    0x10

typedef struct {
    u32 size;
    u16 select;
    u16 reserved;
    char name[56];
} __attribute__((packed)) FWCfgFile;

typedef struct {
    u32 control;
    u32 length;
    u64 address;
} __attribute__((packed)) FWCfgDmaAccess;

typedef struct {
    u64 addr;
    u32 fourcc;
    u32 flags;
    u32 width;
    u32 height;
    u32 stride;
} __attribute__((packed)) RamfbCfg;

static inline u16 bswap16(u16 x) {
    return (x >> 8) | (x << 8);
}

static inline u32 bswap32(u32 x) {
    return __builtin_bswap32(x); // GCC builtin is safer/faster
}

static inline u64 bswap64(u64 x) {
    return __builtin_bswap64(x);
}

void fw_cfg_init(u64 base_addr);
u16 find_ramfb_file();
void fw_cfg_dma(u32 control, void* data, u32 len);

#endif