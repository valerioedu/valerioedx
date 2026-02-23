#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#define PASSWD_FILE "/etc/passwd"
#define PASSWD_BUFSZ 1024

static struct passwd pw_entry;
static char pw_buf[PASSWD_BUFSZ];
static int pw_fd = -1;
static off_t pw_offset = 0;

static char pw_name[64];
static char pw_passwd[128];
static char pw_gecos[128];
static char pw_dir[128];
static char pw_shell[128];

static int parse_passwd_line(const char *line, struct passwd *pw) {
    char *fields[7];
    char *buf_copy = pw_buf;
    int i = 0;

    strncpy(buf_copy, line, PASSWD_BUFSZ - 1);
    buf_copy[PASSWD_BUFSZ - 1] = '\0';

    char *nl = strchr(buf_copy, '\n');
    if (nl) *nl = '\0';

    fields[0] = buf_copy;
    for (char *p = buf_copy; *p && i < 6; p++) {
        if (*p == ':') {
            *p = '\0';
            fields[++i] = p + 1;
        }
    }

    if (i != 6) return -1;

    strncpy(pw_name, fields[0], sizeof(pw_name) - 1);
    strncpy(pw_passwd, fields[1], sizeof(pw_passwd) - 1);
    strncpy(pw_gecos, fields[4], sizeof(pw_gecos) - 1);
    strncpy(pw_dir, fields[5], sizeof(pw_dir) - 1);
    strncpy(pw_shell, fields[6], sizeof(pw_shell) - 1);

    pw->pw_name = pw_name;
    pw->pw_passwd = pw_passwd;
    pw->pw_uid = (uid_t)atoi(fields[2]);
    pw->pw_gid = (gid_t)atoi(fields[3]);
    pw->pw_gecos = pw_gecos;
    pw->pw_dir = pw_dir;
    pw->pw_shell = pw_shell;

    return 0;
}

static int read_line(int fd, char *buf, int bufsz) {
    int i = 0;
    char c;

    while (i < bufsz - 1) {
        ssize_t n = read(fd, &c, 1);
        if (n <= 0) {
            if (i == 0) return -1;
            break;
        }

        buf[i++] = c;
        if (c == '\n') break;
    }

    buf[i] = '\0';
    return i;
}

struct passwd *getpwnam(const char *name) {
    char line[PASSWD_BUFSZ];

    int fd = open(PASSWD_FILE, O_RDONLY);
    if (fd < 0) return NULL;

    while (read_line(fd, line, sizeof(line)) > 0) {
        if (parse_passwd_line(line, &pw_entry) == 0) {
            if (strcmp(pw_entry.pw_name, name) == 0) {
                close(fd);
                return &pw_entry;
            }
        }
    }

    close(fd);
    return NULL;
}

struct passwd *getpwuid(uid_t uid) {
    char line[PASSWD_BUFSZ];
    
    int fd = open(PASSWD_FILE, O_RDONLY);
    if (fd < 0) return NULL;

    while (read_line(fd, line, sizeof(line)) > 0) {
        if (parse_passwd_line(line, &pw_entry) == 0) {
            if (pw_entry.pw_uid == uid) {
                close(fd);
                return &pw_entry;
            }
        }
    }

    close(fd);
    return NULL;
}

void setpwent(void) {
    if (pw_fd >= 0) close(pw_fd);
    pw_fd = open(PASSWD_FILE, O_RDONLY);
    pw_offset = 0;
}

void endpwent(void) {
    if (pw_fd >= 0) {
        close(pw_fd);
        pw_fd = -1;
    }
}

struct passwd *getpwent(void) {
    if (pw_fd < 0) return NULL;

    char line[PASSWD_BUFSZ];
    if (read_line(pw_fd, line, sizeof(line)) <= 0)
        return NULL;

    if (parse_passwd_line(line, &pw_entry) == 0)
        return &pw_entry;

    return NULL;
}