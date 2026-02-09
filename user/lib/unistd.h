#ifndef UNISTD_H
#define UNISTD_H

#include <stdint.h>
#include <sys/types.h>

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

ssize_t write(int fildes, const char *buf, size_t nbytes);
ssize_t read(int fildes, char *buf, size_t nbytes);
char *getcwd(char *buf, uint64_t size);
int close(int fildes);
_Noreturn void _exit(int status);
pid_t getpid();
pid_t getppid();
pid_t fork();
int chdir(const char *path);
int fchdir(int fildes);
int execve(const char *path, char *const argv[], char *const envp[]);
int rmdir(const char *path);
int unlink(const char *path);
int setuid(uid_t uid);
int setgid(gid_t gid);
int seteuid(uid_t uid);
int setegid(gid_t gid);
int getuid();
int geteuid();
int getgid();
int getegid();
pid_t getpgrp();
int setpgid(pid_t pid, pid_t pgid);
pid_t setsid();
pid_t getpgid(pid_t pid);
pid_t getsid(pid_t pid);
int getgroups(int gidsetsize, gid_t grouplist[]);
int setgroups(int gidsetsize, const gid_t grouplist[]);
off_t lseek(int fildes, off_t offset, int whence);
int symlink(const char *path1, const char *path2);
unsigned int sleep(unsigned int seconds);
int dup(int fildes);
int dup2(int fildes, int fildes2);

#endif