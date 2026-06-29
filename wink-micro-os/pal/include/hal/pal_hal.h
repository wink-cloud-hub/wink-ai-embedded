#ifndef PAL_HAL_H
#define PAL_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"

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
} pal_gpio_mode_t;

typedef enum {
    PAL_GPIO_INTR_DISABLE         = 0,
    PAL_GPIO_INTR_RISING_EDGE     = 1,
    PAL_GPIO_INTR_FALLING_EDGE    = 2,
    PAL_GPIO_INTR_ANY_EDGE        = 3,
} pal_gpio_intr_t;

extern const uint16_t pal_pwm_pin_map[PAL_PWM_CHANNELS];

/* I2C 物理引脚路由：[port][0] = SDA, [port][1] = SCL */
extern const uint16_t pal_i2c_pin_map[PAL_I2C_PORTS][2];

WINK_WARN_UNUSED_RESULT
wink_status_t pal_pwm_init(uint8_t channel, uint32_t frequency_hz);

WINK_WARN_UNUSED_RESULT
wink_status_t pal_pwm_set_duty(uint8_t channel, float duty);

void pal_pwm_deinit(uint8_t channel);

WINK_WARN_UNUSED_RESULT
wink_status_t pal_gpio_init(uint16_t pin, pal_gpio_mode_t mode);

void pal_gpio_write(uint16_t pin, bool level);

bool pal_gpio_read(uint16_t pin);

typedef void (*pal_gpio_isr_t)(void *arg);

WINK_WARN_UNUSED_RESULT
wink_status_t pal_gpio_enable_interrupt(uint16_t pin, pal_gpio_intr_t intr_type,
                                         pal_gpio_isr_t callback, void *arg);

WINK_WARN_UNUSED_RESULT
wink_status_t pal_gpio_disable_interrupt(uint16_t pin);

WINK_WARN_UNUSED_RESULT
wink_status_t pal_gpio_pulse_in(uint16_t pin, bool level, uint32_t timeout_us,
                                 uint32_t *pulse_us);

WINK_WARN_UNUSED_RESULT
wink_status_t pal_i2c_transfer(uint8_t port, uint16_t dev_addr,
                               const uint8_t *write_buf, uint32_t write_len,
                               uint8_t *read_buf, uint32_t read_len);

#ifdef __cplusplus
}
#endif

#endif
