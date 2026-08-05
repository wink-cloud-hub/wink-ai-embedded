// SPDX-License-Identifier: Apache-2.0
#ifndef DAL_RELAY_H
#define DAL_RELAY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Default latching pulse duration in ms */
#define DAL_RELAY_DEFAULT_PULSE_MS 50u

/** @brief Maximum latching pulse duration in ms */
#define DAL_RELAY_MAX_PULSE_MS 1000u

/**
 * @brief Relay topology variant enum
 */
typedef enum {
    DAL_RELAY_VARIANT_DIRECT_GPIO       = 0, /**< Direct GPIO / Optocoupler isolated (default) */
    DAL_RELAY_VARIANT_SSR               = 1, /**< Solid State Relay (SSR) */
    DAL_RELAY_VARIANT_LATCHING_DUAL_PIN = 2, /**< Dual-coil latching relay */
} dal_relay_variant_t;

/**
 * @brief Relay configuration struct (POD config_t)
 */
typedef struct {
    const char *owner;              /**< Instance owner static string */
    uint16_t pin;                   /**< Main control / Set pin */
    int16_t reset_pin;              /**< Reset pin for dual-coil latching (-1 if unused) */
    uint16_t pulse_duration_ms;     /**< Latching pulse duration in ms (0 -> default 50ms) */
    dal_relay_variant_t variant;    /**< Topology variant enum */
    bool active_low;                /**< Trigger active-low polarity (true = active low) */
    bool initial_state;             /**< Initial state after init (true = closed, false = open) */
} dal_relay_config_t;

/**
 * @brief Relay handle struct (POD instance_t)
 */
typedef struct {
    dal_relay_config_t config;      /**< Config copy */
    uint32_t pulse_start_ms;        /**< Latching pulse start timestamp */
    bool is_on;                     /**< Current logical switch state */
    bool pulse_active;              /**< Latching pulse active flag */
    bool initialized;               /**< Init state flag */
    volatile wink_status_t last_status; /**< Last operation status code */
} dal_relay_t;

_Static_assert(offsetof(dal_relay_t, config) == 0, "config must be the first member");

#if INTPTR_MAX == INT32_MAX   /* ILP32: ESP32 xtensa, wasm32 */
_Static_assert(sizeof(dal_relay_config_t) == 20, "ABI break: config size changed on 32-bit target");
_Static_assert(offsetof(dal_relay_t, initialized) == 26, "ABI break: initialized offset changed on 32-bit");
_Static_assert(sizeof(dal_relay_t) == 32, "ABI break: handle size changed on 32-bit target");
#else                         /* LP64 / LLP64: 64-bit Host Simulation */
_Static_assert(sizeof(dal_relay_config_t) == 24, "ABI break: config size changed on 64-bit host");
_Static_assert(offsetof(dal_relay_t, initialized) == 30, "ABI break: initialized offset changed on 64-bit host");
_Static_assert(sizeof(dal_relay_t) == 40, "ABI break: handle size changed on 64-bit host");
#endif

/**
 * @brief Initialize relay driver instance
 *
 * @param[in,out] dev Relay instance handle.
 * @param[in] cfg Configuration struct.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_relay_init(dal_relay_t *dev, const dal_relay_config_t *cfg);

/**
 * @brief Deinitialize relay driver
 *
 * @param[in,out] dev Relay instance handle.
 * @return WINK_OK on success, error status code otherwise.
 */
wink_status_t dal_relay_deinit(dal_relay_t *dev);

/**
 * @brief Set relay switch state
 *
 * @param[in,out] dev Relay instance handle.
 * @param[in] on Target state (true = closed/on, false = open/off).
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_relay_set(dal_relay_t *dev, bool on);

/**
 * @brief Turn on / close relay
 *
 * @param[in,out] dev Relay instance handle.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_relay_on(dal_relay_t *dev);

/**
 * @brief Turn off / open relay
 *
 * @param[in,out] dev Relay instance handle.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_relay_off(dal_relay_t *dev);

/**
 * @brief Toggle relay switch state
 *
 * @param[in,out] dev Relay instance handle.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_relay_toggle(dal_relay_t *dev);

/**
 * @brief Read relay current logical state
 *
 * @param[in] dev Relay instance handle.
 * @param[out] out_on Output pointer (true = closed/on, false = open/off).
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_relay_is_on(const dal_relay_t *dev, bool *out_on);

/**
 * @brief Read last operation status code
 *
 * @param[in] dev Relay instance handle.
 * @return Last operation status code.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_relay_get_last_status(const dal_relay_t *dev);

/**
 * @brief Poll relay pulse timer (non-blocking pulse clearing for latching relays)
 *
 * @param[in,out] dev Relay instance handle.
 * @return WINK_OK on success, error status code otherwise.
 */
wink_status_t dal_relay_poll(dal_relay_t *dev);

/**
 * @brief Relay safe-off (emergency / fault safe-off)
 *
 * @param[in,out] dev Relay instance handle.
 * @return WINK_OK on success, error status code otherwise.
 */
wink_status_t dal_relay_safe_off(dal_relay_t *dev);

#ifdef __cplusplus
}
#endif

#if !defined(WINK_USE_RELAY) || !WINK_USE_RELAY
#define WINK_RELAY_DISABLED_MSG \
    "Relay driver not enabled; add a \"relay\" device to wink-app.json " \
    "(or set -DWINK_USE_RELAY=ON)."
WINK_UNAVAILABLE_MSG(WINK_RELAY_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_relay_init(dal_relay_t *dev, const dal_relay_config_t *cfg);
WINK_UNAVAILABLE_MSG(WINK_RELAY_DISABLED_MSG)
wink_status_t dal_relay_deinit(dal_relay_t *dev);
WINK_UNAVAILABLE_MSG(WINK_RELAY_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_relay_set(dal_relay_t *dev, bool on);
WINK_UNAVAILABLE_MSG(WINK_RELAY_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_relay_on(dal_relay_t *dev);
WINK_UNAVAILABLE_MSG(WINK_RELAY_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_relay_off(dal_relay_t *dev);
WINK_UNAVAILABLE_MSG(WINK_RELAY_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_relay_toggle(dal_relay_t *dev);
WINK_UNAVAILABLE_MSG(WINK_RELAY_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_relay_is_on(const dal_relay_t *dev, bool *out_on);
WINK_UNAVAILABLE_MSG(WINK_RELAY_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_relay_get_last_status(const dal_relay_t *dev);
WINK_UNAVAILABLE_MSG(WINK_RELAY_DISABLED_MSG)
wink_status_t dal_relay_poll(dal_relay_t *dev);
WINK_UNAVAILABLE_MSG(WINK_RELAY_DISABLED_MSG)
wink_status_t dal_relay_safe_off(dal_relay_t *dev);

#endif /* !WINK_USE_RELAY */

#endif /* DAL_RELAY_H */
