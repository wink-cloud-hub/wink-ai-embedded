// SPDX-License-Identifier: Apache-2.0
/**
 * @file wink_runtime.h
 * @brief OS main loop entry point + app-facing fault/task helpers.
 */
#ifndef WINK_RUNTIME_H
#define WINK_RUNTIME_H

#include "wink_app.h"
#include "wink_fault.h"
#include "wink_tasks.h"
#include "wink_config.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WINK_NORETURN
#  if defined(__GNUC__) || defined(__clang__)
#    define WINK_NORETURN __attribute__((noreturn))
#  elif defined(_MSC_VER)
#    define WINK_NORETURN __declspec(noreturn)
#  else
#    define WINK_NORETURN
#  endif
#endif

/**
 * @brief Run OS main loop
 *
 * @param[in] callbacks App lifecycle callbacks struct.
 * @param[in] max_ticks Maximum loop ticks to run (0 for infinite loop).
 * @return WINK_OK on completion, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_runtime_run(const wink_app_callbacks_t *callbacks, uint32_t max_ticks);

#define WINK_BOOT_LOCK_THRESHOLD    3u
#define WINK_BOOT_HEALTHY_TICKS     200u

/**
 * @brief Raise a fatal fault from app code or DAL
 *
 * @param[in] fault_code Application/runtime fault code.
 */
void wink_runtime_raise_fault(uint32_t fault_code);

/**
 * @brief Internal fault entry point used by wink_runtime_run
 *
 * @param[in] callbacks Callbacks struct pointer.
 * @param[in] fault_code Fault code.
 */
void wink_runtime_fault(const wink_app_callbacks_t *callbacks, uint32_t fault_code);

typedef struct {
    uint32_t uptime_ms;            /**< Uptime in ms */
    uint32_t free_heap;            /**< Free heap bytes */
    uint32_t min_free_stack;       /**< Min free stack bytes */
    uint32_t fault_count;          /**< Fault count */
    uint32_t warn_count;           /**< Warning count */
    uint32_t abnormal_boot_count;  /**< Consecutive abnormal boot count */
    wink_reset_reason_t last_reset_reason; /**< Last reset reason */
} wink_runtime_stats_t;

/**
 * @brief Query runtime statistics snapshot
 *
 * @param[out] out Output stats pointer.
 */
void wink_runtime_get_stats(wink_runtime_stats_t *out);

/**
 * @brief Trigger a controlled WDT reset test (noreturn)
 *
 * @param[in] timeout_ms Timeout in ms.
 */
WINK_NORETURN void wink_runtime_trigger_wdt_test(uint32_t timeout_ms);

/**
 * @brief Register a tick poll callback
 *
 * @param[in] fn Poll function.
 * @param[in] ctx Opaque context pointer.
 * @return WINK_OK on success, error status code otherwise.
 */
wink_status_t wink_runtime_register_poll(void (*fn)(void *ctx), void *ctx);

#define WINK_DEFINE_POLL_THUNK(thunk_name, fn, dev_type) \
    static void thunk_name(void *_ctx) { WINK_IGNORE_UNUSED(fn((dev_type *)_ctx)); }

static inline wink_status_t wink_runtime_spawn_periodic(
    const char *name, uint32_t stack_bytes, uint32_t period_ms,
    void (*fn)(void *ctx), void *ctx,
    int32_t priority, pal_os_core_id_t core)
{
    wink_periodic_handle_t h = wink_periodic_start_ex(
        name, stack_bytes, period_ms, fn, ctx,
        WINK_PERIODIC_MAY_BLOCK, priority, core);
    return (h >= 0) ? WINK_OK : (wink_status_t)h;
}

#ifdef __cplusplus
}
#endif

#endif /* WINK_RUNTIME_H */
