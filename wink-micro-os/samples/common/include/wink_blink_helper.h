/**
 * @file wink_blink_helper.h
 * @brief Sample helper: blink a DAL LED via soft_timer.
 *
 * NOT part of the OS core — lives in samples/common because blinking is a
 * common demo pattern.  Core `wink_soft_timer` mechanism is always available
 * for apps that want to roll their own.
 */
#ifndef WINK_BLINK_HELPER_H
#define WINK_BLINK_HELPER_H

#include <stdint.h>
#include "wink_status.h"
#include "dal_led.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start a periodic 50%-duty blink on @p led.
 *
 * Fire-and-forget: the return value is optional. Failures are logged
 * internally at LOG_D level (visible symptom = the LED simply does not blink).
 * Callers that need to stop the blink should capture the handle; callers that
 * blink for the lifetime of the process may safely ignore the return.
 *
 * @param led        Initialised LED instance.
 * @param period_ms  Full blink period (on+off).  Must be a multiple of
 *                   WINK_RUNTIME_TICK_MS (10ms) for exact timing.
 * @return >=0 soft_timer handle on success; <0 WINK_ERR_* on failure
 *         (logged internally at LOG_D; safe to ignore).
 */
int32_t wink_led_blink_start(dal_led_t *led, uint32_t period_ms);

/**
 * @brief Stop a blink previously started with wink_led_blink_start().
 * @param handle  Value returned by wink_led_blink_start().
 */
void wink_led_blink_stop(int32_t handle);

#ifdef __cplusplus
}
#endif

#endif /* WINK_BLINK_HELPER_H */
