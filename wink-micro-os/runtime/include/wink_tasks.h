#ifndef WINK_TASKS_H
#define WINK_TASKS_H

/**
 * @file wink_tasks.h
 * @brief Periodic/spawn helpers — single API for app-level recurring work.
 *
 * The runtime chooses the mechanism (lightweight soft-timer callback vs
 * dedicated preemptive task) based on @p flags.  Apps declare their intent
 * (lightweight non-blocking vs may-block) rather than reaching for raw
 * pal_os_task_create or wink_soft_timer directly.
 *
 * Copyright (c) 2026 Wink-AI.
 */

#include "wink_status.h"
#include "pal_osal.h"   /* pal_os_core_id_t */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque handle returned by wink_periodic_start().  Negative = invalid. */
typedef int32_t wink_periodic_handle_t;

/* Default priority used by the convenience wink_periodic_start() wrapper. */
#define WINK_PERIODIC_DEFAULT_PRIORITY   2
/* Default core affinity used by the convenience wrapper (scheduler decides). */
#define WINK_PERIODIC_DEFAULT_CORE       PAL_OS_CORE_ANY

/**
 * @brief Spawn a periodic task/callback with full scheduling control.
 *
 * @param name       Human-readable label (used in task list / telemetry).
 * @param stack_hint Stack size hint in bytes.  Ignored for LIGHT callbacks
 *                   (which share the main-loop stack); used as stack size
 *                   for MAY_BLOCK tasks.  0 picks a runtime default (2KB).
 * @param period_ms  Period in milliseconds.
 * @param fn         Callback invoked every period.
 * @param ctx        Opaque pointer forwarded to fn.
 * @param flags      Bitwise OR of WINK_PERIODIC_* flags (see below).
 * @param priority   FreeRTOS-style task priority for MAY_BLOCK tasks
 *                   (higher = more urgent).  Ignored for LIGHT callbacks.
 *                   Use WINK_PERIODIC_DEFAULT_PRIORITY (2) for background
 *                   telemetry/polling; raise for latency-sensitive work
 *                   (e.g. 5-10 for sonar RMT capture, per ADR-0016).
 * @param core       Core affinity.  PAL_OS_CORE_ANY lets the scheduler
 *                   decide; PAL_OS_CORE_0/1 pins to a specific core
 *                   (ESP32 dual-core).  Ignored on single-core targets.
 * @return Handle >= 0 on success; negative WINK_ERR_* on failure.
 *
 * Flags (pick one execution model; default with flags=0 lets runtime
 * decide based on period_ms / stack_hint):
 *   WINK_PERIODIC_LIGHT     — force soft-timer path (tick context).
 *                             fn MUST NOT block, MUST NOT call WINK_BLOCKING APIs.
 *                             Zero extra stack, lowest overhead.
 *   WINK_PERIODIC_MAY_BLOCK — force independent preemptive task.
 *                             fn may call blocking APIs (I2C, RMT wait, printf).
 *                             Allocates stack_hint bytes of stack.
 *
 * Absolute-time scheduling: the task path uses anchor-time sleep to avoid
 * cumulative drift (see implementation for details).
 */
wink_periodic_handle_t wink_periodic_start_ex(
    const char *name,
    uint32_t stack_hint,
    uint32_t period_ms,
    void (*fn)(void *ctx),
    void *ctx,
    uint32_t flags,
    int32_t priority,
    pal_os_core_id_t core);

/**
 * @brief Convenience wrapper: spawn with default priority (2) and no core
 *        affinity (PAL_OS_CORE_ANY).  Equivalent to:
 *
 * @code
 *     wink_periodic_start_ex(name, stack_hint, period_ms, fn, ctx, flags,
 *                            WINK_PERIODIC_DEFAULT_PRIORITY,
 *                            WINK_PERIODIC_DEFAULT_CORE);
 * @endcode
 */
static inline wink_periodic_handle_t wink_periodic_start(
    const char *name,
    uint32_t stack_hint,
    uint32_t period_ms,
    void (*fn)(void *ctx),
    void *ctx,
    uint32_t flags)
{
    return wink_periodic_start_ex(name, stack_hint, period_ms, fn, ctx, flags,
                                  WINK_PERIODIC_DEFAULT_PRIORITY,
                                  WINK_PERIODIC_DEFAULT_CORE);
}

/** Stop and (for MAY_BLOCK) delete a previously started periodic callback. */
void wink_periodic_stop(wink_periodic_handle_t h);

/* Flag bits. */
#define WINK_PERIODIC_LIGHT       (1u << 0)
#define WINK_PERIODIC_MAY_BLOCK   (1u << 1)

#ifdef __cplusplus
}
#endif

#endif /* WINK_TASKS_H */
