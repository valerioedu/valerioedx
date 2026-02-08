#include <tty.h>
#include <sched.h>
#include <file.h>
#include <string.h>
#include <heap.h>
#include <syscalls.h>
#include <kio.h>
#include <signal.h>

extern task_t *current_task;
extern int signal_send_group(u64 pgrp, int sig);
extern int signal_send_pid(u64 pid, int sig);

tty_t *tty_table[MAX_TTYS];
tty_t tty_console;
tty_t tty_serial;

spinlock_t console_lock;
spinlock_t serial_lock;
spinlock_t tty_table_lock;

inode_ops tty_console_ops;
inode_ops tty_serial_ops;
inode_ops stdin_ops;
inode_ops stdout_ops;
inode_ops stderr_ops;

struct termios tty_std_termios = {
    .c_iflag = ICRNL | IXON | IXOFF | IUTF8,
    .c_oflag = OPOST | ONLCR,
    .c_cflag = B38400 | CS8 | CREAD | HUPCL,
    .c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHOCTL | ECHOKE | IEXTEN,
    .c_line = N_TTY,
    .c_cc = {
        [VINTR]    = 3,      // ^C
        [VQUIT]    = 28,     // ^\  (FS)
        [VERASE]   = 127,    // DEL (or 8 for ^H)
        [VKILL]    = 21,     // ^U
        [VEOF]     = 4,      // ^D
        [VTIME]    = 0,
        [VMIN]     = 1,
        [VSWTC]    = 0,
        [VSTART]   = 17,     // ^Q
        [VSTOP]    = 19,     // ^S
        [VSUSP]    = 26,     // ^Z
        [VEOL]     = 0,
        [VREPRINT] = 18,     // ^R
        [VDISCARD] = 15,     // ^O
        [VWERASE]  = 23,     // ^W
        [VLNEXT]   = 22,     // ^V
        [VEOL2]    = 0,
    },
    .c_ispeed = B38400,
    .c_ospeed = B38400,
};

static const u32 ramfb_ansi_colors[] = {
    0x00000000,
    0x00FF0000,
    0x0000FF00,
    0x00FFFF00,
    0x000000FF,
    0x00FF00FF,
    0x0000FFFF,
    0x00FFFFFF 
};

static void tty_update_color(tty_t *tty) {
    if (tty->fg_color < 8) set_fg_color(ramfb_ansi_colors[tty->fg_color]);
    if (tty->bg_color < 8) set_bg_color(ramfb_ansi_colors[tty->bg_color]);
}

static void tty_do_clear(tty_t *tty) {
#ifdef ARM
    ramfb_clear();
#else
    vga_clear();
#endif
}

