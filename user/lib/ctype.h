#ifndef _CTYPE_H
#define _CTYPE_H

#define _CTYPE_U    0x01    //Upper case
#define _CTYPE_L    0x02    //Lower case
#define _CTYPE_D    0x04    //Digit
#define _CTYPE_S    0x08    //Space
#define _CTYPE_P    0x10    //Punctuation
#define _CTYPE_C    0x20    //Control
#define _CTYPE_X    0x40    //Hex digit (A-F, a-f)
#define _CTYPE_B    0x80    //Blank (space, tab)

#define _CTYPE_A    (_CTYPE_U | _CTYPE_L)               //Alpha
#define _CTYPE_AN   (_CTYPE_U | _CTYPE_L | _CTYPE_D)    //Alphanumeric
#define _CTYPE_G    (_CTYPE_AN | _CTYPE_P)              //Graph (visible)
#define _CTYPE_PR   (_CTYPE_G | _CTYPE_B)               //Printable

extern const unsigned char __ctype_table[256];

static inline int isalnum(int c) {
    return (unsigned)c < 256 && (__ctype_table[(unsigned char)c] & _CTYPE_AN);
}

static inline int isalpha(int c) {
    return (unsigned)c < 256 && (__ctype_table[(unsigned char)c] & _CTYPE_A);
}

static inline int isascii(int c) {
    return (unsigned)c <= 0x7F;
}

static inline int isblank(int c) {
    return (unsigned)c < 256 && (__ctype_table[(unsigned char)c] & _CTYPE_B);
}

static inline int iscntrl(int c) {
    return (unsigned)c < 256 && (__ctype_table[(unsigned char)c] & _CTYPE_C);
}

static inline int isdigit(int c) {
    return (unsigned)c < 256 && (__ctype_table[(unsigned char)c] & _CTYPE_D);
}

static inline int isgraph(int c) {
    return (unsigned)c < 256 && (__ctype_table[(unsigned char)c] & _CTYPE_G);
}

static inline int islower(int c) {
    return (unsigned)c < 256 && (__ctype_table[(unsigned char)c] & _CTYPE_L);
}

static inline int isprint(int c) {
    return (unsigned)c < 256 && (__ctype_table[(unsigned char)c] & _CTYPE_PR);
}

static inline int ispunct(int c) {
    return (unsigned)c < 256 && (__ctype_table[(unsigned char)c] & _CTYPE_P);
}

static inline int isspace(int c) {
    return (unsigned)c < 256 && (__ctype_table[(unsigned char)c] & _CTYPE_S);
}

static inline int isupper(int c) {
    return (unsigned)c < 256 && (__ctype_table[(unsigned char)c] & _CTYPE_U);
}

static inline int isxdigit(int c) {
    return (unsigned)c < 256 && (__ctype_table[(unsigned char)c] & (_CTYPE_D | _CTYPE_X));
}

static inline int tolower(int c) {
    if ((unsigned)c < 256 && (__ctype_table[(unsigned char)c] & _CTYPE_U))
        return c + ('a' - 'A');
    return c;
}

static inline int toupper(int c) {
    if ((unsigned)c < 256 && (__ctype_table[(unsigned char)c] & _CTYPE_L))
        return c - ('a' - 'A');
    return c;
}

static inline int toascii(int c) {
    return c & 0x7F;
}

#endif