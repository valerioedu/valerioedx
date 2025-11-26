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

#   Enables FP/SIMD
    mrs x0, CPACR_EL1
    orr x0, x0, #(3 << 20)
    msr CPACR_EL1, x0
    isb

#   Sets MMU off, will be implemented later
    mrs x0, SCTLR_EL1
    bic x0, x0, #(1 << 0)
    msr SCTLR_EL1, x0
    isb

#   Unmask interrupts (DAIF) to allow IRQs to be handled
    msr daifclr, #2

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

.macro SAVE_CONTEXT
    sub sp, sp, #256
    stp x0, x1, [sp, #16 * 0]
    stp x2, x3, [sp, #16 * 1]
    stp x4, x5, [sp, #16 * 2]
    stp x6, x7, [sp, #16 * 3]
    stp x8, x9, [sp, #16 * 4]
    stp x10, x11, [sp, #16 * 5]
    stp x12, x13, [sp, #16 * 6]
    stp x14, x15, [sp, #16 * 7]
    stp x16, x17, [sp, #16 * 8]
    stp x18, x19, [sp, #16 * 9]
    stp x20, x21, [sp, #16 * 10]
    stp x22, x23, [sp, #16 * 11]
    stp x24, x25, [sp, #16 * 12]
    stp x26, x27, [sp, #16 * 13]
    stp x28, x29, [sp, #16 * 14]
    str x30, [sp, #16 * 15] 
.endm

.macro RESTORE_CONTEXT
    ldp x0, x1, [sp, #16 * 0]
    ldp x2, x3, [sp, #16 * 1]
    ldp x4, x5, [sp, #16 * 2]
    ldp x6, x7, [sp, #16 * 3]
    ldp x8, x9, [sp, #16 * 4]
    ldp x10, x11, [sp, #16 * 5]
    ldp x12, x13, [sp, #16 * 6]
    ldp x14, x15, [sp, #16 * 7]
    ldp x16, x17, [sp, #16 * 8]
    ldp x18, x19, [sp, #16 * 9]
    ldp x20, x21, [sp, #16 * 10]
    ldp x22, x23, [sp, #16 * 11]
    ldp x24, x25, [sp, #16 * 12]
    ldp x26, x27, [sp, #16 * 13]
    ldp x28, x29, [sp, #16 * 14]
    ldr x30, [sp, #16 * 15]
    add sp, sp, #256
    eret
.endm

.section .data
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

.global irq_entry
irq_entry:
    SAVE_CONTEXT
    bl el1_irq_handler
    RESTORE_CONTEXT

.global sync_entry
sync_entry:
    SAVE_CONTEXT 
    bl el1_sync_handler
    RESTORE_CONTEXT
    
.align 11
.global el1_vector_table
el1_vector_table:
    .align 7
    b sync_entry
    .align 7
    b irq_entry
    .align 7
    b el1_fiq_handler
    .align 7
    b el1_serr_handler
    
    .align 7
    b sync_entry
    .align 7
    b irq_entry
    .align 7
    b el1_fiq_handler
    .align 7
    b el1_serr_handler
    
    .align 7
    b sync_entry
    .align 7
    b irq_entry
    .align 7
    b el1_fiq_handler
    .align 7
    b el1_serr_handler
    
    .align 7
    b sync_entry
    .align 7
    b irq_entry
    .align 7
    b el1_fiq_handler
    .align 7
    b el1_serr_handler
