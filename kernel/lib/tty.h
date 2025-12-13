#ifndef TTY_H
#define TTY_H

#include <lib.h>
#include <spinlock.h>
#include <vfs.h>

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
    int read_write;

    char write_buffer[BUF_SIZE];
    int write_head;
    int write_tail;

    void (*putc)(u8 c);   // Output function

    spinlock_t lock;

    // Flags
    bool echo;
} tty_t;

extern tty_t tty_console;
extern tty_t tty_serial;

u64 tty_console_write(struct vfs_node* file, u64 format, u64 size, u8* buffer);
u64 tty_serial_write(struct vfs_node* file, u64 format, u64 size, u8* buffer);
void tty_push_char(char c, tty_t *tty);

static inode_ops tty_console_ops = {
    .write = tty_console_write
};

static inode_ops tty_serial_ops = {
    .write = tty_serial_write
};

#endif