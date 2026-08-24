// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_target_caps.h
 * @brief PAL Target Hardware Capabilities & Capacity Limits SSOT Header (ADR-0064).
 */

#ifndef PAL_TARGET_CAPS_H
#define PAL_TARGET_CAPS_H

#include "wink_compiler.h"

#if defined(ESP_PLATFORM)
    #include "soc/soc_caps.h"
    #define PAL_PWM_CHANNEL_MAX     SOC_LEDC_CHANNEL_NUM
    #define PAL_I2C_PORT_MAX        SOC_I2C_NUM
    #define PAL_GPIO_PIN_MAX        SOC_GPIO_PIN_COUNT
    #define PAL_PWM_MAX_BITS        SOC_LEDC_TIMER_BIT_WIDTH
    #define PAL_ADC_CHANNEL_MAX     SOC_ADC_MAX_CHANNEL_NUM
#elif defined(__wasm__)
    #define PAL_PWM_CHANNEL_MAX     8
    #define PAL_I2C_PORT_MAX        2
    #define PAL_GPIO_PIN_MAX        50
    #define PAL_PWM_MAX_BITS        16
    #define PAL_ADC_CHANNEL_MAX     16
#else /* host / simulation fallback */
    #define PAL_PWM_CHANNEL_MAX     8
    #define PAL_I2C_PORT_MAX        2
    #define PAL_GPIO_PIN_MAX        50
    #define PAL_PWM_MAX_BITS        16
    #define PAL_ADC_CHANNEL_MAX     16
#endif

/* Backward compatibility aliases */
#ifndef PAL_PWM_CHANNELS
#define PAL_PWM_CHANNELS PAL_PWM_CHANNEL_MAX
#endif

#ifndef PAL_I2C_PORTS
#define PAL_I2C_PORTS PAL_I2C_PORT_MAX
#endif

#ifndef PAL_ADC_CHANNELS
#define PAL_ADC_CHANNELS PAL_ADC_CHANNEL_MAX
#endif

#endif /* PAL_TARGET_CAPS_H */
