.section .text.boot
.global _start

_start:
    ldr x1, =dtb_phys_addr
    str x0, [x1]

#   Stack setup
    ldr x0, =_stack_top
    mov sp, x0

#   EL1 vector base
    ldr x0, =el1_vector_table
    msr VBAR_EL1, x0
    isb

#   Sets MMU off, will be implemented later
    mrs x0, SCTLR_EL1
    bic x0, x0, #(1 << 0)
    msr SCTLR_EL1, x0
    isb

#   Clears BSS
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

.extern el1_sync_handler
.extern el1_irq_handler
.extern el1_fiq_handler
.extern el1_serr_handler

.align 11
.global el1_vector_table

el1_vector_table:
    b el1_sync_handler
    b el1_irq_handler
    b el1_fiq_handler
    b el1_serr_handler

    .org el1_vector_table + 0x80
    b el1_sync_handler
    b el1_irq_handler
    b el1_fiq_handler
    b el1_serr_handler

#   EL0_64 starts at 0x100 offset
#   EL0_32 starts at 0x180 offset
