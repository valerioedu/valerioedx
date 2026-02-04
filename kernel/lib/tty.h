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

#define BUF_SIZE 4096
#define MAX_CANON 255
#define MAX_INPUT 255

#define NCCS 32

// Input flags (c_iflag)
#define IGNBRK  0000001  // Ignore BREAK condition
#define BRKINT  0000002  // BREAK causes interrupt signal
#define IGNPAR  0000004  // Ignore framing/parity errors
#define PARMRK  0000010  // Mark parity errors
#define INPCK   0000020  // Enable input parity check
#define ISTRIP  0000040  // Strip 8th bit
#define INLCR   0000100  // Map NL to CR on input
#define IGNCR   0000200  // Ignore CR on input
#define ICRNL   0000400  // Map CR to NL on input
#define IUCLC   0001000  // Map uppercase to lowercase
#define IXON    0002000  // Enable output flow control (XON/XOFF)
#define IXANY   0004000  // Any char restarts output
#define IXOFF   0010000  // Enable input flow control
#define IMAXBEL 0020000  // Ring bell on input queue full
#define IUTF8   0040000  // Input is UTF-8

// Output flags (c_oflag)
#define OPOST   0000001  // Post-process output
#define OLCUC   0000002  // Map lowercase to uppercase
#define ONLCR   0000004  // Map NL to CR-NL
#define OCRNL   0000010  // Map CR to NL
#define ONOCR   0000020  // No CR output at column 0
#define ONLRET  0000040  // NL performs CR function
#define OFILL   0000100  // Use fill characters for delay
#define OFDEL   0000200  // Fill is DEL
#define NLDLY   0000400  // NL delay mask
#define NL0     0000000
#define NL1     0000400
#define CRDLY   0003000  // CR delay mask
#define CR0     0000000
#define CR1     0001000
#define CR2     0002000
#define CR3     0003000
#define TABDLY  0014000  // TAB delay mask
#define TAB0    0000000
#define TAB1    0004000
#define TAB2    0010000
#define TAB3    0014000  // Expand tabs to spaces
#define XTABS   0014000
#define BSDLY   0020000  // BS delay mask
#define BS0     0000000
#define BS1     0020000
#define VTDLY   0040000  // VT delay mask
#define VT0     0000000
#define VT1     0040000
#define FFDLY   0100000  // FF delay mask
#define FF0     0000000
#define FF1     0100000

// Control flags (c_cflag)
#define CBAUD   0010017  // Baud rate mask
#define CSIZE   0000060  // Character size mask
#define CS5     0000000
#define CS6     0000020
#define CS7     0000040
#define CS8     0000060
#define CSTOPB  0000100  // 2 stop bits
#define CREAD   0000200  // Enable receiver
#define PARENB  0000400  // Parity enable
#define PARODD  0001000  // Odd parity
#define HUPCL   0002000  // Hangup on last close
#define CLOCAL  0004000  // Ignore modem control lines
#define CBAUDEX 0010000  // Extended baud rate mask
#define CMSPAR  0x40000000  // Mark/space parity
#define CRTSCTS 0x80000000  // Hardware flow control

// Baud Rates
#define B0      0000000
#define B50     0000001
#define B75     0000002
#define B110    0000003
#define B134    0000004
#define B150    0000005
#define B200    0000006
#define B300    0000007
#define B600    0000010
#define B1200   0000011
#define B1800   0000012
#define B2400   0000013
#define B4800   0000014
#define B9600   0000015
#define B19200  0000016
#define B38400  0000017
#define B57600  0010001
#define B115200 0010002
#define B230400 0010003
#define B460800 0010004

// Local flags (c_lflag)
#define ISIG    0000001  // Enable signals (INTR, QUIT, SUSP)
#define ICANON  0000002  // Canonical input (line-based)
#define XCASE   0000004  // Canonical upper/lower presentation
#define ECHO    0000010  // Echo input characters
#define ECHOE   0000020  // Echo ERASE as error-correcting backspace
#define ECHOK   0000040  // Echo KILL
#define ECHONL  0000100  // Echo NL even if ECHO is off
#define NOFLSH  0000200  // Disable flush after interrupt/quit
#define TOSTOP  0000400  // Send SIGTTOU for background output
#define ECHOCTL 0001000  // Echo control chars as ^X
#define ECHOPRT 0002000  // Hardcopy visual erase
#define ECHOKE  0004000  // Visual erase for KILL
#define FLUSHO  0010000  // Output being flushed
#define PENDIN  0040000  // Retype pending input
#define IEXTEN  0100000  // Enable extended input processing
#define EXTPROC 0200000  // External processing

