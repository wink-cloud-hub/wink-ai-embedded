// SPDX-License-Identifier: Apache-2.0
#ifndef DAL_NTC_H
#define DAL_NTC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_status.h"
#include "hal/pal_pin_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief NTC hardware interface variant.
 *
 * - @c DAL_NTC_VARIANT_SINGLE_ENDED_ADC: standard 3-pin resistor divider [VCC,GND,AO].
 * - @c DAL_NTC_VARIANT_DIFFERENTIAL_BRIDGE: 4-pin Wheatstone bridge. Phase 1 returns
 *   @c WINK_ERR_UNSUPPORTED (PAL PGA/differential ADC not yet available).
 */
typedef enum {
    DAL_NTC_VARIANT_SINGLE_ENDED_ADC    = 0,
    DAL_NTC_VARIANT_DIFFERENTIAL_BRIDGE = 1,
    DAL_NTC_VARIANT_COUNT               = 2,
} dal_ntc_variant_t;

/**
 * @brief Divider topology (absorbs board wiring variation; see ADR-0069).
 */
typedef enum {
    DAL_NTC_DIVIDER_PULL_UP   = 0, /**< VCC -> R_pull -> (AO) -> NTC -> GND (temp rises, V falls) */
    DAL_NTC_DIVIDER_PULL_DOWN = 1, /**< VCC -> NTC -> (AO) -> R_pull -> GND (temp rises, V rises) */
} dal_ntc_divider_t;

/**
 * @brief NTC configuration (flat POD, members ordered descending by alignment).
 *
 * First member MUST be @c owner (DAL-S-001). @c ao_pin is required (uint16_t, DAL-S-006);
 * @c diff_neg_pin is optional and uses the @c WINK_PIN_NC sentinel.
 *
 * @c lut_table is required on the Micro Profile for @c dal_ntc_read_ddegc (no soft-float);
 * codegen generates a per-instance 33-entry table from @c r25_ohm / @c b_value /
 * @c r_pull_ohm / @c divider_type and fills this field. On the Full Profile the float
 * path @c dal_ntc_read_degc works with @c lut_table == NULL.
 */
typedef struct {
    const char           *owner;             /**< Instance owner static string (MUST be a literal) */
    uint32_t              r25_ohm;           /**< Nominal resistance at 25 degC (e.g. 100000) */
    uint32_t              r_pull_ohm;        /**< Fixed divider resistor (e.g. 4700) */
    dal_ntc_variant_t     variant;           /**< Hardware variant */
    dal_ntc_divider_t     divider_type;      /**< Divider topology */
    uint16_t              ao_pin;            /**< Analog input pin (DAL-S-006: required -> uint16_t) */
    wink_pin_t            diff_neg_pin;      /**< Differential negative pin (optional, WINK_PIN_NC if unused) */
    uint16_t              b_value;           /**< B constant 25/50 (e.g. 3950) */
    uint16_t              vref_mv;           /**< ADC Vref in mV (0 = query pal_adc_full_scale_mv) */
    int16_t               min_valid_temp_c;  /**< Low valid-limit in degC (below -> WINK_ERR_OUT_OF_RANGE) */
    int16_t               max_valid_temp_c;  /**< High safe-limit in degC (above -> WINK_ERR_OVERTEMPERATURE) */
    uint8_t               debounce_count;    /**< Consecutive abnormal samples before latching (0 = immediate) */
    uint8_t               reserved[3];       /**< Explicit alignment padding */
    const int16_t        *lut_table;         /**< 33-entry x 0.1 degC table for read_ddegc (codegen-injected) */
} dal_ntc_config_t;

/**
 * @brief NTC instance handle (POD).
 *
 * Read-ordering contract (DAL-C-010): read @c last_status first; if it is
 * @c WINK_ERR_HARDWARE, read @c fault_open / @c fault_short to distinguish the
 * safety fault. A latched fault persists until @c dal_ntc_clear_faults().
 */
typedef struct {
    dal_ntc_config_t      config;            /**< Config copy (MUST be at offset 0) */
    uint8_t               adc_channel;       /**< Bound PAL ADC logical channel */
    bool                  initialized;       /**< Init flag */
    uint16_t              last_raw;          /**< Last raw ADC sample */
    uint16_t              last_mv;           /**< Last converted voltage in mV */
    int16_t               last_ddegc;        /**< Last reading in 0.1 degC (254 = 25.4 degC) */
#if !defined(WINK_PROFILE_MICRO) && !defined(WINK_NO_FLOAT)
    float                 last_degc;         /**< Full-profile float cache in degC */
#endif
    volatile bool         fault_open;        /**< Latched open-circuit safety flag */
    volatile bool         fault_short;       /**< Latched short-circuit safety flag */
    uint8_t               fault_debounce;    /**< Running abnormal-sample debounce counter */
    volatile wink_status_t last_status;      /**< Last status code */
} dal_ntc_t;

/* --- ABI size / layout guards --- */
_Static_assert(offsetof(dal_ntc_t, config) == 0, "config must be at offset 0");
_Static_assert(DAL_NTC_VARIANT_COUNT == 2, "Variant count mismatch");
_Static_assert(DAL_NTC_VARIANT_DIFFERENTIAL_BRIDGE + 1 == DAL_NTC_VARIANT_COUNT,
               "Sequential variant ordering check failed");

