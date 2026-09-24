/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef ESP_INTR_ALLOC_H
#define ESP_INTR_ALLOC_H

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *intr_handle_t;
typedef void (*intr_handler_t)(void *arg);

#define ESP_INTR_FLAG_SHARED    (1 << 3)
#define ESP_INTR_FLAG_EDGE      (1 << 9)
#define ESP_INTR_FLAG_IRAM      (1 << 10)

esp_err_t esp_intr_alloc(int source, int flags, intr_handler_t handler, void *arg, intr_handle_t *ret_handle);
esp_err_t esp_intr_free(intr_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* ESP_INTR_ALLOC_H */
