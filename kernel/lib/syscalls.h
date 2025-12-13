#ifndef SYSCALLS_H
#define SYSCALLS_H

#include <lib.h>

typedef struct trapframe_t {
    u64 x[30];
    u64 lr;
    u64 padding;
    u64 elr;
    u64 spsr;
} trapframe_t;

#endif