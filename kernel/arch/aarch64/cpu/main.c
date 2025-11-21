#include <lib.h>
#include <string.h>

/* QEMU virt Machine Hardware Constants */
#define FW_CFG_BASE             0x09020000
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

static volatile u64* fw_cfg_dma_addr = (u64*)(FW_CFG_BASE + FW_CFG_DMA_OFF);

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

#define WIDTH  1366
#define HEIGHT 768

/* 1366*768*4 = 4,196,000 bytes (~4.2MB) */
u32 fb_buffer[WIDTH * HEIGHT]; 

void ramfb_init() {
    u16 select = find_ramfb_file();
    if (select == 0) return; 

    RamfbCfg cfg;
    cfg.addr = bswap64((u64)fb_buffer);
    
    /* XR24 (XRGB8888) is the most standard format for QEMU. */
    /* 'X','R','2','4' -> 0x34325258 (Little Endian) */
    cfg.fourcc = bswap32(0x34325258); 
    
    cfg.flags = bswap32(0);
    cfg.width = bswap32(WIDTH);
    cfg.height = bswap32(HEIGHT);
    cfg.stride = bswap32(WIDTH * 4);

    fw_cfg_dma(FW_CFG_DMA_CTL_WRITE | FW_CFG_DMA_CTL_SELECT | (select << 16), &cfg, sizeof(cfg));
}

/* --- 6. Main --- */
void kernel_main(void) {
    ramfb_init();

    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            uint8_t r = x * 255 / WIDTH;
            uint8_t g = y * 255 / HEIGHT;
            uint8_t b = 128;
                
            /* XRGB format */
            fb_buffer[y * WIDTH + x] = (0xFF << 24) | (r << 16) | (g << 8) | b;
        }    
    }    

    while(1);
}

void main() {
    volatile char* uart = (char*)0x09000000;
    const char* str = "Hello, World!\n";
    while (*str) *uart = *str++;
    
    kernel_main();
    
    while (1);
}