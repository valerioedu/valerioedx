#ifndef STDIO_H
#define STDIO_H

#include <stdarg.h>
#include <stddef.h>

#define BUFSIZ 1024
#define FOPEN_MAX 20
#define EOF (-1)

typedef struct _FILE {
    int fd;
    int flags;
    char *buf;
    char *ptr;
    int cnt;
    int bufsiz;
    char smallbuf[1];
} FILE;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

int getc(FILE *stream);
int ungetc(int c, FILE *stream);
int fputc(int c, FILE *stream);
int getchar(void);
int putchar(int c);

// String I/O
char *fgets(char *s, int n, FILE *stream);
int fputs(const char *s, FILE *stream);
int puts(const char *s);

// Formatted I/O
int printf(const char *restrict format, ...);
int fprintf(FILE *stream, const char *format, ...);
int vprintf(const char *format, va_list ap);
int vfprintf(FILE *stream, const char *format, va_list ap);
int scanf(const char *restrict format, ...);
int fscanf(FILE *stream, const char *restrict format, ...);
int vfscanf(FILE *stream, const char *restrict format, va_list ap);
int sscanf(const char *str, const char *format, ...);

// Block I/O
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);

// Stream operations
int fflush(FILE *stream);

// Error handling
int feof(FILE *stream);
int ferror(FILE *stream);
void clearerr(FILE *stream);

int vsnprintf(char *str, size_t size, const char *format, va_list ap);
int snprintf(char *str, size_t size, const char *format, ...);

FILE *fopen(const char *filename, const char *mode);
int fclose(FILE *stream);

#endif