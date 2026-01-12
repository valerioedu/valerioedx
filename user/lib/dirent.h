#ifndef DIRENT_H
#define DIRENT_H

#include <sys/types.h>
#include <stdint.h>

struct dirent {
    uint64_t d_ino;
    uint64_t d_off;
    uint16_t d_reclen;
    uint8_t d_type;
    char d_name[256];
};

ssize_t getdirentries(int fildes, char buf[], size_t nbytes, int64_t *basep);

#endif