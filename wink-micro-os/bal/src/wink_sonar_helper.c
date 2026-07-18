/**
 * @file wink_sonar_helper.c
 * @brief BAL ultrasonic sonar helper �?periodically triggers distance measurements
 *        via the runtime periodic scheduler (MAY_BLOCK path).
 *
 * Copyright (c) 2026 Wink-AI.
 */
#define LOG_TAG "bal.sonar"

#include "sensor/wink_sonar_helper.h"
#include "sensor/wink_ultrasonic_distance_events.h"
#include "wink_tasks.h"
#include "wink_blocking_region.h"
#include "pal_log.h"
#include "wink_status.h"

#include <stddef.h>

/* HC-SR04 acoustic-ringing budget: a measurement fires an 8-cycle 40 kHz
 * burst (~200 µs) then listens for the echo. After the echo returns the
 * transducer keeps ringing for ~10-30 ms; polling faster than 50 ms causes
 * the next burst's transmitted energy to be picked up as a false echo.
 * Runtime-enforced (see start_ex / set_period) per 2026-07-06 hardening
 * review Task 3.1. */
#define WINK_SONAR_MIN_PERIOD_MS 50u

/* ADR-0017 BAL-exception: helper 内部通过 wink_periodic MAY_BLOCK 路径调用 WINK_BLOCKING API */
WINK_INTERNAL_BLOCKING_REGION_BEGIN

#if WINK_SONAR_HELPER_MAX > 0

typedef struct {
    dal_ultrasonic_t       *dev;
    wink_periodic_handle_t  period_h;
} sonar_ctx_t;

static sonar_ctx_t s_slots[WINK_SONAR_HELPER_MAX];

/* ── internal helpers ────────────────────────────────────────── */

/* Map BAL core-affinity enum to pal_os_core_id_t for the runtime call. */
static pal_os_core_id_t map_core(wink_bal_core_t c) {
    switch (c) {
        case WINK_BAL_CORE_0: return PAL_OS_CORE_0;
        case WINK_BAL_CORE_1: return PAL_OS_CORE_1;
        case WINK_BAL_CORE_ANY:        /* fallthrough */
        case WINK_BAL_CORE_INVALID:    /* fallthrough */
        default:                      return PAL_OS_CORE_ANY;
    }
}

/* Find a free slot index (dev == NULL), or -1 if pool exhausted. */
static int find_free_slot(void) {
    for (int i = 0; i < WINK_SONAR_HELPER_MAX; i++) {
        if (s_slots[i].dev == NULL) {
            return i;
        }
    }
    return -1;
}

/* Find slot index currently owning @p dev, or -1. */
static int find_slot_by_dev(dal_ultrasonic_t *dev) {
    for (int i = 0; i < WINK_SONAR_HELPER_MAX; i++) {
        if (s_slots[i].dev == dev) {
            return i;
        }
    }
    return -1;
}

/* ── periodic callback (MAY_BLOCK path �?void return, void* ctx) ─── */
static void sonar_tick(void *arg) {
    sonar_ctx_t *ctx = (sonar_ctx_t *)arg;
    if (ctx->dev != NULL) {
        WINK_IGNORE_RESULT(dal_ultrasonic_request_measurement(ctx->dev));
    }
}

/* ── public API ──────────────────────────────────────────────── */

