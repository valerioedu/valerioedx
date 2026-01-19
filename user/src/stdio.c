#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdbool.h>
#include <ctype.h>

#define _READ   0x01
#define _WRITE  0x02
#define _UNBUF  0x04
#define _EOF    0x08
#define _ERR    0x10

static char _stdin_buf[BUFSIZ];
static char _stdout_buf[BUFSIZ];
static char _stderr_buf[1];

static FILE _stdin_file  = { 0, _READ,  _stdin_buf,  _stdin_buf,  0, BUFSIZ, {0} };
static FILE _stdout_file = { 1, _WRITE, _stdout_buf, _stdout_buf, 0, BUFSIZ, {0} };
static FILE _stderr_file = { 2, _WRITE | _UNBUF, _stderr_buf, _stderr_buf, 0, 1, {0} };

FILE *stdin  = &_stdin_file;
FILE *stdout = &_stdout_file;
FILE *stderr = &_stderr_file;

static FILE _iob[FOPEN_MAX] = {0};

int fflush(FILE *stream) {
    if (stream == NULL) {
        fflush(stdout);
        fflush(stderr);
        return 0;
    }
    
    if (!(stream->flags & _WRITE))
        return 0;
    
    int n = stream->ptr - stream->buf;
    if (n > 0) {
        if (write(stream->fd, stream->buf, n) != n) {
            stream->flags |= _ERR;
            return EOF;
        }
    }
    stream->ptr = stream->buf;
    stream->cnt = stream->bufsiz;
    return 0;
}

static int _fillbuf(FILE *stream) {
    if (!(stream->flags & _READ) || (stream->flags & (_EOF | _ERR)))
        return EOF;

    int toread = stream->bufsiz;
    if (stream->fd == 0)
        toread = 1;
    
    int n = read(stream->fd, stream->buf, toread);
    if (n <= 0) {
        stream->flags |= (n == 0) ? _EOF : _ERR;
        return EOF;
    }

    stream->cnt = n - 1;
    stream->ptr = stream->buf;
    return (unsigned char)*stream->ptr++;
}

int getc(FILE *stream) {
    if (--stream->cnt >= 0)
        return (unsigned char)*stream->ptr++;

    return _fillbuf(stream);
}

int fputc(int c, FILE *stream) {
    if (!(stream->flags & _WRITE))
        return EOF;
    
    if (stream->flags & _UNBUF) {
        char ch = c;
        if (write(stream->fd, &ch, 1) != 1) {
            stream->flags |= _ERR;
            return EOF;
        }

        return (unsigned char)c;
    }
    
    if (stream->cnt <= 0) {
        if (fflush(stream) == EOF)
            return EOF;
    }
    
    *stream->ptr++ = c;
    stream->cnt--;
    
    if (c == '\n')
        fflush(stream);
    
    return (unsigned char)c;
}

int getchar(void) {
    return getc(stdin);
}

int putchar(int c) {
    return fputc(c, stdout);
}

char *fgets(char *s, int n, FILE *stream) {
    char *p = s;
    int c;
    
    while (--n > 0 && (c = getc(stream)) != EOF) {
        if (c == '\b') {
            if (p > s) p--;

            n++;
        } else {
            *p++ = c;
            if (c == '\n') break;
        }
    }

    *p = '\0';
    return (p == s && c == EOF) ? NULL : s;
}

int fputs(const char *s, FILE *stream) {
    while (*s) {
        if (fputc(*s++, stream) == EOF)
            return EOF;
    }

    return 0;
}

int puts(const char *s) {
    if (fputs(s, stdout) == EOF)
        return EOF;

    return fputc('\n', stdout);
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    const unsigned char *p = ptr;
    size_t total = size * nmemb;
    size_t written = 0;
    
    for (size_t i = 0; i < total; i++) {
        if (fputc(p[i], stream) == EOF)
            return written / size;

        written++;
    }

    return nmemb;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    unsigned char *p = ptr;
    size_t total = size * nmemb;
    size_t nread = 0;
    
    for (size_t i = 0; i < total; i++) {
        int c = getc(stream);
        if (c == EOF)
            return nread / size;

        p[i] = c;
        nread++;
    }

    return nmemb;
}

int feof(FILE *stream) {
    return (stream->flags & _EOF) != 0;
}

int ferror(FILE *stream) {
    return (stream->flags & _ERR) != 0;
}

void clearerr(FILE *stream) {
    stream->flags &= ~(_EOF | _ERR);
}

