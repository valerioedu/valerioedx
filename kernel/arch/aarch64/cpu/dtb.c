#include <dtb.h>
#include <string.h>
#include <uart.h>

static u8* dtb_base = NULL;
static u8* strings_ptr = NULL;

// Big Endian to Little Endian 32 bit
static inline u32 be2le32(u32 x) {
    return ((x >> 24) & 0xFF) | ((x << 8) & 0xFF0000) | 
           ((x >> 8) & 0xFF00) | ((x << 24) & 0xFF000000);
}

// Big Endian to Little Endian 64 bit
static inline u64 be2le64(u64 x) {
    u32 hi = be2le32(x >> 32);
    u32 lo = be2le32(x & 0xFFFFFFFF);
    return ((u64)lo << 32) | hi;
}

void dtb_init(uintptr_t dtb_addr) {
    fdt_header_t* header = (fdt_header_t*)dtb_addr;
    
    if (be2le32(header->magic) != FDT_MAGIC) {
        kprintf("[DTB] Invalid Magic: 0x%x (Expected 0x%x)\n", 
                be2le32(header->magic), FDT_MAGIC);
        return;
    }

    dtb_base = (u8*)dtb_addr;
    strings_ptr = dtb_base + be2le32(header->off_dt_strings);
    
    kprintf("[DTB] Initialized. Size: %d bytes\n", be2le32(header->totalsize));
}

static u8* skip_node_name(u8* ptr) {
    while (*ptr) ptr++;
    ptr++;
    
    // Align to 4 bytes
    return (u8*)(((uintptr_t)ptr + 3) & ~3);
}

// Finds a property within the CURRENT node scope
// Returns pointer to data, or NULL if end of node reached
static u8* find_prop_in_node(u8** iter, const char* target_prop, u32* out_len) {
    u8* ptr = *iter;

    while (1) {
        u32 token = be2le32(*(u32*)ptr);
        ptr += 4;

        if (token == FDT_PROP) {
            u32 len = be2le32(*(u32*)ptr);
            ptr += 4;
            u32 nameoff = be2le32(*(u32*)ptr);
            ptr += 4;
            
            const char* prop_name = (const char*)(strings_ptr + nameoff);
            u8* data = ptr;

            // Aligns pointer for next iteration
            ptr += len;
            ptr = (u8*)(((uintptr_t)ptr + 3) & ~3);

            if (strcmp(prop_name, target_prop) == 0) {
                *out_len = len;
                *iter = ptr; // Updates iterator state
                return data;
            }
        } 
        else if (token == FDT_NOP) {
            continue;
        }
        else {
            ptr -= 4; 
            *iter = ptr;
            return NULL;
        }
    }
}

// Finds a specific node name (substring match) and return a property
// Example: node_name="pl011", prop_name="reg"
int dtb_get_property(const char* node_name, const char* prop_name, void* out_buf, u32 buf_len) {
    if (!dtb_base) return 0;

    fdt_header_t* header = (fdt_header_t*)dtb_base;
    u8* ptr = dtb_base + be2le32(header->off_dt_struct);

    int depth = 0;

    while (1) {
        u32 token = be2le32(*(u32*)ptr);
        ptr += 4;

        if (token == FDT_END) break;

        if (token == FDT_BEGIN_NODE) {
            const char* name = (const char*)ptr;
            ptr = skip_node_name(ptr);
            
            bool match = false;
            
            // Simple substring check
            const char *a = name;
            const char *b = node_name;
            while (*a) {
                const char *temp_a = a;
                const char *temp_b = b;
                while (*temp_a && *temp_b && *temp_a == *temp_b) {
                    temp_a++; temp_b++;
                }

                // Found a match
                if (!*temp_b) { 
                    match = true;
                    break;
                } 
                a++;
            }

            if (match) {
                u32 len = 0;
                u8* data = find_prop_in_node(&ptr, prop_name, &len);
                if (data) {
                    // Copy result
                    u32 copy_len = (len < buf_len) ? len : buf_len;
                    memcpy(out_buf, data, copy_len);
                    return copy_len;
                }
            }
            depth++;
        }
        else if (token == FDT_END_NODE) {
            depth--;
        }
        else if (token == FDT_PROP) {
            u32 len = be2le32(*(u32*)ptr);
            ptr += 4; // len
            ptr += 4; // nameoff
            ptr += len;
            ptr = (u8*)(((uintptr_t)ptr + 3) & ~3);
        }
    }
    return 0;
}

// Gets the 'reg' property
u64 dtb_get_reg(const char* node_name) {
    // Reg property is usually 2 or 4 cells (Address + Size)
    // For 64-bit: Address (u64) + Size (u64) = 16 bytes
    // Will return the address
    u64 buffer[2]; // Read address and size
    if (dtb_get_property(node_name, "reg", buffer, sizeof(buffer))) {
        return be2le64(buffer[0]); 
    }
    return 0;
}