.section .text.boot
.global _start

_start:
    ldr x1, =dtb_phys_addr
    str x0, [x1]

    ldr x0, =_stack_top
    mov sp, x0

    ldr x1, =__bss_start
    ldr x2, =__bss_end
    sub x2, x2, x1
    cbz x2, 2f
    mov x3, #0
1:
    str x3, [x1], #8
    sub x2, x2, #8
    cbnz x2, 1b
2:
    bl main
hang:
    wfe
    b hang

.section .bss
.align 3
.global dtb_phys_addr
dtb_phys_addr:
    .quad 0