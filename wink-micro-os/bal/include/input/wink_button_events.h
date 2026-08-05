// SPDX-License-Identifier: Apache-2.0
/**
 * @file wink_button_events.h
 * @brief BAL public API - button event stream
 */
#ifndef WINK_BUTTON_EVENTS_H
#define WINK_BUTTON_EVENTS_H

#include <stdbool.h>
#include <stdint.h>
#include "wink_status.h"
#include "dal_button.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Slot-pool size for concurrent button event streams.
 */
#ifndef WINK_BUTTON_EVENTS_MAX
#define WINK_BUTTON_EVENTS_MAX 4
#endif

/**
 * @brief Event source selector
 */
typedef enum {
    WINK_BUTTON_DRIVE_SOFT_POLL = 0, /**< Periodic soft-timer poll */
    WINK_BUTTON_DRIVE_GPIO_IRQ  = 1, /**< GPIO edge IRQ */
} wink_button_event_drive_t;

/**
 * @brief Configuration for button event stream
 */
typedef struct {
    wink_button_event_drive_t drive;     /**< Event drive mode */
    uint32_t auto_poll_ms;                /**< Poll period in ms */
    uint32_t debounce_ms;                 /**< Debounce window in ms */
    bool wake_from_sleep;                 /**< Wake from sleep flag */
} wink_button_event_config_t;

/**
 * @brief Enable button event dispatch
 *
 * @param[in,out] btn Initialized button instance handle.
 * @param[in] cfg Configuration struct.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_button_enable_events(dal_button_t *btn,
                                         const wink_button_event_config_t *cfg);

/**
 * @brief Disable button event dispatch
 *
 * @param[in,out] btn Button instance handle (NULL-safe).
 */
void wink_button_disable_events(dal_button_t *btn);

/**
 * @brief Check if current target supports GPIO-IRQ backend
 *
 * @return True if GPIO-IRQ supported, false otherwise.
 */
bool wink_button_events_irq_supported(void);

WINK_WARN_UNUSED_RESULT WINK_DEPRECATED("use wink_button_enable_events (ADR-0032 B-class)")
static inline wink_status_t wink_button_events_start(dal_button_t *btn,
                                                      const wink_button_event_config_t *cfg) {
    return wink_button_enable_events(btn, cfg);
}

WINK_DEPRECATED("use wink_button_disable_events (ADR-0032 B-class)")
static inline void wink_button_events_stop(dal_button_t *btn) {
    wink_button_disable_events(btn);
}

#ifdef __cplusplus
}
#endif

#endif /* WINK_BUTTON_EVENTS_H */
