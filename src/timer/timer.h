#pragma once

#include <stdint.h>
#include <time.h>

#define CLOCK_REALTIME 0   // wall time, UNIX timestamp. not reliable
#define CLOCK_MONOTONIC 1  // monotonic time. always increment by 1 so reliable

const uint64_t k_idle_timeout_ms = 5 * 1000;

// struct timespec {
//     time_t tv_sec;  // seconds
//     long tv_nsec;   // nanoseconds [0, 999'999'999]
// };

// return a timer handle
int timerfd_create(int clockid, int flags);
// set the expiration time
int timerfd_settime(int fd, int flags, const struct itimerspec *new_value,
                    struct itimerspec *old_value);

// syscall to ge the time
int clock_gettime(clockid_t clockif, struct timespec *tp);

// monotonic time cannot be adjusted and moves only forward.
// it is not related to any real-world time keeping.
// only used to measure durations
static uint64_t get_monotonic_msec() {
    struct timespec tv = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &tv);
    return uint64_t(tv.tv_sec) * 1000 + tv.tv_nsec / 1000 / 1000;
}