wink_status_t wink_sonar_helper_start_ex(dal_ultrasonic_t *dev, uint32_t period_ms,
                                         const wink_bal_opts_t *opts)
{
    if (dev == NULL || period_ms < WINK_SONAR_MIN_PERIOD_MS) {
        LOG_D("start: invalid arg (dev=%p period_ms=%u, need >=%u)",
              (void *)dev, (unsigned)period_ms, (unsigned)WINK_SONAR_MIN_PERIOD_MS);
        return WINK_ERR_INVALID_ARG;
    }

    /* Reject duplicate start on the same device */
    if (find_slot_by_dev(dev) >= 0) {
        LOG_D("start: dev=%p already running", (void *)dev);
        return WINK_ERR_INVALID_STATE;
    }

    /* ADR-0033: mutually exclusive with B-class distance events on same dev. */
    if (wink_ultrasonic_distance_events_is_enabled(dev)) {
        LOG_D("start: distance events already own dev=%p", (void *)dev);
        return WINK_ERR_INVALID_STATE;
    }

    int free_idx = find_free_slot();
    if (free_idx < 0) {
        LOG_D("start: out of slots (%d)", WINK_SONAR_HELPER_MAX);
        return WINK_ERR_RESOURCE_EXHAUSTED;
    }

    wink_bal_opts_t effective = WINK_BAL_OPTS_DEFAULT;
    if (opts != NULL) {
        effective = *opts;
    }

    uint32_t flags = effective.flags;
    if (flags == 0u) {
        flags = WINK_PERIODIC_MAY_BLOCK;
    }

    uint32_t stack = (effective.stack_bytes != 0u) ? effective.stack_bytes : 3072u;
    int32_t prio = (effective.priority >= 0) ? effective.priority : 5;
    pal_os_core_id_t core = map_core(effective.core_id);

    sonar_ctx_t *ctx = &s_slots[free_idx];
    ctx->dev = dev;
    ctx->period_h = WINK_PERIODIC_INVALID;

    /* Preflight: fire one measurement request so an uninitialised device
     * fails fast at start() rather than spinning a silent periodic.
     * NOT_INITIALIZED is fatal; other errors (hardware fault, BUSY on a
     * still-ringing transducer) are transient and will be retried by the
     * tick callback. */
    wink_status_t probe_st = dal_ultrasonic_request_measurement(dev);
    if (probe_st == WINK_ERR_NOT_INITIALIZED) {
        LOG_D("start: sonar not initialized");
        ctx->dev = NULL; /* roll back slot allocation */
        return WINK_ERR_NOT_INITIALIZED;
    }

    wink_periodic_handle_t h = wink_periodic_start_ex(
        "sonar", stack, period_ms, sonar_tick, ctx, flags, prio, core);
    if (h < 0) {
        LOG_D("start: periodic_start failed: %d", (int)h);
        ctx->dev = NULL;
        return (wink_status_t)h;
    }

    ctx->period_h = h;
    return WINK_OK;
}

wink_status_t wink_sonar_helper_start(dal_ultrasonic_t *dev, uint32_t period_ms) {
    return wink_sonar_helper_start_ex(dev, period_ms, NULL);
}

wink_status_t wink_sonar_helper_stop(dal_ultrasonic_t *dev) {
    if (dev == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    int idx = find_slot_by_dev(dev);
    if (idx < 0) {
        return WINK_OK; /* Idempotent stop */
    }

    sonar_ctx_t *ctx = &s_slots[idx];
    if (ctx->period_h > 0) {
        wink_periodic_stop(ctx->period_h);
    }
    ctx->period_h = WINK_PERIODIC_INVALID;
    ctx->dev = NULL;
    return WINK_OK;
}

wink_status_t wink_sonar_helper_set_period(dal_ultrasonic_t *dev, uint32_t period_ms) {
    if (dev == NULL || period_ms < WINK_SONAR_MIN_PERIOD_MS) {
        return WINK_ERR_INVALID_ARG;
    }
    int idx = find_slot_by_dev(dev);
    if (idx < 0) {
        return WINK_ERR_INVALID_STATE;
    }
    sonar_ctx_t *ctx = &s_slots[idx];
    return wink_periodic_change_period(ctx->period_h, period_ms);
}

bool wink_sonar_helper_is_running(dal_ultrasonic_t *dev) {
    if (dev == NULL) {
        return false;
    }
    return (find_slot_by_dev(dev) >= 0);
}

void wink_sonar_helper_reset(void) {
    for (int i = 0; i < WINK_SONAR_HELPER_MAX; i++) {
        if (s_slots[i].dev != NULL) {
            if (s_slots[i].period_h > 0) {
                wink_periodic_stop(s_slots[i].period_h);
            }
            s_slots[i].period_h = WINK_PERIODIC_INVALID;
            s_slots[i].dev = NULL;
        }
    }
}

#else /* WINK_SONAR_HELPER_MAX == 0 */

wink_status_t wink_sonar_helper_start_ex(dal_ultrasonic_t *dev, uint32_t period_ms,
                                         const wink_bal_opts_t *opts)
{
    (void)dev; (void)period_ms; (void)opts;
    return WINK_ERR_UNAVAILABLE;
}

wink_status_t wink_sonar_helper_start(dal_ultrasonic_t *dev, uint32_t period_ms) {
    (void)dev; (void)period_ms;
    return WINK_ERR_UNAVAILABLE;
}

wink_status_t wink_sonar_helper_stop(dal_ultrasonic_t *dev) {
    (void)dev;
    return WINK_OK;
}

wink_status_t wink_sonar_helper_set_period(dal_ultrasonic_t *dev, uint32_t period_ms) {
    (void)dev; (void)period_ms;
    return WINK_ERR_UNAVAILABLE;
}

bool wink_sonar_helper_is_running(dal_ultrasonic_t *dev) {
    (void)dev;
    return false;
}

void wink_sonar_helper_reset(void) {
    /* No-op on stub */
}

#endif /* WINK_SONAR_HELPER_MAX > 0 */

WINK_INTERNAL_BLOCKING_REGION_END
