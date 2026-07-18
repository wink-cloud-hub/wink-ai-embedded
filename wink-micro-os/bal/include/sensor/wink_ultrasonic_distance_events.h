/**
 * @file wink_ultrasonic_distance_events.h
 * @brief BAL B-class API — ultrasonic distance completion → wink_event queue
 *        (ADR-0033).
 *
 * L1 Role verb: `{name}_enable_distance_events` / `_disable_distance_events`.
 * Each completed measurement posts one WINK_EVENT_DISTANCE_READY (continuous
 * sampling while enabled — not a one-shot).
 *
 * Layering (ADR-0023 §8): this header MUST NOT include any pal_*.h.
 *
 * Copyright (c) 2026 Wink-AI.
 */
#ifndef WINK_ULTRASONIC_DISTANCE_EVENTS_H
#define WINK_ULTRASONIC_DISTANCE_EVENTS_H

#include <stdbool.h>
#include <stdint.h>
#include "wink_status.h"
#include "dal_ultrasonic.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WINK_ULTRASONIC_DISTANCE_EVENTS_MAX
# ifdef WINK_APP_MAX_ULTRASONIC_INSTANCES
#  define WINK_ULTRASONIC_DISTANCE_EVENTS_MAX WINK_APP_MAX_ULTRASONIC_INSTANCES
# else
#  define WINK_ULTRASONIC_DISTANCE_EVENTS_MAX 4
# endif
#endif

/** Minimum period_ms (HC-SR04 acoustic crosstalk budget; matches ultrasonic poll). */
#define WINK_ULTRASONIC_DISTANCE_EVENT_MIN_PERIOD_MS 50u

/**
 * @brief Config for one ultrasonic distance-event stream.
 *
 * Typically `static const` from codegen (`auto_poll_ms` baked in).
 */
typedef struct {
    uint32_t period_ms; /**< Measurement cycle; must be >= 50. */
} wink_ultrasonic_distance_event_config_t;

/**
 * @brief Enable distance-event posting for @p dev (B-class, ADR-0032/0033).
 *
 * Periodically requests a measurement and, on each completion, posts
 * WINK_EVENT_DISTANCE_READY with param = round(cm * 10).
 *
 * @return WINK_OK /
 *         WINK_ERR_INVALID_ARG (NULL or period_ms < 50) /
 *         WINK_ERR_INVALID_STATE (already enabled, or ultrasonic poll owns @p dev) /
 *         WINK_ERR_RESOURCE_EXHAUSTED /
 *         WINK_ERR_NOT_INITIALIZED /
 *         codes from wink_periodic_start_ex.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_ultrasonic_enable_distance_events(
    dal_ultrasonic_t *dev,
    const wink_ultrasonic_distance_event_config_t *cfg);

/**
 * @brief Stop posting distance events for @p dev (idempotent).
 */
void wink_ultrasonic_disable_distance_events(dal_ultrasonic_t *dev);

/**
 * @brief True if @p dev currently has an active distance-event stream.
 */
bool wink_ultrasonic_distance_events_is_enabled(const dal_ultrasonic_t *dev);

/**
 * @brief Test helper: stop all slots.
 */
void wink_ultrasonic_distance_events_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* WINK_ULTRASONIC_DISTANCE_EVENTS_H */
