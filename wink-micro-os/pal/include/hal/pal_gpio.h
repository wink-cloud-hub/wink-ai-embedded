// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_gpio.h
 * @brief PAL GPIO Interface Subsystem with Glitch-Free, Pin Hold, and SMP Synchronization (ADR-0065).
 */

#ifndef PAL_GPIO_H
#define PAL_GPIO_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"
#include "pal_irq.h"
#include "hal/pal_pin_types.h"
#include "hal/pal_target_caps.h"
#include "wink_compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PAL_GPIO_INPUT               = 0,
    PAL_GPIO_INPUT_PULLUP        = 1,
    PAL_GPIO_INPUT_PULLDOWN      = 2,
    PAL_GPIO_OUTPUT_PUSH_PULL    = 3,
    PAL_GPIO_OUTPUT_OPEN_DRAIN   = 4,
    PAL_GPIO_INPUT_OUTPUT        = 5,
} pal_gpio_mode_t;

typedef enum {
    PAL_GPIO_INTR_DISABLE         = 0,
    PAL_GPIO_INTR_RISING_EDGE     = 1,
    PAL_GPIO_INTR_FALLING_EDGE    = 2,
    PAL_GPIO_INTR_ANY_EDGE        = 3,
    PAL_GPIO_INTR_LOW_LEVEL       = 4,
    PAL_GPIO_INTR_HIGH_LEVEL      = 5,
} pal_gpio_intr_t;

/** @brief Backward compatibility alias for GPIO ISR callback. */
typedef pal_isr_t pal_gpio_isr_t;

/* --- Lifecycle & Configuration (Task Context) --- */

/**
 * @brief Initialize a GPIO pin to specified mode
 * @note Task-only. Automatically claims PAL_RESOURCE_GPIO_PIN under RAII model (ADR-0065).
 * @param[in] pin Logical/physical GPIO pin
 * @param[in] mode GPIO operation mode
 * @return WINK_OK on success, WINK_ERR_BUSY if pin claimed, WINK_ERR_INVALID_ARG if invalid
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_gpio_init(wink_pin_t pin, pal_gpio_mode_t mode);

/**
 * @brief Atomic glitch-free output initialization (pre-sets hardware output register before configuring direction)
 * @note Task-only. Safety-critical outputs (relays/CS) should also use external pull-ups/downs.
 * @param[in] pin Logical/physical GPIO pin
 * @param[in] mode Output mode (PAL_GPIO_OUTPUT_PUSH_PULL or PAL_GPIO_OUTPUT_OPEN_DRAIN)
 * @param[in] initial_level Output level to establish atomically (true = high, false = low)
 * @return WINK_OK on success, error status code otherwise
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_gpio_init_output(wink_pin_t pin, pal_gpio_mode_t mode, bool initial_level);

/**
 * @brief Enable or disable hardware pin level retention during sleep or reset
 * @note ESP32 maps to gpio_hold_en/dis. Host/Wasm returns WINK_OK or WINK_ERR_UNSUPPORTED.
 * @param[in] pin Logical/physical GPIO pin
 * @param[in] hold_enable True to lock pin state, false to unlock
 * @return WINK_OK on success, error code otherwise
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_gpio_set_hold(wink_pin_t pin, bool hold_enable);

/**
 * @brief Comprehensive one-stop GPIO deinitialization
 * @note Task-only. Automatically disables IRQ -> waits in-flight ISR -> releases resource -> resets to high-Z.
 * @param[in] pin Logical/physical GPIO pin
 * @return WINK_OK on success, error code otherwise
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_gpio_deinit(wink_pin_t pin);

/**
 * @brief Legacy reset pin function (calls pal_gpio_deinit internally)
 * @param[in] pin Logical GPIO pin
 */
void pal_gpio_reset_pin(wink_pin_t pin);

/**
 * @brief Change GPIO direction/mode after initialization
 * @param[in] pin Logical GPIO pin
 * @param[in] mode New GPIO mode
 * @return WINK_OK on success, WINK_ERR_INVALID_ARG if pin invalid/uninitialized
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_gpio_set_direction(wink_pin_t pin, pal_gpio_mode_t mode);

/* --- Fast Read / Write (ISR-Safe) --- */

/**
 * @brief Write digital level to GPIO pin
 * @note ISR-Safe.
 * @param[in] pin Logical GPIO pin
 * @param[in] level true for High (1), false for Low (0)
 * @return WINK_OK on success, error status code otherwise
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_gpio_write(wink_pin_t pin, bool level);

/**
 * @brief Read digital level from GPIO pin
 * @note ISR-Safe.
 * @param[in] pin Logical GPIO pin
 * @param[out] out_level Output pointer to receive current level
 * @return WINK_OK on success, error status code otherwise
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_gpio_read(wink_pin_t pin, bool *out_level);

/* --- Interrupt Management & SMP Safety --- */

/**
 * @brief Enable GPIO pin interrupt (extended version with locked priority tier)
 * @param[in] pin Logical GPIO pin ID
 * @param[in] intr_type Interrupt trigger mode
 * @param[in] prio Interrupt priority level tier (locked on first registration)
 * @param[in] callback ISR handler function pointer
 * @param[in] arg Context argument passed to ISR
 * @return WINK_OK on success, WINK_ERR_INVALID_ARG on out of bounds or priority tier mismatch
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_gpio_enable_interrupt_ex(wink_pin_t pin,
                                            pal_gpio_intr_t intr_type,
                                            pal_irq_prio_t prio,
                                            pal_isr_t callback,
                                            void *arg);

/**
 * @brief Enable GPIO pin interrupt (default NORMAL priority tier)
 */
static inline wink_status_t
pal_gpio_enable_interrupt(wink_pin_t pin, pal_gpio_intr_t intr_type,
                           pal_isr_t callback, void *arg)
{
    return pal_gpio_enable_interrupt_ex(pin, intr_type,
                                         PAL_IRQ_PRIO_NORMAL,
                                         callback, arg);
}

/**
 * @brief Disable GPIO pin interrupt
 * @param[in] pin Logical GPIO pin ID
 * @return WINK_OK on success, error code otherwise
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_gpio_disable_interrupt(wink_pin_t pin);

/**
 * @brief Wait for in-flight ISR execution on specified GPIO pin to finish (SMP safe synchronization primitive)
 * @param[in] pin Logical GPIO pin ID
 * @return WINK_OK on success, WINK_ERR_INVALID_ARG if pin invalid
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_gpio_synchronize_interrupt(wink_pin_t pin);

#ifndef WINK_STRICT_NONBLOCKING
/**
 * @brief Measure GPIO pulse width in microseconds (legacy software bit-bang)
 * @deprecated Prefer hardware acceleration engines (RMT / Timer capture) in Stage 0/Stage 1.
 * @param[in] pin Logical GPIO pin ID
 * @param[in] level Target pulse level state to measure
 * @param[in] timeout_us Timeout in microseconds
 * @param[out] pulse_us Output pointer for pulse width in microseconds
 * @return WINK_OK on success, error status code otherwise
 */
WINK_BLOCKING
WINK_WARN_UNUSED_RESULT
wink_status_t pal_gpio_pulse_in(wink_pin_t pin, bool level, uint32_t timeout_us,
                                 uint32_t *pulse_us);
#endif /* WINK_STRICT_NONBLOCKING */

#ifdef __cplusplus
}
#endif

#endif /* PAL_GPIO_H */
