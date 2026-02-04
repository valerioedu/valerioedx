#include <errno.h>

//TODO: implement thread local
_Thread_local int errno_value = 0;

int *__error() {
    return &errno_value;
}