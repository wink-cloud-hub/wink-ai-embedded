/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef ESP_TIMER_H
#define ESP_TIMER_H

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

int64_t esp_timer_get_time(void);

#ifdef __cplusplus
}
#endif

#endif /* ESP_TIMER_H */
