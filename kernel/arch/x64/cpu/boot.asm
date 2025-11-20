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
    
    ; stage1() should NEVER return if it successfully entered long mode.
    ; If it returns, just halt.
.hang:
    cli
    hlt
    jmp .hang

; -------------------------------
; Long mode entry helper
; -------------------------------
global enter_long_mode
enter_long_mode:
    ; Argument 1 (entry_point) is in [esp + 4] (cdecl 32-bit)
    mov ecx, [esp + 4]
    mov [kernel_entry], ecx

    cli

    ; Load PML4
    mov eax, [pml4_physical_addr]
    mov cr3, eax

    ; Enable PAE
    mov eax, cr4
    or  eax, 1 << 5
    mov cr4, eax

    ; Enable Long Mode in EFER
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8          ; LME
    wrmsr

    ; Enable paging
    mov eax, cr0
    or  eax, 1 << 31        ; PG
    mov cr0, eax

    ; Load 64-bit GDT and far jump to 64-bit code
    lgdt [gdt64_pointer]
    jmp 0x08:long_mode_start

section .data
kernel_entry: dd 0

[BITS 64]
extern load_idt64

long_mode_start:
    ; Set up a 64-bit stack (reuse same physical stack; identity-mapped)
    mov rsp, stack_top

    ; DEBUG: indicate we've reached long_mode_start via QEMU debugcon
    mov al, 'L'
    mov dx, 0xE9
    out dx, al

    ; Load a minimal 64-bit IDT (avoids triple faults)
    call load_idt64

    ; Clear data segment registers
    xor eax, eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Retrieve the 32-bit entry point (zero-extends to RAX in long mode)
    mov eax, [kernel_entry]

    ; Jump to the 64-bit kernel entry
    call rax

.halt:
    hlt
    jmp .halt