/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef ESP_ADC_CALI_SCHEME_H
#define ESP_ADC_CALI_SCHEME_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "hal/adc_types.h"
#include "esp_adc/adc_cali.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
#define ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED  0
#endif

#ifndef ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
#define ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED   1
#endif

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
typedef struct {
    adc_unit_t unit_id;
    adc_channel_t chan;
    adc_atten_t atten;
    adc_bitwidth_t bitwidth;
} adc_cali_curve_fitting_config_t;

esp_err_t adc_cali_create_scheme_curve_fitting(const adc_cali_curve_fitting_config_t *config, adc_cali_handle_t *ret_handle);
esp_err_t adc_cali_delete_scheme_curve_fitting(adc_cali_handle_t handle);
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
typedef enum {
    ADC_CALI_LINE_FITTING_EFUSE_VAL_EFUSE_VREF = 0,
    ADC_CALI_LINE_FITTING_EFUSE_VAL_EFUSE_TP = 1,
    ADC_CALI_LINE_FITTING_EFUSE_VAL_DEFAULT_VREF = 2,
} adc_cali_line_fitting_efuse_val_t;

typedef struct {
    adc_unit_t unit_id;
    adc_atten_t atten;
    adc_bitwidth_t bitwidth;
    uint32_t default_vref;
} adc_cali_line_fitting_config_t;

esp_err_t adc_cali_create_scheme_line_fitting(const adc_cali_line_fitting_config_t *config, adc_cali_handle_t *ret_handle);
esp_err_t adc_cali_delete_scheme_line_fitting(adc_cali_handle_t handle);
esp_err_t adc_cali_scheme_line_fitting_check_efuse(adc_cali_line_fitting_efuse_val_t *cali_val);
#endif

#ifdef __cplusplus
}
#endif

#endif /* ESP_ADC_CALI_SCHEME_H */