#define VINTR    0   // Interrupt (^C)
#define VQUIT    1   // Quit (^\)
#define VERASE   2   // Erase (^? or ^H)
#define VKILL    3   // Kill line (^U)
#define VEOF     4   // End of file (^D)
#define VTIME    5   // Timeout for non-canonical read
#define VMIN     6   // Minimum chars for non-canonical read
#define VSWTC    7   // Switch character
#define VSTART   8   // Start output (^Q)
#define VSTOP    9   // Stop output (^S)
#define VSUSP    10  // Suspend (^Z)
#define VEOL     11  // End of line
#define VREPRINT 12  // Reprint (^R)
#define VDISCARD 13  // Discard (^O)
#define VWERASE  14  // Word erase (^W)
#define VLNEXT   15  // Literal next (^V)
#define VEOL2    16  // Second EOL
#define VSTATUS  18  // Status request (^T) - BSD

#define TCGETS      0x5401
#define TCSETS      0x5402
#define TCSETSW     0x5403
#define TCSETSF     0x5404
#define TCGETA      0x5405
#define TCSETA      0x5406
#define TCSETAW     0x5407
#define TCSETAF     0x5408
#define TCSBRK      0x5409
#define TCXONC      0x540A
#define TCFLSH      0x540B
#define TIOCEXCL    0x540C
#define TIOCNXCL    0x540D
#define TIOCSCTTY   0x540E  // Set controlling terminal
#define TIOCGPGRP   0x540F  // Get foreground process group
#define TIOCSPGRP   0x5410  // Set foreground process group
#define TIOCOUTQ    0x5411
#define TIOCSTI     0x5412
#define TIOCGWINSZ  0x5413
#define TIOCSWINSZ  0x5414
#define TIOCMGET    0x5415
#define TIOCMBIS    0x5416
#define TIOCMBIC    0x5417
#define TIOCMSET    0x5418
#define TIOCGSOFTCAR 0x5419
#define TIOCSSOFTCAR 0x541A
#define FIONREAD    0x541B  // Bytes available to read
#define TIOCINQ     FIONREAD
#define TIOCLINUX   0x541C
#define TIOCCONS    0x541D
#define TIOCGSERIAL 0x541E
#define TIOCSSERIAL 0x541F
#define TIOCPKT     0x5420
#define FIONBIO     0x5421  // Set non-blocking I/O
#define TIOCNOTTY   0x5422  // Give up controlling terminal
#define TIOCSETD    0x5423
#define TIOCGETD    0x5424
#define TCSBRKP     0x5425
#define TIOCSBRK    0x5427
#define TIOCCBRK    0x5428
#define TIOCGSID    0x5429
#define TIOCGRS485  0x542E
#define TIOCSRS485  0x542F

#define TCOOFF  0  // Suspend output
#define TCOON   1  // Restart output
#define TCIOFF  2  // Transmit STOP character
#define TCION   3  // Transmit START character

#define TCIFLUSH  0  // Flush input
#define TCOFLUSH  1  // Flush output
#define TCIOFLUSH 2  // Flush both

#define N_TTY       0
#define N_SLIP      1
#define N_MOUSE     2
#define N_PPP       3
#define N_STRIP     4
#define N_AX25      5

#define EIO         5
#define EAGAIN      11
#define EINTR       4
#define ENOTTY      25
#define EPERM       1
#define ENOENT      2

typedef unsigned int tcflag_t;
typedef unsigned char cc_t;
typedef unsigned int speed_t;

struct termios {
    tcflag_t c_iflag;    // Input mode flags
    tcflag_t c_oflag;    // Output mode flags
    tcflag_t c_cflag;    // Control mode flags
    tcflag_t c_lflag;    // Local mode flags
    cc_t c_line;         // Line discipline
    cc_t c_cc[NCCS];     // Control characters
    speed_t c_ispeed;    // Input speed
    speed_t c_ospeed;    // Output speed
};

