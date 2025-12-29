#ifndef ELF_H
#define ELF_H

#include <lib.h>
#include <vma.h>

#define ELF_MAGIC 0x464C457F  // "\x7FELF" in little endian

// ELF Class (32-bit or 64-bit)
#define ELFCLASS32 1
#define ELFCLASS64 2

// ELF Data Encoding
#define ELFDATA2LSB 1  // Little Endian
#define ELFDATA2MSB 2  // Big Endian

// ELF Type
#define ET_NONE   0  // No file type
#define ET_REL    1  // Relocatable file
#define ET_EXEC   2  // Executable file
#define ET_DYN    3  // Shared object file
#define ET_CORE   4  // Core file

// ELF Machine (Architecture)
#define EM_AARCH64 183  // ARM 64-bit

// Program Header Types
#define PT_NULL    0  // Unused
#define PT_LOAD    1  // Loadable segment
#define PT_DYNAMIC 2  // Dynamic linking info
#define PT_INTERP  3  // Interpreter path
#define PT_NOTE    4  // Auxiliary info
#define PT_SHLIB   5  // Reserved
#define PT_PHDR    6  // Program header table
#define PT_TLS     7  // Thread-local storage

// Program Header Flags
#define PF_X 0x1  // Execute
#define PF_W 0x2  // Write
#define PF_R 0x4  // Read

// Section Header Types
#define SHT_NULL     0   // Inactive
#define SHT_PROGBITS 1   // Program data
#define SHT_SYMTAB   2   // Symbol table
#define SHT_STRTAB   3   // String table
#define SHT_RELA     4   // Relocation entries with addends
#define SHT_HASH     5   // Symbol hash table
#define SHT_DYNAMIC  6   // Dynamic linking info
#define SHT_NOTE     7   // Notes
#define SHT_NOBITS   8   // BSS (no file data)
#define SHT_REL      9   // Relocation entries
#define SHT_DYNSYM   11  // Dynamic symbol table

// ELF64 Header
typedef struct {
    u8  e_ident[16];    // Magic number and other info
    u16 e_type;         // Object file type
    u16 e_machine;      // Architecture
    u32 e_version;      // Object file version
    u64 e_entry;        // Entry point virtual address
    u64 e_phoff;        // Program header table file offset
    u64 e_shoff;        // Section header table file offset
    u32 e_flags;        // Processor-specific flags
    u16 e_ehsize;       // ELF header size
    u16 e_phentsize;    // Program header table entry size
    u16 e_phnum;        // Program header table entry count
    u16 e_shentsize;    // Section header table entry size
    u16 e_shnum;        // Section header table entry count
    u16 e_shstrndx;     // Section name string table index
} __attribute__((packed)) elf64_ehdr_t;

// ELF64 Program Header
typedef struct {
    u32 p_type;         // Segment type
    u32 p_flags;        // Segment flags
    u64 p_offset;       // Segment file offset
    u64 p_vaddr;        // Segment virtual address
    u64 p_paddr;        // Segment physical address (unused)
    u64 p_filesz;       // Segment size in file
    u64 p_memsz;        // Segment size in memory
    u64 p_align;        // Segment alignment
} __attribute__((packed)) elf64_phdr_t;

// ELF64 Section Header
typedef struct {
    u32 sh_name;        // Section name (string table index)
    u32 sh_type;        // Section type
    u64 sh_flags;       // Section flags
    u64 sh_addr;        // Section virtual address
    u64 sh_offset;      // Section file offset
    u64 sh_size;        // Section size
    u32 sh_link;        // Link to another section
    u32 sh_info;        // Additional section info
    u64 sh_addralign;   // Section alignment
    u64 sh_entsize;     // Entry size (for tables)
} __attribute__((packed)) elf64_shdr_t;

// Auxiliary vector types (for passing info to user space)
#define AT_NULL   0   // End of vector
#define AT_PHDR   3   // Program headers address
#define AT_PHENT  4   // Size of program header entry
#define AT_PHNUM  5   // Number of program headers
#define AT_PAGESZ 6   // System page size
#define AT_BASE   7   // Base address of interpreter
#define AT_FLAGS  8   // Flags
#define AT_ENTRY  9   // Program entry point
#define AT_UID    11  // Real user ID
#define AT_EUID   12  // Effective user ID
#define AT_GID    13  // Real group ID
#define AT_EGID   14  // Effective group ID

// Auxiliary vector entry
typedef struct {
    u64 a_type;
    u64 a_val;
} auxv_t;

// ELF loading result
typedef struct {
    u64 entry_point;    // Entry point address
    u64 phdr_addr;      // Address of program headers in memory
    u16 phdr_count;     // Number of program headers
    u64 base_addr;      // Base load address (for PIE)
    u64 brk;            // Initial program break (heap start)
} elf_load_result_t;

int elf_validate(const u8* data, size_t size);
int elf_load(mm_struct_t* mm, const u8* data, size_t size, elf_load_result_t* result);
int elf_load_from_file(mm_struct_t* mm, struct vfs_node* file, elf_load_result_t* result);

#endif