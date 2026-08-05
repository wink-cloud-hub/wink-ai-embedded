// SPDX-License-Identifier: Apache-2.0
#ifndef DAL_BUTTON_BAL_H
#define DAL_BUTTON_BAL_H

/**
 * @file dal_button_bal.h
 * @brief BAL-internal button APIs - NOT part of the public DAL frozen surface.
 *
 * These APIs are used exclusively by the BAL layer (IRQ daemon, event backend management).
 */

#include "dal_button.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Set BAL event backend type
 * @param[in,out] dev Button instance pointer
 * @param[in] backend Event backend type (DAL_BUTTON_BACKEND_*)
 */
void dal_button_set_event_backend(dal_button_t *dev, uint8_t backend);

/**
 * @brief Enable shared GPIO ISR thunk
 * @param[in,out] dev Button instance pointer
 * @return WINK_OK on success, error status code otherwise
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_enable_gpio_isr(dal_button_t *dev);

/**
 * @brief Disable shared GPIO ISR thunk
 * @param[in,out] dev Button instance pointer
 */
void dal_button_disable_gpio_isr(dal_button_t *dev);

/**
 * @brief Read and clear irq_pending flag
 * @param[in,out] dev Button instance pointer
 * @param[out] out_was_pending Output pointer (true if pending)
 * @return WINK_OK on success, error status code otherwise
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_consume_irq_pending(dal_button_t *dev,
                                             bool *out_was_pending);

/**
 * @brief Register process-level IRQ notify hook
 * @param[in] fn Hook function pointer
 * @param[in] ctx Context pointer
 */
void dal_button_set_irq_hook(dal_button_irq_notify_hook_t fn, void *ctx);

#ifdef __cplusplus
}
#endif

/* Compile-time pruning stubs for BAL-internal APIs */
#if !defined(WINK_USE_BUTTON) || !WINK_USE_BUTTON
#ifndef WINK_BUTTON_DISABLED_MSG
#define WINK_BUTTON_DISABLED_MSG \
    "Button driver not enabled; add a \"button\" device to wink-app.json " \
    "(or set -DWINK_USE_BUTTON=ON)."
#endif
WINK_UNAVAILABLE_MSG(WINK_BUTTON_DISABLED_MSG)
void dal_button_set_event_backend(dal_button_t *dev, uint8_t backend);
WINK_UNAVAILABLE_MSG(WINK_BUTTON_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_enable_gpio_isr(dal_button_t *dev);
WINK_UNAVAILABLE_MSG(WINK_BUTTON_DISABLED_MSG)
void dal_button_disable_gpio_isr(dal_button_t *dev);
WINK_UNAVAILABLE_MSG(WINK_BUTTON_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_button_consume_irq_pending(dal_button_t *dev,
                                             bool *out_was_pending);
WINK_UNAVAILABLE_MSG(WINK_BUTTON_DISABLED_MSG)
void dal_button_set_irq_hook(dal_button_irq_notify_hook_t fn, void *ctx);
#endif /* !WINK_USE_BUTTON */

#endif /* DAL_BUTTON_BAL_H */
