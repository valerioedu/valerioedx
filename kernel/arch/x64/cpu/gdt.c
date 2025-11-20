#include <gdt.h>

//TODO: Build TSS (Task State Segment)

struct gdt_entry gdt64[5];
struct gdt_ptr gdt64_pointer;

void set_gdt_entry(int index, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt64[index].base_low    = (base & 0xFFFF);
    gdt64[index].base_middle = (base >> 16) & 0xFF;
    gdt64[index].base_high   = (base >> 24) & 0xFF;

    gdt64[index].limit_low   = (limit & 0xFFFF);
    gdt64[index].granularity = (limit >> 16) & 0x0F;

    gdt64[index].granularity |= gran & 0xF0;
    gdt64[index].access      = access;
}

void setup_gdt() {
    // 1. Null Descriptor
    set_gdt_entry(0, 0, 0, 0, 0);

    // 2. Kernel Code (Ring 0)
    // Access: 0x9A (Present, Ring 0, Code, Exec/Read)
    // Gran:   0xAF (Long Mode)
    set_gdt_entry(1, 0, 0xFFFFFFFF, 0x9A, 0xAF);

    // 3. Kernel Data (Ring 0)
    // Access: 0x92 (Present, Ring 0, Data, RW)
    // Gran:   0xCF 
    set_gdt_entry(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    // 4. User Data (Ring 3) - Index 3
    // Access: 0xF2 
    //   0xF... -> Present, Ring 3 (11)
    //   ...2   -> Data, Read/Write
    set_gdt_entry(3, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    // 5. User Code (Ring 3) - Index 4
    // Access: 0xFA
    //   0xF... -> Present, Ring 3 (11)
    //   ...A   -> Code, Exec/Read
    // Gran:   0xAF (Long Mode bit set)
    set_gdt_entry(4, 0, 0xFFFFFFFF, 0xFA, 0xAF);

    // Setup Pointer
    gdt64_pointer.limit = sizeof(gdt64) - 1;
    gdt64_pointer.base  = (uint32_t)&gdt64;
}