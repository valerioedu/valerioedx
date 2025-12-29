.global cpu_switch_to
.global ret_from_fork
.global enter_usermode
.extern sched_unlock_release

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

# void enter_usermode(u64 entry, u64 sp)
# x0 = entry point address
# x1 = user stack pointer
#
# This function properly transitions from kernel mode (EL1) to user mode (EL0)
# It sets up the exception return registers and uses ERET to drop to EL0
enter_usermode:
    # Disable interrupts during transition
    msr daifset, #2

    # Set up SPSR_EL1 for return to EL0
    # SPSR = 0 means:
    #   - M[3:0] = 0b0000 = EL0t (EL0 with SP_EL0)
    #   - DAIF = 0 (interrupts enabled in EL0)
    #   - NZCV = 0
    mov x2, #0
    msr spsr_el1, x2

    # Set up ELR_EL1 (return address = entry point)
    msr elr_el1, x0

    # Set up SP_EL0 (user stack pointer)
    msr sp_el0, x1

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
