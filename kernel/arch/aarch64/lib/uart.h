#ifndef UART_H
#define UART_H

#include <lib.h>
#include <stdarg.h>
#include <vfs.h>

void uart_kprintf(const char* format, va_list args);
void uart_putc(u8 c);
u64 uart_read_fs(inode_t* node, u64 size, u8* buffer);
u64 uart_write_fs(inode_t* node, u64 size, u8* buffer);

static inode_ops uart_ops = {
    .read  = uart_read_fs,
    .write = uart_write_fs,
    .open  = NULL,
    .close = NULL,
};

#endif