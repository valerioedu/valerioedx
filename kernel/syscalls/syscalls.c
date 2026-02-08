#include <syscalls.h>
#include <file.h>
#include <tty.h>
#include <sched.h>
#include <kio.h>
#include <signal.h>

typedef struct stat {
    u64 st_dev;
    u64 st_ino;
    u32 st_mode;
    u32 st_nlink;
    u32 st_uid;
    u32 st_gid;
    u64 st_rdev;
    i64 st_size;
    i64 st_blksize;
    i64 st_blocks;
    i64 st_atime;
    i64 st_mtime;
    i64 st_ctime;
} stat_t;

struct timespec {
    i64 tv_sec;
    i64 tv_nsec;
};

#define MAX_SYSCALLS        512
#define SYS_EXIT            1
#define SYS_FORK            2
#define SYS_READ            3
#define SYS_WRITE           4
#define SYS_OPEN            5
#define SYS_CLOSE           6
#define SYS_WAIT3           7
#define SYS_UNLINK          10
#define SYS_CHDIR           12
#define SYS_FCHDIR          13
#define SYS_LSEEK           19
#define SYS_GETPID          20
#define SYS_SETUID          23
#define SYS_GETUID          24
#define SYS_GETEUID         25
#define SYS_KILL            37
#define SYS_STAT            38
#define SYS_GETPPID         39
#define SYS_LSTAT           40
#define SYS_DUP             41
#define SYS_GETEGID         43
#define SYS_SIGACTION       46
#define SYS_GETGID          47
#define SYS_SIGPROCMASK     48
#define SYS_SIGPENDING      52
#define SYS_IOCTL           54
#define SYS_REBOOT          55
#define SYS_SYMLINK         57
#define SYS_EXECVE          59
#define SYS_MUNMAP          73
#define SYS_GETCWD          76
#define SYS_GETGROUPS       79
#define SYS_SETGROUPS       80
#define SYS_GETPGRP         81
#define SYS_SETPGID         82
#define SYS_DUP2            90
#define SYS_MKDIR           136
#define SYS_RMDIR           137
#define SYS_SETSID          147
#define SYS_GETPGID         151
#define SYS_SETGID          181
#define SYS_SETEGID         182
#define SYS_SETEUID         183
#define SYS_SIGRETURN       184
#define SYS_FSTAT           189
#define SYS_GETDIRENTRIES   196
#define SYS_MMAP            197
#define SYS_SYSCTL          202
#define SYS_NANOSLEEP       240
#define SYS_GETSID          310

extern i64 sys_write(u32 fd, const char *buf, size_t count);
extern i64 sys_read(u32 fd, char *buf, size_t count);
extern i64 sys_fork(trapframe_t *tf);
extern i64 sys_getpid();
extern i64 sys_getppid();
extern i64 sys_open(const char *path, int flags);
extern i64 sys_close(int fd);
extern void sys_exit(int code);
extern i64 sys_wait3(i64 pid, int *status, int options);
extern i64 sys_stat(const char *path, stat_t *statbuf);
extern i64 sys_lstat(const char *path, stat_t *statbuf);
extern i64 sys_dup(int oldfd);
extern i64 sys_execve(const char* path, const char* argv[], const char* envp[]);
extern i64 sys_unlink(const char *path);
extern i64 sys_chdir(const char *path);
extern i64 sys_fchdir(int fd);
extern i64 sys_lseek(int fd, i64 offset, int whence);
extern i64 sys_munmap(void *addr, size_t length);
extern i64 sys_getcwd(char *buf, size_t size);
extern i64 sys_dup2(int oldfd, int newfd);
extern i64 sys_mkdir(const char *path, mode_t mode);
extern i64 sys_rmdir(const char *path);
extern i64 sys_fstat(int fd, stat_t *statbuf);
extern i64 sys_getdirentries(int fd, char *buf, size_t nbytes, i64 *basep);
extern i64 sys_mmap(void *addr, size_t length, int prot, int flags, int fd, i64 offset);
extern i64 sys_sysctl(int *name, u32 namelen, void *oldp, u64 *oldlenp, void *newp, u64 newlen);
extern i64 sys_kill(i64 pid, int sig);
extern i64 sys_sigaction(int sig, const struct sigaction* act, struct sigaction* oldact);
extern i64 sys_sigprocmask(int how, const sigset_t* set, sigset_t* oldset);
extern i64 sys_sigpending(sigset_t* set);
extern i64 sys_sigreturn(trapframe_t* tf);
extern i64 sys_getuid();
extern i64 sys_setuid(u32 uid);
extern i64 sys_seteuid(u32 euid);
extern i64 sys_getgid();
extern i64 sys_setgid(u32 gid);
extern i64 sys_setegid(u32 egid);
extern i64 sys_geteuid();
extern i64 sys_getegid();
extern i64 sys_setsid();
extern i64 sys_getsid(u64 pid);
extern i64 sys_getpgid(u64 pid);
extern i64 sys_setpgid(u64 pid, u64 pgid);
extern i64 sys_getpgrp();
extern i64 sys_getgroups(int size, gid_t *list);
extern i64 sys_setgroups(int size, const gid_t *list);
extern i64 sys_ioctl(int fd, u64 request, u64 arg);
extern i64 sys_nanosleep(const struct timespec *req, struct timespec *rem);
extern i64 sys_symlink(const char *path1, const char *path2);

