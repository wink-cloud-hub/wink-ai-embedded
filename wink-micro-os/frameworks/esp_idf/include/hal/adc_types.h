/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef ESP_IDF_HAL_ADC_TYPES_H
#define ESP_IDF_HAL_ADC_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ADC_UNIT_1 = 0,
    ADC_UNIT_2 = 1,
} adc_unit_t;

typedef enum {
    ADC_CHANNEL_0 = 0,
    ADC_CHANNEL_1 = 1,
    ADC_CHANNEL_2 = 2,
    ADC_CHANNEL_3 = 3,
    ADC_CHANNEL_4 = 4,
    ADC_CHANNEL_5 = 5,
    ADC_CHANNEL_6 = 6,
    ADC_CHANNEL_7 = 7,
    ADC_CHANNEL_8 = 8,
    ADC_CHANNEL_9 = 9,
} adc_channel_t;

typedef enum {
    ADC_ATTEN_DB_0   = 0,
    ADC_ATTEN_DB_2_5 = 1,
    ADC_ATTEN_DB_6   = 2,
    ADC_ATTEN_DB_11  = 3,
    ADC_ATTEN_DB_12  = 3,
} adc_atten_t;

typedef enum {
    ADC_BITWIDTH_DEFAULT = 0,
    ADC_BITWIDTH_9  = 9,
    ADC_BITWIDTH_10 = 10,
    ADC_BITWIDTH_11 = 11,
    ADC_BITWIDTH_12 = 12,
    ADC_BITWIDTH_13 = 13,
} adc_bitwidth_t;

typedef enum {
    ADC_ULP_MODE_DISABLE = 0,
    ADC_ULP_MODE_FSM     = 1,
    ADC_ULP_MODE_RISCV   = 2,
} adc_ulp_mode_t;

typedef int adc_oneshot_clk_src_t;

typedef enum {
    ADC_CONV_SINGLE_UNIT_1 = 1,
    ADC_CONV_SINGLE_UNIT_2 = 2,
    ADC_CONV_BOTH_UNIT     = 3,
    ADC_CONV_ALTER_UNIT    = 7,
} adc_digi_convert_mode_t;

typedef enum {
    ADC_DIGI_OUTPUT_FORMAT_TYPE1 = 0,
    ADC_DIGI_OUTPUT_FORMAT_TYPE2 = 1,
} adc_digi_output_format_t;

typedef struct {
    uint8_t atten;
    uint8_t channel;
    uint8_t unit;
    uint8_t bit_width;
} adc_digi_pattern_config_t;

/* ESP32 Type 1 output data structure (16-bit) */
typedef union {
    struct {
        uint16_t data: 12;
        uint16_t channel: 4;
    } type1;
    uint16_t val;
} adc_digi_output_data_t;

#ifdef __cplusplus
}
#endif

#endif /* ESP_IDF_HAL_ADC_TYPES_H */
