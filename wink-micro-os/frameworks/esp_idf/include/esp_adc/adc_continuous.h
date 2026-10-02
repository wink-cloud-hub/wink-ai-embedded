/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "sdkconfig.h"
#include "hal/adc_types.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ADC_MAX_DELAY UINT32_MAX

typedef struct adc_continuous_ctx_t *adc_continuous_handle_t;

typedef struct {
    uint32_t max_store_buf_size;
    uint32_t conv_frame_size;
    struct {
        uint32_t flush_pool: 1;
    } flags;
} adc_continuous_handle_cfg_t;

typedef struct {
    uint32_t pattern_num;
    adc_digi_pattern_config_t *adc_pattern;
    uint32_t sample_freq_hz;
    adc_digi_convert_mode_t conv_mode;
    adc_digi_output_format_t format;
} adc_continuous_config_t;

typedef struct {
    uint8_t *conv_frame_buffer;
    uint32_t size;
} adc_continuous_evt_data_t;

typedef bool (*adc_continuous_callback_t)(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data);

typedef struct {
    adc_continuous_callback_t on_conv_done;
    adc_continuous_callback_t on_pool_ovf;
} adc_continuous_evt_cbs_t;

typedef struct {
    adc_unit_t unit;
    adc_channel_t channel;
    uint32_t raw_data;
    bool valid;
} adc_continuous_data_t;

esp_err_t adc_continuous_new_handle(const adc_continuous_handle_cfg_t *hdl_config, adc_continuous_handle_t *ret_handle);
esp_err_t adc_continuous_config(adc_continuous_handle_t handle, const adc_continuous_config_t *config);
esp_err_t adc_continuous_register_event_callbacks(adc_continuous_handle_t handle, const adc_continuous_evt_cbs_t *cbs, void *user_data);
esp_err_t adc_continuous_start(adc_continuous_handle_t handle);
esp_err_t adc_continuous_read(adc_continuous_handle_t handle, uint8_t *buf, uint32_t length_max, uint32_t *out_length, uint32_t timeout_ms);
esp_err_t adc_continuous_stop(adc_continuous_handle_t handle);
esp_err_t adc_continuous_deinit(adc_continuous_handle_t handle);
esp_err_t adc_continuous_flush_pool(adc_continuous_handle_t handle);
esp_err_t adc_continuous_io_to_channel(int io_num, adc_unit_t * const unit_id, adc_channel_t * const channel);
esp_err_t adc_continuous_channel_to_io(adc_unit_t unit_id, adc_channel_t channel, int * const io_num);
esp_err_t adc_continuous_parse_data(adc_continuous_handle_t handle,
                                    const uint8_t *raw_data,
                                    uint32_t raw_data_size,
                                    adc_continuous_data_t *parsed_data,
                                    uint32_t *num_parsed_samples);

#ifdef __cplusplus
}
#endif
