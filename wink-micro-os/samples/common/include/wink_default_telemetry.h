/**
 * @file wink_default_telemetry.h
 * @brief Sample helper: one-call default 2s telemetry task.
 *
 * Print format is a DEBUG / BRINGUP policy choice — does NOT belong in the OS
 * core.  Apps wanting different fields/cadence should roll their own using
 * wink_runtime_spawn_periodic() + wink_runtime_get_stats().
 */
#ifndef WINK_DEFAULT_TELEMETRY_H
#define WINK_DEFAULT_TELEMETRY_H

#include "wink_status.h"
#include "dal_ultrasonic.h"
#include "dal_button.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start a fire-and-forget 2s telemetry task (uptime/heap/faults/warns
 *        + optional sonar distance + button ISR count).
 *
 * @param sonar  Ultrasonic device to report (NULL → skip sonar field).
 * @param btn    Button whose edge_count is reported (NULL → skip isr field).
 * @return WINK_OK on spawn; WINK_ERR_* if the underlying task create fails.
 */
wink_status_t wink_default_telemetry_start(const dal_ultrasonic_t *sonar,
                                           const dal_button_t     *btn);

#ifdef __cplusplus
}
#endif

#endif /* WINK_DEFAULT_TELEMETRY_H */
