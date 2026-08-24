// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_hal.h
 * @brief PAL HAL Umbrella Header (Aggregates GPIO, PWM, and I2C interfaces).
 *
 * @deprecated 新代码严禁直接包含此头文件，请包含专属头文件：
 *             - <hal/pal_gpio.h>
 *             - <hal/pal_pwm.h>
 *             - <hal/pal_i2c.h>
 *             此聚合头将在 v3.0 版本彻底废除。
 */

#ifndef PAL_HAL_H
#define PAL_HAL_H

#include "hal/pal_target_caps.h"
#include "hal/pal_pin_types.h"
#include "hal/pal_gpio.h"
#include "hal/pal_pwm.h"
#include "hal/pal_i2c.h"

#endif /* PAL_HAL_H */
