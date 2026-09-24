/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef ESP_RANDOM_H
#define ESP_RANDOM_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t esp_random(void);
void esp_fill_random(void *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* ESP_RANDOM_H */
