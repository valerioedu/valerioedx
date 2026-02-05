#ifndef TIME_H
#define TIME_H

#include <sys/types.h>

struct timespec {
    time_t tv_sec;
    long tv_nsec;
};

int nanosleep(const struct timespec*, struct timespec*);

#endif