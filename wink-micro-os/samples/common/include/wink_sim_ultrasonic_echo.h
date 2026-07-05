/**
 * @file wink_sim_ultrasonic_echo.h
 * @brief Sample helper: one-line ultrasonic echo simulator (S10 shadow task).
 *
 * Wraps the 130-line TRIG-ISR + sem + core-pinned mock task + GPIO direction
 * bookkeeping.  Lab bring-up only — NOT a product DAL feature.
 */
#ifndef WINK_SIM_ULTRASONIC_ECHO_H
#define WINK_SIM_ULTRASONIC_ECHO_H

#include <stdint.h>
#include "wink_status.h"
#include "dal_ultrasonic.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start the echo-simulator shadow task.
 *
 * @param dev           Initialised ultrasonic device.
 * @param simulated_cm  Distance to simulate (clamped to [2,400]cm).
 * @param trig_pin      TRIG GPIO (must match cfg->trig_pin).
 * @param echo_pin      ECHO GPIO (must match cfg->echo_pin).
 * @return WINK_OK on success; WINK_ERR_* on failure.  On host/wasm this is a
 *         no-op that returns WINK_OK (no real GPIO ISR there).
 */
wink_status_t wink_sim_ultrasonic_echo_start(dal_ultrasonic_t *dev,
                                             float simulated_cm,
                                             uint16_t trig_pin,
                                             uint16_t echo_pin);

/**
 * @brief Stop the echo simulator (disarm ISR; mock task goes idle).
 *
 * Safe to call even if start was never called or failed (idempotent).
 */
void wink_sim_ultrasonic_echo_stop(dal_ultrasonic_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* WINK_SIM_ULTRASONIC_ECHO_H */
