// SPDX-License-Identifier: Apache-2.0
#ifndef DAL_ULTRASONIC_H
#define DAL_ULTRASONIC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Non-blocking measurement state machine */
typedef enum {
    DAL_ULTRASONIC_IDLE      = 0,
    DAL_ULTRASONIC_MEASURING = 1,
    DAL_ULTRASONIC_READY     = 2,
    DAL_ULTRASONIC_ERROR     = 3,
} dal_ultrasonic_state_t;

/**
 * @brief Ultrasonic sensor configuration struct
 */
typedef struct {
    const char *owner;    /**< Instance owner static string */
    uint16_t trig_pin;    /**< Trigger pin */
    uint16_t echo_pin;    /**< Echo pin */
    bool use_rmt;         /**< ESP32: true = RMT hardware capture, false = busy-wait fallback */
} dal_ultrasonic_config_t;

/**
 * @brief Ultrasonic sensor non-blocking handle struct (POD)
 */
typedef struct {
    dal_ultrasonic_config_t config;          /**< Config copy */
    volatile float          last_distance;   /**< Last measured distance in cm */
    volatile uint32_t       last_pulse_us;   /**< Last measured echo pulse in µs */
    volatile wink_status_t  last_status;     /**< Last measurement status code */
    volatile dal_ultrasonic_state_t  state;  /**< State machine state */
    bool                    initialized;     /**< Set to true after successful init */
} dal_ultrasonic_t;

_Static_assert(offsetof(dal_ultrasonic_t, config) == 0, "config must be the first member");

#if INTPTR_MAX == INT32_MAX   /* ILP32: ESP32 xtensa, wasm32 */
_Static_assert(sizeof(dal_ultrasonic_config_t) == 12, "ABI break: config size changed on 32-bit target");
_Static_assert(offsetof(dal_ultrasonic_t, initialized) == 28, "ABI break: initialized offset changed on 32-bit");
_Static_assert(sizeof(dal_ultrasonic_t) == 32, "ABI break: handle size changed on 32-bit target");
#else                         /* LP64 / LLP64: 64-bit Host Simulation */
_Static_assert(sizeof(dal_ultrasonic_config_t) == 16, "ABI break: config size changed on 64-bit host");
_Static_assert(offsetof(dal_ultrasonic_t, initialized) == 32, "ABI break: initialized offset changed on 64-bit host");
_Static_assert(sizeof(dal_ultrasonic_t) == 40, "ABI break: handle size changed on 64-bit host");
#endif

/**
 * @brief Initialize ultrasonic sensor driver instance
 *
 * @param[in,out] dev Ultrasonic instance handle.
 * @param[in] cfg Configuration struct.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_ultrasonic_init(dal_ultrasonic_t *dev, const dal_ultrasonic_config_t *cfg);

/**
 * @brief Request a new distance measurement (non-blocking trigger)
 *
 * @param[in,out] dev Ultrasonic instance handle.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_ultrasonic_request_measurement(dal_ultrasonic_t *dev);

/**
 * @brief Read cached distance and measurement status (non-blocking)
 *
 * @param[in] dev Ultrasonic instance handle.
 * @param[out] out_distance_cm Output pointer for distance in cm.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_ultrasonic_get_cached_distance(const dal_ultrasonic_t *dev, float *out_distance_cm);

#ifndef WINK_STRICT_NONBLOCKING
/**
 * @brief Read distance in cm (blocking measurement, @deprecated)
 *
 * @param[in,out] dev Ultrasonic instance handle.
 * @param[out] out_distance_cm Output pointer for distance in cm.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_BLOCKING WINK_WARN_UNUSED_RESULT
wink_status_t dal_ultrasonic_read(dal_ultrasonic_t *dev, float *out_distance_cm);
#endif

/**
 * @brief Apply Flash parameter overrides before initialization (ADR-0008)
 *
 * @param[in,out] dev Ultrasonic instance handle pointer.
 * @param[in] params Binary parameter payload.
 * @param[in] len Parameter length in bytes.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_ultrasonic_apply_override(void *dev, const uint8_t *params, uint16_t len);

/**
 * @brief Deinitialize ultrasonic sensor driver
 *
 * @param[in,out] dev Ultrasonic instance handle.
 * @return WINK_OK on success, error status code otherwise.
 */
wink_status_t dal_ultrasonic_deinit(dal_ultrasonic_t *dev);

#ifdef __cplusplus
}
#endif

/* Compile-time pruning stubs */
#if !defined(WINK_USE_ULTRASONIC) || !WINK_USE_ULTRASONIC
#define WINK_ULTRASONIC_DISABLED_MSG \
    "Ultrasonic driver not enabled; add an \"ultrasonic\" device to " \
    "wink-app.json (or set -DWINK_USE_ULTRASONIC=ON)."
WINK_UNAVAILABLE_MSG(WINK_ULTRASONIC_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_ultrasonic_init(dal_ultrasonic_t *dev, const dal_ultrasonic_config_t *cfg);
WINK_UNAVAILABLE_MSG(WINK_ULTRASONIC_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_ultrasonic_request_measurement(dal_ultrasonic_t *dev);
WINK_UNAVAILABLE_MSG(WINK_ULTRASONIC_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_ultrasonic_get_cached_distance(const dal_ultrasonic_t *dev, float *out_distance_cm);
WINK_UNAVAILABLE_MSG(WINK_ULTRASONIC_DISABLED_MSG) WINK_BLOCKING WINK_WARN_UNUSED_RESULT
wink_status_t dal_ultrasonic_read(dal_ultrasonic_t *dev, float *out_distance_cm);
WINK_UNAVAILABLE_MSG(WINK_ULTRASONIC_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_ultrasonic_apply_override(void *dev, const uint8_t *params, uint16_t len);
WINK_UNAVAILABLE_MSG(WINK_ULTRASONIC_DISABLED_MSG)
wink_status_t dal_ultrasonic_deinit(dal_ultrasonic_t *dev);
#endif /* !WINK_USE_ULTRASONIC */

#endif /* DAL_ULTRASONIC_H */
