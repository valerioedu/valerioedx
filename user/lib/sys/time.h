#ifndef SYS_TIME_H
#define SYS_TIME_H

#include <sys/types.h>
#include <time.h>

typedef long long suseconds_t;

struct timeval {
    time_t      tv_sec;
    suseconds_t tv_usec;
};

struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};

int gettimeofday(struct timeval *restrict tv, struct timezone *restrict tz);
int settimeofday(const struct timeval *tv, const struct timezone *tz);

#endif