/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef ESP_ADC_ONESHOT_H
#define ESP_ADC_ONESHOT_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "hal/adc_types.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *adc_oneshot_unit_handle_t;

typedef struct {
    adc_unit_t unit_id;
    adc_oneshot_clk_src_t clk_src;
    adc_ulp_mode_t ulp_mode;
} adc_oneshot_unit_init_cfg_t;

typedef struct {
    adc_atten_t atten;
    adc_bitwidth_t bitwidth;
} adc_oneshot_chan_cfg_t;

esp_err_t adc_oneshot_new_unit(const adc_oneshot_unit_init_cfg_t *init_config, adc_oneshot_unit_handle_t *ret_unit);
esp_err_t adc_oneshot_config_channel(adc_oneshot_unit_handle_t handle, adc_channel_t channel, const adc_oneshot_chan_cfg_t *config);
esp_err_t adc_oneshot_read(adc_oneshot_unit_handle_t handle, adc_channel_t chan, int *out_raw);
esp_err_t adc_oneshot_del_unit(adc_oneshot_unit_handle_t handle);
esp_err_t adc_oneshot_io_to_channel(int io_num, adc_unit_t *unit_id, adc_channel_t *channel);
esp_err_t adc_oneshot_channel_to_io(adc_unit_t unit_id, adc_channel_t channel, int *io_num);
esp_err_t adc_oneshot_get_calibrated_result(adc_oneshot_unit_handle_t handle, adc_cali_handle_t cali_handle, adc_channel_t chan, int *cali_result);

#ifdef __cplusplus
}
#endif

#endif /* ESP_ADC_ONESHOT_H */
