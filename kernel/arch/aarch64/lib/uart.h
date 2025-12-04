#ifndef UART_H
#define UART_H

#include <lib.h>
#include <stdarg.h>
#include <vfs.h>

void uart_kprintf(const char* format, va_list args);
void uart_putc(u8 c);
u64 uart_read_fs(inode_t* node, u64 format, u64 size, u8* buffer);
u64 uart_write_fs(inode_t* node, u64 format, u64 size, u8* buffer);
u8 uart_getc();
void uart_irq_handler();
void uart_init();

static inode_ops uart_ops = {
    .read  = uart_read_fs,
    .write = uart_write_fs,
    .open  = NULL,
    .close = NULL,
};

#endif