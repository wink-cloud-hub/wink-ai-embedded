/**
 * @file wink_sim_ultrasonic_echo.h
 * @brief Bringup selftest helper: one-line ultrasonic echo simulator (S10 shadow task).
 *
 * Wraps the TRIG-ISR + sem + core-pinned mock task + GPIO direction bookkeeping.
 * Lab bring-up / smoke-test only — NOT a product DAL feature, and NOT a stable
 * public API. Lives under runtime/selftest/src/ (same internal-header surface
 * as the other selftest entries) rather than runtime/include/ because consumers
 * add runtime/selftest/src as a PRIVATE include dir.
 *
 * STRICT_NONBLOCKING gate (ADR-0017 layer-2): the whole implementation calls
 * blocking APIs (sem_take FOREVER, task_create, busy_wait_us). Under
 * -DWINK_STRICT_NONBLOCKING=1 the declarations and definitions compile out so
 * that strict-mirror TUs don't pull in blocking symbols.
 */
#ifndef WINK_SIM_ULTRASONIC_ECHO_H
#define WINK_SIM_ULTRASONIC_ECHO_H

#include <stdint.h>
#include "wink_status.h"
#include "dal_ultrasonic.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ADR-0017 layer-2 hard isolation: bringup helpers legitimately call blocking
 * APIs; strict non-blocking images must not see them. Guard the whole API. */
#ifndef WINK_STRICT_NONBLOCKING

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

#endif /* WINK_STRICT_NONBLOCKING */

#ifdef __cplusplus
}
#endif

#endif /* WINK_SIM_ULTRASONIC_ECHO_H */