static void _itoa(long n, char *s, int base, bool uppercase) {
    static const char *lower = "0123456789abcdef";
    static const char *upper = "0123456789ABCDEF";
    const char *digits = uppercase ? upper : lower;
    char buf[32];
    int i = 0;
    bool neg = false;
    unsigned long un;
    
    if (n < 0 && base == 10) {
        neg = true;
        un = -n;
    } else {
        un = (unsigned long)n;
    }
    
    do {
        buf[i++] = digits[un % base];
        un /= base;
    } while (un);
    
    if (neg)
        buf[i++] = '-';
    
    int j = 0;
    while (i > 0)
        s[j++] = buf[--i];

    s[j] = '\0';
}

static void _uitoa(unsigned long n, char *s, int base, bool uppercase) {
    static const char *lower = "0123456789abcdef";
    static const char *upper = "0123456789ABCDEF";
    const char *digits = uppercase ? upper : lower;
    char buf[32];
    int i = 0;
    
    do {
        buf[i++] = digits[n % base];
        n /= base;
    } while (n);
    
    int j = 0;
    while (i > 0)
        s[j++] = buf[--i];

    s[j] = '\0';
}

int vfprintf(FILE *stream, const char *format, va_list ap) {
    int count = 0;
    char numbuf[32];
    
    for (const char *p = format; *p; p++) {
        if (*p != '%') {
            fputc(*p, stream);
            count++;
            continue;
        }
        
        p++;
        if (!*p) break;
        
        switch (*p) {
            case 'd':
            case 'i': {
                int n = va_arg(ap, int);
                _itoa(n, numbuf, 10, false);
                for (char *s = numbuf; *s; s++) {
                    fputc(*s, stream);
                    count++;
                }

                break;
            }
            case 'u': {
                unsigned int n = va_arg(ap, unsigned int);
                _uitoa(n, numbuf, 10, false);
                for (char *s = numbuf; *s; s++) {
                    fputc(*s, stream);
                    count++;
                }

                break;
            }
            case 'x': {
                unsigned int n = va_arg(ap, unsigned int);
                _uitoa(n, numbuf, 16, false);
                for (char *s = numbuf; *s; s++) {
                    fputc(*s, stream);
                    count++;
                }

                break;
            }
            case 'X': {
                unsigned int n = va_arg(ap, unsigned int);
                _uitoa(n, numbuf, 16, true);
                for (char *s = numbuf; *s; s++) {
                    fputc(*s, stream);
                    count++;
                }

                break;
            }
            case 'p': {
                void *ptr = va_arg(ap, void *);
                fputc('0', stream);
                fputc('x', stream);
                count += 2;
                _uitoa((unsigned long)ptr, numbuf, 16, false);
                for (char *s = numbuf; *s; s++) {
                    fputc(*s, stream);
                    count++;
                }

                break;
            }
            case 'f': {
                double num = va_arg(ap, double);
                if (num < 0) {
                    fputc('-', stream);
                    count++;
                    num = -num;
                }
                
                long int_part = (long)num;
                double frac = num - (double)int_part;
                frac += 0.5e-6;

                _uitoa((unsigned long)int_part, numbuf, 10, false);
                for (char *s = numbuf; *s; s++) {
                    fputc(*s, stream);
                    count++;
                }

                fputc('.', stream); count++;
                for (int i = 0; i < 6; i++) {
                    frac *= 10.0;
                    int digit = (int)frac;
                    fputc('0' + digit, stream); count++;
                    frac -= digit;
                }

                break;
            }
            case 's': {
                char *s = va_arg(ap, char *);
                if (!s) s = "(null)";
                while (*s) {
                    fputc(*s++, stream);
                    count++;
                }

                break;
            }
            case 'c': {
                int c = va_arg(ap, int);
                fputc(c, stream);
                count++;
                break;
            }
            case '%':
                fputc('%', stream);
                count++;
                break;
            case 'l': {
                p++;
                if (*p == 'd' || *p == 'i') {
                    long n = va_arg(ap, long);
                    _itoa(n, numbuf, 10, false);
                    for (char *s = numbuf; *s; s++) {
                        fputc(*s, stream);
                        count++;
                    }
                } else if (*p == 'u') {
                    unsigned long n = va_arg(ap, unsigned long);
                    _uitoa(n, numbuf, 10, false);
                    for (char *s = numbuf; *s; s++) {
                        fputc(*s, stream);
                        count++;
                    }
                } else if (*p == 'x') {
                    unsigned long n = va_arg(ap, unsigned long);
                    _uitoa(n, numbuf, 16, false);
                    for (char *s = numbuf; *s; s++) {
                        fputc(*s, stream);
                        count++;
                    }
                }

                break;
            }
            default:
                fputc('%', stream);
                fputc(*p, stream);
                count += 2;
                break;
        }
    }
    
    return count;
}

int fprintf(FILE *stream, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vfprintf(stream, format, ap);
    va_end(ap);
    return ret;
}

