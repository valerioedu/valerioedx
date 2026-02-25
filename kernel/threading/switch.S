.global cpu_switch_to
.global ret_from_fork
.global ret_from_fork_child
.global enter_usermode
.extern sched_unlock_release
.extern switch_to_child_mm

# void cpu_switch_to(struct task* prev, struct task* next);
# x0 = pointer to 'prev' task struct
# x1 = pointer to 'next' task struct
cpu_switch_to:
    stp x19, x20, [x0, #0]
    stp x21, x22, [x0, #16]
    stp x23, x24, [x0, #32]
    stp x25, x26, [x0, #48]
    stp x27, x28, [x0, #64]
    stp x29, x30, [x0, #80]

    mov x9, sp
    str x9, [x0, #96]

    stp q8,  q9,  [x0, #112]
    stp q10, q11, [x0, #144]
    stp q12, q13, [x0, #176]
    stp q14, q15, [x0, #208]

    ldp x19, x20, [x1, #0]
    ldp x21, x22, [x1, #16]
    ldp x23, x24, [x1, #32]
    ldp x25, x26, [x1, #48]
    ldp x27, x28, [x1, #64]
    ldp x29, x30, [x1, #80]

    ldr x9, [x1, #96]
    mov sp, x9

    ldp q8,  q9,  [x1, #112]
    ldp q10, q11, [x1, #144]
    ldp q12, q13, [x1, #176]
    ldp q14, q15, [x1, #208]

    ret

ret_from_fork:
    bl sched_unlock_release
    
#   Calls the entry point function
    blr x19
    bl task_exit
    b .

ret_from_fork_child:
    bl sched_unlock_release
    bl switch_to_child_mm
    
    # Restore Floating-Point Status and Control Registers
    ldr w21, [sp, #800]
    ldr w22, [sp, #804]
    msr fpsr, x21
    msr fpcr, x22

    # Restore all 32 Vector/SIMD registers
    ldp q0, q1, [sp, #288]
    ldp q2, q3, [sp, #320]
    ldp q4, q5, [sp, #352]
    ldp q6, q7, [sp, #384]
    ldp q8, q9, [sp, #416]
    ldp q10, q11, [sp, #448]
    ldp q12, q13, [sp, #480]
    ldp q14, q15, [sp, #512]
    ldp q16, q17, [sp, #544]
    ldp q18, q19, [sp, #576]
    ldp q20, q21, [sp, #608]
    ldp q22, q23, [sp, #640]
    ldp q24, q25, [sp, #672]
    ldp q26, q27, [sp, #704]
    ldp q28, q29, [sp, #736]
    ldp q30, q31, [sp, #768]

    # Restore ELR and SPSR (updated offsets)
    ldp x0, x1, [sp, #264]
    msr elr_el1, x0
    msr spsr_el1, x1

    # Restore SP_EL0 (updated offset)
    ldr x0, [sp, #256]
    msr sp_el0, x0

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
    
    add sp, sp, #816
    eret

# void enter_usermode(u64 entry, u64 sp, u64 kernel_sp)
# x0 = entry point address
# x1 = user stack pointer
# x2 = kernel stack pointer
#
# This function properly transitions from kernel mode (EL1) to user mode (EL0)
# It sets up the exception return registers and uses ERET to drop to EL0
enter_usermode:
    # Disable interrupts during transition
    msr daifset, #2

    mov x3, #0
    msr spsr_el1, x3
    msr elr_el1, x0

    # Set up SP_EL0 (user stack pointer)
    msr sp_el0, x1

    # Reset kernel stack pointer to avoid stack leaking
    mov sp, x2

    # Clear all general-purpose registers for clean state
    mov x0, #0
    mov x1, #0
    mov x2, #0
    mov x3, #0
    mov x4, #0
    mov x5, #0
    mov x6, #0
    mov x7, #0
    mov x8, #0
    mov x9, #0
    mov x10, #0
    mov x11, #0
    mov x12, #0
    mov x13, #0
    mov x14, #0
    mov x15, #0
    mov x16, #0
    mov x17, #0
    mov x18, #0
    mov x19, #0
    mov x20, #0
    mov x21, #0
    mov x22, #0
    mov x23, #0
    mov x24, #0
    mov x25, #0
    mov x26, #0
    mov x27, #0
    mov x28, #0
    mov x29, #0
    mov x30, #0

    # Instruction synchronization barrier
    isb

    # Return to EL0
    # ERET will:
    #   - Set PC to ELR_EL1 (entry point)
    #   - Set PSTATE from SPSR_EL1 (EL0 mode)
    #   - Switch to using SP_EL0
    eret
