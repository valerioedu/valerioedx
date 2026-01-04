#ifndef UNISTD_H
#define UNISTD_H

#include <stdint.h>

uint64_t write(uint32_t fd, const char *buf, uint64_t size);
uint64_t read(uint32_t fd, char *buf, uint64_t size);
void getcwd(char *buf, uint64_t size);

#endif