.section .text
.global _start
.extern main
.extern environ
.extern exit

_start:
#   x0 = argc
    ldr x0, [sp]
#   x1 = argv 
    add x1, sp, #8
#   envp = argv + (argc + 1) * 8
    mov x2, x0
    add x2, x2, #1
    lsl x2, x2, #3
#   x2 = envp
    add x2, x1, x2

    adrp x3, environ
    add x3, x3, :lo12:environ
    str x2, [x3]

    bl main

#   exit with return value from main
    bl exit
    b .
