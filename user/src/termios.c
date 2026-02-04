#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>

speed_t cfgetispeed(const struct termios *termios_p) {
    return termios_p->c_ispeed;
}

speed_t cfgetospeed(const struct termios *termios_p) {
    return termios_p->c_ospeed;
}

int cfsetispeed(struct termios *termios_p, speed_t speed) {
    termios_p->c_ispeed = speed;
    return 0;
}

int cfsetospeed(struct termios *termios_p, speed_t speed) {
    termios_p->c_ospeed = speed;
    return 0;
}

int cfsetspeed(struct termios *termios_p, speed_t speed) {
    termios_p->c_ispeed = speed;
    termios_p->c_ospeed = speed;
    return 0;
}

void cfmakeraw(struct termios *termios_p) {
    termios_p->c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP |
                            INLCR | IGNCR | ICRNL | IXON);
    
    termios_p->c_oflag &= ~OPOST;
    
    termios_p->c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    
    termios_p->c_cflag &= ~(CSIZE | PARENB);
    termios_p->c_cflag |= CS8;
    
    termios_p->c_cc[VMIN] = 1;
    termios_p->c_cc[VTIME] = 0;
}

int tcdrain(int fd) {
    // output is synchronous so this is a no-op
    return 0;
}

int tcflow(int fd, int action) {
    return ioctl(fd, TCXONC, action);
}

int tcflush(int fd, int queue_selector) {
    return ioctl(fd, TCFLSH, queue_selector);
}

int tcgetattr(int fd, struct termios *termios_p) {
    return ioctl(fd, TCGETS, termios_p);
}

pid_t tcgetsid(int fd) {
    pid_t sid;
    if (ioctl(fd, TIOCGSID, &sid) < 0)
        return -1;

    return sid;
}

// Not implemented for virtual terminals
int tcsendbreak(int fd, int duration) {
    return 0;
}

int tcsetattr(int fd, int optional_actions, const struct termios *termios_p) {
    int cmd;
    
    switch (optional_actions) {
        case TCSANOW:
            cmd = TCSETS;
            break;
        case TCSADRAIN:
            cmd = TCSETSW;
            break;
        case TCSAFLUSH:
            cmd = TCSETSF;
            break;
        default:
            errno = EINVAL;
            return -1;
    }
    
    return ioctl(fd, cmd, (void *)termios_p);
}

int tcgetwinsize(int fd, struct winsize *ws) {
    return ioctl(fd, TIOCGWINSZ, ws);
}

int tcsetwinsize(int fd, const struct winsize *ws) {
    return ioctl(fd, TIOCSWINSZ, (void *)ws);
}

pid_t tcgetpgrp(int fd) {
    pid_t pgrp;
    if (ioctl(fd, TIOCGPGRP, &pgrp) < 0)
        return -1;
    
    return pgrp;
}

int tcsetpgrp(int fd, pid_t pgrp) {
    return ioctl(fd, TIOCSPGRP, &pgrp);
}