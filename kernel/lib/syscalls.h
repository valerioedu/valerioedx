#ifndef SYSCALLS_H
#define SYSCALLS_H

#include <lib.h>

typedef struct trapframe_t {
    u64 x[31];
    u64 sp_el0;
    u64 elr;
    u64 spsr;
} trapframe_t;

i64 syscall_handler(u64 syscall_num, u64 arg0, u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5);
int exec_init(const char* path);
int copy_to_user(void *user_dst, const void *kernel_src, size_t size);
int copy_from_user(void *kernel_dst, const void *user_src, size_t size);

#endif