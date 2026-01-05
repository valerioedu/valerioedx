#ifndef STDDEF_H
#define STDDEF_H

#define NULL ((void*)0)

typedef unsigned long size_t;
typedef long ptrdiff_t;
typedef unsigned int wchar_t;   // 32 bits for UTF32

typedef struct { 
    long long __max_align_ll;
    long double __max_align_ld;
} max_align_t;

#endif