// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_rmt.h
 * @brief PAL generic pulse-capture API.
 *
 * Provides a hardware-independent non-blocking pulse width measurement primitive.
 * The legacy name "pal_rmt" is retained from the RMT (Remote Control Transceiver)
 * peripheral on ESP32, which serves as the physical execution backend. The semantics
 * have been generalized to generic pulse capture: given a digital input pin and trigger
 * edge type, measure the edge-to-opposite-edge pulse duration in microseconds.
 *
 * Typical applications:
 *   - Ultrasonic (HC-SR04) ECHO pulse width (PAL_RMT_EDGE_RISING)
 *   - Infrared receiver (IR receiver) decoding
 *   - Incremental encoder pulse width / duty cycle measurement
 *
 * @note Single Instance Semantics: The current implementation supports a single active
 *   pulse-capture channel (static singleton design). Multi-channel capture will be
 *   expanded in a future ADR if required.
 *
 * @note Platform Support Matrix:
 *   - ESP32: Fully implemented via RMT RX channel.
 *   - Wasm/Host: Direct pal_gpio_pulse_in fallback. This API acts as a stub returning
 *     WINK_ERR_UNSUPPORTED (is_active returns false).
 */

#ifndef PAL_RMT_H
#define PAL_RMT_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"
#include "pal_hal.h"      /* wink_pin_t */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Pulse capture start edge direction.
 *
 * Determines which voltage transition starts the measurement:
 *   - RISING  : Start on rising edge, stop on falling edge (high pulse width)
 *   - FALLING : Start on falling edge, stop on rising edge (low pulse width)
 */
typedef enum {
    PAL_RMT_EDGE_RISING  = 0,   /**< Start at rising edge, measure to falling edge (high pulse width). */
    PAL_RMT_EDGE_FALLING = 1,   /**< Start at falling edge, measure to rising edge (low pulse width). */
} pal_rmt_edge_t;

/**
 * @brief Initialize pulse-capture channel and bind to input pin.
 *
 * @param[in] pin Input pin number (wink_pin_t)
 * @param[in] start_edge Start edge trigger direction (RISING / FALLING)
 * @return WINK_OK on success, error status on failure.
 *
 * @note Idempotency: If already bound to the same pin, returns WINK_OK.
 *   If bound to a different pin, deinits the old binding first.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_rmt_pulse_capture_init(wink_pin_t pin, pal_rmt_edge_t start_edge);

/**
 * @brief Arm pulse-capture receiver: start listening for next start edge (non-blocking).
 *
 * Used in software-triggered pulse capture mode: arm first to set receiver into
 * listening state, drive signal in caller software, and then call wait_armed to block for result.
 *
 * @return WINK_OK on success, WINK_ERR_INVALID_ARG if uninitialized, or hardware error.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_rmt_pulse_capture_arm(void);

#ifndef WINK_STRICT_NONBLOCKING
/**
 * @brief Wait for a previously armed pulse-capture to complete and return pulse duration.
 *
 * @param[in] timeout_us Timeout in microseconds.
 * @param[out] pulse_us_out Output pulse duration pointer in microseconds.
 * @return WINK_OK on success, WINK_ERR_TIMEOUT on timeout, or error status.
 *
 * @note Blocking: Yes. Not available under WINK_STRICT_NONBLOCKING (ADR-0017 Layer 2).
 */
WINK_BLOCKING
WINK_WARN_UNUSED_RESULT
wink_status_t pal_rmt_pulse_capture_wait_armed(uint32_t timeout_us, uint32_t *pulse_us_out);

/**
 * @brief Wait for pulse capture completion and return pulse duration in microseconds.
 *
 * Convenience wrapper over arm() + wait_armed().
 *
 * @param[in] timeout_us Timeout in microseconds.
 * @param[out] pulse_us_out Output pulse duration pointer in microseconds.
 * @return WINK_OK on success, WINK_ERR_TIMEOUT on timeout, or error status.
 *
 * @note Blocking: Yes. Not available under WINK_STRICT_NONBLOCKING (ADR-0017 Layer 2).
 */
WINK_BLOCKING
WINK_WARN_UNUSED_RESULT
wink_status_t pal_rmt_pulse_capture_wait(uint32_t timeout_us, uint32_t *pulse_us_out);
#endif /* WINK_STRICT_NONBLOCKING */

/**
 * @brief De-initialize pulse-capture channel and release resources.
 *
 * Idempotent: No-op if not initialized.
 */
void pal_rmt_pulse_capture_deinit(void);

/**
 * @brief Query if pulse-capture channel is currently active.
 *
 * @return true if initialized, false otherwise.
 */
bool pal_rmt_pulse_capture_is_active(void);

#ifdef __cplusplus
}
#endif

#endif /* PAL_RMT_H */
