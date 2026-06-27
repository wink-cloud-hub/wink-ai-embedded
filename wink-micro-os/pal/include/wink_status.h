#ifndef WINK_STATUS_H
#define WINK_STATUS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__) || defined(__clang__)
    #define WINK_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
    #define WINK_WARN_UNUSED_RESULT
#endif

typedef enum {
    WINK_OK = 0,

    WINK_ERR_INVALID_ARG        = -1,
    WINK_ERR_TIMEOUT            = -2,
    WINK_ERR_DISCONNECTED       = -3,
    WINK_ERR_OUT_OF_RANGE       = -4,
    WINK_ERR_IO                 = -5,
    WINK_ERR_BUSY               = -6,
    WINK_ERR_UNSUPPORTED        = -7,
    WINK_ERR_CHECKSUM           = -8,
    WINK_ERR_PERMISSION         = -9,
    WINK_ERR_RESOURCE_EXHAUSTED = -10,
    WINK_ERR_NOT_INITIALIZED    = -11,
    WINK_ERR_HARDWARE           = -12,
    WINK_ERR_NO_MEM             = -13,
    WINK_ERR_EMPTY              = -14,
    WINK_ERR_FULL               = -15,
    WINK_ERR_INVALID_STATE      = -16,
    WINK_ERR_LOCKED             = -17,

    WINK_ERR_OVERCURRENT        = -20,
    WINK_ERR_OVERTEMPERATURE    = -21,

    WINK_ERR_WATCHDOG           = -30,

    WINK_ERR_OVERFLOW           = -40,

    WINK_ERR_CONFIG_CORRUPT_DEGRADED = -50,
    WINK_ERR_FAILED_INIT             = -51,

    WINK_ERR_PANIC              = -99,
} wink_status_t;

static inline int wink_status_is_error(wink_status_t s) {
    return s < 0;
}

#ifndef PAL_PWM_CHANNELS
#define PAL_PWM_CHANNELS 8
#endif

#ifndef WINK_MAX_SOFT_TIMERS
#define WINK_MAX_SOFT_TIMERS 16
#endif

#ifndef WINK_RUNTIME_TICK_MS
#define WINK_RUNTIME_TICK_MS 10
#endif

#ifdef __cplusplus
}
#endif

#endif