int printf(const char *restrict format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vfprintf(stdout, format, ap);
    va_end(ap);
    return ret;
}

int vprintf(const char *format, va_list ap) {
    return vfprintf(stdout, format, ap);
}

int vsnprintf(char *str, size_t size, const char *format, va_list ap) {
    size_t idx = 0;
    int total = 0;
    char numbuf[32];

    for (const char *p = format; *p; p++) {
        if (*p != '%') {
            if (size > 0 && idx < size - 1) str[idx] = *p;
            idx++;
            total++;
            continue;
        }

        p++;
        if (!*p) break;

        switch (*p) {
            case 'd':
            case 'i': {
                int n = va_arg(ap, int);
                _itoa(n, numbuf, 10, false);
                for (char *s = numbuf; *s; s++) {
                    if (size > 0 && idx < size - 1)
                        str[idx] = *s;
                    
                    idx++; total++;
                }

                break;
            }
            case 'u': {
                unsigned int n = va_arg(ap, unsigned int);
                _uitoa(n, numbuf, 10, false);
                for (char *s = numbuf; *s; s++) {
                    if (size > 0 && idx < size - 1)
                        str[idx] = *s;
                    
                    idx++; total++;
                }

                break;
            }
            case 'x': {
                unsigned int n = va_arg(ap, unsigned int);
                _uitoa(n, numbuf, 16, false);
                for (char *s = numbuf; *s; s++) {
                    if (size > 0 && idx < size - 1)
                        str[idx] = *s;
                    
                    idx++; total++;
                }

                break;
            }
            case 'X': {
                unsigned int n = va_arg(ap, unsigned int);
                _uitoa(n, numbuf, 16, true);
                for (char *s = numbuf; *s; s++) {
                    if (size > 0 && idx < size - 1)
                        str[idx] = *s;
                    
                    idx++; total++;
                }

                break;
            }
            case 'p': {
                void *ptr = va_arg(ap, void *);
                if (size > 0 && idx < size - 1)
                    str[idx] = '0';
                    
                idx++; total++;
                
                if (size > 0 && idx < size - 1)
                    str[idx] = 'x';
                    
                idx++; total++;
                
                _uitoa((unsigned long)ptr, numbuf, 16, false);
                for (char *s = numbuf; *s; s++) {
                    if (size > 0 && idx < size - 1)
                        str[idx] = *s;
                    
                    idx++; total++;
                }

                break;
            }
            case 'f': {
                double num = va_arg(ap, double);
                if (num < 0) {
                    if (size > 0 && idx < size - 1)
                        str[idx] = '-';
                    
                    idx++; total++;
                    num = -num;
                }

                long int_part = (long)num;
                double frac = num - (double)int_part;
                frac += 0.5e-6;

                _uitoa((unsigned long)int_part, numbuf, 10, false);
                for (char *s = numbuf; *s; s++) {
                    if (size > 0 && idx < size - 1)
                        str[idx] = *s;
                        
                    idx++; total++;
                }

                if (size > 0 && idx < size - 1)
                    str[idx] = '.'; 
                
                idx++; total++;
                for (int i = 0; i < 6; i++) {
                    frac *= 10.0;
                    int digit = (int)frac;
                    if (size > 0 && idx < size - 1)
                        str[idx] = '0' + digit;
                    
                    idx++; total++;
                    frac -= digit;
                }

                break;
            }
            case 's': {
                char *s = va_arg(ap, char *);
                if (!s) s = "(null)";
                while (*s) {
                    if (size > 0 && idx < size - 1)
                        str[idx] = *s;
                    
                    idx++; total++; s++;
                }

                break;
            }
            case 'c': {
                int c = va_arg(ap, int);
                if (size > 0 && idx < size - 1)
                    str[idx] = c;
                
                idx++; total++;
                break;
            }
            case '%':
                if (size > 0 && idx < size - 1)
                    str[idx] = '%';

                idx++; total++;
                break;
            case 'l': {
                p++;
                if (*p == 'd' || *p == 'i') {
                    long n = va_arg(ap, long);
                    _itoa(n, numbuf, 10, false);
                    for (char *s = numbuf; *s; s++) {
                        if (size > 0 && idx < size - 1)
                            str[idx] = *s;
                            
                        idx++; total++;
                    }
                } else if (*p == 'u') {
                    unsigned long n = va_arg(ap, unsigned long);
                    _uitoa(n, numbuf, 10, false);
                    for (char *s = numbuf; *s; s++) {
                        if (size > 0 && idx < size - 1)
                            str[idx] = *s;
                        
                        idx++; total++;
                    }
                } else if (*p == 'x') {
                    unsigned long n = va_arg(ap, unsigned long);
                    _uitoa(n, numbuf, 16, false);
                    for (char *s = numbuf; *s; s++) {
                        if (size > 0 && idx < size - 1)
                            str[idx] = *s;
                        
                        idx++; total++;
                    }
                }

                break;
            }
            default:
                if (size > 0 && idx < size - 1)
                    str[idx] = '%';
                
                idx++; total++;

                if (size > 0 && idx < size - 1)
                    str[idx] = *p;
                
                idx++; total++;
                break;
        }
    }

    if (size > 0) {
        if (idx < size) str[idx < size ? idx : size - 1] = '\0';
        else str[size - 1] = '\0';
    }

    return total;
}

