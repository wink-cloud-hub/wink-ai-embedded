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
 * @brief Ultrasonic sensor physical variant (affects pinout, affects_pins: true)
 */
typedef enum {
    DAL_ULTRASONIC_VARIANT_HCSR04          = 0, /**< Default: 4Pin dual-pin pulse (HC-SR04/HY-SRF05) */
    DAL_ULTRASONIC_VARIANT_SINGLE_PIN_PING = 1, /**< 3Pin single-pin bidirectional pulse (Parallax PING))) */
    DAL_ULTRASONIC_VARIANT_UART_STREAM     = 2, /**< 4Pin UART serial stream mode (US-100/A02YYUW) */
    DAL_ULTRASONIC_VARIANT_I2C             = 3, /**< 4Pin I2C register mode (Devantech SRF02/SRF08) */
    DAL_ULTRASONIC_VARIANT_COUNT           = 4, /**< Total variant count for static assertion */
} dal_ultrasonic_variant_t;

/**
 * @brief MCU pulse capture demodulation backend (affects_pins: false)
 */
typedef enum {
    DAL_ULTRASONIC_BACKEND_AUTO      = 0, /**< Auto select (ESP32 RMT hardware capture if available, GPIO poll fallback) */
    DAL_ULTRASONIC_BACKEND_GPIO_POLL = 1, /**< Standard GPIO polling / ISR capture */
    DAL_ULTRASONIC_BACKEND_ESP32_RMT = 2, /**< ESP32 RMT peripheral hardware pulse capture */
} dal_ultrasonic_backend_t;

/**
 * @brief Ultrasonic sensor configuration struct (Flat layout with sentinel trimming)
 */
typedef struct {
    const char               *owner;      /**< Instance owner static string */
    uint32_t                  baud_rate;  /**< Serial baud rate (typically 9600) */
    uint32_t                  timeout_us; /**< Measurement timeout threshold in µs (default 30000us) */
    dal_ultrasonic_variant_t  variant;    /**< Hardware interface variant */
    dal_ultrasonic_backend_t  backend;    /**< MCU pulse capture backend (pulse modes only) */
    int16_t                   trig_pin;   /**< Trigger pin (-1 if unused) */
    int16_t                   echo_pin;   /**< Echo pin (-1 if unused) */
    int16_t                   sig_pin;    /**< Bidirectional SIG pin (-1 if unused) */
    uint16_t                  i2c_addr;   /**< 7-bit I2C slave address (e.g. 0x70) */
    uint8_t                   uart_port;  /**< Logical UART port index */
    uint8_t                   i2c_port;   /**< Logical I2C port index */
    uint8_t                   _reserved[2];/**< ABI alignment padding */
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
_Static_assert(DAL_ULTRASONIC_VARIANT_COUNT == 4, 
               "Variant count mismatch with SSOT §2 and codegen YAML");
_Static_assert(DAL_ULTRASONIC_VARIANT_I2C + 1 == DAL_ULTRASONIC_VARIANT_COUNT, 
               "Sequential variant ordering error: last member index check failed");

#if INTPTR_MAX == INT32_MAX   /* ILP32: ESP32 xtensa, wasm32 */
_Static_assert(sizeof(dal_ultrasonic_config_t) == 32, "ABI break: config size changed on 32-bit target");
_Static_assert(offsetof(dal_ultrasonic_t, initialized) == 48, "ABI break: initialized offset changed on 32-bit");
_Static_assert(sizeof(dal_ultrasonic_t) == 52, "ABI break: handle size changed on 32-bit target");
#else                         /* LP64 / LLP64: 64-bit Host Simulation */
_Static_assert(sizeof(dal_ultrasonic_config_t) == 40, "ABI break: config size changed on 64-bit host");
_Static_assert(offsetof(dal_ultrasonic_t, initialized) == 56, "ABI break: initialized offset changed on 64-bit host");
_Static_assert(sizeof(dal_ultrasonic_t) == 64, "ABI break: handle size changed on 64-bit host");
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
