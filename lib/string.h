#ifndef STRING_H
#define STRING_H

#include <lib.h>

u64 strlen(const char* str);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, u64 n);
char* strncat(char* dest, const char* src, size_t n);
char *strcat(char *dest, const char *src);
char* strchr(const char* str, int c);
char* strrchr(const char* str, int c);
char* strncpy(char* dest, const char* src, size_t n);
char* strcpy(char* dest, const char* src);
char* strtok(char *str, const char *delim);
char* strtok_r(char *str, const char *delim, char **saveptr);

void* memcpy(void* dest, const void* src, size_t n);
void* memset(void* dest, int c, size_t n);
void* memmove(void* dest, const void* src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);

#endif