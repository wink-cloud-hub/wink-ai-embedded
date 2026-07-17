/**
 * @file wink_ultrasonic_distance_events.c
 * @brief BAL ultrasonic distance events — periodic measure + DISTANCE_READY post.
 *
 * Copyright (c) 2026 Wink-AI.
 */
#define LOG_TAG "bal.us.dist"

#include "sensor/wink_ultrasonic_distance_events.h"
#include "sensor/wink_sonar_helper.h"
#include "wink_tasks.h"
#include "wink_blocking_region.h"
#include "wink_event.h"
#include "wink_fault.h"
#include "wink_trace.h"
#include "wink_status.h"
#include "pal_log.h"
#include "pal_osal.h"

#include <stddef.h>

/* ADR-0017: tick may call request_measurement (currently sync pulse_in on
 * host/ESP32). Routed via WINK_PERIODIC_LIGHT so host soft_timer / App main
 * tick drive the stream (same model as button soft_poll). ESP32 RMT
 * non-blocking completion remains a follow-up (see tech-design §6). */
WINK_INTERNAL_BLOCKING_REGION_BEGIN

#if WINK_ULTRASONIC_DISTANCE_EVENTS_MAX > 0

typedef enum {
    US_DIST_NEED_TRIGGER = 0,
    US_DIST_WAITING      = 1,
} us_dist_phase_t;

typedef struct {
    dal_ultrasonic_t       *dev;
    wink_periodic_handle_t  period_h;
    us_dist_phase_t         phase;
} us_dist_slot_t;

static us_dist_slot_t s_slots[WINK_ULTRASONIC_DISTANCE_EVENTS_MAX];

static int find_free_slot(void)
{
    for (int i = 0; i < WINK_ULTRASONIC_DISTANCE_EVENTS_MAX; i++) {
        if (s_slots[i].dev == NULL) {
            return i;
        }
    }
    return -1;
}

static int find_slot_by_dev(const dal_ultrasonic_t *dev)
{
    for (int i = 0; i < WINK_ULTRASONIC_DISTANCE_EVENTS_MAX; i++) {
        if (s_slots[i].dev == dev) {
            return i;
        }
    }
    return -1;
}

static void post_distance_ready(dal_ultrasonic_t *dev, float distance_cm)
{
    wink_event_t ev;
    float scaled;

    ev.type      = WINK_EVENT_DISTANCE_READY;
    ev.device    = dev;
    ev.timestamp = pal_os_get_ms();
    scaled = distance_cm * 10.0f + 0.5f;
    if (scaled < 0.0f) {
        scaled = 0.0f;
    }
    ev.param = (uint32_t)scaled;

    if (wink_event_post(&ev) != WINK_OK) {
        wink_trace_warn(WINK_WARN_DISTANCE_EVENT_QUEUE_FULL);
    }
}

static void us_dist_tick(void *arg)
{
    us_dist_slot_t *ctx = (us_dist_slot_t *)arg;
    float distance = 0.0f;
    wink_status_t st;

    if (ctx == NULL || ctx->dev == NULL) {
        return;
    }

    if (ctx->phase == US_DIST_NEED_TRIGGER) {
        wink_status_t rq = dal_ultrasonic_request_measurement(ctx->dev);
        if (wink_status_is_error(rq) && rq != WINK_ERR_BUSY) {
            /* Transient / hardware — retry next period. */
            return;
        }
        ctx->phase = US_DIST_WAITING;
    }

    st = dal_ultrasonic_get_cached_distance(ctx->dev, &distance);
    if (st == WINK_OK) {
        post_distance_ready(ctx->dev, distance);
        ctx->phase = US_DIST_NEED_TRIGGER;
    } else if (st == WINK_ERR_BUSY) {
        /* Still measuring. */
    } else {
        ctx->phase = US_DIST_NEED_TRIGGER;
    }
}

