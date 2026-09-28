/* SPDX-License-Identifier: GPL-3.0-only */
#include <cstdint>

#include "freertos/queue.h"
#include "freertos/semphr.h"

QueueHandle_t g_ctor_queue = nullptr;
SemaphoreHandle_t g_ctor_semaphore = nullptr;
constexpr uint32_t kCtorValue = 0xC701;

namespace {
struct CreateResourcesBeforeMain {
    CreateResourcesBeforeMain() {
        g_ctor_queue = xQueueCreate(1, sizeof(uint32_t));
        g_ctor_semaphore = xSemaphoreCreateBinary();
        if (g_ctor_queue != nullptr) {
            const uint32_t value = kCtorValue;
            (void)xQueueSend(g_ctor_queue, &value, 0);
        }
        if (g_ctor_semaphore != nullptr) {
            (void)xSemaphoreGive(g_ctor_semaphore);
        }
    }
};

CreateResourcesBeforeMain g_create_resources_before_main;
} // namespace

extern "C" uint32_t esp_idf_wasm_ctor_expected_value(void) {
    return kCtorValue;
}
