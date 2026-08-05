// SPDX-License-Identifier: Apache-2.0
#ifndef DAL_ENCODER_H
#define DAL_ENCODER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_status.h"
#include "pal_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Quadrature decode mode (Phase 1: x1 only).
 */
typedef enum {
    DAL_ENCODER_VARIANT_X1_RISING = 0, /**< Default: A rising samples B */
    DAL_ENCODER_VARIANT_X2 = 1,        /**< Reserved */
    DAL_ENCODER_VARIANT_X4 = 2,        /**< Reserved */
} dal_encoder_variant_t;

/**
 * @brief Encoder pin pull mode
 */
typedef enum {
    DAL_ENCODER_PULL_UP   = 0,  /**< Internal pull-up (mechanical encoders) */
    DAL_ENCODER_PULL_DOWN = 1,  /**< Internal pull-down */
    DAL_ENCODER_PULL_NONE = 2,  /**< Floating input (external push-pull driver) */
} dal_encoder_pull_t;

/**
 * @brief Encoder configuration struct
 */
typedef struct {
    const char *owner;              /**< Instance owner static string */
    wink_pin_t pin_a;               /**< Encoder phase A pin (required, >= 0) */
    wink_pin_t pin_b;               /**< Encoder phase B pin (optional, -1 if unused) */
    dal_encoder_pull_t pull;        /**< Input pull mode */
    dal_encoder_variant_t variant;  /**< Quadrature decode variant */
    bool invert;                    /**< Direction swap flag */
} dal_encoder_config_t;

/**
 * @brief Encoder handle struct (POD)
 */
typedef struct {
    dal_encoder_config_t config; /**< Config copy */
    volatile int32_t count;      /**< Current pulse count */
    bool initialized;            /**< Set to true after successful init */
    bool isr_registered;         /**< Set to true if ISR is registered */
} dal_encoder_t;

_Static_assert(offsetof(dal_encoder_t, config) == 0, "config must be the first member");

#if INTPTR_MAX == INT32_MAX   /* ILP32: ESP32 xtensa, wasm32 */
_Static_assert(sizeof(dal_encoder_config_t) == 20, "ABI break: config size changed on 32-bit target");
_Static_assert(offsetof(dal_encoder_t, initialized) == 24, "ABI break: initialized offset changed on 32-bit");
_Static_assert(sizeof(dal_encoder_t) == 28, "ABI break: handle size changed on 32-bit target");
#else                         /* LP64 / LLP64: 64-bit Host Simulation */
_Static_assert(sizeof(dal_encoder_config_t) == 24, "ABI break: config size changed on 64-bit host");
_Static_assert(offsetof(dal_encoder_t, initialized) == 28, "ABI break: initialized offset changed on 64-bit host");
_Static_assert(sizeof(dal_encoder_t) == 32, "ABI break: handle size changed on 64-bit host");
#endif

/**
 * @brief Initialize encoder driver instance
 *
 * @param[in,out] dev Encoder instance handle.
 * @param[in] cfg Configuration struct.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_encoder_init(dal_encoder_t *dev, const dal_encoder_config_t *cfg);

/**
 * @brief Get current pulse count
 *
 * @param[in] dev Encoder instance handle.
 * @param[out] out_count Pointer to store pulse count.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_encoder_get_count(const dal_encoder_t *dev, int32_t *out_count);

/**
 * @brief Reset pulse count to zero
 *
 * @param[in,out] dev Encoder instance handle.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_encoder_reset(dal_encoder_t *dev);

/**
 * @brief Deinitialize encoder driver
 *
 * @param[in,out] dev Encoder instance handle.
 * @return WINK_OK on success, error status code otherwise.
 */
wink_status_t dal_encoder_deinit(dal_encoder_t *dev);

#ifdef __cplusplus
}
#endif

/* Compile-time pruning stubs */
#if !defined(WINK_USE_ENCODER) || !WINK_USE_ENCODER
#define WINK_ENCODER_DISABLED_MSG \
    "Encoder driver not enabled; add an \"encoder\" device to wink-app.json " \
    "(or set -DWINK_USE_ENCODER=ON)."
WINK_UNAVAILABLE_MSG(WINK_ENCODER_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_encoder_init(dal_encoder_t *dev, const dal_encoder_config_t *cfg);
WINK_UNAVAILABLE_MSG(WINK_ENCODER_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_encoder_get_count(const dal_encoder_t *dev, int32_t *out_count);
WINK_UNAVAILABLE_MSG(WINK_ENCODER_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_encoder_reset(dal_encoder_t *dev);
WINK_UNAVAILABLE_MSG(WINK_ENCODER_DISABLED_MSG)
wink_status_t dal_encoder_deinit(dal_encoder_t *dev);
#endif /* !WINK_USE_ENCODER */

#endif /* DAL_ENCODER_H */
