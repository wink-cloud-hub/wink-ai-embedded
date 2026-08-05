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

typedef struct {
    wink_pin_t   pin;              /* int16_t 语义，兼容 NC(-1) */
    uint16_t     full_scale_mv;    /* 0 = 平台默认（ESP32≈3100, wasm/host=3300） */
    uint8_t      resolution_bits;  /* 0 = 平台默认（12-bit） */
} pal_adc_config_t;

WINK_WARN_UNUSED_RESULT
wink_status_t pal_adc_init(pal_adc_channel_t ch, const pal_adc_config_t *cfg);

void pal_adc_deinit(pal_adc_channel_t ch);

WINK_WARN_UNUSED_RESULT
wink_status_t pal_adc_channel_pin(pal_adc_channel_t ch, wink_pin_t *out_pin);

WINK_WARN_UNUSED_RESULT
wink_status_t pal_adc_pin_channel(wink_pin_t pin, pal_adc_channel_t *out_ch);

WINK_WARN_UNUSED_RESULT
wink_status_t pal_adc_full_scale_mv(pal_adc_channel_t ch, uint16_t *out_mv);

#ifndef WINK_STRICT_NONBLOCKING
/**
 * @brief 读取指定 ADC 逻辑通道的 Raw 采样值（oneshot 转换）。
 * @param ch 逻辑通道号 [0, PAL_ADC_CHANNELS)
 * @param out_raw 输出 Raw 采样值
 * @return WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED / WINK_ERR_BUSY / WINK_ERR_TIMEOUT
 * @note Blocking: Yes (最坏 = 一次转换时间，µs 级)。
 */
WINK_BLOCKING
WINK_WARN_UNUSED_RESULT
wink_status_t pal_adc_read_raw(pal_adc_channel_t ch, uint16_t *out_raw);

/**
 * @brief 读取指定 ADC 逻辑通道的毫伏 (mV) 转换结果。
 * @param ch 逻辑通道号 [0, PAL_ADC_CHANNELS)
 * @param out_mv 输出 mV 采样值
 * @return WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED / WINK_ERR_BUSY / WINK_ERR_TIMEOUT
 * @note Blocking: Yes (复用 Raw 缓存换算；最坏 = 一次转换时间，µs 级)。
 */
WINK_BLOCKING
WINK_WARN_UNUSED_RESULT
wink_status_t pal_adc_read_mv(pal_adc_channel_t ch, uint16_t *out_mv);
#endif /* WINK_STRICT_NONBLOCKING */

#ifdef __cplusplus
}
#endif

#endif /* PAL_ADC_H */
