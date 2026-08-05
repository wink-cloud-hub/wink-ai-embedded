// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_storage_host.c
 * @brief Host in-memory single-slot storage implementation.
 */
#include "pal_storage.h"

#include <stdbool.h>
#include <string.h>

#define HOST_STORAGE_SLOT_BYTES 256u

static uint8_t  s_buf[HOST_STORAGE_SLOT_BYTES];
static uint16_t s_len;
static bool     s_used;

void pal_storage_reset(void) {
    s_used = false;
    s_len = 0;
}

wink_status_t pal_storage_read(const char *key, uint8_t *buf, uint16_t cap, uint16_t *out_len) {
    (void)key;
    if (buf == NULL || out_len == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!s_used) {
        return WINK_ERR_EMPTY;
    }
    if (s_len > cap) {
        return WINK_ERR_INVALID_ARG;
    }
    memcpy(buf, s_buf, s_len);
    *out_len = s_len;
    return WINK_OK;
}

wink_status_t pal_storage_write(const char *key, const uint8_t *buf, uint16_t len) {
    (void)key;
    if (buf == NULL && len > 0u) {
        return WINK_ERR_INVALID_ARG;
    }
    if (len > HOST_STORAGE_SLOT_BYTES) {
        return WINK_ERR_INVALID_ARG;
    }
    if (len > 0u) {
        memcpy(s_buf, buf, len);
    }
    s_len = len;
    s_used = true;
    return WINK_OK;
}

wink_status_t pal_storage_erase(const char *key) {
    (void)key;
    s_used = false;
    s_len = 0;
    return WINK_OK;
}
