/**
 * @file wink_telemetry_helper.c
 * @brief BAL default telemetry helper �?prints runtime stats + optional
 *        sensor/button telemetry every 2s via a MAY_BLOCK periodic task.
 *
 * ADR-0017 BAL-exception: this TU legitimately calls WINK_BLOCKING APIs
 * (LOG_I �?printf/UART write) from within a periodic MAY_BLOCK task body.
 * The file-scope WINK_INTERNAL_BLOCKING_REGION_BEGIN/END suppression is
 * placed after all #includes so the pragma does NOT leak into PAL/DAL
 * headers.
 *
 * Slot management: uses a free-list scan (mirroring blink/button helpers)
 * so stop() marks slots free (in_use = false) and the pool is recycled
 * correctly across start/stop cycles.  An explicit `in_use` bool is used
 * (rather than a pointer sentinel) because NULL sonar/btn are both
 * legitimate values meaning "skip that field".
 *
 * The callback reads cached DAL state (dal_ultrasonic_get_cached_distance,
 * dal_button_get_edge_count) which are non-blocking; only LOG_I may block,
 * which is why we pin this to WINK_PERIODIC_MAY_BLOCK.
 *
 * Copyright (c) 2026 Wink-AI.
 */
#define LOG_TAG "bal.telem"

#include "wink_telemetry_helper.h"
#include "wink_tasks.h"
#include "wink_runtime.h"
#include "wink_status.h"
#include "wink_blocking_region.h"
#include "pal_log.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ADR-0017 BAL-exception: helper 内部通过 wink_periodic MAY_BLOCK 路径
 * 调用 WINK_BLOCKING API (LOG_I/printf). */
WINK_INTERNAL_BLOCKING_REGION_BEGIN

/* ── per-telemetry slot ────────────────────────────────────────
 * in_use marks the slot occupied; sonar/btn are user-passed pointers
 * (NULL is valid = "skip that field").  BSS zero-init sets in_use=false,
 * which is exactly the free-slot state we want. */
typedef struct {
    bool                    in_use;
    const dal_ultrasonic_t *sonar;
    const dal_button_t     *btn;
    wink_periodic_handle_t  period_h;
} telem_ctx_t;

static telem_ctx_t s_slots[WINK_TELEMETRY_HELPER_MAX];

/* ── internal helpers ────────────────────────────────────────── */

static int find_free_slot(void) {
    for (int i = 0; i < WINK_TELEMETRY_HELPER_MAX; i++) {
        if (!s_slots[i].in_use) {
            return i;
        }
    }
    return -1;
}

/* Map BAL core-affinity enum to pal_os_core_id_t for the runtime call. */
static pal_os_core_id_t map_core(wink_bal_core_t c) {
    switch (c) {
        case WINK_BAL_CORE_0: return PAL_OS_CORE_0;
        case WINK_BAL_CORE_1: return PAL_OS_CORE_1;
        case WINK_BAL_CORE_ANY:        /* fallthrough */
        case WINK_BAL_CORE_INVALID:    /* fallthrough */
        default:             return PAL_OS_CORE_ANY;
    }
}

/* ── periodic callback (MAY_BLOCK path �?void return, void* ctx) ── */
static void telem_tick(void *arg) {
    telem_ctx_t *ctx = (telem_ctx_t *)arg;
    wink_runtime_stats_t st;
    wink_runtime_get_stats(&st);

    float         dist_cm  = -1.0f;
    wink_status_t sonar_st = WINK_ERR_UNSUPPORTED;
    uint32_t      isrs     = 0u;

    if (ctx->sonar != NULL) {
        sonar_st = dal_ultrasonic_get_cached_distance(ctx->sonar, &dist_cm);
    }
    if (ctx->btn != NULL) {
        uint32_t cnt = 0u;
        WINK_IGNORE_RESULT(dal_button_get_edge_count(ctx->btn, &cnt));
        isrs = cnt;
    }

    LOG_I("uptime=%lums heap=%lu stack_min=%lu faults=%lu warns=%lu "
          "isrs=%lu sonar_st=%d dist=%.2fcm reset=%d abn=%lu",
          (unsigned long)st.uptime_ms,
          (unsigned long)st.free_heap,
          (unsigned long)st.min_free_stack,
          (unsigned long)st.fault_count,
          (unsigned long)st.warn_count,
          (unsigned long)isrs,
          (int)sonar_st, dist_cm,
          (int)st.last_reset_reason,
          (unsigned long)st.abnormal_boot_count);
}

/* ── public API ──────────────────────────────────────────────── */

