// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_adc.h
 * @brief PAL Analog-to-Digital Converter (ADC) Interface Subsystem.
 */

#ifndef PAL_ADC_H
#define PAL_ADC_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"
#include "hal/pal_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PAL_ADC_CHANNELS
#define PAL_ADC_CHANNELS 16
#endif

typedef uint8_t pal_adc_channel_t;

/**
 * @brief PAL ADC Channel Configuration Struct
 */
typedef struct {
    wink_pin_t   pin;              /**< Physical GPIO pin number (supports NC = -1) */
    uint16_t     full_scale_mv;    /**< Full scale millivolts (0 = target default: ESP32 ≈ 3100mV, Wasm/Host = 3300mV) */
    uint8_t      resolution_bits;  /**< Resolution in bits (0 = target default 12-bit) */
} pal_adc_config_t;

/**
 * @brief Initialize PAL ADC channel
 * @param[in] ch Logical ADC channel number [0, PAL_ADC_CHANNELS)
 * @param[in] cfg Pointer to configuration struct
 * @return WINK_OK on success, error status code otherwise
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_adc_init(pal_adc_channel_t ch, const pal_adc_config_t *cfg);

/**
 * @brief Deinitialize PAL ADC channel
 * @param[in] ch Logical ADC channel number
 */
void pal_adc_deinit(pal_adc_channel_t ch);

/**
 * @brief Query physical pin mapped to logical ADC channel
 * @param[in] ch Logical ADC channel number
 * @param[out] out_pin Output pointer for GPIO pin
 * @return WINK_OK on success, error status code otherwise
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_adc_channel_pin(pal_adc_channel_t ch, wink_pin_t *out_pin);

/**
 * @brief Query logical ADC channel mapped to physical pin
 * @param[in] pin GPIO pin number
 * @param[out] out_ch Output pointer for logical ADC channel
 * @return WINK_OK on success, error status code otherwise
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_adc_pin_channel(wink_pin_t pin, pal_adc_channel_t *out_ch);

/**
 * @brief Query full scale millivolt rating for ADC channel
 * @param[in] ch Logical ADC channel number
 * @param[out] out_mv Output pointer for full scale mV
 * @return WINK_OK on success, error status code otherwise
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_adc_full_scale_mv(pal_adc_channel_t ch, uint16_t *out_mv);

#ifndef WINK_STRICT_NONBLOCKING
/**
 * @brief Read raw sample value from specified logical ADC channel (oneshot sample)
 * @param[in] ch Logical ADC channel number [0, PAL_ADC_CHANNELS)
 * @param[out] out_raw Output pointer for raw ADC sample value
 * @return WINK_OK on success, WINK_ERR_INVALID_ARG, WINK_ERR_NOT_INITIALIZED, WINK_ERR_BUSY, or WINK_ERR_TIMEOUT
 */
WINK_BLOCKING
WINK_WARN_UNUSED_RESULT
wink_status_t pal_adc_read_raw(pal_adc_channel_t ch, uint16_t *out_raw);

/**
 * @brief Read millivolt (mV) sample value from specified logical ADC channel
 * @param[in] ch Logical ADC channel number [0, PAL_ADC_CHANNELS)
 * @param[out] out_mv Output pointer for calculated millivolts
 * @return WINK_OK on success, WINK_ERR_INVALID_ARG, WINK_ERR_NOT_INITIALIZED, WINK_ERR_BUSY, or WINK_ERR_TIMEOUT
 */
WINK_BLOCKING
WINK_WARN_UNUSED_RESULT
wink_status_t pal_adc_read_mv(pal_adc_channel_t ch, uint16_t *out_mv);
#endif /* WINK_STRICT_NONBLOCKING */

#ifdef __cplusplus
}
#endif

#endif /* PAL_ADC_H */
