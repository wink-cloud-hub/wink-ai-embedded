// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_dma.h
 * @brief PAL DMA Cache Synchronization and Buffer Contract Subsystem.
 *
 * DMA Buffer Contract (v5 G3 / A2 Mandatory):
 * ===========================================
 * 1. Lifetime Contract:
 *    Any buffer passed to an asynchronous DMA API (SPI DMA, RMT TX, ADC DMA)
 *    must remain valid and unmodified by the caller until the completion
 *    callback is fired. Mutating or freeing the buffer while DMA transfer
 *    is in-flight causes undefined behavior and data corruption.
 *
 * 2. Alignment & Sizing:
 *    DMA buffers must be decorated with `PAL_DMA_BUF_ATTR` and `PAL_DMA_BUF_ALIGN`
 *    (32-byte cache line alignment). Buffer length should ideally be rounded up
 *    to a multiple of 32 bytes (cache line size).
 *
 * 3. PSRAM Restriction (Classic ESP32):
 *    Classic ESP32 (and ESP32-S2) GDMA cannot access external PSRAM (SPIRAM).
 *    All DMA buffers must reside strictly in internal SRAM (DRAM / IRAM).
 *
 * 4. Cache Operations:
 *    - TX (CPU -> Device): Call pal_dma_cache_clean(addr, len) before starting DMA.
 *    - RX (Device -> CPU): Call pal_dma_cache_invalidate(addr, len) after DMA completes.
 */
#ifndef PAL_DMA_H
#define PAL_DMA_H

#include "wink_status.h"
#include "wink_compiler.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PAL_DMA_CACHE_LINE_SIZE 32

/**
 * @brief Write back CPU cache to main memory before DMA transmission (CPU -> Device).
 *
 * Flushes dirty cache lines to DRAM so peripheral DMA hardware reads updated memory.
 *
 * @param[in] addr Pointer to DMA buffer (should be 32-byte aligned).
 * @param[in] len Length of buffer in bytes.
 */
void pal_dma_cache_clean(const void *addr, size_t len);

/**
 * @brief Invalidate CPU cache lines after DMA reception so CPU reads fresh device data (Device -> CPU).
 *
 * Invalidates stale cache lines so subsequent CPU reads fetch new data written by DMA.
 *
 * @param[in] addr Pointer to DMA buffer (should be 32-byte aligned).
 * @param[in] len Length of buffer in bytes.
 */
void pal_dma_cache_invalidate(const void *addr, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* PAL_DMA_H */
