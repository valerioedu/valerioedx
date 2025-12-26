#include <syscalls.h>
#include <file.h>
#include <tty.h>
#include <sched.h>
#include <kio.h>

#define MAX_SYSCALLS 128
#define SYS_EXIT     1
#define SYS_FORK     2
#define SYS_READ     3
#define SYS_WRITE    4
#define SYS_OPEN     5
#define SYS_CLOSE    6
#define SYS_WAIT     7
#define SYS_EXECVE   11
#define SYS_GETPID   20
#define SYS_GETPPID  39

extern task_t *current_task;
extern i64 sys_write(u32 fd, const char *buf, size_t count);
extern i64 sys_read(u32 fd, char *buf, size_t count);
extern i64 sys_fork();
extern i64 sys_getpid();
extern i64 sys_getppid();
extern i64 sys_open(const char *path, int flags);
extern i64 sys_close(int fd);
extern void sys_exit(int code);
extern i64 sys_wait(int *status);
extern i64 sys_execve(const char* path, const char* argv[], const char* envp[]);

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
    [SYS_EXECVE]             = (syscalls_fn_t)sys_execve,
    [SYS_GETPID]             = (syscalls_fn_t)sys_getpid,
    [SYS_GETPPID]            = (syscalls_fn_t)sys_getppid
};

i64 syscall_handler(u64 syscall_num, u64 arg0, u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5) {
    if (syscall_num >= MAX_SYSCALLS)
        return -1; // -ENOSYS

    if (syscall_num == 4) return sys_write((u32)arg0, (const char*)arg1, arg2);
    if (syscall_num == 1) {
        sys_exit(arg0);
        return 0;
    }
    
    syscalls_fn_t func = syscall_table[syscall_num];
    return func(arg0, arg1, arg2, arg3, arg4, arg5);
}