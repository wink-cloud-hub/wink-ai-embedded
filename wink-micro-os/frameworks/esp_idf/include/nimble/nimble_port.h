/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t nimble_port_init(void);
esp_err_t nimble_port_deinit(void);
void nimble_port_run(void);
esp_err_t nimble_port_stop(void);

#ifdef __cplusplus
}
#endif
