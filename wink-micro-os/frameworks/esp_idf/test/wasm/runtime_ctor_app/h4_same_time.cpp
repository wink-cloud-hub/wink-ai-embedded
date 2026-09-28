/* SPDX-License-Identifier: GPL-3.0-only */
#include <cstdint>
#include <cstring>
#include <emscripten/emscripten.h>

#include "freertos/queue.h"
#include "freertos/task.h"
#include "pal_irq.h"

extern "C" void pal_wasm_advance_virtual_clock(uint64_t us);

namespace {
constexpr uint32_t kIrq = 7;
constexpr uint32_t kValue = 0xA53C;
QueueHandle_t queue_handle = nullptr;
TaskHandle_t reader_handle = nullptr;
TaskHandle_t delayed_handle = nullptr;
char trace[4] = {};
uint32_t trace_count = 0;
uint32_t reader_runs = 0;
uint32_t irq_runs = 0;
uint32_t delayed_runs = 0;
uint32_t received = 0;
TickType_t start_tick = 0;
TickType_t irq_tick = 0;
TickType_t reader_tick = 0;
TickType_t delayed_tick = 0;
BaseType_t receive_result = -1;
BaseType_t isr_send_result = -1;
BaseType_t isr_woken = pdFALSE;

void reader(void *) {
    ++reader_runs;
    receive_result = xQueueReceive(queue_handle, &received, 1);
    reader_tick = xTaskGetTickCount();
    if (trace_count < sizeof(trace)) trace[trace_count++] = 'T';
    vTaskDelete(nullptr);
}

void delayed(void *) {
    vTaskDelay(1);
    ++delayed_runs;
    delayed_tick = xTaskGetTickCount();
    if (trace_count < sizeof(trace)) trace[trace_count++] = 'D';
    vTaskDelete(nullptr);
}

void irq(void *) {
    ++irq_runs;
    irq_tick = xTaskGetTickCountFromISR();
    if (trace_count < sizeof(trace)) trace[trace_count++] = 'I';
    isr_send_result = xQueueSendFromISR(queue_handle, &kValue, &isr_woken);
}
}

extern "C" EMSCRIPTEN_KEEPALIVE int esp_idf_wasm_h4_begin(void) {
    std::memset(trace, 0, sizeof(trace));
    trace_count = reader_runs = irq_runs = delayed_runs = received = 0;
    receive_result = isr_send_result = -1;
    isr_woken = pdFALSE;
    start_tick = xTaskGetTickCount();
    queue_handle = xQueueCreate(1, sizeof(uint32_t));
    if (queue_handle == nullptr) return 1;
    if (xTaskCreate(reader, "h4_reader", 32768, nullptr, 5, &reader_handle) != pdPASS) return 2;
    if (xTaskCreate(delayed, "h4_delayed", 32768, nullptr, 5, &delayed_handle) != pdPASS) return 3;
    if (pal_irq_enable(kIrq, PAL_IRQ_PRIO_NORMAL, irq, nullptr) != WINK_OK) return 4;
    return 0;
}

extern "C" EMSCRIPTEN_KEEPALIVE void esp_idf_wasm_h4_at_deadline(void) {
    pal_wasm_advance_virtual_clock(static_cast<uint64_t>(portTICK_PERIOD_MS) * 1000);
    pal_irq_set_pending(kIrq);
}

extern "C" EMSCRIPTEN_KEEPALIVE int esp_idf_wasm_h4_verify(int stage) {
    if (stage == 0) {
        return reader_runs == 1 && irq_runs == 0 && delayed_runs == 0 && trace_count == 0 &&
               eTaskGetState(reader_handle) == eBlocked &&
               eTaskGetState(delayed_handle) == eBlocked &&
               uxQueueMessagesWaiting(queue_handle) == 0 ? 0 : 1;
    }
    const bool passed = reader_runs == 1 && irq_runs == 1 && delayed_runs == 1 &&
                        trace_count == 3 && trace[0] == 'I' && trace[1] == 'T' && trace[2] == 'D' &&
                        isr_send_result == pdPASS && isr_woken == pdTRUE &&
                        receive_result == pdPASS && received == kValue &&
                        irq_tick == start_tick + 1 && reader_tick == irq_tick &&
                        delayed_tick == irq_tick &&
                        uxQueueMessagesWaiting(queue_handle) == 0 &&
                        eTaskGetState(reader_handle) == eDeleted &&
                        eTaskGetState(delayed_handle) == eDeleted;
    (void)pal_irq_disable(kIrq);
    vQueueDelete(queue_handle);
    return passed ? 0 : 2;
}
