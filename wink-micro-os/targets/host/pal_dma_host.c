// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_dma_host.c
 * @brief Host implementation of PAL DMA cache operations.
 */
#include "hal/pal_dma.h"

void pal_dma_cache_clean(const void *addr, size_t len) {
    (void)addr;
    (void)len;
}

void pal_dma_cache_invalidate(const void *addr, size_t len) {
    (void)addr;
    (void)len;
}
