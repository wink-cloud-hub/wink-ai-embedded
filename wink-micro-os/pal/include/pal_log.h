// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_log.h
 * @brief PAL Level-Based Logging Interface: Error / Warn / Info / Debug.
 *
 * Design goals:
 *   - Unified cross-platform logging entry, eliminating direct dependency on ESP_LOG* / printf / console.log.
 *   - Debug logs compile to zero overhead in release builds (NDEBUG) (macro expands to ((void)0)).
 *   - Supports file-level implicit TAG (LOG_TAG) defined once per compilation unit.
 *   - ISR Safety: Automatically redirects to lockless ROM print in ISR context (ERROR/WARN) or silences (INFO/DEBUG).
 *
 * Platform mappings:
 *   - ESP32: Routes to esp_log_writev(), reusing ESP-IDF color/timestamp/tag filtering.
 *   - WASM: Formatted and bridged via js_pal_log() to JS console.error/warn/log/debug.
 *   - Host: Formatted stderr output with millisecond timestamps + thread IDs.
 *
 * Usage:
 *   @code
 *   #define LOG_TAG "dal_rc_servo"
 *   #include "pal_log.h"
 *
 *   LOG_E("init failed: pin=%d rc=%d", pin, rc);   // Error
 *   LOG_W("angle %d out of range, clamped", angle); // Warn
 *   LOG_I("servo initialized");                    // Info
 *   LOG_D("set_angle=%d", angle);                  // Debug
 *   @endcode
 *
 * Constraints:
 *   - `fmt` must be a compile-time string literal.
 *   - LOG_D arguments must not contain expressions with side effects.
 */
#ifndef PAL_LOG_H
#define PAL_LOG_H

#include <stdarg.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 *  Log Levels - Dual representation as numeric macros & C enum
 * ========================================================================= */
#define PAL_LOG_LEVEL_NONE    0
#define PAL_LOG_LEVEL_ERROR   1
#define PAL_LOG_LEVEL_WARN    2
#define PAL_LOG_LEVEL_INFO    3
#define PAL_LOG_LEVEL_DEBUG   4

typedef enum {
    PAL_LOG_NONE  = PAL_LOG_LEVEL_NONE,
    PAL_LOG_ERROR = PAL_LOG_LEVEL_ERROR,
    PAL_LOG_WARN  = PAL_LOG_LEVEL_WARN,
    PAL_LOG_INFO  = PAL_LOG_LEVEL_INFO,
    PAL_LOG_DEBUG = PAL_LOG_LEVEL_DEBUG,
} pal_log_level_t;

/**
 * @brief Compile-time minimum log level.
 *
 * Logs below this level expand to ((void)0) with zero runtime cost.
 * Overridable via -DPAL_LOG_COMPILE_LEVEL=N in build system.
 */
#ifndef PAL_LOG_COMPILE_LEVEL
#  ifdef NDEBUG
#    define PAL_LOG_COMPILE_LEVEL PAL_LOG_LEVEL_INFO
#  else
#    define PAL_LOG_COMPILE_LEVEL PAL_LOG_LEVEL_DEBUG
#  endif
#endif

/**
 * @brief Log backend function - Target implementation for synchronous path.
 *
 * @param[in] level Log level
 * @param[in] tag Module tag string
 * @param[in] fmt printf format string (compile-time literal)
 * @param[in] ap Variadic argument list
 */
void pal_log_vprintf(pal_log_level_t level, const char *tag,
                     const char *fmt, va_list ap);

/**
 * @brief Query if current execution is inside ISR context.
 * @return true if in ISR context, false otherwise.
 */
bool pal_log_in_isr(void);

/**
 * @brief Lockless minimal log output in ISR context (ERROR/WARN only).
 *
 * @param[in] level Log level
 * @param[in] tag Module tag string
 * @param[in] fmt printf format string
 * @param[in] ap Variadic argument list
 */
void pal_log_isr_write(pal_log_level_t level, const char *tag,
                       const char *fmt, va_list ap);

/* =========================================================================
 *  Log Macros - Compile-time gating + format validation + ISR routing
 * ========================================================================= */

/**
 * @brief Log Error level message (unrecoverable errors, init failures).
 */
#if PAL_LOG_COMPILE_LEVEL >= PAL_LOG_LEVEL_ERROR
#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 2, 3)))
#endif
static inline void pal_log_e(const char *tag, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if (pal_log_in_isr()) {
        pal_log_isr_write(PAL_LOG_ERROR, tag, fmt, ap);
    } else {
        pal_log_vprintf(PAL_LOG_ERROR, tag, fmt, ap);
    }
    va_end(ap);
}
#else
#define pal_log_e(tag, fmt, ...) ((void)0)
#endif

/**
 * @brief Log Warn level message (recoverable anomalies, clamped values).
 */
#if PAL_LOG_COMPILE_LEVEL >= PAL_LOG_LEVEL_WARN
#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 2, 3)))
#endif
static inline void pal_log_w(const char *tag, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if (pal_log_in_isr()) {
        pal_log_isr_write(PAL_LOG_WARN, tag, fmt, ap);
    } else {
        pal_log_vprintf(PAL_LOG_WARN, tag, fmt, ap);
    }
    va_end(ap);
}
#else
#define pal_log_w(tag, fmt, ...) ((void)0)
#endif

/**
 * @brief Log Info level message (normal startup sequence, status changes).
 */
#if PAL_LOG_COMPILE_LEVEL >= PAL_LOG_LEVEL_INFO
#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 2, 3)))
#endif
static inline void pal_log_i(const char *tag, const char *fmt, ...)
{
    if (pal_log_in_isr()) {
        return;   /* Silently drop INFO in ISR */
    }
    va_list ap;
    va_start(ap, fmt);
    pal_log_vprintf(PAL_LOG_INFO, tag, fmt, ap);
    va_end(ap);
}
#else
#define pal_log_i(tag, fmt, ...) ((void)0)
#endif

/**
 * @brief Log Debug level message (detailed debugging, high frequency data).
 */
#if PAL_LOG_COMPILE_LEVEL >= PAL_LOG_LEVEL_DEBUG
#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 2, 3)))
#endif
static inline void pal_log_d(const char *tag, const char *fmt, ...)
{
    if (pal_log_in_isr()) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    pal_log_vprintf(PAL_LOG_DEBUG, tag, fmt, ap);
    va_end(ap);
}
#else
#define pal_log_d(tag, fmt, ...) ((void)0)
#endif

/* =========================================================================
 *  Implicit TAG Convenience Macros: LOG_E / LOG_W / LOG_I / LOG_D
 * ========================================================================= */
#ifdef LOG_TAG
#  define LOG_E(fmt, ...) pal_log_e(LOG_TAG, fmt, ##__VA_ARGS__)
#  define LOG_W(fmt, ...) pal_log_w(LOG_TAG, fmt, ##__VA_ARGS__)
#  define LOG_I(fmt, ...) pal_log_i(LOG_TAG, fmt, ##__VA_ARGS__)
#  define LOG_D(fmt, ...) pal_log_d(LOG_TAG, fmt, ##__VA_ARGS__)
#else
#  define LOG_E(fmt, ...) pal_log_e("SYS", fmt, ##__VA_ARGS__)
#  define LOG_W(fmt, ...) pal_log_w("SYS", fmt, ##__VA_ARGS__)
#  define LOG_I(fmt, ...) pal_log_i("SYS", fmt, ##__VA_ARGS__)
#  define LOG_D(fmt, ...) pal_log_d("SYS", fmt, ##__VA_ARGS__)
#endif

#ifdef __cplusplus
}
#endif

#endif /* PAL_LOG_H */
