#ifndef FCNTL_H
#define FCNTL_H

#ifdef __cplusplus
extern "C" {
#endif

#define O_RDONLY  0x0001
#define O_WRONLY  0x0002
#define O_RDWR    0x0003
#define O_CREAT   0x0100
#define O_TRUNC   0x0200
#define O_APPEND  0x0400

int open(const char *path, int oflag);

#ifdef __cplusplus
}
#endif

#endif