struct winsize {
    unsigned short ws_row;    // Rows in characters
    unsigned short ws_col;    // Columns in characters
    unsigned short ws_xpixel; // Horizontal size in pixels
    unsigned short ws_ypixel; // Vertical size in pixels
};

// TTY state flags
#define TTY_THROTTLED    0x01  // Input throttled (XOFF sent)
#define TTY_STOPPED      0x02  // Output stopped (XOFF received)
#define TTY_EXCLUSIVE    0x04  // Exclusive open mode
#define TTY_HUPPED       0x08  // Hangup occurred
#define TTY_CLOSING      0x10  // TTY is being closed
#define TTY_LNEXT        0x20  // Next char is literal
#define TTY_FLUSHING     0x40  // Discarding output (FLUSHO)
#define TTY_NO_WRITE_SPLIT 0x80

#define TTY_MAX_COLUMNS 256

typedef struct tty_ldisc {
    int num;
    const char *name;
    int  (*open)(struct tty *tty);
    void (*close)(struct tty *tty);
    u64  (*read)(struct tty *tty, u8 *buf, u64 count);
    u64  (*write)(struct tty *tty, const u8 *buf, u64 count);
    void (*receive_buf)(struct tty *tty, const u8 *buf, int count);
    int  (*ioctl)(struct tty *tty, u64 cmd, u64 arg);
    void (*flush_buffer)(struct tty *tty);
} tty_ldisc_t;

typedef struct tty_driver {
    const char *name;
    int major;
    int minor_start;
    int num;
    int  (*open)(struct tty *tty);
    void (*close)(struct tty *tty);
    int  (*write)(struct tty *tty, const u8 *buf, int count);
    int  (*write_room)(struct tty *tty);
    void (*putc)(struct tty *tty, u8 c);
    void (*flush_chars)(struct tty *tty);
    int  (*chars_in_buffer)(struct tty *tty);
    int  (*ioctl)(struct tty *tty, u64 cmd, u64 arg);
    void (*set_termios)(struct tty *tty, struct termios *old);
    void (*throttle)(struct tty *tty);
    void (*unthrottle)(struct tty *tty);
    void (*stop)(struct tty *tty);
    void (*start)(struct tty *tty);
    void (*hangup)(struct tty *tty);
    int  (*break_ctl)(struct tty *tty, int state);
    void (*send_xchar)(struct tty *tty, char ch);
} tty_driver_t;

typedef struct tty {
    int index;
    int count;
    u32 flags;
    char read_buf[BUF_SIZE];
    int read_head;
    int read_tail;
    int read_cnt;
    char canon_buf[MAX_CANON];
    int canon_head;
    int canon_column;
    char raw_buf[BUF_SIZE];
    int raw_head;
    int raw_tail;
    char write_buf[BUF_SIZE];
    int write_head;
    int write_tail;
    int write_cnt;
    int column;
    int canon_lines;
    void (*putc)(u8 c);
    spinlock_t lock;
    wait_queue_t read_wait;
    wait_queue_t write_wait;
    process_t *session;
    u64 session_id;
    u64 pgrp;
    struct termios termios;
    struct termios termios_locked;
    struct winsize winsize;
    tty_ldisc_t *ldisc;
    void *ldisc_data;
    tty_driver_t *driver;
    void *driver_data;
    struct tty *link;             // PTY peer
    struct tty *next;
} tty_t;

#define MAX_TTYS 64

extern tty_t *tty_table[MAX_TTYS];
extern tty_t tty_console;
extern tty_t tty_serial;

u64 tty_console_write(struct vfs_node* file, u64 format, u64 size, u8* buffer);
void tty_push_char(char c, tty_t *tty);
void tty_init();
u64 tty_console_read(struct vfs_node *file, u64 format, u64 size, u8 *buffer);

extern inode_ops tty_console_ops;
extern inode_ops tty_serial_ops;
extern inode_ops stdin_ops;
extern inode_ops stdout_ops;
extern inode_ops stderr_ops;
extern struct termios tty_std_termios;

#endif