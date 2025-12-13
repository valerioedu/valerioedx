#include <tty.h>

tty_t tty_console;
tty_t tty_serial;

spinlock_t console_lock;
spinlock_t serial_lock;

void tty_init() {
    tty_console.id = 0;
    tty_serial.id = 0;
    
#ifdef ARM
    tty_console.putc = ramfb_putc;
    tty_serial.putc = uart_putc;
#endif

    tty_console.echo = true;
    tty_serial.echo = true;

    tty_console.lock = console_lock;
    tty_serial.lock = serial_lock;
}

u64 tty_console_write(struct vfs_node* file, u64 format, u64 size, u8* buffer) {
    for (u32 i = 0; i < size; i++) tty_console.putc(buffer[i]);
}

u64 tty_serial_write(struct vfs_node* file, u64 format, u64 size, u8* buffer) {
    for (u32 i = 0; i < size; i++) tty_serial.putc(buffer[i]);
}

void tty_push_char(char c, tty_t *tty) {
    u32 flags = spinlock_acquire_irqsave(&tty->lock);

    tty->read_buffer[tty->read_head] = c;
    tty->read_head = (tty->read_head + 1) % 1024;

    if (tty->echo && tty->putc) tty->putc(c);

    spinlock_release_irqrestore(&tty->lock, flags);
}