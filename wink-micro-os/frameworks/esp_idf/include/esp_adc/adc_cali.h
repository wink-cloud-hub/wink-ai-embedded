/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef ESP_ADC_CALI_H
#define ESP_ADC_CALI_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "hal/adc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *adc_cali_handle_t;

typedef enum {
    ADC_CALI_SCHEME_VER_LINE_FITTING  = (1 << 0),
    ADC_CALI_SCHEME_VER_CURVE_FITTING = (1 << 1),
} adc_cali_scheme_ver_t;

esp_err_t adc_cali_check_scheme(adc_cali_scheme_ver_t *scheme_mask);
esp_err_t adc_cali_raw_to_voltage(adc_cali_handle_t handle, int raw, int *voltage);

#ifdef __cplusplus
}
#endif

#endif /* ESP_ADC_CALI_H */
