#include <syscalls.h>
#include <file.h>
#include <tty.h>
#include <sched.h>
#include <kio.h>

#define MAX_SYSCALLS        256
#define SYS_EXIT            1
#define SYS_FORK            2
#define SYS_READ            3
#define SYS_WRITE           4
#define SYS_OPEN            5
#define SYS_CLOSE           6
#define SYS_WAIT            7
#define SYS_CHDIR           12
#define SYS_FCHDIR          13
#define SYS_LSEEK           19
#define SYS_GETPID          20
#define SYS_GETPPID         39
#define SYS_DUP             41
#define SYS_EXECVE          59
#define SYS_MUNMAP          73
#define SYS_GETCWD          76
#define SYS_DUP2            90
#define SYS_MKDIR           136
#define SYS_RMDIR           137
#define SYS_GETDIRENTRIES   196
#define SYS_MMAP            197

extern task_t *current_task;
extern i64 sys_write(u32 fd, const char *buf, size_t count);
extern i64 sys_read(u32 fd, char *buf, size_t count);
extern i64 sys_fork(trapframe_t *tf);
extern i64 sys_getpid();
extern i64 sys_getppid();
extern i64 sys_open(const char *path, int flags);
extern i64 sys_close(int fd);
extern void sys_exit(int code);
extern i64 sys_wait(int *status);
extern i64 sys_dup(int oldfd);
extern i64 sys_execve(const char* path, const char* argv[], const char* envp[]);
extern i64 sys_chdir(const char *path);
extern i64 sys_fchdir(int fd);
extern i64 sys_lseek(int fd, i64 offset, int whence);
extern i64 sys_munmap(void *addr, size_t length);
extern i64 sys_getcwd(char *buf, size_t size);
extern i64 sys_dup2(int oldfd, int newfd);
extern i64 sys_mkdir(const char *path, mode_t mode);
extern i64 sys_rmdir(const char *path);
extern i64 sys_getdirentries(int fd, char *buf, size_t nbytes, i64 *basep);
extern i64 sys_mmap(void *addr, size_t length, int prot, int flags, int fd, i64 offset);

typedef i64 (*syscalls_fn_t)(i64, i64, i64, i64, i64, i64);

i64 sys_not_implemented() {
    kprintf("System call not implemented\n");
    return 0;
}

i64 sys_0() {
    asm volatile("nop");
    return 0;
}

static syscalls_fn_t syscall_table[MAX_SYSCALLS] = {
    [0 ... MAX_SYSCALLS - 1] = (syscalls_fn_t)sys_not_implemented,
    [0]                      = (syscalls_fn_t)sys_0,
    [SYS_EXIT]               = (syscalls_fn_t)sys_exit,
    [SYS_FORK]               = (syscalls_fn_t)sys_fork,
    [SYS_READ]               = (syscalls_fn_t)sys_read,
    [SYS_WRITE]              = (syscalls_fn_t)sys_write,
    [SYS_OPEN]               = (syscalls_fn_t)sys_open,
    [SYS_CLOSE]              = (syscalls_fn_t)sys_close,
    [SYS_WAIT]               = (syscalls_fn_t)sys_wait,
    [SYS_CHDIR]              = (syscalls_fn_t)sys_chdir,
    [SYS_FCHDIR]             = (syscalls_fn_t)sys_fchdir,
    [SYS_LSEEK]              = (syscalls_fn_t)sys_lseek,
    [SYS_GETPID]             = (syscalls_fn_t)sys_getpid,
    [SYS_GETPPID]            = (syscalls_fn_t)sys_getppid,
    [SYS_DUP]                = (syscalls_fn_t)sys_dup,
    [SYS_EXECVE]             = (syscalls_fn_t)sys_execve,
    [SYS_GETCWD]             = (syscalls_fn_t)sys_getcwd,
    [SYS_DUP2]               = (syscalls_fn_t)sys_dup2,
    [SYS_MKDIR]              = (syscalls_fn_t)sys_mkdir,
    [SYS_RMDIR]              = (syscalls_fn_t)sys_rmdir,
    [SYS_GETDIRENTRIES]      = (syscalls_fn_t)sys_getdirentries
};

i64 syscall_handler(trapframe_t *tf, u64 syscall_num, u64 arg0, u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5) {
    if (syscall_num >= MAX_SYSCALLS)
        return -1; // -ENOSYS

    switch (syscall_num) {
        case SYS_EXIT: sys_exit(arg0); return 0;
        case SYS_FORK: return sys_fork(tf); break;
        case SYS_READ: return sys_read((u32)arg0, (char*)arg1, (size_t)arg2); break;
        case SYS_WRITE: return sys_write((u32)arg0, (const char*)arg1, (size_t)arg2); break;
        case SYS_OPEN: return sys_open((const char*)arg0, (int)arg1); break;
        case SYS_CLOSE: return sys_close((int)arg0); break;
        case SYS_WAIT: return sys_wait((int*)arg0); break;
        case SYS_CHDIR: return sys_chdir((const char*)arg0); break;
        case SYS_FCHDIR: return sys_fchdir((int)arg0); break;
        case SYS_LSEEK: return sys_lseek((int)arg0, (i64)arg1, (int)arg2); break;
        case SYS_GETPID: return sys_getpid(); break;
        case SYS_GETPPID: return sys_getppid(); break;
        case SYS_DUP: return sys_dup((int)arg0); break;
        case SYS_EXECVE: return sys_execve((const char*)arg0, (const char**)arg1, (const char**)arg2); break;
        case SYS_MUNMAP: return sys_munmap((void*)arg0, (size_t)arg1); break;
        case SYS_GETCWD: return sys_getcwd((char*)arg0, (size_t)arg1); break;
        case SYS_DUP2: return sys_dup2((int)arg0, (int)arg1); break;
        case SYS_MKDIR: return sys_mkdir((const char*)arg0, (mode_t)arg1); break;
        case SYS_RMDIR: return sys_rmdir((const char*)arg0); break;
        case SYS_GETDIRENTRIES: return sys_getdirentries((int)arg0, (char*)arg1, (size_t)arg2, (i64*)arg3); break;
        case SYS_MMAP: return sys_mmap((void*)arg0, (size_t)arg1, (int)arg2, (int)arg3, (int)arg4, (i64)arg5); break;
    }

    return -1;
}