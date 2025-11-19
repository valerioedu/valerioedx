[BITS 32]

; Multiboot header constants
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
global _start
_start:
    ; Set up stack
    mov esp, stack_top

    ; Store Multiboot pointer
    mov [mboot_ptr_addr], ebx
    push ebx                ; Multiboot info pointer (arg 2)
    push eax                ; Multiboot magic number (arg 1)

    ; Call stage 1 function
    call stage1
    
.hang:
    cli
    hlt
    jmp .hang