#ifndef KIO_H
#define KIO_H

#include <lib.h>

#ifdef ARM
#define UART 0
#define RAMFB 1

void set_stdio(u8 device);
void kprintf(const char* fmt, ...);
#endif

int snprintf(char* buf, size_t size, const char* fmt, ...);

#endif