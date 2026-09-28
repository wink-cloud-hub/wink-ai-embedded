/* SPDX-License-Identifier: GPL-3.0-only */
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"

extern QueueHandle_t g_ctor_queue;
extern SemaphoreHandle_t g_ctor_semaphore;
extern "C" uint32_t esp_idf_wasm_ctor_expected_value(void);
extern "C" void pal_wasm_target_request_reset(void);

extern "C" EMSCRIPTEN_KEEPALIVE void esp_idf_wasm_request_reset(void) {
    pal_wasm_target_request_reset();
}

extern "C" EMSCRIPTEN_KEEPALIVE int esp_idf_wasm_verify_reset_resources(void) {
    uint32_t stale_value = 0;
    if (g_ctor_queue == nullptr ||
        xQueueReceive(g_ctor_queue, &stale_value, 0) != pdFALSE ||
        g_ctor_semaphore == nullptr ||
        xSemaphoreTake(g_ctor_semaphore, 0) != pdFALSE) {
        return 1;
    }

    QueueHandle_t fresh_queue = xQueueCreate(1, sizeof(uint32_t));
    SemaphoreHandle_t fresh_semaphore = xSemaphoreCreateBinary();
    if (fresh_queue == nullptr || fresh_semaphore == nullptr) return 2;
    const uint32_t value = 0xBEEF;
    if (xQueueSend(fresh_queue, &value, 0) != pdPASS ||
        xQueueReceive(fresh_queue, &stale_value, 0) != pdPASS ||
        stale_value != value ||
        xSemaphoreGive(fresh_semaphore) != pdPASS ||
        xSemaphoreTake(fresh_semaphore, 0) != pdPASS) {
        return 3;
    }
    vQueueDelete(fresh_queue);
    vSemaphoreDelete(fresh_semaphore);
    return 0;
}

extern "C" void app_main(void) {
    uint32_t value = 0;
    if (g_ctor_queue == nullptr ||
        xQueueReceive(g_ctor_queue, &value, 0) != pdPASS ||
        value != esp_idf_wasm_ctor_expected_value() ||
        g_ctor_semaphore == nullptr ||
        xSemaphoreTake(g_ctor_semaphore, 0) != pdPASS) {
        std::fprintf(stderr, "ESP-IDF Wasm constructor resources were lost\n");
        std::abort();
    }
    std::puts("ESP-IDF_WASM_CTOR_RESOURCES_OK");

    const size_t ordinary_hint = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    for (size_t i = 0; i < 160; ++i) {
        void *ordinary = heap_caps_malloc(8, MALLOC_CAP_DEFAULT);
        if (ordinary == nullptr) std::abort();
        std::free(ordinary);
    }
    if (heap_caps_get_free_size(MALLOC_CAP_DEFAULT) != ordinary_hint) std::abort();

    const size_t dma_free = heap_caps_get_free_size(MALLOC_CAP_DMA);
    void *dma = heap_caps_malloc(64, MALLOC_CAP_DMA);
    if (dma == nullptr || (reinterpret_cast<uintptr_t>(dma) % 32) != 0 ||
        heap_caps_get_free_size(MALLOC_CAP_DMA) != dma_free - 64) {
        std::abort();
    }
    heap_caps_free(dma);
    if (heap_caps_get_free_size(MALLOC_CAP_DMA) != dma_free) std::abort();

    void *aligned = heap_caps_aligned_alloc(64, 127, MALLOC_CAP_DEFAULT);
    if (aligned == nullptr || (reinterpret_cast<uintptr_t>(aligned) % 64) != 0) {
        std::abort();
    }
    heap_caps_free(aligned);
    if (heap_caps_calloc(static_cast<size_t>(-1) / 2 + 1, 2,
                         MALLOC_CAP_DEFAULT) != nullptr) {
        std::abort();
    }

    auto *ordinary_across_reset = static_cast<uint8_t *>(
        heap_caps_malloc(8, MALLOC_CAP_DEFAULT));
    void *special_across_reset = heap_caps_aligned_alloc(64, 127, MALLOC_CAP_DEFAULT);
    if (ordinary_across_reset == nullptr || special_across_reset == nullptr) std::abort();
    std::memset(ordinary_across_reset, 0xC3, 8);
    esp_heap_caps_reset();
    if (ordinary_across_reset[0] != 0xC3 || ordinary_across_reset[7] != 0xC3 ||
        heap_caps_get_free_size(MALLOC_CAP_DMA) != 320u * 1024u) {
        std::abort();
    }
    std::free(ordinary_across_reset);
    std::puts("ESP_IDF_WASM_HEAP_CONTRACT_OK");
}