#if INTPTR_MAX == INT32_MAX   /* ILP32: ESP32 xtensa, wasm32 */
_Static_assert(sizeof(dal_ntc_config_t) == 40, "ABI break: config size changed on 32-bit target");
_Static_assert(offsetof(dal_ntc_t, initialized) == 41, "ABI break: initialized offset changed on 32-bit");
/* Full profile (float present): handle is 60 bytes. The Micro profile
 * (WINK_PROFILE_MICRO, no float) shrinks the handle to 56 bytes; that size is
 * asserted when the 8051 toolchain CI lands. */
_Static_assert(sizeof(dal_ntc_t) == 60, "ABI break: handle size changed on 32-bit target");
#else                         /* LP64 / LLP64: 64-bit host simulation */
_Static_assert(sizeof(dal_ntc_config_t) == 48, "ABI break: config size changed on 64-bit host");
_Static_assert(offsetof(dal_ntc_t, initialized) == 49, "ABI break: initialized offset changed on 64-bit");
_Static_assert(sizeof(dal_ntc_t) == 72, "ABI break: handle size changed on 64-bit host");
#endif

/* --- C API --- */

/**
 * @brief Initialize an NTC instance: acquire the ADC channel and claim both
 *        the ADC-channel and GPIO-pin resources.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_ntc_init(dal_ntc_t *dev, const dal_ntc_config_t *cfg);

/**
 * @brief Deinitialize an NTC instance and release the ADC / GPIO resources.
 */
wink_status_t dal_ntc_deinit(dal_ntc_t *dev);

#ifndef WINK_STRICT_NONBLOCKING
/**
 * @brief Read temperature in tenths of a degree Celsius (254 = 25.4 degC).
 *
 * Pure-integer path with no soft-float (Micro-profile preferred). Requires
 * @c config.lut_table to be a 33-entry table; returns @c WINK_ERR_INVALID_STATE
 * when it is NULL. Single ADC snapshot shared by safety check and interpolation.
 */
WINK_BLOCKING WINK_WARN_UNUSED_RESULT
wink_status_t dal_ntc_read_ddegc(dal_ntc_t *dev, int16_t *out_ddegc);

#if !defined(WINK_PROFILE_MICRO) && !defined(WINK_NO_FLOAT)
/**
 * @brief Read temperature in floating-point degrees Celsius (Full profile).
 *
 * B-parameter equation; compiled out on the Micro Profile.
 */
WINK_BLOCKING WINK_WARN_UNUSED_RESULT
wink_status_t dal_ntc_read_degc(dal_ntc_t *dev, float *out_degc);
#endif

/**
 * @brief Read the raw 12-bit ADC sample (single hardware sample).
 */
WINK_BLOCKING WINK_WARN_UNUSED_RESULT
wink_status_t dal_ntc_read_raw(dal_ntc_t *dev, uint16_t *out_raw);

/**
 * @brief Read the divider midpoint voltage in mV.
 */
WINK_BLOCKING WINK_WARN_UNUSED_RESULT
wink_status_t dal_ntc_read_mv(dal_ntc_t *dev, uint16_t *out_mv);
#endif /* !WINK_STRICT_NONBLOCKING */

/**
 * @brief Clear latched open/short fault flags and reset the debounce counter.
 */
wink_status_t dal_ntc_clear_faults(dal_ntc_t *dev);

/**
 * @brief Return the last status code (WINK_ERR_INVALID_ARG for a NULL dev).
 */
static inline wink_status_t dal_ntc_get_last_status(const dal_ntc_t *dev) {
    return dev ? dev->last_status : WINK_ERR_INVALID_ARG;
}

#ifdef __cplusplus
}
#endif

/* --- Compile-time pruning stubs (DAL-HDR-STUB) --- */
#if !defined(WINK_USE_NTC) || !WINK_USE_NTC
#define WINK_NTC_DISABLED_MSG \
    "NTC driver not enabled; add a \"ntc\" device to wink-app.json " \
    "(or set -DWINK_USE_NTC=ON)."
WINK_UNAVAILABLE_MSG(WINK_NTC_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_ntc_init(dal_ntc_t *dev, const dal_ntc_config_t *cfg);
WINK_UNAVAILABLE_MSG(WINK_NTC_DISABLED_MSG)
wink_status_t dal_ntc_deinit(dal_ntc_t *dev);

#ifndef WINK_STRICT_NONBLOCKING
WINK_UNAVAILABLE_MSG(WINK_NTC_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_ntc_read_ddegc(dal_ntc_t *dev, int16_t *out_ddegc);
#if !defined(WINK_PROFILE_MICRO) && !defined(WINK_NO_FLOAT)
WINK_UNAVAILABLE_MSG(WINK_NTC_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_ntc_read_degc(dal_ntc_t *dev, float *out_degc);
#endif
WINK_UNAVAILABLE_MSG(WINK_NTC_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_ntc_read_raw(dal_ntc_t *dev, uint16_t *out_raw);
WINK_UNAVAILABLE_MSG(WINK_NTC_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_ntc_read_mv(dal_ntc_t *dev, uint16_t *out_mv);
#endif /* !WINK_STRICT_NONBLOCKING */

WINK_UNAVAILABLE_MSG(WINK_NTC_DISABLED_MSG)
wink_status_t dal_ntc_clear_faults(dal_ntc_t *dev);
#endif /* !WINK_USE_NTC */

#endif /* DAL_NTC_H */
