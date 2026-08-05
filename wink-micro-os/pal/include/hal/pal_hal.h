// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_hal.h
 * @brief PAL Hardware Abstraction Layer Interface (GPIO, PWM, I2C).
 */

#ifndef PAL_HAL_H
#define PAL_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"
#include "pal_irq.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PAL_PWM_CHANNELS
#define PAL_PWM_CHANNELS 8
#endif

#ifndef PAL_I2C_PORTS
#define PAL_I2C_PORTS 2
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

/**
 * @brief Unified pin number type
 * @note Uses int16_t to ensure GPIO_NUM_NC (-1) is not truncated to 65535.
 */
typedef int16_t wink_pin_t;

/**
 * @brief Query physical GPIO mapped to specified PWM channel
 *
 * @param[in] channel PWM channel ID [0, PAL_PWM_CHANNELS)
 * @param[out] out_pin Output pointer for mapped GPIO pin
 * @return WINK_OK on success, WINK_ERR_INVALID_ARG on out of bounds, WINK_ERR_UNSUPPORTED if target lacks routing
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_pwm_channel_pin(uint8_t channel, wink_pin_t *out_pin);

/**
 * @brief Query physical SDA/SCL GPIO pins mapped to specified I2C port
 *
 * @param[in] port I2C port ID [0, PAL_I2C_PORTS)
 * @param[out] out_sda Output pointer for SDA pin
 * @param[out] out_scl Output pointer for SCL pin
 * @return WINK_OK on success, WINK_ERR_INVALID_ARG on out of bounds, WINK_ERR_UNSUPPORTED if target lacks routing
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_i2c_port_pins(uint8_t port, wink_pin_t *out_sda, wink_pin_t *out_scl);

/* --- PWM (ADR-0034 progressive disclosure) --- */

typedef enum {
    PAL_PWM_CLOCK_AUTO            = 0,
    PAL_PWM_CLOCK_STABLE_REQUIRED = 1,
} pal_pwm_clock_requirement_t;

typedef struct {
    uint32_t                    freq_hz;
    uint8_t                     resolution_bits;  /* 0 = AUTO → target default (ESP32: 13) */
    pal_pwm_clock_requirement_t clock_requirement;
} pal_pwm_config_t;

/** @brief Legacy thin wrapper (13-bit + AUTO clock). */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_pwm_init(uint8_t channel, uint32_t frequency_hz);

/** @brief Extended init: freq + resolution + clock requirement. */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_pwm_init_ex(uint8_t channel, const pal_pwm_config_t *cfg);

WINK_WARN_UNUSED_RESULT
wink_status_t pal_pwm_set_duty(uint8_t channel, float duty);

void pal_pwm_deinit(uint8_t channel);

WINK_WARN_UNUSED_RESULT
wink_status_t pal_gpio_init(wink_pin_t pin, pal_gpio_mode_t mode);

/**
 * @brief Reset a GPIO pin to its default high-impedance state and release hardware reservation
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

WINK_WARN_UNUSED_RESULT
wink_status_t pal_gpio_write(wink_pin_t pin, bool level);

WINK_WARN_UNUSED_RESULT
wink_status_t pal_gpio_read(wink_pin_t pin, bool *out_level);

typedef void (*pal_gpio_isr_t)(void *arg);

/**
 * @brief Enable GPIO pin interrupt (extended version with locked priority tier)
 *
 * @param[in] pin Logical GPIO pin ID
 * @param[in] intr_type Interrupt trigger mode
 * @param[in] prio Interrupt priority level tier (locked on first registration)
 * @param[in] callback ISR handler function pointer
 * @param[in] arg Context argument passed to ISR
 *
 * @return WINK_OK on success, WINK_ERR_INVALID_ARG on out of bounds or priority tier mismatch
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_gpio_enable_interrupt_ex(wink_pin_t pin,
                                            pal_gpio_intr_t intr_type,
                                            pal_irq_prio_t prio,
                                            pal_gpio_isr_t callback,
                                            void *arg);

/**
 * @brief Enable GPIO pin interrupt (default NORMAL priority tier)
 */
static inline wink_status_t
pal_gpio_enable_interrupt(wink_pin_t pin, pal_gpio_intr_t intr_type,
                           pal_gpio_isr_t callback, void *arg)
{
    return pal_gpio_enable_interrupt_ex(pin, intr_type,
                                         PAL_IRQ_PRIO_NORMAL,
                                         callback, arg);
}

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
 * @brief Measure GPIO pulse width in microseconds
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

/**
 * @brief Perform synchronous I2C bus transfer (write + optional read)
 * @param[in] port I2C hardware port ID
 * @param[in] dev_addr I2C 7-bit target slave address
 * @param[in] write_buf Data buffer to write
 * @param[in] write_len Byte length of write buffer
 * @param[out] read_buf Data buffer for read response
 * @param[in] read_len Byte length of read buffer
 * @return WINK_OK on success, error status code otherwise
 */
WINK_BLOCKING
WINK_WARN_UNUSED_RESULT
wink_status_t pal_i2c_transfer(uint8_t port, uint16_t dev_addr,
                               const uint8_t *write_buf, uint32_t write_len,
                               uint8_t *read_buf, uint32_t read_len);

/**
 * @brief Scan a range of I2C 7-bit addresses for responding devices
 * @param[in] port I2C hardware port ID [0, PAL_I2C_PORTS)
 * @param[in] start_addr First 7-bit address to probe
 * @param[in] end_addr Last 7-bit address to probe
 * @param[out] out_found_bitmap 16-byte buffer (128 bits) receiving scan results
 * @param[in] bitmap_bytes Must be 16
 * @return WINK_OK on completion, error status code otherwise
 */
WINK_BLOCKING
WINK_WARN_UNUSED_RESULT
wink_status_t pal_i2c_scan(uint8_t port, uint8_t start_addr, uint8_t end_addr,
                            uint8_t *out_found_bitmap, size_t bitmap_bytes);
#endif /* WINK_STRICT_NONBLOCKING */

#ifdef __cplusplus
}
#endif

#endif /* PAL_HAL_H */
