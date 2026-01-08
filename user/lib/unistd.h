#ifndef UNISTD_H
#define UNISTD_H

#include <stdint.h>
#include <sys/types.h>

ssize_t write(int fildes, const char *buf, size_t nbytes);
ssize_t read(int fildes, char *buf, size_t nbytes);
char *getcwd(char *buf, uint64_t size);
int close(int fildes);
_Noreturn void _exit(int status);
pid_t getpid();
int chdir(const char *path);
int fchdir(int fildes);

#endif