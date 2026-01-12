#ifndef DTB_H
#define DTB_H

#include <lib.h>

#define FDT_MAGIC       0xD00DFEED
#define FDT_BEGIN_NODE  0x00000001
#define FDT_END_NODE    0x00000002
#define FDT_PROP        0x00000003
#define FDT_NOP         0x00000004
#define FDT_END         0x00000009

typedef struct {
    u32 magic;
    u32 totalsize;
    u32 off_dt_struct;
    u32 off_dt_strings;
    u32 off_mem_rsvmap;
    u32 version;
    u32 last_comp_version;
    u32 boot_cpuid_phys;
    u32 size_dt_strings;
    u32 size_dt_struct;
} fdt_header_t;

void dtb_init(uintptr_t dtb_addr);
int dtb_get_property(const char* node_name, const char* prop_name, void* out_buf, u32 buf_len);
u64 dtb_get_reg(const char* node_name);
u64 dtb_get_memory_size();
int dtb_get_model(char* buf, u32 buf_len);
int dtb_get_compatible(char* buf, u32 buf_len);
int dtb_get_cpu_count();
u64 dtb_get_cpu_freq();

#endif