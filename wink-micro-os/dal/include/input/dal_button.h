// SPDX-License-Identifier: Apache-2.0
#ifndef DAL_BUTTON_H
#define DAL_BUTTON_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Debounce sampling threshold: stable state flips after consecutive consistent samples */
#define DAL_BUTTON_DEBOUNCE_THRESHOLD 3

/** @brief Default long press threshold in milliseconds */
#define DAL_BUTTON_DEFAULT_LONG_PRESS_MS 3000u

/**
 * @brief Button event types (dispatched in task context via dal_button_poll)
 */
typedef enum {
    DAL_BUTTON_EVT_PRESS       = 0,  /**< Stable press event (release -> press edge) */
    DAL_BUTTON_EVT_RELEASE     = 1,  /**< Stable release event (press -> release edge) */
    DAL_BUTTON_EVT_LONG_PRESS  = 2,  /**< Long press event (triggered once when held >= long_press_ms) */
} dal_button_event_t;

/**
 * @brief Button event callback signature
 * @param[in] evt Event type
 * @param[in] ctx Opaque context pointer passed during registration
 */
typedef void (*dal_button_event_cb)(dal_button_event_t evt, void *ctx);

typedef uint8_t dal_button_pull_t;

enum {
    DAL_BUTTON_PULL_AUTO = 0, /**< Auto pull-up/down based on active_low */
    DAL_BUTTON_PULL_UP   = 1,
    DAL_BUTTON_PULL_DOWN = 2,
    DAL_BUTTON_PULL_NONE = 3,
};

/**
 * @brief Button configuration struct
 */
typedef struct {
    const char *owner;       /**< Instance owner static string */
    uint16_t pin;            /**< Logical GPIO pin */
    bool active_low;         /**< true: active low (press = 0); false: active high */
    dal_button_pull_t pull;  /**< Pull configuration */
} dal_button_config_t;

/**
 * @brief Button handle (POD).
 */
typedef struct {
    dal_button_config_t config; /**< Config copy */
    bool stable_pressed;     /**< Debounced stable pressed state */
    bool last_reported;      /* Edge debounced reported state */
    bool initialized;        /**< Set to true after successful init */
    uint8_t debounce_counter;/**< Debounce consecutive sample counter */
    uint8_t debounce_threshold; /**< Required consecutive sample threshold */

    dal_button_event_cb event_cb;     /**< Event callback pointer */
    void *event_cb_ctx;               /**< Callback context pointer */
    bool long_press_fired;            /**< Flag indicating if long press event already fired for current press */
    bool prev_pressed_for_event;      /**< Previous poll stable state */
    uint32_t long_press_ms;           /**< Long press threshold in ms */
    uint64_t press_start_ms;          /**< Start timestamp of current press cycle */

    volatile wink_status_t last_status; /**< Last poll status code for observability */

    bool isr_counter_enabled;          /**< Set to true if ISR edge counter enabled */
    volatile uint32_t edge_count;      /**< Total accumulated ISR edge triggers */

    uint8_t  event_backend;            /**< DAL_BUTTON_BACKEND_{NONE,POLL,IRQ} */
    bool     gpio_isr_registered;      /**< True if shared ISR thunk is installed */
    volatile bool irq_pending;         /**< Set by ISR, cleared by consume_irq_pending */
} dal_button_t;

_Static_assert(offsetof(dal_button_t, config) == 0, "config must be the first member");

#if INTPTR_MAX == INT32_MAX   /* ILP32: ESP32 xtensa, wasm32 */
_Static_assert(sizeof(dal_button_config_t) == 8,  "ABI break: config size changed on 32-bit target");
_Static_assert(offsetof(dal_button_t, initialized) == 10, "ABI break: initialized offset changed on 32-bit");
_Static_assert(sizeof(dal_button_t) == 56, "ABI break: handle size changed on 32-bit target");
#else                         /* LP64 / LLP64: 64-bit Host Simulation */
_Static_assert(sizeof(dal_button_config_t) == 16, "ABI break: config size changed on 64-bit host");
_Static_assert(offsetof(dal_button_t, initialized) == 18, "ABI break: initialized offset changed on 64-bit host");
_Static_assert(sizeof(dal_button_t) == 72, "ABI break: handle size changed on 64-bit host");
#endif

typedef enum {
    DAL_BUTTON_BACKEND_NONE = 0, /**< No BAL event tracking on this button */
    DAL_BUTTON_BACKEND_POLL = 1, /**< BAL soft-poll from periodic tick */
    DAL_BUTTON_BACKEND_IRQ  = 2, /**< BAL GPIO IRQ + debounce timer */
} dal_button_backend_t;

typedef void (*dal_button_irq_notify_hook_t)(void *ctx);

