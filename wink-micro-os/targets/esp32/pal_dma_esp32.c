// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_dma_esp32.c
 * @brief ESP32 PAL DMA Cache Synchronization Implementation.
 */
#include "hal/pal_dma.h"
#include "pal_log.h"

#define LOG_TAG "pal_dma"

#if defined(ESP_PLATFORM)
#include "esp_err.h"
#include "esp_memory_utils.h"
#if __has_include("esp_cache.h")
#include "esp_cache.h"
#define HAVE_ESP_CACHE_H 1
#endif
#if __has_include("rom/cache.h")
#include "rom/cache.h"
#endif

static inline void check_psram_boundary(const void *addr) {
#if defined(CONFIG_IDF_TARGET_ESP32)
    if (addr != NULL && !esp_ptr_in_dram(addr) && !esp_ptr_in_iram(addr)) {
        LOG_E(LOG_TAG, "FATAL: DMA buffer %p is not in internal DRAM (Classic ESP32 DMA cannot access PSRAM)", addr);
    }
#else
    (void)addr;
#endif
}

void pal_dma_cache_clean(const void *addr, size_t len) {
    if (addr == NULL || len == 0) return;
    check_psram_boundary(addr);

#if defined(HAVE_ESP_CACHE_H)
    /* ESP-IDF 5.x unified cache sync API */
    esp_cache_msync((void *)(uintptr_t)addr, len, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
#elif defined(Cache_WriteBack_Addr)
    uint32_t start_addr = (uint32_t)(uintptr_t)addr & ~(PAL_DMA_CACHE_LINE_SIZE - 1);
    uint32_t end_addr = ((uint32_t)(uintptr_t)addr + (uint32_t)len + PAL_DMA_CACHE_LINE_SIZE - 1) & ~(PAL_DMA_CACHE_LINE_SIZE - 1);
    Cache_WriteBack_Addr(start_addr, end_addr - start_addr);
#endif
}

void pal_dma_cache_invalidate(const void *addr, size_t len) {
    if (addr == NULL || len == 0) return;
    check_psram_boundary(addr);

#if defined(HAVE_ESP_CACHE_H)
    /* ESP-IDF 5.x unified cache invalidate API */
    esp_cache_msync((void *)(uintptr_t)addr, len, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
#elif defined(Cache_Invalidate_Addr)
    uint32_t start_addr = (uint32_t)(uintptr_t)addr & ~(PAL_DMA_CACHE_LINE_SIZE - 1);
    uint32_t end_addr = ((uint32_t)(uintptr_t)addr + (uint32_t)len + PAL_DMA_CACHE_LINE_SIZE - 1) & ~(PAL_DMA_CACHE_LINE_SIZE - 1);
    Cache_Invalidate_Addr(start_addr, end_addr - start_addr);
#endif
}

#else

void pal_dma_cache_clean(const void *addr, size_t len) { (void)addr; (void)len; }
void pal_dma_cache_invalidate(const void *addr, size_t len) { (void)addr; (void)len; }

#endif
