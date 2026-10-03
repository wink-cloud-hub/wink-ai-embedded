// SPDX-License-Identifier: LGPL-3.0-only
#pragma once

#include "driver/dac_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dac_cosine_s *dac_cosine_handle_t;

typedef struct {
    dac_channel_t chan_id;
    uint32_t freq_hz;
    dac_cosine_clk_src_t clk_src;
    dac_cosine_atten_t atten;
    dac_cosine_phase_t phase;
    int8_t offset;
    struct {
        bool force_set_freq: 1;
    } flags;
} dac_cosine_config_t;

esp_err_t dac_cosine_new_channel(const dac_cosine_config_t *cos_cfg, dac_cosine_handle_t *ret_handle);
esp_err_t dac_cosine_del_channel(dac_cosine_handle_t handle);
esp_err_t dac_cosine_start(dac_cosine_handle_t handle);
esp_err_t dac_cosine_stop(dac_cosine_handle_t handle);

#ifdef __cplusplus
}
#endif
