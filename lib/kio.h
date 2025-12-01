#ifndef KIO_H
#define KIO_H

#ifdef ARM
#define UART 0
#define RAMFB 1

void set_stdio(u8 device);
#endif

#endif