wink_status_t wink_telemetry_default_start_ex(const dal_ultrasonic_t *sonar,
                                              const dal_button_t     *btn,
                                              const wink_bal_opts_t *opts)
{
    int free_idx = find_free_slot();
    if (free_idx < 0) {
        /* With the default MAX=1 a full pool means "telemetry already
         * started" �?semantically INVALID_STATE.  When a build overrides
         * MAX to >1 this still reports INVALID_STATE (consistent with the
         * no-arg stop() which tears down ALL default-telemetry slots);
         * users wanting multiple independent telemetry streams should
         * write their own periodic tasks instead. */
        LOG_D("start: telemetry already running (pool_max=%d)",
              WINK_TELEMETRY_HELPER_MAX);
        return WINK_ERR_INVALID_STATE;
    }

    /* Resolve options (NULL �?defaults). */
    wink_bal_opts_t effective = WINK_BAL_OPTS_DEFAULT;
    if (opts != NULL) {
        effective = *opts;
    }

    /* Default flags for telemetry: MAY_BLOCK (LOG_I may block on UART).
     * If caller explicitly sets flags (non-zero), honour them. */
    uint32_t flags = effective.flags;
    if (flags == 0u) {
        flags = WINK_PERIODIC_MAY_BLOCK;
    }

    /* Defaults: 2 KB stack, low priority (1 = background telemetry),
     * any core, 2 s period. */
    uint32_t stack = (effective.stack_bytes != 0u) ? effective.stack_bytes : 2048u;
    int32_t  prio  = (effective.priority >= 0)       ? effective.priority : 1;
    pal_os_core_id_t core = map_core(effective.core_id);

    telem_ctx_t *ctx = &s_slots[free_idx];
    ctx->in_use   = true;
    ctx->sonar    = sonar;
    ctx->btn      = btn;
    ctx->period_h = WINK_PERIODIC_INVALID;

    /* Preflight: if a non-NULL device is supplied, verify it is initialised.
     * Mirrors blink/button/sonar helper preflight pattern. NOT_INITIALIZED
     * on either pointer fails start; BUSY/transient errors are tolerated
     * (the tick will retry/report on the next 2 s cycle). NULL pointers
     * are legitimate ("don't report that field") and are skipped. */
    if (sonar != NULL) {
        float d = 0.0f;
        wink_status_t st = dal_ultrasonic_get_cached_distance(sonar, &d);
        if (st == WINK_ERR_NOT_INITIALIZED) {
            LOG_D("start: sonar not initialized");
            ctx->in_use = false;
            ctx->sonar  = NULL;
            ctx->btn    = NULL;
            return WINK_ERR_NOT_INITIALIZED;
        }
    }
    if (btn != NULL) {
        bool pressed = false;
        wink_status_t st = dal_button_is_pressed(btn, &pressed);
        if (st == WINK_ERR_NOT_INITIALIZED) {
            LOG_D("start: button not initialized");
            ctx->in_use = false;
            ctx->sonar  = NULL;
            ctx->btn    = NULL;
            return WINK_ERR_NOT_INITIALIZED;
        }
    }

    wink_periodic_handle_t h = wink_periodic_start_ex(
        "default_telem", stack, WINK_TELEMETRY_DEFAULT_PERIOD_MS,
        telem_tick, ctx, flags, prio, core);
    if (h < 0) {
        LOG_D("start: periodic_start failed: %d", (int)h);
        ctx->in_use = false;   /* roll back slot allocation */
        ctx->sonar  = NULL;
        ctx->btn    = NULL;
        return (wink_status_t)h;
    }

    ctx->period_h = h;
    return WINK_OK;
}

wink_status_t wink_telemetry_default_start(const dal_ultrasonic_t *sonar,
                                           const dal_button_t     *btn)
{
    return wink_telemetry_default_start_ex(sonar, btn, NULL);
}

void wink_telemetry_default_stop(void)
{
    for (int i = 0; i < WINK_TELEMETRY_HELPER_MAX; i++) {
        if (s_slots[i].in_use) {
            wink_periodic_stop(s_slots[i].period_h);
            s_slots[i].period_h = WINK_PERIODIC_INVALID;
            s_slots[i].sonar    = NULL;
            s_slots[i].btn      = NULL;
            s_slots[i].in_use   = false;   /* mark slot free for reuse */
        }
    }
}

bool wink_telemetry_default_is_running(void)
{
    for (int i = 0; i < WINK_TELEMETRY_HELPER_MAX; i++) {
        if (s_slots[i].in_use) {
            return true;
        }
    }
    return false;
}

WINK_INTERNAL_BLOCKING_REGION_END
