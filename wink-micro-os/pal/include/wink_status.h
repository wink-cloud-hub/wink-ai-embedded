// SPDX-License-Identifier: Apache-2.0
#ifndef WINK_STATUS_H
#define WINK_STATUS_H

#include <stddef.h>

#ifndef _Static_assert
#  if defined(__cplusplus)
#    define _Static_assert(cond, msg) static_assert(cond, msg)
#  elif defined(_MSC_VER) && !defined(__clang__)
#    define _Static_assert(cond, msg) static_assert(cond, msg)
#  endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__) || defined(__clang__)
    #define WINK_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
    #define WINK_WARN_UNUSED_RESULT
#endif

/* ─────────────────────────────────────────────────────────
 * Blocking API Hard Isolation (ADR-0017)
 * `WINK_DEPRECATED_MSG(msg)` - Generic deprecation attribute
 * `WINK_BLOCKING`             - Applied to APIs that busy-wait > 1 tick (10ms)
 * ───────────────────────────────────────────────────────── */
#if defined(__GNUC__) || defined(__clang__)
    #define WINK_DEPRECATED_MSG(msg) __attribute__((deprecated(msg)))
#elif defined(_MSC_VER)
    #define WINK_DEPRECATED_MSG(msg) __declspec(deprecated(msg))
#else
    #define WINK_DEPRECATED_MSG(msg)
#endif

/* Short alias for deprecation */
#define WINK_DEPRECATED(msg) WINK_DEPRECATED_MSG(msg)

#define WINK_BLOCKING \
    WINK_DEPRECATED_MSG("Blocking API forbidden in cooperative runtime; use non-blocking variant")

/*
 * Suppress -Wunused-result on WINK_WARN_UNUSED_RESULT APIs when the error
 * is intentionally discarded.
 */
#define WINK_IGNORE_RESULT(expr) do { \
    wink_status_t _wink_ignored_result = (expr); \
    (void)_wink_ignored_result; \
} while (0)

#define WINK_IGNORE_UNUSED(expr) WINK_IGNORE_RESULT(expr)

/*
 * WINK_UNAVAILABLE_MSG(msg) - Compile-time "driver disabled" stub marker.
 */
#if defined(__clang__)
    #define WINK_UNAVAILABLE_MSG(msg) __attribute__((unavailable(msg)))
#elif defined(__GNUC__)
    #if __GNUC__ >= 5
        #define WINK_UNAVAILABLE_MSG(msg) __attribute__((unavailable(msg)))
    #else
        #define WINK_UNAVAILABLE_MSG(msg) __attribute__((deprecated(msg)))
    #endif
#elif defined(_MSC_VER)
    #define WINK_UNAVAILABLE_MSG(msg) __declspec(deprecated(msg))
#else
    #define WINK_UNAVAILABLE_MSG(msg)
#endif

/**
 * @brief Wink Platform Status Codes
 *
 * Negative error code convention (ADR-0001).
 * All status codes < 0 represent errors or special yield signals.
 */
typedef enum {
    WINK_OK = 0,                              /**< Operation succeeded; evaluates to false in `if (status)`. */

    WINK_ERR_INVALID_ARG        = -1,         /**< Invalid argument (NULL / out of bounds / invalid enum); caller bug. */
    WINK_ERR_TIMEOUT            = -2,         /**< Operation timeout (I2C ACK / GPIO wait / RMT). */
    WINK_ERR_DISCONNECTED       = -3,         /**< Device disconnected (NACK / no response); enter fail-safe. */
    WINK_ERR_OUT_OF_RANGE       = -4,         /**< Value out of range (ADC overrange / limit reached); clamp and proceed. */
    WINK_ERR_IO                 = -5,         /**< General I/O error (unclassified bus level error). */
    WINK_ERR_BUSY               = -6,         /**< Resource busy / not ready; also serves as PT yield signal. */
    WINK_ERR_UNSUPPORTED        = -7,         /**< Feature unsupported on current target/build. */
    WINK_ERR_CHECKSUM           = -8,         /**< Checksum failure (CRC / data integrity). */
    WINK_ERR_PERMISSION         = -9,         /**< Permission denied (sandbox / flash protection). */
    WINK_ERR_RESOURCE_EXHAUSTED = -10,        /**< Resource pool exhausted (claim table full / PWM channels exhausted). */
    WINK_ERR_NOT_INITIALIZED    = -11,        /**< Device uninitialized when invoked; caller bug. */
    WINK_ERR_HARDWARE           = -12,        /**< Hardware driver underlying failure. */
    WINK_ERR_NO_MEM             = -13,        /**< Memory allocation failure / out of memory. */
    WINK_ERR_EMPTY              = -14,        /**< Container / queue empty; normal poll API return. */
    WINK_ERR_FULL               = -15,        /**< Container / queue full. */
    WINK_ERR_INVALID_STATE      = -16,        /**< Invalid state machine transition. */
    WINK_ERR_LOCKED             = -17,        /**< Resource locked (boot safe-lock / config flash lock). */
    WINK_ERR_NOT_FOUND          = -18,        /**< Target item not found in registry / lookup table. */
    WINK_ERR_CANCELED           = -19,        /**< Concurrency benign cancellation. */

    WINK_ERR_OVERCURRENT        = -20,        /**< Overcurrent condition detected. */
    WINK_ERR_OVERTEMPERATURE    = -21,        /**< Overtemperature condition detected. */
    WINK_ERR_ALREADY_INITIALIZED = -22,       /**< Duplicate init call on initialized device. */

    WINK_ERR_WATCHDOG           = -30,        /**< Watchdog timeout (fatal). */

    WINK_ERR_OVERFLOW           = -40,        /**< Value overflow / arithmetic UB (fatal). */

    WINK_ERR_CONFIG_CORRUPT_DEGRADED = -50,   /**< Config corrupt -> fall back to safe defaults. */
    WINK_ERR_FAILED_INIT             = -51,   /**< Device init failed -> isolate device, system continues. */

    WINK_ERR_PANIC              = -99,        /**< Unrecoverable internal error (INVARIANT / illegal call); halt. */
} wink_status_t;

/**
 * @brief Helper function to check if a status code indicates an error
 * @param[in] s Status code to evaluate
 * @return Non-zero (true) if error (s < 0), zero (false) if success (s == 0)
 */
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

#endif /* WINK_STATUS_H */
