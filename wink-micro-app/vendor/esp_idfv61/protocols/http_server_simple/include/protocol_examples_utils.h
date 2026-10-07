/* SPDX-License-Identifier: CC0-1.0 */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static inline uint32_t example_uri_encode(char *dest, const char *src, size_t len) {
    if (dest && src && len > 0) {
        strncpy(dest, src, len);
    }
    return (uint32_t)len;
}

static inline void example_uri_decode(char *dest, const char *src, size_t len) {
    if (dest && src && len > 0) {
        strncpy(dest, src, len);
    }
}
