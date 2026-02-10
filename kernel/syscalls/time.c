#include <syscalls.h>
#include <sched.h>
#include <heap.h>
#include <pl031.h>
#include <timer.h>

typedef i64 time_t;
typedef i64 suseconds_t;
typedef i64 clockid_t;

struct timeval  { time_t tv_sec; suseconds_t tv_usec; };
struct timespec { time_t tv_sec; i64 tv_nsec; };
struct timezone { int tz_minuteswest; int tz_dsttime; };

#define CLOCK_REALTIME       0
#define CLOCK_MONOTONIC      1
#define CLOCK_MONOTONIC_RAW  4

extern task_t *current_task;

i64 sys_gettimeofday(struct timeval *tv, struct timezone *tz) {
    if (tv) {
        u64 sec, nsec;
        get_realtime(&sec, &nsec);

        struct timeval ktv = { .tv_sec = sec, .tv_usec = nsec / 1000 };
        if (copy_to_user(tv, &ktv, sizeof(ktv)) < 0)
            return -1;
    }

    if (tz) {
        struct timezone ktz = { 0, 0 };
        if (copy_to_user(tz, &ktz, sizeof(ktz)) < 0)
            return -1;
    }

    return 0;
}

i64 sys_settimeofday(const struct timeval *tv, const struct timezone *tz) {
    if (!current_task || !current_task->proc)
        return -1;
    if (current_task->proc->euid != 0)
        return -1;

    if (tv) {
        struct timeval ktv;
        if (copy_from_user(&ktv, tv, sizeof(ktv)) < 0)
            return -1;
        pl031_set_time((u32)ktv.tv_sec);
    }

    return 0;
}

i64 sys_clock_gettime(clockid_t clk_id, struct timespec *tp) {
    if (!tp) return -1;

    struct timespec ktp;
    u64 sec, nsec;

    switch (clk_id) {
    case CLOCK_REALTIME:
        get_realtime(&sec, &nsec);
        break;
    case CLOCK_MONOTONIC:
    case CLOCK_MONOTONIC_RAW:
        get_monotonic_time(&sec, &nsec);
        break;
    default:
        return -1;
    }

    ktp.tv_sec  = sec;
    ktp.tv_nsec = nsec;

    if (copy_to_user(tp, &ktp, sizeof(ktp)) < 0)
        return -1;

    return 0;
}

i64 sys_clock_settime(clockid_t clk_id, const struct timespec *tp) {
    if (!current_task || !current_task->proc)
        return -1;
    if (current_task->proc->euid != 0)
        return -1;
    if (clk_id != CLOCK_REALTIME)
        return -1;

    struct timespec ktp;
    if (copy_from_user(&ktp, tp, sizeof(ktp)) < 0)
        return -1;

    pl031_set_time((u32)ktp.tv_sec);
    return 0;
}

i64 sys_clock_getres(clockid_t clk_id, struct timespec *res) {
    if (!res) return 0;

    struct timespec kres;

    switch (clk_id) {
    case CLOCK_REALTIME:
        kres.tv_sec = 1;
        kres.tv_nsec = 0;
        break;
    case CLOCK_MONOTONIC:
    case CLOCK_MONOTONIC_RAW:
        kres.tv_sec = 0;
        kres.tv_nsec = 1;
        break;
    default:
        return -1;
    }

    if (copy_to_user(res, &kres, sizeof(kres)) < 0)
        return -1;

    return 0;
}

i64 sys_time(time_t *tloc) {
    u64 sec, nsec;
    get_realtime(&sec, &nsec);

    if (tloc) {
        time_t ksec = sec;
        if (copy_to_user(tloc, &ksec, sizeof(ksec)) < 0)
            return -1;
    }

    return (i64)sec;
}