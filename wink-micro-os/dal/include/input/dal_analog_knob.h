// SPDX-License-Identifier: Apache-2.0
#ifndef DAL_ANALOG_KNOB_H
#define DAL_ANALOG_KNOB_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Full scale normalized position constant (1000 promille = 100.0%) */
#define DAL_ANALOG_KNOB_FULL_SCALE_PROMILLE 1000u

/**
 * @brief HMI Analog Knob Topology Variant Enum (DAL-S-001)
 */
typedef enum {
    DAL_ANALOG_KNOB_VARIANT_STANDARD = 0,        /**< Standard 3-Pin voltage divider potentiometer (default) */
    DAL_ANALOG_KNOB_VARIANT_LOGARITHMIC = 1,     /**< Audio logarithmic taper A (logarithmic-to-linear correction) */
    DAL_ANALOG_KNOB_VARIANT_ANTI_LOGARITHMIC = 2,/**< Anti-logarithmic taper C (anti-logarithmic correction) */
    DAL_ANALOG_KNOB_VARIANT_CENTER_DETENT = 3,  /**< Center detent with 50% midpoint deadzone clamping */
} dal_analog_knob_variant_t;

/**
 * @brief HMI Analog Knob / Slider Configuration Struct (POD config)
 * Members ordered descending by alignment for padding optimization.
 * First member MUST be owner (DAL-S-001).
 */
typedef struct {
    const char *owner;              /**< Instance owner static string (DAL-S-001) */
    uint16_t min_mv;                /**< Minimum voltage mV (0 && max_mv==0 -> platform full scale) */
    uint16_t max_mv;                /**< Maximum voltage mV (0 && min_mv==0 -> platform full scale) */
    uint16_t hysteresis_promille;   /**< Hysteresis anti-jitter threshold in promille [0, 500] */
    uint16_t pin;                   /**< Physical GPIO / ADC input pin */
    int16_t enable_pin;             /**< Optional power enable pin (-1 = unused) */
    dal_analog_knob_variant_t variant; /**< Topology variant enum (DAL-S-001) */
    bool inverted;                  /**< Direction reversal: false=0->1000, true=1000->0 */
} dal_analog_knob_config_t;

/**
 * @brief HMI Analog Knob Handle Struct (POD instance)
 * config is embedded as the first member (offsetof == 0, DAL-S-011).
 */
typedef struct {
    dal_analog_knob_config_t config;     /**< Config copy (offsetof == 0) */
    uint16_t last_knob_promille;         /**< Last normalized reading (promille) */
    uint16_t last_raw;                   /**< Last raw ADC sample */
    bool initialized;                    /**< Init status flag */
    volatile wink_status_t last_status;  /**< Observability last status code */
} dal_analog_knob_t;

/* Static assertions for ABI freeze and first-member guard (DAL-S-011 / DAL-S-014) */
_Static_assert(offsetof(dal_analog_knob_t, config) == 0, "config must be the first member");

#if INTPTR_MAX == INT32_MAX   /* ILP32: ESP32 xtensa, wasm32 */
_Static_assert(sizeof(dal_analog_knob_config_t) == 24, "ABI break: config size changed on 32-bit target");
_Static_assert(offsetof(dal_analog_knob_t, initialized) == 28, "ABI break: initialized offset changed on 32-bit");
_Static_assert(sizeof(dal_analog_knob_t) == 36, "ABI break: handle size changed on 32-bit target");
#else                         /* LP64 / LLP64: 64-bit Host Simulation */
_Static_assert(sizeof(dal_analog_knob_config_t) == 32, "ABI break: config size changed on 64-bit host");
_Static_assert(offsetof(dal_analog_knob_t, initialized) == 36, "ABI break: initialized offset changed on 64-bit host");
_Static_assert(sizeof(dal_analog_knob_t) == 48, "ABI break: handle size changed on 64-bit host");
#endif

WINK_WARN_UNUSED_RESULT
wink_status_t dal_analog_knob_init(dal_analog_knob_t *dev, const dal_analog_knob_config_t *cfg);

wink_status_t dal_analog_knob_deinit(dal_analog_knob_t *dev);

WINK_WARN_UNUSED_RESULT
wink_status_t dal_analog_knob_read_promille(dal_analog_knob_t *dev, uint16_t *out_knob_promille);

WINK_WARN_UNUSED_RESULT
wink_status_t dal_analog_knob_read_mv(dal_analog_knob_t *dev, uint16_t *out_mv);

WINK_WARN_UNUSED_RESULT
wink_status_t dal_analog_knob_poll(dal_analog_knob_t *dev, bool *out_changed, uint16_t *out_knob_promille);

WINK_WARN_UNUSED_RESULT
wink_status_t dal_analog_knob_get_status(const dal_analog_knob_t *dev, wink_status_t *out_status);

#ifdef __cplusplus
}
#endif

/* Compile-time pruning stubs */
#if !defined(WINK_USE_ANALOG_KNOB) || !WINK_USE_ANALOG_KNOB
#define WINK_ANALOG_KNOB_DISABLED_MSG \
    "AnalogKnob driver not enabled; add a \"analog_knob\" device to wink-app.json " \
    "(or set -DWINK_USE_ANALOG_KNOB=ON)."
WINK_UNAVAILABLE_MSG(WINK_ANALOG_KNOB_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_analog_knob_init(dal_analog_knob_t *dev, const dal_analog_knob_config_t *cfg);
WINK_UNAVAILABLE_MSG(WINK_ANALOG_KNOB_DISABLED_MSG)
wink_status_t dal_analog_knob_deinit(dal_analog_knob_t *dev);
WINK_UNAVAILABLE_MSG(WINK_ANALOG_KNOB_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_analog_knob_read_promille(dal_analog_knob_t *dev, uint16_t *out_promille);
WINK_UNAVAILABLE_MSG(WINK_ANALOG_KNOB_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_analog_knob_read_mv(dal_analog_knob_t *dev, uint16_t *out_mv);
WINK_UNAVAILABLE_MSG(WINK_ANALOG_KNOB_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_analog_knob_poll(dal_analog_knob_t *dev, bool *out_changed, uint16_t *out_promille);
WINK_UNAVAILABLE_MSG(WINK_ANALOG_KNOB_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_analog_knob_get_status(const dal_analog_knob_t *dev, wink_status_t *out_status);
#endif /* !WINK_USE_ANALOG_KNOB */

#endif /* DAL_ANALOG_KNOB_H */
