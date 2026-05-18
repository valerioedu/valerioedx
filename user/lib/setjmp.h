#ifndef SETJMP_H
#define SETJMP_H

/* * AArch64 requires saving:
 * x19-x29 (11 registers)
 * x30 (LR - Return Address) (1 register)
 * sp (Stack Pointer) (1 register)
 * d8-d15 (FP callee-saved) (8 registers)
 * Total: 21 slots of 8 bytes. We use 22 for alignment.
 */
typedef unsigned long long jmp_buf[22];

int setjmp(jmp_buf env);
void longjmp(jmp_buf env, int val) __attribute__((noreturn));

#endif