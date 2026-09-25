/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef NVS_FLASH_H_
#define NVS_FLASH_H_

#include "nvs.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t nvs_flash_init(void);
esp_err_t nvs_flash_erase(void);
esp_err_t nvs_flash_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* NVS_FLASH_H_ */
