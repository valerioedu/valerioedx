#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>

typedef uint64_t u64;
typedef int64_t i64;
typedef uint32_t u32;

//#ifdef ARM
ssize_t write(int fildes, const char *buf, u64 nbyte) {
    register int x0 asm("x0") = fildes;
    register const char *x1 asm("x1") = buf;
    register size_t x2 asm("x2") = nbyte;
    register u64 x8 asm("x8") = 4;
    asm volatile("svc #0"
                 : "+r"(x0)
                 : "r"(x1), "r"(x2), "r"(x8)
                 : "memory");
    return x0;
}

ssize_t read(int fildes, char *buf, size_t nbyte) {
    register int x0 asm("x0") = fildes;
    register char *x1 asm("x1") = buf;
    register size_t x2 asm("x2") = nbyte;
    register u64 x8 asm("x8") = 3;
    asm volatile("svc #0"
                 : "+r"(x0)
                 : "r"(x1), "r"(x2), "r"(x8)
                 : "memory");
    return x0;
}

char *getcwd(char *buf, u64 size) {
    register char *x0 asm("x0") = buf;
    register u64 x1 asm("x1") = size;
    register u64 x8 asm("x8") = 76;
    asm volatile("svc #0"
                 : "+r"(x0) 
                 : "r"(x1), "r"(x8) 
                 : "memory");

    if ((long)x0 < 0)
        return NULL; 

    return buf;
}

int close(int fildes) {
    register int x0 asm("x0") = fildes;
    register u64 x8 asm("x8") = 6;
    asm volatile("svc #0"
        : "+r"(x0) : "r"(x8) : "memory");
    return x0;
}

_Noreturn void _exit(int status) {
    register int x0 asm("x0") = status;
    register u64 x8 asm("x8") = 1;
    asm volatile("svc #0"
        :: "r"(x0), "r"(x8) : "memory");
    __builtin_unreachable();
}

pid_t getppid() {
    register pid_t x0 asm("x0") = 0;
    register u64 x8 asm("x8") = 39;
    asm volatile("svc #0"
        : "+r"(x0) : "r"(x8) : "memory");
    return x0;
}

pid_t getpid() {
    register pid_t x0 asm("x0") = 0;
    register u64 x8 asm("x8") = 20;
    asm volatile("svc #0"
        : "+r"(x0) : "r"(x8) : "memory");
    return x0;
}

pid_t fork() {
    register pid_t x0 asm("x0") = 0;
    register u64 x8 asm("x8") = 2;
    asm volatile("svc #0"
        : "+r"(x0) : "r"(x8) : "memory");
    return x0;
}

int chdir(const char *path) {
    register const char *x0 asm("x0") = path;
    register u64 x8 asm("x8") = 12;
    asm volatile("svc #0"
        : "+r"(x0) : "r"(x8) : "memory");
    return (int)(long)x0;
}

int fchdir(int fildes) {
    register int x0 asm("x0") = fildes;
    register u64 x8 asm("x8") = 13;
    asm volatile("svc #0"
        : "+r"(x0) : "r"(x8) : "memory");
    return (int)x0;
}

int execve(const char *path, char *const argv[], char *const envp[]) {
    register const char *x0 asm("x0") = path;
    register char *const *x1 asm("x1") = argv;
    register char *const *x2 asm("x2") = envp;
    register u64 x8 asm("x8") = 59;
    asm volatile("svc #0"
                 : "+r"(x0)
                 : "r"(x1), "r"(x2), "r"(x8)
                 : "memory");
    return (int)(long)x0;
}

int rmdir(const char *path) {
    register const char *x0 asm("x0") = path;
    register uint64_t x8 asm("x8") = 137;
    asm volatile("svc #0"
        : "+r"(x0) : "r"(x8) : "memory");
    return (int)(long)x0;
}

int unlink(const char *path) {
    register const char *x0 asm("x0") = path;
    register uint64_t x8 asm("x8") = 10;
    asm volatile("svc #0"
        : "+r"(x0) : "r"(x8) : "memory");
    return (int)(long)x0;
}

int setuid(uid_t uid) {
    register uid_t x0 asm("x0") = uid;
    register u64 x8 asm("x8") = 23;
    asm volatile("svc #0" : "+r"(x0) : "r"(x8) : "memory");
    return x0;
}

int setgid(gid_t gid) {
    register gid_t x0 asm("x0") = gid;
    register u64 x8 asm("x8") = 181;
    asm volatile("svc #0" : "+r"(x0) : "r"(x8) : "memory");
    return x0;
}

int seteuid(uid_t uid) {
    register uid_t x0 asm("x0") = uid;
    register u64 x8 asm("x8") = 183;
    asm volatile("svc #0" : "+r"(x0) : "r"(x8) : "memory");
    return x0;
}

int setegid(gid_t gid) {
    register gid_t x0 asm("x0") = gid;
    register u64 x8 asm("x8") = 182;
    asm volatile("svc #0" : "+r"(x0) : "r"(x8) : "memory");
    return x0;
}

int getuid() {
    register uid_t x0 asm("x0") = 0;
    register u64 x8 asm("x8") = 24;
    asm volatile("svc #0"
        : "+r"(x0) : "r"(x8) : "memory");
    return x0;
}

int geteuid() {
    register uid_t x0 asm("x0") = 0;
    register u64 x8 asm("x8") = 25;
    asm volatile("svc #0"
        : "+r"(x0) : "r"(x8) : "memory");
    return x0;
}

int getgid() {
    register gid_t x0 asm("x0") = 0;
    register u64 x8 asm("x8") = 47;
    asm volatile("svc #0"
        : "+r"(x0) : "r"(x8) : "memory");
    return x0;
}

int getegid() {
    register gid_t x0 asm("x0") = 0;
    register u64 x8 asm("x8") = 43;
    asm volatile("svc #0"
        : "+r"(x0) : "r"(x8) : "memory");
    return x0;
}

pid_t getpgrp() {
    register pid_t x0 asm("x0") = 0;
    register u64 x8 asm("x8") = 81;
    asm volatile("svc #0"
        : "+r"(x0) : "r"(x8) : "memory");
    return x0;
}

int setpgid(pid_t pid, pid_t pgid) {
    register pid_t x0 asm("x0") = pid;
    register pid_t x1 asm("x1") = pgid;
    register u64 x8 asm("x8") = 82;
    asm volatile("svc #0" : "+r"(x0) : "r"(x1), "r"(x8) : "memory");
    return (int)x0;
}

pid_t setsid() {
    register pid_t x0 asm("x0") = 0;
    register u64 x8 asm("x8") = 147;
    asm volatile("svc #0"
        : "+r"(x0) : "r"(x8) : "memory");
    return x0;
}

pid_t getpgid(pid_t pid) {
    register pid_t x0 asm("x0") = pid;
    register u64 x8 asm("x8") = 182;
    asm volatile("svc #0" : "+r"(x0) : "r"(x8) : "memory");
    return x0;
}

pid_t getsid(pid_t pid) {
    register pid_t x0 asm("x0") = pid;
    register u64 x8 asm("x8") = 310;
    asm volatile("svc #0" : "+r"(x0) : "r"(x8) : "memory");
    return x0;
}
//#endif