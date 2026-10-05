// SPDX-License-Identifier: LGPL-3.0-only
/**
 * @file pal_dac.h
 * @brief PAL Digital-to-Analog Converter (DAC) Interface Subsystem (ADR-0092 Tier 1).
 */

#ifndef PAL_DAC_H
#define PAL_DAC_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"
#include "hal/pal_pin_types.h"
#include "hal/pal_target_caps.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t pal_dac_channel_t;

/**
 * @brief PAL DAC Channel Configuration Struct
 */
typedef struct {
    wink_pin_t   pin;              /**< Physical GPIO pin number (supports -1 for target default) */
    uint16_t     full_scale_mv;    /**< Full scale millivolts (0 = target default 3300mV) */
    uint8_t      resolution_bits;  /**< Resolution in bits (0 = target default 8-bit) */
} pal_dac_config_t;

/**
 * @brief PAL DAC Cosine Wave Configuration Struct
 */
typedef struct {
    uint32_t freq_hz; /**< CW frequency in Hz */
    uint8_t  atten;   /**< Attenuation level (0: none, 1: 1/2, 2: 1/4, 3: 1/8) */
    int8_t   offset;  /**< DC offset value */
    uint8_t  phase;   /**< Initial phase (0 = 0 deg, 1 = 180 deg) */
} pal_dac_cw_config_t;

/**
 * @brief Initialize PAL DAC channel
 * @param[in] ch Logical DAC channel number [0, PAL_DAC_CHANNELS)
 * @param[in] cfg Pointer to configuration struct
 * @return WINK_OK on success, error status code otherwise
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_dac_init(pal_dac_channel_t ch, const pal_dac_config_t *cfg);

/**
 * @brief Deinitialize PAL DAC channel
 * @param[in] ch Logical DAC channel number
 */
void pal_dac_deinit(pal_dac_channel_t ch);

/**
 * @brief Write raw digital value to specified logical DAC channel
 * @param[in] ch Logical DAC channel number
 * @param[in] raw_val Raw digital value (0 to (1<<bits)-1)
 * @return WINK_OK on success, error status code otherwise
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_dac_write_raw(pal_dac_channel_t ch, uint16_t raw_val);

/**
 * @brief Write analog voltage in millivolts to specified logical DAC channel
 * @param[in] ch Logical DAC channel number
 * @param[in] millivolts Voltage in millivolts [0, full_scale_mv]
 * @return WINK_OK on success, error status code otherwise
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_dac_write_voltage(pal_dac_channel_t ch, uint16_t millivolts);

/**
 * @brief Read back current output voltage in millivolts
 * @param[in] ch Logical DAC channel number
 * @param[out] out_millivolts Output pointer for millivolts
 * @return WINK_OK on success, error status code otherwise
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_dac_get_voltage(pal_dac_channel_t ch, uint16_t *out_millivolts);

/**
 * @brief Query physical pin mapped to logical DAC channel
 * @param[in] ch Logical DAC channel number
 * @param[out] out_pin Output pointer for GPIO pin
 * @return WINK_OK on success, error status code otherwise
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_dac_channel_pin(pal_dac_channel_t ch, wink_pin_t *out_pin);

/**
 * @brief Query logical DAC channel mapped to physical pin
 * @param[in] pin GPIO pin number
 * @param[out] out_ch Output pointer for logical DAC channel
 * @return WINK_OK on success, error status code otherwise
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_dac_pin_channel(wink_pin_t pin, pal_dac_channel_t *out_ch);

/**
 * @brief Acquire and initialize a DAC channel for a physical GPIO pin
 * @param[in] pin Physical GPIO pin number
 * @param[in] cfg Configuration struct
 * @param[out] out_ch Output pointer for acquired DAC channel handle
 * @return WINK_OK on success, WINK_ERR_NO_MEM if no channel slot available
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_dac_acquire(wink_pin_t pin, const pal_dac_config_t *cfg, pal_dac_channel_t *out_ch);

/**
 * @brief Release an acquired DAC channel
 * @param[in] ch Logical DAC channel number
 * @return WINK_OK on success, error status code otherwise
 */
wink_status_t pal_dac_release(pal_dac_channel_t ch);

/**
 * @brief Start continuous cosine wave generation on DAC channel
 * @param[in] ch Logical DAC channel number
 * @param[in] cw_cfg Cosine wave configuration
 * @return WINK_OK on success, error status code otherwise
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_dac_start_cw(pal_dac_channel_t ch, const pal_dac_cw_config_t *cw_cfg);

/**
 * @brief Stop cosine wave generation on DAC channel
 * @param[in] ch Logical DAC channel number
 * @return WINK_OK on success, error status code otherwise
 */
wink_status_t pal_dac_stop_cw(pal_dac_channel_t ch);

#ifdef __cplusplus
}
#endif

#endif /* PAL_DAC_H */