int snprintf(char *str, size_t size, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vsnprintf(str, size, format, ap);
    va_end(ap);
    return ret;
}

int ungetc(int c, FILE *stream) {
    if (c == EOF || !stream) return EOF;
    
    if (stream->ptr > stream->buf) {
        stream->ptr--;
        stream->cnt++;
        *stream->ptr = (char)c;
        stream->flags &= ~_EOF;
    
        return (unsigned char)c;
    }

    return EOF;
}

int vfscanf(FILE *stream, const char *format, va_list ap) {
    int count = 0;
    const char *p = format;

    while (*p) {
        if (isspace(*p)) {
            int c;
            while (isspace(c = getc(stream)));
            ungetc(c, stream);
            p++;
            continue;
        }

        if (*p != '%') {
            int c = getc(stream);
            if (c == EOF) return count > 0 ? count : EOF;
            if (c != *p) {
                ungetc(c, stream);
                return count;
            }

            p++;
            continue;
        }

        p++;    // skip '%'
        if (*p == '\0') break;

        int is_long = 0;
        if (*p == 'l') {
            is_long = 1;
            p++;
        }

        switch (*p) {
            case 'd':
            case 'i':
            case 'u': {
                int c;
                while (isspace(c = getc(stream)));
                
                if (c == EOF) return count > 0 ? count : EOF;

                int sign = 1;
                if (c == '-') {
                    sign = -1;
                    c = getc(stream);
                } else if (c == '+') {
                    c = getc(stream);
                }

                if (!isdigit(c)) {
                    ungetc(c, stream);
                    return count;
                }

                long num = 0;
                while (isdigit(c)) {
                    num = num * 10 + (c - '0');
                    c = getc(stream);
                }

                ungetc(c, stream); // Push back the non-digit

                if (*p == 'u') {
                    if (is_long)
                        *va_arg(ap, unsigned long*) = num;
                    
                    else *va_arg(ap, unsigned int*) = (unsigned int)num;
                } else {
                    num *= sign;
                    if (is_long)
                        *va_arg(ap, long*) = num;
                    
                    else *va_arg(ap, int*) = (int)num;
                }

                count++;
                break;
            }
            case 'x':
            case 'X': {
                int c;
                while (isspace(c = getc(stream)));
                if (c == EOF) return count > 0 ? count : EOF;

                if (!isxdigit(c)) {
                    ungetc(c, stream);
                    return count;
                }

                unsigned long num = 0;
                while (isxdigit(c)) {
                    int val;
                    if (c >= '0' && c <= '9')
                        val = c - '0';
                        
                    else if (c >= 'a' && c <= 'f')
                        val = c - 'a' + 10;
                    
                    else val = c - 'A' + 10;
                    
                    num = num * 16 + val;
                    c = getc(stream);
                }

                ungetc(c, stream);

                if (is_long)
                    *va_arg(ap, unsigned long*) = num;
                
                else *va_arg(ap, unsigned int*) = (unsigned int)num;
                count++;
                break;
            }
            case 's': {
                char *s = va_arg(ap, char *);
                int c;
                while (isspace(c = getc(stream)));
                
                if (c == EOF) return count > 0 ? count : EOF;

                while (c != EOF && !isspace(c)) {
                    *s++ = c;
                    c = getc(stream);
                }

                *s = '\0';
                ungetc(c, stream);
                count++;
                break;
            }
            case 'c': {
                char *s = va_arg(ap, char *);
                int c = getc(stream);
                if (c == EOF) return count > 0 ? count : EOF;
                *s = c;
                count++;
                break;
            }
            case '%': {
                int c;
                while (isspace(c = getc(stream)));
                if (c != '%') {
                    ungetc(c, stream);
                    return count;
                }

                break;
            }
            default:
                return count;
        }
        p++;
    }

    return count;
}

int fscanf(FILE *stream, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vfscanf(stream, format, ap);
    va_end(ap);
    return ret;
}

int scanf(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vfscanf(stdin, format, ap);
    va_end(ap);
    return ret;
}