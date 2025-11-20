[BITS 32]

MBALIGN  equ  1 << 0
MEMINFO  equ  1 << 1
VIDEO    equ  1 << 2
FLAGS    equ  MBALIGN | MEMINFO | VIDEO
MAGIC    equ  0x1BADB002
CHECKSUM equ -(MAGIC + FLAGS)

section .multiboot
align 4
multiboot_header:
    dd MAGIC
    dd FLAGS
    dd CHECKSUM
    
    ; Video Request
    dd 0    ; Linear Graphics
    dd 1024 ; Width
    dd 768  ; Height
    dd 32   ; Depth

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:

section .text
extern stage1
global _start

_start:
    mov esp, stack_top

    push ebx ; Multiboot info
    push eax ; Magic

    call stage1

    cli
    hlt
.hang:
    jmp .hang