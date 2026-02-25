#ifndef SYSCALLS_H
#define SYSCALLS_H

#include <lib.h>

typedef struct trapframe_t {
    u64 x[32];         // x0-x30, plus 8 bytes padding (x31/dummy) to align to 256 bytes
    u64 sp_el0;        // Offset: 256
    u64 elr;           // Offset: 264
    u64 spsr;          // Offset: 272
    u64 padding1;      // Offset: 280 (Keeps q array 16-byte aligned)
    __uint128_t q[32]; // Offset: 288. 32x 128-bit SIMD/FP registers
    u32 fpsr;          // Offset: 800
    u32 fpcr;          // Offset: 804
    u64 padding2;      // Offset: 808. Pads total struct size to 816 bytes (multiple of 16)
} trapframe_t;

i64 syscall_handler(trapframe_t *tf, u64 syscall_num, u64 arg0, u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5);
int exec_init(const char* path);
int copy_to_user(void *user_dst, const void *kernel_src, size_t size);
int copy_from_user(void *kernel_dst, const void *user_src, size_t size);
void sysctl_init();

struct sigaction;
typedef u32 sigset_t;
typedef void (*sighandler_t)(int);

i64 sys_kill(i64 pid, int sig);
i64 sys_sigaction(int sig, const struct sigaction* act, struct sigaction* oldact);
i64 sys_sigprocmask(int how, const sigset_t* set, sigset_t* oldset);
i64 sys_sigpending(sigset_t* set);
i64 sys_sigreturn(trapframe_t* tf);

#endif