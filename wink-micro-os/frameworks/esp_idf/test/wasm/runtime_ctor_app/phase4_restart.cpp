/* SPDX-License-Identifier: GPL-3.0-only */
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "driver/gptimer.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_sim_handle.h"

extern QueueHandle_t g_ctor_queue;
extern SemaphoreHandle_t g_ctor_semaphore;

static void dummy_task_fn(void *arg) {
    (void)arg;
    vTaskDelay(100);
}

extern "C" {

EMSCRIPTEN_KEEPALIVE int esp_idf_wasm_phase4_export_handles(uint32_t *out_handles, uint32_t max_count) {
    if (!out_handles || max_count < 8) return -1;

    // 0: ctor queue
    out_handles[0] = (uint32_t)(uintptr_t)g_ctor_queue;
    // 1: ctor sem
    out_handles[1] = (uint32_t)(uintptr_t)g_ctor_semaphore;

    // 2: dynamic queue
    QueueHandle_t dyn_q = xQueueCreate(2, sizeof(uint32_t));
    out_handles[2] = (uint32_t)(uintptr_t)dyn_q;

    // 3: dynamic sem
    SemaphoreHandle_t dyn_sem = xSemaphoreCreateBinary();
    out_handles[3] = (uint32_t)(uintptr_t)dyn_sem;

    // 4: dynamic event group
    EventGroupHandle_t dyn_eg = xEventGroupCreate();
    out_handles[4] = (uint32_t)(uintptr_t)dyn_eg;

    // 5: dynamic task
    TaskHandle_t dyn_tsk = NULL;
    xTaskCreate(dummy_task_fn, "p4_tsk", 32768, NULL, 5, &dyn_tsk);
    out_handles[5] = (uint32_t)(uintptr_t)dyn_tsk;

    // 6: dynamic gptimer
    gptimer_handle_t dyn_timer = NULL;
    gptimer_config_t timer_cfg = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,
        .flags = { .intr_shared = 0 }
    };
    (void)gptimer_new_timer(&timer_cfg, &dyn_timer);
    out_handles[6] = (uint32_t)(uintptr_t)dyn_timer;

    // 7: dynamic nvs
    nvs_flash_init();
    nvs_handle_t dyn_nvs = 0;
    (void)nvs_open("p4_ns", NVS_READWRITE, &dyn_nvs);
    out_handles[7] = (uint32_t)(uintptr_t)dyn_nvs;

    return 8;
}

EMSCRIPTEN_KEEPALIVE int esp_idf_wasm_phase4_verify_stale_handles(const uint32_t *stale_handles, uint32_t count) {
    if (!stale_handles || count < 8) return -1;

    // Stale Queues (0: ctor queue, 2: dynamic queue)
    for (int idx : {0, 2}) {
        uint32_t dummy = 0xAA;
        QueueHandle_t q = (QueueHandle_t)(uintptr_t)stale_handles[idx];
        if (xQueueSend(q, &dummy, 0) != pdFALSE) return 10 + idx;
        if (xQueueReceive(q, &dummy, 0) != pdFALSE) return 20 + idx;
        if (xQueuePeek(q, &dummy, 0) != pdFALSE) return 30 + idx;
    }

    // Stale Semaphores (1: ctor sem, 3: dynamic sem)
    for (int idx : {1, 3}) {
        SemaphoreHandle_t sem = (SemaphoreHandle_t)(uintptr_t)stale_handles[idx];
        if (xSemaphoreGive(sem) != pdFALSE) return 40 + idx;
        if (xSemaphoreTake(sem, 0) != pdFALSE) return 50 + idx;
    }

    // Stale EventGroup (4)
    EventGroupHandle_t eg = (EventGroupHandle_t)(uintptr_t)stale_handles[4];
    if (xEventGroupSetBits(eg, 0x01) != 0) return 61;
    if (xEventGroupClearBits(eg, 0x01) != 0) return 62;
    if (xEventGroupGetBits(eg) != 0) return 63;

    // Stale Task (5)
    TaskHandle_t tsk = (TaskHandle_t)(uintptr_t)stale_handles[5];
    if (eTaskGetState(tsk) != eDeleted) return 71;
    if (uxTaskPriorityGet(tsk) != 0) return 72;

    // Stale GPTimer (6)
    gptimer_handle_t timer = (gptimer_handle_t)(uintptr_t)stale_handles[6];
    if (gptimer_start(timer) == ESP_OK) return 81;
    if (gptimer_stop(timer) == ESP_OK) return 82;
    uint64_t count_val = 0;
    if (gptimer_get_raw_count(timer, &count_val) == ESP_OK) return 83;

    // Stale NVS (7)
    nvs_handle_t nvs_h = (nvs_handle_t)stale_handles[7];
    int32_t val = 0;
    if (nvs_get_i32(nvs_h, "key", &val) == ESP_OK) return 91;
    if (nvs_set_i32(nvs_h, "key", 123) == ESP_OK) return 92;
    if (nvs_commit(nvs_h) == ESP_OK) return 93;

    return 0; // All stale handles successfully rejected!
}

