#ifndef STDLIB_H
#define STDLIB_H

#include <stddef.h>

#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0

extern char **environ;

#ifdef __cplusplus
extern "C" {
#endif
void *malloc(size_t size);
void *calloc(size_t nelem, size_t elsize);
void *realloc(void *ptr, size_t size);
void free(void *ptr);
int atoi(const char *str);
void exit(int status);
unsigned long strtoul(const char *nptr, char **endptr, int base);
char *getenv(const char *name);
int setenv(const char *name, const char *value, int overwrite);
int unsetenv(const char *name);
int posix_memalign(void **memptr, size_t alignment, size_t size);
#ifdef __cplusplus
}
#endif

#endif