// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_storage_wasm.c
 * @brief Wasm no-op storage implementation.
 */
#include "pal_storage.h"

void pal_storage_reset(void) {
}

wink_status_t pal_storage_read(const char *key, uint8_t *buf, uint16_t cap, uint16_t *out_len) {
    (void)key; (void)buf; (void)cap; (void)out_len;
    return WINK_ERR_UNSUPPORTED;
}

wink_status_t pal_storage_write(const char *key, const uint8_t *buf, uint16_t len) {
    (void)key; (void)buf; (void)len;
    return WINK_ERR_UNSUPPORTED;
}

wink_status_t pal_storage_erase(const char *key) {
    (void)key;
    return WINK_ERR_UNSUPPORTED;
}