/**
 * @brief Initialize button driver instance
 *
 * @param[in,out] dev Button instance handle.
 * @param[in] cfg Configuration struct.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_init(dal_button_t *dev, const dal_button_config_t *cfg);

/**
 * @brief Sample GPIO pin and advance debounce state machine (non-blocking)
 *
 * @param[in,out] dev Button instance handle.
 * @return WINK_OK on success, error status code otherwise.
 */
wink_status_t dal_button_poll(dal_button_t *dev);

/**
 * @brief Read debounced stable pressed state
 *
 * @param[in] dev Button instance handle.
 * @param[out] out_pressed Output pointer (true = pressed, false = released).
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_is_pressed(const dal_button_t *dev, bool *out_pressed);

/**
 * @brief Read last poll status result code (for error observability)
 *
 * @param[in] dev Button instance handle.
 * @param[out] out_status Output pointer for last status code.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_get_status(const dal_button_t *dev, wink_status_t *out_status);

/**
 * @brief Read and clear button press edge event (read-once latch)
 *
 * @param[in,out] dev Button instance handle.
 * @param[out] out_was_pressed Output pointer (true if press event occurred).
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_was_pressed(dal_button_t *dev, bool *out_was_pressed);

/**
 * @brief Register button event callback (PRESS / RELEASE / LONG_PRESS)
 *
 * @param[in,out] dev Button instance handle.
 * @param[in] cb Callback function pointer (pass NULL to unregister).
 * @param[in] ctx Context pointer passed to callback.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_on_event(dal_button_t *dev, dal_button_event_cb cb, void *ctx);

/**
 * @brief Configure long press threshold duration
 *
 * @param[in,out] dev Button instance handle.
 * @param[in] ms Long press duration threshold in milliseconds.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_set_long_press_ms(dal_button_t *dev, uint32_t ms);

/**
 * @brief Configure debounce window duration in milliseconds
 *
 * @param[in,out] dev Button instance handle.
 * @param[in] ms Debounce duration window in milliseconds.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_set_debounce_ms(dal_button_t *dev, uint32_t ms);

/**
 * @brief Enable hardware GPIO edge interrupt counter
 *
 * @param[in,out] dev Button instance handle.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_enable_isr_counter(dal_button_t *dev);

/**
 * @brief Read accumulated ISR edge trigger count
 *
 * @param[in] dev Button instance handle.
 * @param[out] out_count Output pointer for edge trigger count.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_get_edge_count(const dal_button_t *dev, uint32_t *out_count);

/**
 * @brief Atomically reset ISR edge count to zero
 *
 * @param[in,out] dev Button instance handle.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_reset_edge_count(dal_button_t *dev);

/**
 * @brief Deinitialize button driver
 *
 * @param[in,out] dev Button instance handle.
 * @return WINK_OK on success, error status code otherwise.
 */
wink_status_t dal_button_deinit(dal_button_t *dev);

#ifdef __cplusplus
}
#endif

/* Compile-time pruning stubs */
#if !defined(WINK_USE_BUTTON) || !WINK_USE_BUTTON
#define WINK_BUTTON_DISABLED_MSG \
    "Button driver not enabled; add a \"button\" device to wink-app.json " \
    "(or set -DWINK_USE_BUTTON=ON)."
WINK_UNAVAILABLE_MSG(WINK_BUTTON_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_init(dal_button_t *dev, const dal_button_config_t *cfg);
WINK_UNAVAILABLE_MSG(WINK_BUTTON_DISABLED_MSG)
wink_status_t dal_button_poll(dal_button_t *dev);
WINK_UNAVAILABLE_MSG(WINK_BUTTON_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_is_pressed(const dal_button_t *dev, bool *out_pressed);
WINK_UNAVAILABLE_MSG(WINK_BUTTON_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_get_status(const dal_button_t *dev, wink_status_t *out_status);
WINK_UNAVAILABLE_MSG(WINK_BUTTON_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_was_pressed(dal_button_t *dev, bool *out_was_pressed);
WINK_UNAVAILABLE_MSG(WINK_BUTTON_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_on_event(dal_button_t *dev, dal_button_event_cb cb, void *ctx);
WINK_UNAVAILABLE_MSG(WINK_BUTTON_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_set_long_press_ms(dal_button_t *dev, uint32_t ms);
WINK_UNAVAILABLE_MSG(WINK_BUTTON_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_set_debounce_ms(dal_button_t *dev, uint32_t ms);
WINK_UNAVAILABLE_MSG(WINK_BUTTON_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_enable_isr_counter(dal_button_t *dev);
WINK_UNAVAILABLE_MSG(WINK_BUTTON_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_get_edge_count(const dal_button_t *dev, uint32_t *out_count);
WINK_UNAVAILABLE_MSG(WINK_BUTTON_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_reset_edge_count(dal_button_t *dev);
WINK_UNAVAILABLE_MSG(WINK_BUTTON_DISABLED_MSG)
wink_status_t dal_button_deinit(dal_button_t *dev);
#endif /* !WINK_USE_BUTTON */

#endif /* DAL_BUTTON_H */
