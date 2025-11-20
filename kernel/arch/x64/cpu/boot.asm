[BITS 32]

MBALIGN  equ  1 << 0
MEMINFO  equ  1 << 1
FLAGS    equ  MBALIGN | MEMINFO
MAGIC    equ  0x1BADB002
CHECKSUM equ -(MAGIC + FLAGS)

; Multiboot header - must be in first 8KB of kernel
section .multiboot
align 4
multiboot_header:
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

; Bootstrap stack
section .bss
align 16
stack_bottom:
    resb 16384  ; 16 KiB stack
stack_top:

global mboot_ptr_addr
mboot_ptr_addr: resd 1 ; 32-bit double word

section .text

extern stage1

extern pml4_physical_addr

extern gdt64_pointer

global _start
_start:
    mov esp, stack_top

    ; Stores Multiboot pointer
    mov [mboot_ptr_addr], ebx
    push ebx                ; Multiboot info pointer (arg 2)
    push eax                ; Multiboot magic number (arg 1)

    ; Call stage 1 function
    call stage1
    
    cli

    mov eax, [pml4_physical_addr]
    mov cr3, eax

    ; Enables PAE (Physical Address Extension) with bit 5 = 1 because of long mode
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; Enables Long Mode
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; Enables paging
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    lgdt [gdt64_pointer]
    jmp 0x08:long_mode_start

[BITS 64]
long_mode_start:
    mov rsp, stack_top

    hlt
.loop:
    jmp .loop