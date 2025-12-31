#include <tty.h>

tty_t tty_console;
tty_t tty_serial;

spinlock_t console_lock;
spinlock_t serial_lock;

inode_ops tty_console_ops;
inode_ops tty_serial_ops;
inode_ops stdin_ops;
inode_ops stdout_ops;
inode_ops stderr_ops;

void tty_init() {
    tty_console.id = 0;
    tty_serial.id = 0;
    
#ifdef ARM
    tty_console.putc = ramfb_putc;
    tty_serial.putc = uart_putc;
#endif

    tty_console.echo = false;
    tty_serial.echo = false;

    tty_console.lock = console_lock;
    tty_serial.lock = serial_lock;

    tty_console_ops.write = tty_console_write;
    tty_console_ops.read = tty_console_read;
    
    tty_serial_ops.write = tty_serial_write;
    tty_serial_ops.read = tty_serial_read;
    
    stdin_ops.read = tty_console_read;
    stdout_ops.write = tty_console_write;
    stderr_ops.write = tty_console_write;

}

u64 tty_console_write(struct vfs_node* file, u64 format, u64 size, u8* buffer) {
    for (u32 i = 0; i < size; i++) tty_console.putc(buffer[i]);
}

u64 tty_serial_write(struct vfs_node* file, u64 format, u64 size, u8* buffer) {
    for (u32 i = 0; i < size; i++) tty_serial.putc(buffer[i]);
}

u64 tty_console_read(struct vfs_node *file, u64 format, u64 size, u8 *buffer) {
    tty_console.echo = true;
    u64 read_count = 0;

    while (read_count < size) {
        while (tty_console.read_head == tty_console.read_tail) {
#ifdef ARM
            asm volatile("wfi");
#else
            asm volatile("hlt");
#endif
        }

        u32 flags = spinlock_acquire_irqsave(&tty_console.lock);

        if (tty_console.read_head != tty_console.read_tail) {
            buffer[read_count] = tty_console.read_buffer[tty_console.read_tail];
            tty_console.read_tail = (tty_console.read_tail + 1) % BUF_SIZE;
            read_count++;
        }

        spinlock_release_irqrestore(&tty_console.lock, flags);
    }

    tty_console.echo = false;
    return read_count;
}

u64 tty_serial_read(struct vfs_node *file, u64 format, u64 size, u8 *buffer) {
    tty_serial.echo = true;
    u64 read_count = 0;

    while (read_count < size) {
        while (tty_serial.read_head == tty_serial.read_tail) {
#ifdef ARM
            asm volatile("wfi");
#else
            asm volatile("hlt");
#endif
        }

        u32 flags = spinlock_acquire_irqsave(&tty_serial.lock);

        if (tty_serial.read_head != tty_serial.read_tail) {
            buffer[read_count] = tty_serial.read_buffer[tty_serial.read_tail];
            tty_serial.read_tail = (tty_serial.read_tail + 1) % BUF_SIZE;
            read_count++;
        }

        spinlock_release_irqrestore(&tty_serial.lock, flags);
    }

    tty_serial.echo = false;
    return read_count;
}

void tty_push_char(char c, tty_t *tty) {
    u32 flags = spinlock_acquire_irqsave(&tty->lock);

    tty->read_buffer[tty->read_head] = c;
    
    if (tty->echo && tty->putc) tty->putc(c);
    tty->read_head = (tty->read_head + 1) % BUF_SIZE;

    spinlock_release_irqrestore(&tty->lock, flags);
}