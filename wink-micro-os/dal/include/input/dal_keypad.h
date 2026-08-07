// SPDX-License-Identifier: Apache-2.0
#ifndef DAL_KEYPAD_H
#define DAL_KEYPAD_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DAL_KEYPAD_MAX_ROWS 4
#define DAL_KEYPAD_MAX_COLS 4

/**
 * @brief Matrix Keypad Topology Variant Enum (DAL-S-001)
 */
typedef enum {
    DAL_KEYPAD_VARIANT_MATRIX_4X4 = 0, /**< 4x4 Matrix Keypad (default) */
    DAL_KEYPAD_VARIANT_MATRIX_3X4 = 1, /**< 3x4 Telephone Keypad */
    DAL_KEYPAD_VARIANT_CUSTOM     = 2, /**< Custom N*M Matrix with custom keymap string */
} dal_keypad_variant_t;

/**
 * @brief Matrix Keypad Configuration Struct (POD config)
 * Members ordered descending by alignment for padding optimization.
 * First member MUST be owner (DAL-S-001).
 */
typedef struct {
    const char *owner;                         /**< Instance owner static string (DAL-S-001) */
    const char *custom_keymap;                 /**< Custom ASCII keymap string (row-major N*M chars, or NULL) */
    uint16_t debounce_ms;                      /**< Debounce interval in ms (default 10ms) */
    int16_t row_pins[DAL_KEYPAD_MAX_ROWS];    /**< Row GPIO pins */
    int16_t col_pins[DAL_KEYPAD_MAX_COLS];    /**< Column GPIO pins (-1 for unused) */
    dal_keypad_variant_t variant;              /**< Topology variant enum */
    uint8_t num_rows;                          /**< Actual number of rows (1..4) */
    uint8_t num_cols;                          /**< Actual number of columns (1..4) */
    bool active_low;                           /**< Scan polarity: true = active LOW with pull-up */
} dal_keypad_config_t;

/**
 * @brief Matrix Keypad Handle Struct (POD instance)
 * config is embedded as the first member (offsetof == 0, DAL-S-011).
 */
typedef struct {
    dal_keypad_config_t config;          /**< Embedded config copy (offsetof == 0, DAL-S-011) */
    uint32_t last_scan_ms;               /**< Last scan timestamp */
    char last_key;                       /**< Current pressed key char ('\0' if none) */
    uint8_t last_row;                    /**< Current pressed row index (0xFF if none) */
    uint8_t last_col;                    /**< Current pressed column index (0xFF if none) */
    bool is_pressed;                     /**< Pressed status flag */
    bool initialized;                    /**< Init status flag (DAL-L-004) */
    volatile wink_status_t last_status;  /**< Observability last status */
} dal_keypad_t;

/* Static assertions for ABI freeze and first-member guard (DAL-S-011 / DAL-S-014) */
_Static_assert(offsetof(dal_keypad_t, config) == 0, "config must be the first member");

#if INTPTR_MAX == INT32_MAX   /* ILP32: ESP32 xtensa, wasm32 */
_Static_assert(sizeof(dal_keypad_config_t) == 36, "ABI break: config size changed on 32-bit target");
_Static_assert(offsetof(dal_keypad_t, initialized) == 44, "ABI break: initialized offset changed on 32-bit");
_Static_assert(sizeof(dal_keypad_t) == 52, "ABI break: handle size changed on 32-bit target");
#else                         /* LP64 / LLP64: 64-bit Host Simulation */
_Static_assert(sizeof(dal_keypad_config_t) == 48, "ABI break: config size changed on 64-bit host");
_Static_assert(offsetof(dal_keypad_t, initialized) == 56, "ABI break: initialized offset changed on 64-bit host");
_Static_assert(sizeof(dal_keypad_t) == 64, "ABI break: handle size changed on 64-bit host");
#endif

WINK_WARN_UNUSED_RESULT
wink_status_t dal_keypad_init(dal_keypad_t *dev, const dal_keypad_config_t *cfg);

wink_status_t dal_keypad_deinit(dal_keypad_t *dev);

WINK_WARN_UNUSED_RESULT
wink_status_t dal_keypad_get_key(dal_keypad_t *dev, char *out_key);

WINK_WARN_UNUSED_RESULT
wink_status_t dal_keypad_is_pressed(const dal_keypad_t *dev, bool *out_pressed);

WINK_WARN_UNUSED_RESULT
wink_status_t dal_keypad_poll(dal_keypad_t *dev, bool *out_changed, char *out_key);

WINK_WARN_UNUSED_RESULT
wink_status_t dal_keypad_get_status(const dal_keypad_t *dev, wink_status_t *out_status);

#ifdef __cplusplus
}
#endif

/* Compile-time pruning stubs */
#if !defined(WINK_USE_KEYPAD) || !WINK_USE_KEYPAD
#define WINK_KEYPAD_DISABLED_MSG \
    "Keypad driver not enabled; add a \"keypad\" device to wink-app.json " \
    "(or set -DWINK_USE_KEYPAD=ON)."
WINK_UNAVAILABLE_MSG(WINK_KEYPAD_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_keypad_init(dal_keypad_t *dev, const dal_keypad_config_t *cfg);
WINK_UNAVAILABLE_MSG(WINK_KEYPAD_DISABLED_MSG)
wink_status_t dal_keypad_deinit(dal_keypad_t *dev);
WINK_UNAVAILABLE_MSG(WINK_KEYPAD_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_keypad_get_key(dal_keypad_t *dev, char *out_key);
WINK_UNAVAILABLE_MSG(WINK_KEYPAD_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_keypad_is_pressed(const dal_keypad_t *dev, bool *out_pressed);
WINK_UNAVAILABLE_MSG(WINK_KEYPAD_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_keypad_poll(dal_keypad_t *dev, bool *out_changed, char *out_key);
WINK_UNAVAILABLE_MSG(WINK_KEYPAD_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_keypad_get_status(const dal_keypad_t *dev, wink_status_t *out_status);
#endif /* !WINK_USE_KEYPAD */

#endif /* DAL_KEYPAD_H */
