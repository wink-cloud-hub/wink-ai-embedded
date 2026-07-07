/**
 * @file wink_telemetry_helper.h
 * @brief BAL helper: fire-and-forget 2s telemetry task that prints runtime
 *        stats + optional sensor/button telemetry to the log output.
 *
 * Lives in the BAL (Business Abstraction Layer) rather than samples/common
 * because default runtime telemetry is a building block used by essentially
 * every bringup sample and production debug build; centralising it avoids
 * re-inventing the periodic-task bookkeeping in user code.
 *
 * Print format is a DEBUG / BRINGUP policy choice — it does NOT belong in
 * the OS core.  Apps wanting different fields or cadence should write their
 * own task using wink_periodic_start_ex(WINK_PERIODIC_MAY_BLOCK, ...) +
 * wink_runtime_get_stats().
 *
 * Scheduling: telemetry runs on the MAY_BLOCK path (dedicated preemptive
 * task) because LOG_I() formats strings and writes to UART/stdio, which
 * may block.  The default stack (2 KB), priority (1), and core (ANY) match
 * the historical samples/common helper.
 *
 * Layering (ADR-0023 §1): this header MUST NOT include any pal_*.h.
 *
 * Copyright (c) 2026 Wink-AI.
 */
#ifndef WINK_TELEMETRY_HELPER_H
#define WINK_TELEMETRY_HELPER_H

#include <stdbool.h>
#include <stdint.h>
#include "wink_status.h"
#include "dal_ultrasonic.h"
#include "dal_button.h"
#include "wink_helper_opts.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Default / overridable slot-pool size.
 *
 * Defined at compile time so memory-constrained builds can shrink the pool
 * or multi-sensor rigs can grow it; defaults to 1 (the default telemetry
 * is a singleton in almost all apps).  Override via
 * -DWINK_TELEMETRY_HELPER_MAX=N at compile time.
 */
#ifndef WINK_TELEMETRY_HELPER_MAX
#define WINK_TELEMETRY_HELPER_MAX 1
#endif

/**
 * @brief Default telemetry period in milliseconds (2 s).
 *
 * The "default" helper deliberately uses a fixed cadence — apps needing a
 * different period should write their own periodic task.
 */
#define WINK_TELEMETRY_DEFAULT_PERIOD_MS 2000u

/**
 * @brief Start the default 2s telemetry task.
 *
 * Prints one line every 2 s containing uptime, free heap, min free stack,
 * fault/warn counts, optional sonar distance, optional button edge-count,
 * reset reason, and abnormal-boot count.
 *
 * @param sonar  Ultrasonic device to report (NULL → skip sonar field).
 * @param btn    Button whose edge_count is reported (NULL → skip isr field).
 * @return WINK_OK on success.
 *         WINK_ERR_INVALID_STATE      telemetry is already running
 *                                     (call wink_telemetry_default_stop first;
 *                                     this also covers the slot-pool-full case
 *                                     because the default singleton stop()
 *                                     tears down all default-telemetry slots).
 *         Other codes propagated from wink_periodic_start_ex.
 *
 * @note This is the simple wrapper (defaults: 2 KB stack, prio=1, ANY core,
 *       MAY_BLOCK).  Use wink_telemetry_default_start_ex() for explicit
 *       stack/priority/core/flags control.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_telemetry_default_start(const dal_ultrasonic_t *sonar,
                                           const dal_button_t     *btn);

/**
 * @brief Extended start with explicit helper options.
 *
 * @param sonar  Ultrasonic device (NULL → skip).
 * @param btn    Button device (NULL → skip).
 * @param opts   Helper options (stack/priority/core/flags).  Pass NULL for
 *               defaults (equivalent to wink_telemetry_default_start()).
 *               NOTE: telemetry calls LOG_I (printf/UART) which may block;
 *               WINK_PERIODIC_LIGHT is accepted but will likely trigger
 *               WCET warnings or block the tick — MAY_BLOCK is recommended.
 * @return WINK_OK on success; same error codes as _start().
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_telemetry_default_start_ex(const dal_ultrasonic_t *sonar,
                                              const dal_button_t     *btn,
                                              const wink_helper_opts_t *opts);

/**
 * @brief Stop the default telemetry task.
 *
 * Idempotent: if telemetry is not running this is a silent no-op.  The
 * context slot is freed for reuse, and the underlying periodic task is
 * deleted.
 */
void wink_telemetry_default_stop(void);

/**
 * @brief Query whether the default telemetry task is currently running.
 *
 * @return true if at least one telemetry slot is active; false otherwise.
 */
bool wink_telemetry_default_is_running(void);

#ifdef __cplusplus
}
#endif

#endif /* WINK_TELEMETRY_HELPER_H */