wink_status_t wink_ultrasonic_enable_distance_events(
    dal_ultrasonic_t *dev,
    const wink_ultrasonic_distance_event_config_t *cfg)
{
    int free_idx;
    us_dist_slot_t *ctx;
    wink_periodic_handle_t h;
    wink_status_t probe_st;

    if (dev == NULL || cfg == NULL ||
        cfg->period_ms < WINK_ULTRASONIC_DISTANCE_EVENT_MIN_PERIOD_MS) {
        return WINK_ERR_INVALID_ARG;
    }

    if (find_slot_by_dev(dev) >= 0) {
        return WINK_ERR_INVALID_STATE;
    }

    if (wink_sonar_helper_is_running(dev)) {
        LOG_D("enable: sonar helper already owns dev=%p", (void *)dev);
        return WINK_ERR_INVALID_STATE;
    }

    free_idx = find_free_slot();
    if (free_idx < 0) {
        return WINK_ERR_RESOURCE_EXHAUSTED;
    }

    ctx = &s_slots[free_idx];
    ctx->dev = dev;
    ctx->period_h = WINK_PERIODIC_INVALID;
    ctx->phase = US_DIST_NEED_TRIGGER;

    probe_st = dal_ultrasonic_request_measurement(dev);
    if (probe_st == WINK_ERR_NOT_INITIALIZED) {
        ctx->dev = NULL;
        return WINK_ERR_NOT_INITIALIZED;
    }
    /* Probe may leave a READY cache; tick will consume or re-trigger. */
    ctx->phase = US_DIST_WAITING;

    h = wink_periodic_start_ex(
        "us_dist", 0u, cfg->period_ms, us_dist_tick, ctx,
        WINK_PERIODIC_LIGHT, WINK_PERIODIC_DEFAULT_PRIORITY, WINK_PERIODIC_DEFAULT_CORE);
    if (h < 0) {
        ctx->dev = NULL;
        return (wink_status_t)h;
    }

    ctx->period_h = h;
    return WINK_OK;
}

void wink_ultrasonic_disable_distance_events(dal_ultrasonic_t *dev)
{
    int idx;

    if (dev == NULL) {
        return;
    }
    idx = find_slot_by_dev(dev);
    if (idx < 0) {
        return;
    }
    if (s_slots[idx].period_h > 0) {
        wink_periodic_stop(s_slots[idx].period_h);
    }
    s_slots[idx].period_h = WINK_PERIODIC_INVALID;
    s_slots[idx].dev = NULL;
    s_slots[idx].phase = US_DIST_NEED_TRIGGER;
}

bool wink_ultrasonic_distance_events_is_enabled(const dal_ultrasonic_t *dev)
{
    if (dev == NULL) {
        return false;
    }
    return find_slot_by_dev(dev) >= 0;
}

void wink_ultrasonic_distance_events_reset(void)
{
    for (int i = 0; i < WINK_ULTRASONIC_DISTANCE_EVENTS_MAX; i++) {
        if (s_slots[i].dev != NULL) {
            if (s_slots[i].period_h > 0) {
                wink_periodic_stop(s_slots[i].period_h);
            }
            s_slots[i].period_h = WINK_PERIODIC_INVALID;
            s_slots[i].dev = NULL;
            s_slots[i].phase = US_DIST_NEED_TRIGGER;
        }
    }
}

#else /* WINK_ULTRASONIC_DISTANCE_EVENTS_MAX == 0 */

wink_status_t wink_ultrasonic_enable_distance_events(
    dal_ultrasonic_t *dev,
    const wink_ultrasonic_distance_event_config_t *cfg)
{
    (void)dev;
    (void)cfg;
    return WINK_ERR_UNAVAILABLE;
}

void wink_ultrasonic_disable_distance_events(dal_ultrasonic_t *dev)
{
    (void)dev;
}

bool wink_ultrasonic_distance_events_is_enabled(const dal_ultrasonic_t *dev)
{
    (void)dev;
    return false;
}

void wink_ultrasonic_distance_events_reset(void)
{
}

#endif /* WINK_ULTRASONIC_DISTANCE_EVENTS_MAX > 0 */

WINK_INTERNAL_BLOCKING_REGION_END
