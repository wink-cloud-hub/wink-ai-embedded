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
 * @brief Acquire and initialize an ADC channel for a physical GPIO pin.
 *
 * Searches for an existing channel initialized for pin, or finds an uninitialized free channel,
 * initializes it with cfg, and returns the logical channel handle.
 *
 * @param[in] pin Physical GPIO pin number
 * @param[in] cfg Configuration struct (full_scale_mv, resolution_bits)
 * @param[out] out_ch Output pointer for acquired ADC channel handle
 * @return WINK_OK on success, WINK_ERR_NO_MEMORY if no channel slot available
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_adc_acquire(wink_pin_t pin, const pal_adc_config_t *cfg, pal_adc_channel_t *out_ch);

/**
 * @brief Release an acquired ADC channel
 * @param[in] ch Logical ADC channel number
 * @return WINK_OK on success
 */
wink_status_t pal_adc_release(pal_adc_channel_t ch);

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
WINK_WARN_UNUSED_RESULT
wink_status_t pal_adc_read_raw(pal_adc_channel_t ch, uint16_t *out_raw);

/**
 * @brief Read millivolt (mV) sample value from specified logical ADC channel
 * @param[in] ch Logical ADC channel number [0, PAL_ADC_CHANNELS)
 * @param[out] out_mv Output pointer for calculated millivolts
 * @return WINK_OK on success, WINK_ERR_INVALID_ARG, WINK_ERR_NOT_INITIALIZED, WINK_ERR_BUSY, or WINK_ERR_TIMEOUT
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_adc_read_mv(pal_adc_channel_t ch, uint16_t *out_mv);
#endif /* WINK_STRICT_NONBLOCKING */

/* --- Continuous DMA & PWM-ADC TRGO Subsystem --- */

typedef enum {
    PAL_ADC_TRIG_SOURCE_SW,        /**< Software / built-in timer trigger (ESP32 classic continuous mode) */
    PAL_ADC_TRIG_SOURCE_MCPWM,     /**< MCPWM event hardware trigger (ESP32-S2/S3) */
} pal_adc_trig_source_t;

typedef enum {
    PAL_ADC_TRIG_AT_PWM_PEAK,      /**< Sample at PWM counter peak */
    PAL_ADC_TRIG_AT_PWM_VALLEY,    /**< Sample at PWM counter valley */
    PAL_ADC_TRIG_AT_PWM_BOTH,      /**< Sample at both peak and valley */
} pal_adc_trgo_edge_t;

typedef struct {
    pal_adc_trig_source_t source;
    void                 *pwm_timer;             /**< Associated pal_mcpwm_timer_handle_t if source=MCPWM */
    uint8_t               adc_unit;              /**< ADC unit (0 or 1) */
    const uint8_t        *channels;              /**< Array of logical channels to scan */
    uint8_t               channel_count;         /**< Number of channels */
    pal_adc_trgo_edge_t   edge;                  /**< TRGO trigger edge alignment */
    uint16_t              sampling_period_pwm;   /**< Sample every N PWM cycles */
    uint16_t             *dma_buf_a;             /**< DMA double buffer A (PAL_DMA_BUF_ATTR) */
    uint16_t             *dma_buf_b;             /**< DMA double buffer B (PAL_DMA_BUF_ATTR) */
    size_t                samples_per_buf;       /**< Buffer length in samples */
    void                (*on_half_full)(void *arg, const uint16_t *buf, size_t n);
    void                (*on_full)(void *arg, const uint16_t *buf, size_t n);
    void                 *cb_arg;
} pal_adc_continuous_cfg_t;

/**
 * @brief Start continuous ADC sampling with DMA double buffering.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_adc_continuous_start(const pal_adc_continuous_cfg_t *cfg);

/**
 * @brief Stop continuous ADC DMA sampling.
 */
wink_status_t pal_adc_continuous_stop(uint8_t adc_unit);

#ifdef __cplusplus
}
#endif

#endif /* PAL_ADC_H */