static int clamp(int val, int min, int max) {
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

static void handle_sgr(tty_t *tty) {
    if (tty->csi_param_count == 0) {
        tty->fg_color = 7;
        tty->bg_color = 0;
        tty_update_color(tty);
        return;
    }

    for (int i = 0; i < tty->csi_param_count; i++) {
        int param = tty->csi_params[i];

        switch (param) {
            case 0:
                tty->fg_color = 7;
                tty->bg_color = 0;
                break;
            case 1:
                if (tty->fg_color < 8) tty->fg_color += 8;
                break;
            case 30 ... 37:
                tty->fg_color = param - 30;
                break;
            case 39:
                tty->fg_color = 7;
                break;
            case 40 ... 47:
                tty->bg_color = param - 40;
                break;
            case 49:
                tty->bg_color = 0;
                break;
            case 90 ... 97:
                tty->fg_color = param - 90 + 8;
                break;
            case 100 ... 107:
                tty->bg_color = param - 100 + 8;
                break;
        }
    }

    tty_update_color(tty);
}

#ifdef ARM
#define RAMFB_COLS (WIDTH / 8)

void rmline() {
    int cur_x = 0, cur_y = 0;
    ramfb_get_cursor(&cur_x, &cur_y);
    ramfb_set_cursor(0, cur_y);
    for (int i = 0; i < RAMFB_COLS; i++)
        ramfb_putc(' ');

    ramfb_set_cursor(cur_x, cur_y);
}
#endif

static void handle_csi_command(tty_t *tty, char c) {
    // Default parameters
    int p1 = (tty->csi_param_count > 0 && tty->csi_params[0] > 0) ? tty->csi_params[0] : 1;
    int p2 = (tty->csi_param_count > 1 && tty->csi_params[1] > 0) ? tty->csi_params[1] : 1;

    int rows = tty->winsize.ws_row;
    int cols = tty->winsize.ws_col;
    int cur_x = 0, cur_y = 0;

#ifdef ARM
        ramfb_get_cursor(&cur_x, &cur_y);
#else
        vga_get_cursor(&cur_x, &cur_y);
#endif

    switch (c) {
        case 'A':
            cur_y = clamp(cur_y - p1, 0, rows - 1);
            break;
            
        case 'B':
            cur_y = clamp(cur_y + p1, 0, rows - 1);
            break;
            
        case 'C':
            cur_x = clamp(cur_x + p1, 0, cols - 1);
            break;
            
        case 'D':
            cur_x = clamp(cur_x - p1, 0, cols - 1);
            break;
            
        case 'H':
        case 'f':
            cur_y = clamp(p1 - 1, 0, rows - 1);
            cur_x = clamp(p2 - 1, 0, cols - 1);
            break;
            
        case 'J': {
            if (tty->csi_param_count == 0) p1 = 0; 
            else p1 = tty->csi_params[0];

            int orig_x = cur_x, orig_y = cur_y;

            if (p1 == 2) {
#ifdef ARM
                    ramfb_clear();
                    cur_x = 0; cur_y = 0;
#else
                    vga_clear();
                    cur_x = 0; cur_y = 0;
#endif
            } else if (p1 == 0) {
                for (int y = orig_y; y < rows; y++) {
                    int start_col = (y == orig_y) ? orig_x : 0;
#ifdef ARM
                    ramfb_set_cursor(start_col, y);
#else
                    vga_move_cursor(start_col, y);
#endif

                    int cnt = cols - start_col;
                    for (int i = 0; i < cnt; i++) {
                        if (tty->putc) tty->putc(' ');
                    }
                }
#ifdef ARM
                ramfb_set_cursor(orig_x, orig_y);
#else
                vga_move_cursor(orig_x, orig_y);
#endif
                cur_x = orig_x; cur_y = orig_y;
            } else if (p1 == 1) {
                for (int y = 0; y <= orig_y; y++) {
                    int end_col = (y == orig_y) ? orig_x : (cols - 1);
#ifdef ARM
                    ramfb_set_cursor(0, y);
#else
                    vga_move_cursor(0, y);
#endif

                    int cnt = end_col + 1;
                    for (int i = 0; i < cnt; i++) {
                        if (tty->putc) tty->putc(' ');
                    }
                }
#ifdef ARM
                ramfb_set_cursor(orig_x, orig_y);
#else
                vga_move_cursor(orig_x, orig_y);
#endif
                cur_x = orig_x; cur_y = orig_y;
            }

            break;
        }
            
        case 'K': {
            if (tty->csi_param_count == 0) p1 = 0;
            else p1 = tty->csi_params[0];

            int orig_x = cur_x, orig_y = cur_y;

            if (p1 == 2)
                rmline();
            
            else if (p1 == 0) {
#ifdef ARM
                ramfb_set_cursor(orig_x, orig_y);
#else
                vga_move_cursor(orig_x, orig_y);
#endif
                int cnt = cols - orig_x;
                for (int i = 0; i < cnt; i++) {
                    if (tty->putc) tty->putc(' ');
                }
#ifdef ARM
                ramfb_set_cursor(orig_x, orig_y);
#else
                vga_move_cursor(orig_x, orig_y);
#endif
            } else if (p1 == 1) {
#ifdef ARM
                ramfb_set_cursor(0, orig_y);
#else
                vga_move_cursor(0, orig_y);
#endif

                int cnt = orig_x + 1;
                for (int i = 0; i < cnt; i++) {
                    if (tty->putc) tty->putc(' ');
                }

#ifdef ARM
                ramfb_set_cursor(orig_x, orig_y);
#else
                vga_move_cursor(orig_x, orig_y);
#endif
            }

            break;
        }

        case 'm':
            handle_sgr(tty);
            return;
    }

#ifdef ARM
    ramfb_set_cursor(cur_x, cur_y);
#else
    vga_move_cursor(cur_x, cur_y);
#endif
    
    tty->column = cur_x;
}

static inline int is_ctrl_char(u8 c) {
    return c < 32 || c == 127;
}

static inline int char_width(u8 c, tty_t *tty) {
    if (c == '\t')
        return 8 - (tty->canon_column % 8);

    if (is_ctrl_char(c))
        return (tty->termios.c_lflag & ECHOCTL) ? 2 : 0;

    return 1;
}

static inline int input_available(tty_t *tty) {
    return (tty->read_head - tty->read_tail + BUF_SIZE) % BUF_SIZE;
}

static inline int read_buf_empty(tty_t *tty) {
    return tty->read_head == tty->read_tail;
}

static inline int read_buf_full(tty_t *tty) {
    return ((tty->read_head + 1) % BUF_SIZE) == tty->read_tail;
}

static inline void read_buf_put(tty_t *tty, char c) {
    if (!read_buf_full(tty)) {
        tty->read_buf[tty->read_head] = c;
        tty->read_head = (tty->read_head + 1) % BUF_SIZE;
        tty->read_cnt++;
    }
}

static inline char read_buf_get(tty_t *tty) {
    if (read_buf_empty(tty)) return 0;
    char c = tty->read_buf[tty->read_tail];
    tty->read_tail = (tty->read_tail + 1) % BUF_SIZE;
    tty->read_cnt--;
    return c;
}

static void do_output_char(tty_t *tty, u8 c) {
    if (tty == &tty_console) {
        if (tty->state == TTY_STATE_NORMAL) {
            if (c == 0x1B) { // ESC
                tty->state = TTY_STATE_ESC;
                return;
            }
        } else if (tty->state == TTY_STATE_ESC) {
            if (c == '[') {
                tty->state = TTY_STATE_CSI;
                tty->csi_param_count = 0;
                tty->csi_params[0] = 0;
                return;
            } else 
                tty->state = TTY_STATE_NORMAL;
            
        } else if (tty->state == TTY_STATE_CSI) {
            if (c >= '0' && c <= '9') {
                if (tty->csi_param_count == 0) tty->csi_param_count = 1;
                int current = tty->csi_params[tty->csi_param_count - 1];
                tty->csi_params[tty->csi_param_count - 1] = current * 10 + (c - '0');
                return;
            } else if (c == ';') {
                if (tty->csi_param_count < MAX_CSI_PARAMS) {
                    tty->csi_param_count++;
                    tty->csi_params[tty->csi_param_count - 1] = 0;
                }

                return;
            } else {
                handle_csi_command(tty, c);
                tty->state = TTY_STATE_NORMAL;
                return;
            }
        }
    }

    switch (c) {
        case '\n':
            tty->column = 0;
            break;
        case '\r':
            tty->column = 0;
            break;
        case '\t':
            tty->column = (tty->column + 8) & ~7;
            break;
        case '\b':
            if (tty->column > 0) tty->column--;
            break;
        default:
            if (c >= 32) tty->column++;
            break;
    }
    
    if (tty->putc) tty->putc(c);
}

void tty_put_char(tty_t *tty, u8 c) {
    if (tty->flags & TTY_FLUSHING) return;
    
    if (tty->termios.c_oflag & OPOST) {
        switch (c) {
            case '\n':
                if (tty->termios.c_oflag & ONLCR)
                    do_output_char(tty, '\r');

                do_output_char(tty, '\n');
                return;
                
            case '\r':
                if (tty->termios.c_oflag & OCRNL) {
                    do_output_char(tty, '\n');
                    return;
                }

                if (tty->termios.c_oflag & ONOCR && tty->column == 0)
                    return;  // No CR at column 0

                break;
                
            case '\t':
                if (tty->termios.c_oflag & TABDLY) {
                    // Expand tabs to spaces
                    int spaces = 8 - (tty->column % 8);
                    while (spaces--)
                        do_output_char(tty, ' ');

                    return;
                }
                
                break;
        }
        
        // Uppercase conversion (legacy)
        if ((tty->termios.c_oflag & OLCUC) && c >= 'a' && c <= 'z')
            c = c - 'a' + 'A';
    }
    
    do_output_char(tty, c);
}

void tty_write_string(tty_t *tty, const char *str) {
    while (*str)
        tty_put_char(tty, *str++);
}

void tty_echo_char(tty_t *tty, u8 c) {
    if (!(tty->termios.c_lflag & ECHO)) {
        // Special case: echo NL even if ECHO is off
        if (c == '\n' && (tty->termios.c_lflag & ECHONL))
            tty_put_char(tty, '\n');

        return;
    }
    
    if (c == '\n') {
        tty_put_char(tty, '\n');
    } else if (c == '\t') {
        tty_put_char(tty, '\t');
    } else if (is_ctrl_char(c)) {
        if (tty->termios.c_lflag & ECHOCTL) {
            tty_put_char(tty, '^');
            tty_put_char(tty, c + 64);  // ^A, ^B, etc.
        }
    } else tty_put_char(tty, c);
}

static void erase_char(tty_t *tty, u8 c) {
    if (!(tty->termios.c_lflag & ECHO)) return;
    if (!(tty->termios.c_lflag & ECHOE)) {
        tty_echo_char(tty, tty->termios.c_cc[VERASE]);
        return;
    }
    
    if (c == '\t') {
        int width = 8 - ((tty->canon_column - 8) % 8);
        if (width == 0) width = 8;
        while (width--) {
            tty_put_char(tty, '\b');
            tty_put_char(tty, ' ');
            tty_put_char(tty, '\b');
        }
    } else if (is_ctrl_char(c) && (tty->termios.c_lflag & ECHOCTL)) {
        // Erase ^X (2 chars)
        tty_put_char(tty, '\b');
        tty_put_char(tty, ' ');
        tty_put_char(tty, '\b');
        tty_put_char(tty, '\b');
        tty_put_char(tty, ' ');
        tty_put_char(tty, '\b');
    } else {
        tty_put_char(tty, '\b');
        tty_put_char(tty, ' ');
        tty_put_char(tty, '\b');
    }
}

static void erase_line(tty_t *tty) {
    if (!(tty->termios.c_lflag & ECHO)) return;
    
    if (tty->termios.c_lflag & ECHOKE) {
        while (tty->canon_head > 0) {
            tty->canon_head--;
            u8 c = tty->canon_buf[tty->canon_head];
            erase_char(tty, c);
        }
    } else if (tty->termios.c_lflag & ECHOK)
        tty_put_char(tty, '\n');
    
    else tty_echo_char(tty, tty->termios.c_cc[VKILL]);
    
    tty->canon_head = 0;
    tty->canon_column = 0;
}

static void reprint_line(tty_t *tty) {
    if (!(tty->termios.c_lflag & ECHO)) return;
    
    tty_put_char(tty, '^');
    tty_put_char(tty, 'R');
    tty_put_char(tty, '\n');
    
    for (int i = 0; i < tty->canon_head; i++)
        tty_echo_char(tty, tty->canon_buf[i]);
}

int tty_signal_session(tty_t *tty, int sig) {
    if (tty->pgrp > 0)
        return signal_send_group(tty->pgrp, sig);

    return -1;
}

// Check if current process can perform I/O on this TTY
int tty_check_change(tty_t *tty) {
    if (!current_task || !current_task->proc) return 0;
    
    process_t *proc = current_task->proc;
    
    if (proc->pgid == tty->pgrp) return 0;
    
    if (tty->termios.c_lflag & TOSTOP) {
        int sigttou_blocked = 0, sigttou_ignored = 0;
        if (proc->signals) {
            sigttou_blocked = !!(proc->signals->blocked & (1ULL << (SIGTTOU - 1)));

            sigttou_ignored = (proc->signals->actions[SIGTTOU].sa_handler == SIG_IGN);
        }

        if (!sigttou_blocked && !sigttou_ignored)
            signal_send_group(proc->pgid, SIGTTOU);

        return -1;
    }
    
    return 0;
}

static void finish_canonical_line(tty_t *tty) {
    for (int i = 0; i < tty->canon_head; i++)
        read_buf_put(tty, tty->canon_buf[i]);
    
    tty->canon_lines++;
    tty->canon_head = 0;
    tty->canon_column = 0;
    
    wake_up(&tty->read_wait);
}

void tty_receive_char(tty_t *tty, u8 c) {
    u32 flags = spinlock_acquire_irqsave(&tty->lock);
    struct termios *t = &tty->termios;
    
    if (tty->flags & TTY_LNEXT) {
        tty->flags &= ~TTY_LNEXT;
        goto store_char;
    }
    
    if (t->c_iflag & ISTRIP)
        c &= 0x7F;
    
    // Handle carriage return
    if (c == '\r') {
        if (t->c_iflag & IGNCR) {
            spinlock_release_irqrestore(&tty->lock, flags);
            return;
        }

        if (t->c_iflag & ICRNL)
            c = '\n';

    } else if (c == '\n' && (t->c_iflag & INLCR))
        c = '\r';
    
    if ((t->c_iflag & IUCLC) && c >= 'A' && c <= 'Z')
        c = c - 'A' + 'a';
    
    if (t->c_lflag & ISIG) {
        int sig = 0;
        
        if (c == t->c_cc[VINTR])
            sig = SIGINT;

        else if (c == t->c_cc[VQUIT])
            sig = SIGQUIT;
        
        else if (c == t->c_cc[VSUSP])
            sig = SIGTSTP;
        
        if (sig) {
            if (t->c_lflag & ECHO) {
                tty_put_char(tty, '^');
                tty_put_char(tty, c + 64);
                tty_put_char(tty, '\n');
            }
            
            if (!(t->c_lflag & NOFLSH)) {
                tty->canon_head = 0;
                tty->canon_column = 0;
                tty->read_head = tty->read_tail;
                tty->read_cnt = 0;
                tty->canon_lines = 0;
            }
            
            spinlock_release_irqrestore(&tty->lock, flags);
            
            tty_signal_session(tty, sig);
            
            wake_up(&tty->read_wait);
            return;
        }
    }
    
    if (t->c_iflag & IXON) {
        if (c == t->c_cc[VSTOP]) {
            tty->flags |= TTY_STOPPED;
            spinlock_release_irqrestore(&tty->lock, flags);
            return;
        }

        if (c == t->c_cc[VSTART]) {
            tty->flags &= ~TTY_STOPPED;
            wake_up(&tty->write_wait);
            spinlock_release_irqrestore(&tty->lock, flags);
            return;
        }
        
        if ((t->c_iflag & IXANY) && (tty->flags & TTY_STOPPED)) {
            tty->flags &= ~TTY_STOPPED;
            wake_up(&tty->write_wait);
        }
    }
    
    if (t->c_lflag & ICANON) {
        if ((t->c_lflag & IEXTEN) && c == t->c_cc[VLNEXT]) {
            tty->flags |= TTY_LNEXT;
            if (t->c_lflag & ECHO) {
                tty_put_char(tty, '^');
                tty_put_char(tty, '\b');
            }

            spinlock_release_irqrestore(&tty->lock, flags);
            return;
        }
        
        if (c == t->c_cc[VERASE] || c == '\b' || c == 127) {
            if (tty->canon_head > 0) {
                tty->canon_head--;
                u8 erased = tty->canon_buf[tty->canon_head];
                tty->canon_column -= char_width(erased, tty);
                if (tty->canon_column < 0) tty->canon_column = 0;
                erase_char(tty, erased);
            }

            spinlock_release_irqrestore(&tty->lock, flags);
            return;
        }
        
        if ((t->c_lflag & IEXTEN) && c == t->c_cc[VWERASE]) {
            while (tty->canon_head > 0 && 
                   tty->canon_buf[tty->canon_head - 1] == ' ') {
                tty->canon_head--;
                erase_char(tty, ' ');
            }
            
            while (tty->canon_head > 0 && 
                   tty->canon_buf[tty->canon_head - 1] != ' ') {
                tty->canon_head--;
                u8 erased = tty->canon_buf[tty->canon_head];
                erase_char(tty, erased);
            }

            spinlock_release_irqrestore(&tty->lock, flags);
            return;
        }
        
        if (c == t->c_cc[VKILL]) {
            erase_line(tty);
            spinlock_release_irqrestore(&tty->lock, flags);
            return;
        }
        
        if ((t->c_lflag & IEXTEN) && c == t->c_cc[VREPRINT]) {
            reprint_line(tty);
            spinlock_release_irqrestore(&tty->lock, flags);
            return;
        }
        
        if ((t->c_lflag & IEXTEN) && c == t->c_cc[VDISCARD]) {
            tty->flags ^= TTY_FLUSHING;
            if (t->c_lflag & ECHO) {
                tty_put_char(tty, '^');
                tty_put_char(tty, 'O');
            }

            spinlock_release_irqrestore(&tty->lock, flags);
            return;
        }
        
        if (c == t->c_cc[VEOF]) {
            if (tty->canon_head > 0)
                finish_canonical_line(tty);
            
            else {
                tty->canon_lines++;
                wake_up(&tty->read_wait);
            }

            spinlock_release_irqrestore(&tty->lock, flags);
            return;
        }
        
        if (c == t->c_cc[VEOL] || 
            ((t->c_lflag & IEXTEN) && c == t->c_cc[VEOL2])) {
            if (c != 0) {
                if (tty->canon_head < MAX_CANON - 1) {
                    tty->canon_buf[tty->canon_head++] = c;
                    tty_echo_char(tty, c);
                }

                finish_canonical_line(tty);
            }

            spinlock_release_irqrestore(&tty->lock, flags);
            return;
        }
        
        if (c == '\n') {
            if (tty->canon_head < MAX_CANON - 1)
                tty->canon_buf[tty->canon_head++] = '\n';

            tty_echo_char(tty, '\n');
            finish_canonical_line(tty);
            spinlock_release_irqrestore(&tty->lock, flags);
            return;
        }
    }
    
store_char:
    if (t->c_lflag & ICANON) {
        if (tty->canon_head < MAX_CANON - 1) {
            tty->canon_buf[tty->canon_head++] = c;
            tty->canon_column += char_width(c, tty);
            tty_echo_char(tty, c);
        } else {
            // Buffer full - ring bell
            if (t->c_iflag & IMAXBEL)
                tty_put_char(tty, '\007');
        }
    } else {
        read_buf_put(tty, c);
        tty_echo_char(tty, c);
        wake_up(&tty->read_wait);
    }
    
    spinlock_release_irqrestore(&tty->lock, flags);
}

void tty_receive_buf(tty_t *tty, const u8 *buf, int count) {
    for (int i = 0; i < count; i++)
        tty_receive_char(tty, buf[i]);
}

u64 n_tty_read(tty_t *tty, u8 *buf, u64 count) {
    u64 read_count = 0;
    struct termios *t = &tty->termios;
    
    while (read_count < count) {
        u32 flags = spinlock_acquire_irqsave(&tty->lock);
        
        if (current_task && current_task->proc) {
            process_t *proc = current_task->proc;
            if (proc->signals && 
                (proc->signals->pending & ~proc->signals->blocked)) {
                spinlock_release_irqrestore(&tty->lock, flags);
                return read_count > 0 ? read_count : (u64)-EINTR;
            }
        }
        
        if (t->c_lflag & ICANON) {
            if (tty->canon_lines == 0) {
                spinlock_release_irqrestore(&tty->lock, flags);
                sleep_on(&tty->read_wait, NULL);
                continue;
            }
            
            while (read_count < count && !read_buf_empty(tty)) {
                char c = read_buf_get(tty);
                
                if (c == t->c_cc[VEOF]) {
                    tty->canon_lines--;
                    spinlock_release_irqrestore(&tty->lock, flags);
                    return read_count;
                }
                
                buf[read_count++] = c;
                
                if (c == '\n' || c == t->c_cc[VEOL] || 
                    c == t->c_cc[VEOL2]) {
                    tty->canon_lines--;
                    spinlock_release_irqrestore(&tty->lock, flags);
                    return read_count;
                }
            }
            
            spinlock_release_irqrestore(&tty->lock, flags);
            continue;
        } else {
            u8 vmin = t->c_cc[VMIN];
            u8 vtime = t->c_cc[VTIME];
            int available = input_available(tty);
            
            if (vmin == 0 && vtime == 0) {
                if (available > 0) {
                    int to_read = (available < count - read_count) ? 
                                  available : (count - read_count);
                    for (int i = 0; i < to_read; i++)
                        buf[read_count++] = read_buf_get(tty);
                }
                spinlock_release_irqrestore(&tty->lock, flags);
                return read_count;
            }
            
            if (vmin > 0 && vtime == 0) {
                if (available < vmin && available < count - read_count) {
                    spinlock_release_irqrestore(&tty->lock, flags);
                    sleep_on(&tty->read_wait, NULL);
                    continue;
                }
                
                int to_read = (available < count - read_count) ? 
                              available : (count - read_count);
                for (int i = 0; i < to_read; i++)
                    buf[read_count++] = read_buf_get(tty);
                
                if (read_count >= vmin) {
                    spinlock_release_irqrestore(&tty->lock, flags);
                    return read_count;
                }
            }
            
            if (vmin == 0 && vtime > 0) {
                if (available > 0) {
                    int to_read = (available < count - read_count) ? 
                                available : (count - read_count);
                    for (int i = 0; i < to_read; i++)
                        buf[read_count++] = read_buf_get(tty);

                    spinlock_release_irqrestore(&tty->lock, flags);
                    return read_count;
                }
                
                spinlock_release_irqrestore(&tty->lock, flags);
                
                // VTIME is in 1/10 second units
                extern u32 current_interval;
                u64 timeout_ticks = (vtime * 100) / current_interval;
                if (timeout_ticks == 0) timeout_ticks = 1;
                
                int timed_out = sleep_on_timeout(&tty->read_wait, timeout_ticks);
                if (timed_out) return read_count;
                continue;
            }
            
            if (vmin > 0 && vtime > 0) {
                if (read_count == 0 && available == 0) {
                    spinlock_release_irqrestore(&tty->lock, flags);
                    sleep_on(&tty->read_wait, NULL);
                    continue;
                }
                
                if (available > 0) {
                    int to_read = (available < count - read_count) ? 
                                  available : (count - read_count);
                    for (int i = 0; i < to_read; i++)
                        buf[read_count++] = read_buf_get(tty);
                }
                
                if (read_count >= vmin) {
                    spinlock_release_irqrestore(&tty->lock, flags);
                    return read_count;
                }
                
                if (read_count >= count) {
                    spinlock_release_irqrestore(&tty->lock, flags);
                    return read_count;
                }
                
                spinlock_release_irqrestore(&tty->lock, flags);
                
                extern u32 current_interval;
                u64 timeout_ticks = (vtime * 100) / current_interval;
                if (timeout_ticks == 0) timeout_ticks = 1;
                
                int timed_out = sleep_on_timeout(&tty->read_wait, timeout_ticks);
                if (timed_out)
                    return read_count;

                continue;
            }
            
            // Fallback
            if (available > 0) {
                int to_read = (available < count - read_count) ? 
                              available : (count - read_count);
                for (int i = 0; i < to_read; i++)
                    buf[read_count++] = read_buf_get(tty);

                spinlock_release_irqrestore(&tty->lock, flags);
                return read_count;
            }
            
            spinlock_release_irqrestore(&tty->lock, flags);
            sleep_on(&tty->read_wait, NULL);
        }
    }
    
    return read_count;
}

u64 n_tty_write(tty_t *tty, const u8 *buf, u64 count) {
    if (tty_check_change(tty) < 0)
        return (u64)-EINTR;

    
    for (u64 i = 0; i < count; i++) {
        while (tty->flags & TTY_STOPPED)
            sleep_on(&tty->write_wait, NULL);

        
        tty_put_char(tty, buf[i]);
    }
    
    return count;
}

int tty_ioctl(struct vfs_node *file, u64 request, u64 arg) {
    tty_t *tty = NULL;
    if (current_task && current_task->proc)
        tty = current_task->proc->controlling_tty;
    
    // Fall back to console if no controlling TTY
    if (!tty) tty = &tty_console;
    
    u32 flags = spinlock_acquire_irqsave(&tty->lock);
    int ret = 0;
    
    switch (request) {
        case TCGETS: {
            spinlock_release_irqrestore(&tty->lock, flags);
            struct termios kt;
            flags = spinlock_acquire_irqsave(&tty->lock);
            memcpy(&kt, &tty->termios, sizeof(struct termios));
            spinlock_release_irqrestore(&tty->lock, flags);
            if (copy_to_user((void*)arg, &kt, sizeof(struct termios)) != 0)
                return -1;

            return 0;
        }
            
        case TCSETS: {
            struct termios kt;
            spinlock_release_irqrestore(&tty->lock, flags);
            if (copy_from_user(&kt, (void*)arg, sizeof(struct termios)) != 0)
                return -1;

            flags = spinlock_acquire_irqsave(&tty->lock);
            memcpy(&tty->termios, &kt, sizeof(struct termios));
            spinlock_release_irqrestore(&tty->lock, flags);
            return 0;
        }
            
        case TCSETSW: {
            struct termios kt;
            spinlock_release_irqrestore(&tty->lock, flags);
            if (copy_from_user(&kt, (void*)arg, sizeof(struct termios)) != 0)
                return -1;

            flags = spinlock_acquire_irqsave(&tty->lock);
            memcpy(&tty->termios, &kt, sizeof(struct termios));
            spinlock_release_irqrestore(&tty->lock, flags);
            return 0;
        }
            
        case TCSETSF: {
            struct termios kt;
            spinlock_release_irqrestore(&tty->lock, flags);
            if (copy_from_user(&kt, (void*)arg, sizeof(struct termios)) != 0)
                return -1;

            flags = spinlock_acquire_irqsave(&tty->lock);
            tty->read_head = tty->read_tail;
            tty->read_cnt = 0;
            tty->canon_head = 0;
            tty->canon_lines = 0;
            memcpy(&tty->termios, &kt, sizeof(struct termios));
            spinlock_release_irqrestore(&tty->lock, flags);
            return 0;
        }
            
        case TIOCGWINSZ: {
            struct winsize kws;
            memcpy(&kws, &tty->winsize, sizeof(struct winsize));
            spinlock_release_irqrestore(&tty->lock, flags);
            if (copy_to_user((void*)arg, &kws, sizeof(struct winsize)) != 0)
                return -1;

            return 0;
        }
            
        case TIOCSWINSZ: {
            struct winsize kws;
            struct winsize old_ws = tty->winsize;
            spinlock_release_irqrestore(&tty->lock, flags);
            if (copy_from_user(&kws, (void*)arg, sizeof(struct winsize)) != 0)
                return -1;

            flags = spinlock_acquire_irqsave(&tty->lock);
            memcpy(&tty->winsize, &kws, sizeof(struct winsize));
            if (old_ws.ws_row != tty->winsize.ws_row ||
                old_ws.ws_col != tty->winsize.ws_col) {
                spinlock_release_irqrestore(&tty->lock, flags);
                tty_signal_session(tty, SIGWINCH);
                return 0;
            }

            break;
        }

        case TIOCGPGRP: {
            u64 pgrp = tty->pgrp;
            spinlock_release_irqrestore(&tty->lock, flags);
            if (copy_to_user((void*)arg, &pgrp, sizeof(u64)) != 0)
                return -1;

            return 0;
        }
            
        case TIOCSPGRP: {
            u64 pgrp;
            spinlock_release_irqrestore(&tty->lock, flags);
            if (copy_from_user(&pgrp, (void*)arg, sizeof(u64)) != 0)
                return -1;

            flags = spinlock_acquire_irqsave(&tty->lock);
            if (!current_task->proc || current_task->proc->sid != tty->session_id)
                ret = -ENOTTY;
            
            else tty->pgrp = pgrp;
            break;
        }

        case TIOCGSID: {
            u64 sid = tty->session_id;
            spinlock_release_irqrestore(&tty->lock, flags);
            if (copy_to_user((void*)arg, &sid, sizeof(u64)) != 0)
                return -1;

            return 0;
        }

        case TIOCSCTTY:
            if (current_task->proc && current_task->proc->session_leader) {
                tty->session = current_task->proc;
                tty->session_id = current_task->proc->sid;
                tty->pgrp = current_task->proc->pgid;
                current_task->proc->controlling_tty = tty;
            } else ret = -EPERM;

            break;
            
        case TIOCNOTTY:
            if (current_task->proc && tty->session_id == current_task->proc->sid) {
                tty->session = NULL;
                tty->session_id = 0;
                tty->pgrp = 0;
            }

            break;

        case TIOCEXCL:
            tty->flags |= TTY_EXCLUSIVE;
            break;
            
        case TIOCNXCL:
            tty->flags &= ~TTY_EXCLUSIVE;
            break;
            
        case TCXONC:
            switch (arg) {
                case TCOOFF:
                    tty->flags |= TTY_STOPPED;
                    break;
                case TCOON:
                    tty->flags &= ~TTY_STOPPED;
                    wake_up(&tty->write_wait);
                    break;
                case TCIOFF:
                    tty_put_char(tty, tty->termios.c_cc[VSTOP]);
                    break;
                case TCION:
                    tty_put_char(tty, tty->termios.c_cc[VSTART]);
                    break;
                default:
                    ret = -1;
            }
            break;

        case TCFLSH:
            switch (arg) {
                case TCIFLUSH:
                    tty->read_head = tty->read_tail;
                    tty->read_cnt = 0;
                    tty->canon_head = 0;
                    tty->canon_lines = 0;
                    break;
                case TCOFLUSH:
                    // Output is synchronous, nothing to flush
                    break;
                case TCIOFLUSH:
                    tty->read_head = tty->read_tail;
                    tty->read_cnt = 0;
                    tty->canon_head = 0;
                    tty->canon_lines = 0;
                    break;
                default:
                    ret = -1;
            }
            break;

        case FIONREAD: {
            int avail = input_available(tty);
            spinlock_release_irqrestore(&tty->lock, flags);
            if (copy_to_user((void*)arg, &avail, sizeof(int)) != 0)
                return -1;

            return 0;
        }
            
        case TIOCOUTQ: {
            int zero = 0;
            spinlock_release_irqrestore(&tty->lock, flags);
            if (copy_to_user((void*)arg, &zero, sizeof(int)) != 0)
                return -1;

            return 0;
        }
            
        case TCSBRK:
        case TCSBRKP:
            break;
        
        case TIOCSTI: {
            u8 c;
            spinlock_release_irqrestore(&tty->lock, flags);
            if (copy_from_user(&c, (void*)arg, sizeof(u8)) != 0)
                return -1;

            tty_receive_char(tty, c);
            return 0;
        }

        case FIONBIO:
            break;
            
        // Line discipline
        case TIOCGETD: {
            int ldisc = tty->termios.c_line;
            spinlock_release_irqrestore(&tty->lock, flags);
            if (copy_to_user((void*)arg, &ldisc, sizeof(int)) != 0)
                return -1;

            return 0;
        }
            
        case TIOCSETD: {
            int ldisc;
            spinlock_release_irqrestore(&tty->lock, flags);
            if (copy_from_user(&ldisc, (void*)arg, sizeof(int)) != 0)
                return -1;

            flags = spinlock_acquire_irqsave(&tty->lock);
            tty->termios.c_line = ldisc;
            break;
        }
            
        default:
            ret = -ENOTTY;
    }
    
    spinlock_release_irqrestore(&tty->lock, flags);
    return ret;
}

u64 tty_read(struct vfs_node *file, u64 offset, u64 size, u8 *buffer) {
    tty_t *tty = NULL;
    if (current_task && current_task->proc)
        tty = current_task->proc->controlling_tty;

    if (!tty) tty = &tty_console;
    
    return n_tty_read(tty, buffer, size);
}

u64 tty_write(struct vfs_node *file, u64 offset, u64 size, u8 *buffer) {
    tty_t *tty = NULL;
    if (current_task && current_task->proc)
        tty = current_task->proc->controlling_tty;

    if (!tty) tty = &tty_console;
    
    return n_tty_write(tty, buffer, size);
}

void tty_open(struct vfs_node *file) {
    tty_t *tty = &tty_console;
    
    u32 flags = spinlock_acquire_irqsave(&tty->lock);
    tty->count++;
    
    if (current_task && current_task->proc) {
        process_t *proc = current_task->proc;
        if (proc->session_leader && proc->controlling_tty == NULL) {
            proc->controlling_tty = tty;
            tty->session = proc;
            tty->session_id = proc->sid;
            tty->pgrp = proc->pgid;
        }
    }
    
    spinlock_release_irqrestore(&tty->lock, flags);
}

void tty_close(struct vfs_node *file) {
    tty_t *tty = &tty_console;
    
    u32 flags = spinlock_acquire_irqsave(&tty->lock);
    tty->count--;
    
    if (tty->count == 0 && tty->session) {
        spinlock_release_irqrestore(&tty->lock, flags);
        tty_signal_session(tty, SIGHUP);
        flags = spinlock_acquire_irqsave(&tty->lock);
        
        tty->session = NULL;
        tty->session_id = 0;
        tty->pgrp = 0;
    }
    
    spinlock_release_irqrestore(&tty->lock, flags);
}

// Legacy Interface (for compatibility)
u64 tty_console_read(struct vfs_node *file, u64 format, u64 size, u8 *buffer) {
    return tty_read(file, format, size, buffer);
}

u64 tty_console_write(struct vfs_node *file, u64 format, u64 size, u8 *buffer) {
    return tty_write(file, format, size, buffer);
}

void tty_push_char(char c, tty_t *tty) {
    tty_receive_char(tty, (u8)c);
}

void tty_flush_input(tty_t *tty) {
    u32 flags = spinlock_acquire_irqsave(&tty->lock);
    tty->read_head = tty->read_tail;
    tty->read_cnt = 0;
    tty->canon_head = 0;
    tty->canon_lines = 0;
    spinlock_release_irqrestore(&tty->lock, flags);
}

void tty_flush_output(tty_t *tty) {
    // Output is currently synchronous, nothing to flush
}

void tty_hangup(tty_t *tty) {
    u32 flags = spinlock_acquire_irqsave(&tty->lock);
    
    tty->flags |= TTY_HUPPED;
    
    tty->read_head = tty->read_tail;
    tty->read_cnt = 0;
    tty->canon_head = 0;
    tty->canon_lines = 0;
    
    spinlock_release_irqrestore(&tty->lock, flags);

    if (tty->pgrp > 0) {
        signal_send_group(tty->pgrp, SIGHUP);
        signal_send_group(tty->pgrp, SIGCONT);
    }
    
    wake_up(&tty->read_wait);
    wake_up(&tty->write_wait);
}

int tty_set_termios(tty_t *tty, struct termios *new_termios) {
    u32 flags = spinlock_acquire_irqsave(&tty->lock);
    memcpy(&tty->termios, new_termios, sizeof(struct termios));
    spinlock_release_irqrestore(&tty->lock, flags);
    return 0;
}

int tty_set_pgrp(tty_t *tty, u64 pgrp) {
    u32 flags = spinlock_acquire_irqsave(&tty->lock);
    tty->pgrp = pgrp;
    spinlock_release_irqrestore(&tty->lock, flags);
    return 0;
}

u64 tty_get_pgrp(tty_t *tty) {
    return tty->pgrp;
}

static void init_tty_struct(tty_t *tty) {
    memset(tty, 0, sizeof(tty_t));
    
    memcpy(&tty->termios, &tty_std_termios, sizeof(struct termios));
    
    tty->winsize.ws_col = 80;
    tty->winsize.ws_row = 25;
    tty->winsize.ws_xpixel = 0;
    tty->winsize.ws_ypixel = 0;
    
    tty->read_head = 0;
    tty->read_tail = 0;
    tty->read_cnt = 0;
    tty->canon_head = 0;
    tty->canon_column = 0;
    tty->canon_lines = 0;
    tty->column = 0;
    
    tty->fg_color = 7;
    tty->bg_color = 0;
    tty->state = TTY_STATE_NORMAL;
    
    tty->read_wait = NULL;
    tty->write_wait = NULL;
}

void tty_init() {
    memset(tty_table, 0, sizeof(tty_table));
    
    init_tty_struct(&tty_console);
    tty_console.index = 0;
    tty_console.lock = console_lock;
    
#ifdef ARM
    tty_console.putc = ramfb_putc;
    tty_console.winsize.ws_col = WIDTH / 8;
    tty_console.winsize.ws_row = HEIGHT / 10;
    tty_console.winsize.ws_xpixel = WIDTH;
    tty_console.winsize.ws_ypixel = HEIGHT;
#endif
    
    tty_table[0] = &tty_console;
    
    init_tty_struct(&tty_serial);
    tty_serial.index = 1;
    tty_serial.lock = serial_lock;
    
#ifdef ARM
    tty_serial.putc = uart_putc;
#endif
    
    tty_table[1] = &tty_serial;
    
    tty_console_ops.read  = tty_read;
    tty_console_ops.write = tty_write;
    tty_console_ops.ioctl = tty_ioctl;
    tty_console_ops.open  = tty_open;
    tty_console_ops.close = tty_close;
        
    stdin_ops  = tty_console_ops;
    stdout_ops = tty_console_ops;
    stderr_ops = tty_console_ops;
}