typedef i64 (*syscalls_fn_t)(i64, i64, i64, i64, i64, i64);

i64 sys_not_implemented() {
    kprintf("System call not implemented\n");
    return 0;
}

i64 sys_0() {
    asm volatile("nop");
    return 0;
}

#define RB_AUTOBOOT     0
#define RB_HALT         0x08
#define RB_POWEROFF     0x4000

#ifdef ARM
#define PSCI_SYSTEM_OFF   0x84000008
#define PSCI_SYSTEM_RESET 0x84000009
#endif

i64 sys_reboot(int howto) {
    if (sys_getuid() != 0) return -1;

    if (howto & RB_HALT) {
#ifdef ARM
        while (1) asm volatile("wfi");
#endif
    }

    if (howto & RB_POWEROFF) {
#ifdef ARM
        register u64 x0 asm("x0") = PSCI_SYSTEM_OFF;
        asm volatile("hvc #0" :: "r"(x0) : "memory");
#endif
        return -1;
    }

#ifdef ARM
    register u64 x0 asm("x0") = PSCI_SYSTEM_RESET;
    asm volatile("hvc #0" :: "r"(x0) : "memory");
#endif
    return 0;
}

i64 syscall_handler(trapframe_t *tf, u64 syscall_num, u64 arg0, u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5) {
    if (syscall_num >= MAX_SYSCALLS)
        return -1; // -ENOSYS
    
    i64 ret = -1;

    switch (syscall_num) {
        case 0: ret = sys_0(); break;
        case SYS_EXIT: sys_exit(arg0); ret = 0;
        case SYS_FORK: ret = sys_fork(tf); break;
        case SYS_READ: ret = sys_read((u32)arg0, (char*)arg1, (size_t)arg2); break;
        case SYS_WRITE: ret = sys_write((u32)arg0, (const char*)arg1, (size_t)arg2); break;
        case SYS_OPEN: ret = sys_open((const char*)arg0, (int)arg1); break;
        case SYS_CLOSE: ret = sys_close((int)arg0); break;
        case SYS_WAIT3: ret = sys_wait3((i64)arg0, (int*)arg1, (int)arg2); break;
        case SYS_UNLINK: ret = sys_unlink((const char*)arg0); break;
        case SYS_CHDIR: ret = sys_chdir((const char*)arg0); break;
        case SYS_FCHDIR: ret = sys_fchdir((int)arg0); break;
        case SYS_LSEEK: ret = sys_lseek((int)arg0, (i64)arg1, (int)arg2); break;
        case SYS_GETPID: ret = sys_getpid(); break;
        case SYS_SETUID: ret = sys_setuid((u32)arg0); break;
        case SYS_GETUID: ret = sys_getuid(); break;
        case SYS_GETEUID: ret = sys_geteuid(); break;
        case SYS_KILL: ret = sys_kill((i64)arg0, (int)arg1); break;
        case SYS_STAT: ret = sys_stat((const char*)arg0, (stat_t*)arg1); break;
        case SYS_GETPPID: ret = sys_getppid(); break;
        case SYS_LSTAT: ret = sys_lstat((const char*)arg0, (stat_t*)arg1); break;
        case SYS_DUP: ret = sys_dup((int)arg0); break;
        case SYS_GETEGID: ret = sys_getegid(); break;
        case SYS_SIGACTION: ret = sys_sigaction((int)arg0, (const struct sigaction*)arg1, (struct sigaction*)arg2); break;
        case SYS_GETGID: ret = sys_getgid(); break;
        case SYS_SIGPROCMASK: ret = sys_sigprocmask((int)arg0, (const sigset_t*)arg1, (sigset_t*)arg2); break;
        case SYS_SIGPENDING: ret = sys_sigpending((sigset_t*)arg0); break;
        case SYS_IOCTL: ret = sys_ioctl((int)arg0, (u64)arg1, (u64)arg2); break;
        case SYS_REBOOT: ret = sys_reboot((int)arg0); break;
        case SYS_SYMLINK: ret = sys_symlink((const char*)arg0, (const char*)arg1); break;
        case SYS_EXECVE: ret = sys_execve((const char*)arg0, (const char**)arg1, (const char**)arg2); break;
        case SYS_MUNMAP: ret = sys_munmap((void*)arg0, (size_t)arg1); break;
        case SYS_GETCWD: ret = sys_getcwd((char*)arg0, (size_t)arg1); break;
        case SYS_GETGROUPS: ret = sys_getgroups((int)arg0, (gid_t*)arg1); break;
        case SYS_SETGROUPS: ret = sys_setgroups((int)arg0, (gid_t*)arg1); break;
        case SYS_GETPGRP: ret = sys_getpgrp(); break;
        case SYS_SETPGID: ret = sys_setpgid((u64)arg0, (u64)arg1); break;
        case SYS_DUP2: ret = sys_dup2((int)arg0, (int)arg1); break;
        case SYS_MKDIR: ret = sys_mkdir((const char*)arg0, (mode_t)arg1); break;
        case SYS_RMDIR: ret = sys_rmdir((const char*)arg0); break;
        case SYS_SETSID: ret = sys_setsid(); break;
        case SYS_GETPGID: ret = sys_getpgid((u64)arg0); break;
        case SYS_SETGID: ret = sys_setgid((u32)arg0); break;
        case SYS_SETEGID: ret = sys_setegid((u32)arg0); break;
        case SYS_SETEUID: ret = sys_seteuid((u32)arg0); break;
        case SYS_FSTAT: ret = sys_fstat((int)arg0, (stat_t*)arg1); break;
        case SYS_GETDIRENTRIES: ret = sys_getdirentries((int)arg0, (char*)arg1, (size_t)arg2, (i64*)arg3); break;
        case SYS_MMAP: ret = sys_mmap((void*)arg0, (size_t)arg1, (int)arg2, (int)arg3, (int)arg4, (i64)arg5); break;
        case SYS_SYSCTL: ret = sys_sysctl((int*)arg0, (u32)arg1, (void*)arg2, (u64*)arg3, (void*)arg4, (u64)arg5); break;
        case SYS_NANOSLEEP: ret = sys_nanosleep((const struct timespec*)arg0, (struct timespec*)arg1); break;
        case SYS_GETSID: ret = sys_getsid((i64)arg0); break;
        default: ret = sys_not_implemented(); break;
    }

    extern task_t *current_task;
    if (current_task && current_task->proc && current_task->proc->signals) {
        signal_check_pending(tf);
    }

    return ret;
}