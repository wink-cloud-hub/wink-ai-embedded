// SPDX-License-Identifier: LGPL-3.0-only
#pragma once

#include "driver/dac_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dac_oneshot_s *dac_oneshot_handle_t;

typedef struct {
    dac_channel_t chan_id;
} dac_oneshot_config_t;

esp_err_t dac_oneshot_new_channel(const dac_oneshot_config_t *oneshot_cfg, dac_oneshot_handle_t *ret_handle);
esp_err_t dac_oneshot_del_channel(dac_oneshot_handle_t handle);
esp_err_t dac_oneshot_output_voltage(dac_oneshot_handle_t handle, uint8_t digi_value);

#ifdef __cplusplus
}
#endif
