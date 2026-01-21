#ifndef TTY_H
#define TTY_H

#include <lib.h>
#include <spinlock.h>
#include <vfs.h>
#include <sched.h>

#ifdef ARM
#include <ramfb.h>
#include <uart.h>
#else
#include <vga.h>
#endif

#define BUF_SIZE 2048

typedef struct tty {
    int id;
    char read_buffer[BUF_SIZE];  // Buffering implementation
    int read_head;
    int read_tail;

    char write_buffer[BUF_SIZE];
    int write_head;
    int write_tail;

    void (*putc)(u8 c);   // Output function

    spinlock_t lock;
    wait_queue_t queue;
    struct process_t *proc;

    // Flags
    bool echo;
    u64 fg_pid;
} tty_t;

extern tty_t tty_console;
extern tty_t tty_serial;

u64 tty_console_write(struct vfs_node* file, u64 format, u64 size, u8* buffer);
u64 tty_serial_write(struct vfs_node* file, u64 format, u64 size, u8* buffer);
void tty_push_char(char c, tty_t *tty);
void tty_init();
u64 tty_console_read(struct vfs_node *file, u64 format, u64 size, u8 *buffer);
u64 tty_serial_read(struct vfs_node *file, u64 format, u64 size, u8 *buffer);

extern inode_ops tty_console_ops;
extern inode_ops tty_serial_ops;
extern inode_ops stdin_ops;
extern inode_ops stdout_ops;
extern inode_ops stderr_ops;

#endif