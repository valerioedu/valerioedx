#ifndef UART_H
#define UART_H

#include <lib.h>
#include <stdarg.h>

void uart_kprintf(const char* format, va_list args);

#endif