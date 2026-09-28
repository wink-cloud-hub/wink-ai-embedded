/* SPDX-License-Identifier: GPL-3.0-only */
#include <cstdint>
#include <cstdio>

#include "wink_app.h"
#include "wink_sim_scheduler.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

extern "C" const wink_app_callbacks_t *wink_app_get_callbacks(void);
extern "C" uint32_t esp_idf_get_app_main_task_id(void);

namespace {
constexpr uint32_t kCtorQueueValue = 0xC701;
QueueHandle_t s_ctor_queue = nullptr;
SemaphoreHandle_t s_ctor_semaphore = nullptr;
bool s_queue_survived_framework_init = false;
bool s_semaphore_survived_framework_init = false;

struct GlobalFreeRtosResources {
    GlobalFreeRtosResources() {
        s_ctor_queue = xQueueCreate(1, sizeof(uint32_t));
        s_ctor_semaphore = xSemaphoreCreateBinary();
        if (s_ctor_queue != nullptr) {
            const uint32_t value = kCtorQueueValue;
            (void)xQueueSend(s_ctor_queue, &value, 0);
        }
        if (s_ctor_semaphore != nullptr) {
            (void)xSemaphoreGive(s_ctor_semaphore);
        }
    }
};
GlobalFreeRtosResources s_global_resources;
} // namespace

extern "C" void app_main(void) {
    uint32_t value = 0;
    s_queue_survived_framework_init =
        s_ctor_queue != nullptr &&
        xQueueReceive(s_ctor_queue, &value, 0) == pdPASS &&
        value == kCtorQueueValue;
    s_semaphore_survived_framework_init =
        s_ctor_semaphore != nullptr &&
        xSemaphoreTake(s_ctor_semaphore, 0) == pdPASS;
}

int main() {
    if (s_ctor_queue == nullptr || s_ctor_semaphore == nullptr) {
        std::fprintf(stderr, "global constructor failed to create FreeRTOS resources\n");
        return 1;
    }
    const wink_app_callbacks_t *callbacks = wink_app_get_callbacks();
    if (callbacks == nullptr || callbacks->init == nullptr) {
        std::fprintf(stderr, "ESP-IDF init callback is unavailable\n");
        return 2;
    }
    callbacks->init();
    const uint32_t app_main_task = esp_idf_get_app_main_task_id();
    if (app_main_task == SIM_SCHED_NO_READY ||
        pal_sim_scheduler_run(callbacks, app_main_task, 1) != WINK_OK) {
        std::fprintf(stderr, "app_main fiber did not run\n");
        return 3;
    }
    if (!s_queue_survived_framework_init || !s_semaphore_survived_framework_init) {
        std::fprintf(stderr, "framework init discarded global-constructor resources\n");
        return 4;
    }
    return 0;
}
