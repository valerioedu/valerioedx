#include <lib.h>
#include <string.h>

u64 strlen(const char* str) {
    if (str == NULL) return 0;

    u64 len = 0;
    while (str[len] != '\0') len++;

    return len;
}

int strcmp(const char* s1, const char* s2) {
    const unsigned char *a = (const unsigned char*)(s1 ? s1 : "");
    const unsigned char *b = (const unsigned char*)(s2 ? s2 : "");

    while (*a && (*a == *b)) {
        a++; b++;
    }

    return (*a > *b) - (*a < *b);
}

int strncmp(const char* s1, const char* s2, u64 n) {
    const unsigned char *a = (const unsigned char*)(s1 ? s1 : "");
    const unsigned char *b = (const unsigned char*)(s2 ? s2 : "");

    for (; n > 0; --n, ++a, ++b) {
        if (*a != *b) return (*a > *b) - (*a < *b);
        if (*a == '\0') break;
    }

    return 0;
}

void* memcpy(void* dest, const void* src, size_t n) {
    u8* d = (u8*)dest;
    const u8* s = (const u8*)src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

void* memset(void* dest, int c, size_t n) {
    u8* d = (u8*)dest;
    while (n--) {
        *d++ = (u8)c;
    }
    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *a1 = (const unsigned char*)s1;
    const unsigned char *a2 = (const unsigned char*)s2;
    for (; n > 0; --n, ++a1, ++a2) {
        if (*a1 != *a2) return (*a1 > *a2) - (*a1 < *a2);
    }
    return 0;
}

char* strncpy(char* dest, const char* src, size_t n) {
    char* d = dest;
    while (n && (*d++ = *src++)) {
        n--;
    }
    if (n) {
        while (--n) {
            *d++ = '\0';
        }
    }
    return dest;
}

char* strtok(char *str, const char *delim) {
    static char *last = NULL;
    if (str == NULL) str = last;
    while (*str && strchr(delim, *str)) str++;
    if (*str == '\0') return NULL;
    char *start = str;
    while (*str && !strchr(delim, *str)) str++;
    if (*str) {
        *str++ = '\0';
    }
    last = str;
    return start;
}

char* strtok_r(char *str, const char *delim, char **saveptr) {
    if (str == NULL) str = *saveptr;
    if (str == NULL) return NULL;
    
    while (*str && strchr(delim, *str)) str++;
    if (*str == '\0') return NULL;
    
    char *start = str;
    while (*str && !strchr(delim, *str)) str++;
    if (*str) {
        *str++ = '\0';
    }
    *saveptr = str;
    
    return start;
}

char* strncat(char* dest, const char* src, size_t n) {
    char* d = dest + strlen(dest);
    while (n && (*d++ = *src++)) {
        n--;
    }
    if (n) {
        *d = '\0';
    }
    return dest;
}

char *strcat(char *dest, const char *src) {
    char *d = dest + strlen(dest);
    while ((*d++ = *src++));
    return dest;
}

char* strchr(const char* str, int c) {
    while (*str) {
        if (*str == (char)c) {
            return (char*)str;
        }
        str++;
    }
    return NULL;
}

char* strrchr(const char* str, int c) {
    const char* last = NULL;
    while (*str) {
        if (*str == (char)c) {
            last = str;
        }
        str++;
    }
    return (char*)last;
}

char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++));
    return dest;
}

void* memmove(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    if (d < s) {
        while (n--) {
            *d++ = *s++;
        }
    } else {
        d += n;
        s += n;
        while (n--) {
            *(--d) = *(--s);
        }
    }
    return dest;
}