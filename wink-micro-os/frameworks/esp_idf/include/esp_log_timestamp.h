/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef ESP_LOG_TIMESTAMP_H_
#define ESP_LOG_TIMESTAMP_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t esp_log_timestamp(void);
char *esp_log_system_timestamp(void);
uint32_t esp_log_early_timestamp(void);

#ifdef __cplusplus
}
#endif

#endif /* ESP_LOG_TIMESTAMP_H_ */
