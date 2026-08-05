// SPDX-License-Identifier: Apache-2.0
/**
 * @file host_wall_clock.h
 * @brief Physical wall clock microsecond helper.
 */
#ifndef HOST_WALL_CLOCK_H
#define HOST_WALL_CLOCK_H

#include <stdint.h>

#ifdef _WIN32
#include <windows.h>

static inline uint64_t host_wall_clock_us(void) {
    static LARGE_INTEGER freq = { .QuadPart = 0 };
    LARGE_INTEGER c;
    if (freq.QuadPart == 0) {
        QueryPerformanceFrequency(&freq);
    }
    QueryPerformanceCounter(&c);
    return (uint64_t)(c.QuadPart * 1000000ULL / (uint64_t)freq.QuadPart);
}

#else
#  error "host_wall_clock_us: only Windows QPC supported this wave; POSIX TODO"
#endif

#endif /* HOST_WALL_CLOCK_H */