EMSCRIPTEN_KEEPALIVE int esp_idf_wasm_phase4_verify_fresh_monotonic(uint32_t previous_sequence) {
    // 1. Fresh Queue
    QueueHandle_t fresh_q = xQueueCreate(2, sizeof(uint32_t));
    if (!fresh_q) return 101;
    uint32_t q_tok = (uint32_t)(uintptr_t)fresh_q;
    uint32_t q_seq = (q_tok >> 11);
    if (q_seq <= previous_sequence) return 102;

    uint32_t send_val = 0x1234;
    uint32_t recv_val = 0;
    if (xQueueSend(fresh_q, &send_val, 0) != pdPASS) return 103;
    if (xQueueReceive(fresh_q, &recv_val, 0) != pdPASS || recv_val != send_val) return 104;

    // 2. Fresh Semaphore
    SemaphoreHandle_t fresh_sem = xSemaphoreCreateBinary();
    if (!fresh_sem) return 201;
    uint32_t sem_tok = (uint32_t)(uintptr_t)fresh_sem;
    uint32_t sem_seq = (sem_tok >> 11);
    if (sem_seq <= previous_sequence || sem_seq <= q_seq) return 202;

    if (xSemaphoreGive(fresh_sem) != pdPASS) return 203;
    if (xSemaphoreTake(fresh_sem, 0) != pdPASS) return 204;

    // 3. Fresh EventGroup
    EventGroupHandle_t fresh_eg = xEventGroupCreate();
    if (!fresh_eg) return 301;
    uint32_t eg_tok = (uint32_t)(uintptr_t)fresh_eg;
    uint32_t eg_seq = (eg_tok >> 11);
    if (eg_seq <= previous_sequence || eg_seq <= sem_seq) return 302;

    if (xEventGroupSetBits(fresh_eg, 0x05) != 0x05) return 303;
    if ((xEventGroupGetBits(fresh_eg) & 0x05) != 0x05) return 304;

    // 4. Fresh GPTimer
    gptimer_handle_t fresh_timer = NULL;
    gptimer_config_t timer_cfg = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,
        .flags = { .intr_shared = 0 }
    };
    if (gptimer_new_timer(&timer_cfg, &fresh_timer) != ESP_OK || !fresh_timer) return 401;
    uint32_t timer_tok = (uint32_t)(uintptr_t)fresh_timer;
    uint32_t timer_seq = (timer_tok >> 11);
    if (timer_seq <= previous_sequence || timer_seq <= eg_seq) return 402;

    if (gptimer_enable(fresh_timer) != ESP_OK) return 4025;
    if (gptimer_start(fresh_timer) != ESP_OK) return 403;
    if (gptimer_stop(fresh_timer) != ESP_OK) return 404;
    if (gptimer_disable(fresh_timer) != ESP_OK) return 4045;

    // 5. Fresh NVS
    nvs_flash_init();
    nvs_handle_t fresh_nvs = 0;
    if (nvs_open("fresh_ns", NVS_READWRITE, &fresh_nvs) != ESP_OK || !fresh_nvs) return 501;
    uint32_t nvs_tok = (uint32_t)(uintptr_t)fresh_nvs;
    uint32_t nvs_seq = (nvs_tok >> 11);
    if (nvs_seq <= previous_sequence || nvs_seq <= timer_seq) return 502;

    if (nvs_set_i32(fresh_nvs, "fresh_k", 999) != ESP_OK) return 503;
    if (nvs_commit(fresh_nvs) != ESP_OK) return 504;
    int32_t nvs_readback = 0;
    if (nvs_get_i32(fresh_nvs, "fresh_k", &nvs_readback) != ESP_OK || nvs_readback != 999) return 505;

    // Cleanup
    vQueueDelete(fresh_q);
    vSemaphoreDelete(fresh_sem);
    vEventGroupDelete(fresh_eg);
    gptimer_del_timer(fresh_timer);
    nvs_close(fresh_nvs);

    return 0; // Success! All fresh handles are monotonic and functional.
}

EMSCRIPTEN_KEEPALIVE int esp_idf_wasm_phase4_verify_boundary_exhaustion(void) {
    constexpr uint32_t kMaxSequence = (1u << 21) - 1u; // 2,097,151

    // Set sequence base near limit: kMaxSequence - 1
    esp_sim_handle_set_sequence_base(kMaxSequence - 1);
    if (esp_sim_handle_get_sequence() != kMaxSequence - 1) return 1;

    // First allocation should succeed at kMaxSequence
    QueueHandle_t q1 = xQueueCreate(1, sizeof(uint32_t));
    if (!q1) return 2;
    uint32_t q1_seq = ((uint32_t)(uintptr_t)q1) >> 11;
    if (q1_seq != kMaxSequence) {
        vQueueDelete(q1);
        return 3;
    }
    if (esp_sim_handle_get_sequence() != kMaxSequence) {
        vQueueDelete(q1);
        return 4;
    }

    // Second allocation MUST fail (exhaustion limit reached)
    QueueHandle_t q2 = xQueueCreate(1, sizeof(uint32_t));
    if (q2 != NULL) {
        vQueueDelete(q1);
        vQueueDelete(q2);
        return 5; // Should have been rejected!
    }

    // Sequence must not wrap around
    if (esp_sim_handle_get_sequence() != kMaxSequence) {
        vQueueDelete(q1);
        return 6;
    }

    vQueueDelete(q1);
    return 0;
}

} // extern "C"
