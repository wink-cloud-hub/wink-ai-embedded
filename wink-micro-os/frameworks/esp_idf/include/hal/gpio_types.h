/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef HAL_GPIO_TYPES_H_
#define HAL_GPIO_TYPES_H_

#include <stdint.h>
#include <stdbool.h>
#include "soc/gpio_num.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GPIO_PORT_0 = 0,
    GPIO_PORT_MAX,
} gpio_port_t;

typedef enum {
    GPIO_MODE_DISABLE = 0,
    GPIO_MODE_INPUT = (1 << 0),
    GPIO_MODE_OUTPUT = (1 << 1),
    GPIO_MODE_OUTPUT_OD = ((1 << 1) | (1 << 2)),
    GPIO_MODE_INPUT_OUTPUT_OD = ((1 << 0) | (1 << 1) | (1 << 2)),
    GPIO_MODE_INPUT_OUTPUT = ((1 << 0) | (1 << 1)),
} gpio_mode_t;

typedef enum {
    GPIO_PULLUP_DISABLE = 0x0,
    GPIO_PULLUP_ENABLE = 0x1,
} gpio_pullup_t;

typedef enum {
    GPIO_PULLDOWN_DISABLE = 0x0,
    GPIO_PULLDOWN_ENABLE = 0x1,
} gpio_pulldown_t;

typedef enum {
    GPIO_PULLUP_ONLY,
    GPIO_PULLDOWN_ONLY,
    GPIO_PULLUP_PULLDOWN,
    GPIO_FLOATING,
} gpio_pull_mode_t;

typedef enum {
    GPIO_INTR_DISABLE = 0,
    GPIO_INTR_POSEDGE = 1,
    GPIO_INTR_NEGEDGE = 2,
    GPIO_INTR_ANYEDGE = 3,
    GPIO_INTR_LOW_LEVEL = 4,
    GPIO_INTR_HIGH_LEVEL = 5,
    GPIO_INTR_MAX
} gpio_int_type_t;

typedef enum {
    GPIO_DRIVE_CAP_0 = 0,
    GPIO_DRIVE_CAP_1 = 1,
    GPIO_DRIVE_CAP_2 = 2,
    GPIO_DRIVE_CAP_DEFAULT = 2,
    GPIO_DRIVE_CAP_3 = 3,
    GPIO_DRIVE_CAP_MAX
} gpio_drive_cap_t;

typedef struct {
    uint64_t pin_bit_mask;          /*!< GPIO pin: set with bit mask, each bit maps to a GPIO */
    gpio_mode_t mode;               /*!< GPIO mode: set input/output mode */
    gpio_pullup_t pull_up_en;       /*!< GPIO pull-up */
    gpio_pulldown_t pull_down_en;   /*!< GPIO pull-down */
    gpio_int_type_t intr_type;      /*!< GPIO interrupt type */
} gpio_config_t;

typedef void (*gpio_isr_t)(void *arg);

#ifdef __cplusplus
}
#endif

#endif /* HAL_GPIO_TYPES_H_ */
