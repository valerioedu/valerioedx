#ifndef STDDEF_H
#define STDDEF_H

#ifdef __cplusplus
    #define NULL 0
#else
    #define NULL ((void*)0)
#endif
#define offsetof(type, member) ((size_t) &((type *)0)->member)

typedef unsigned long size_t;
typedef long ptrdiff_t;
#ifndef __cplusplus
typedef unsigned int wchar_t;   // 32 bits for UTF32
#endif

typedef struct { 
    long long __max_align_ll;
    long double __max_align_ld;
} max_align_t;

#endif