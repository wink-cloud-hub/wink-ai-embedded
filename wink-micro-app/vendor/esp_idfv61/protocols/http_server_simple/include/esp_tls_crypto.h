/* SPDX-License-Identifier: CC0-1.0 */
#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int esp_crypto_base64_encode(unsigned char *dst, size_t dlen,
                             size_t *olen, const unsigned char *src,
                             size_t slen);

#ifdef __cplusplus
}
#endif
