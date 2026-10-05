/* SPDX-License-Identifier: LGPL-3.0-only */
#include <stdbool.h>
#include <assert.h>
#include "pal_log.h"
#include "esp_system.h"
#include "esp_idf_wink.h"
#include "wink_sim_scheduler.h"
#include "freertos_sync.h"
#include "driver/ledc.h"
#include "driver/i2c.h"
#include "driver/i2c_master.h"
#include "driver/uart.h"
#include "driver/gptimer.h"
#include "driver/spi_master.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include <stdlib.h>

/* Simulation reset hooks intentionally remain internal to the ESP-IDF facade. */
extern void esp_wifi_sim_reset(void);
extern void esp_mqtt_sim_reset(void);
extern void esp_http_client_sim_reset(void);
extern void sim_http_responder_reset(void);
extern void esp_nimble_sim_reset(void);
extern void esp_event_loop_sim_reset(void);
extern void esp_timer_sim_reset(void);
extern void sim_network_broker_reset(void);
extern void esp_adc_reset(void);
extern void esp_dac_reset(void);
extern void esp_task_wdt_sim_reset(void);

static bool s_esp_pending_reset = false;
static int s_esp_reset_reason = 4; /* SOFTWARE */

void esp_freertos_pools_reset(void) {
    esp_freertos_timers_sim_reset();
    esp_freertos_task_pool_reset();
    esp_freertos_queue_pool_reset();
    esp_freertos_sem_pool_reset();
    esp_freertos_event_pool_reset();
    esp_freertos_spinlock_reset(); /* Phase 2: reset critical-depth + spin counters */
}

void esp_peripherals_reset(void) {
    esp_gpio_reset();
    esp_heap_caps_reset();
    esp_ledc_reset();
    esp_i2c_legacy_reset();
    esp_i2c_master_reset();
    esp_uart_reset();
    esp_gptimer_reset();
    esp_spi_reset();
    esp_adc_reset();
    esp_dac_reset();
    esp_task_wdt_sim_reset();
    nvs_flash_deinit();
}

static void esp_sim_request_restart(void) {
    s_esp_pending_reset = true;
}

/* Test/host adapter for observing the reset request without violating the
 * noreturn contract of the public ESP-IDF esp_restart() API. */
void pal_wasm_target_request_reset(void) {
    pal_log_w("ESP_SYS", "simulation reset requested");
    esp_sim_request_restart();
}

void esp_restart(void) {
    pal_log_w("ESP_SYS", "esp_restart requested -> pending reset flag set");
    esp_sim_request_restart();
    /* Simulate noreturn: yield CPU so main loop can process the pending reset.
     * Guard with scheduler check in case called before scheduler starts. */
    if (sim_scheduler_current_id() != SIM_SCHED_NO_READY) {
        sim_scheduler_yield_context();
        for (;;) { sim_scheduler_yield_context(); } /* noreturn guard */
    }
    pal_log_e("ESP_SYS", "esp_restart called outside a scheduler fiber; aborting to honor noreturn");
    abort();
}

/* 供 targets/wasm 弱钩子查询的导出（命名不得带 wink_mcs51 前缀冲突） */
bool pal_wasm_target_has_pending_reset(void) { return s_esp_pending_reset; }
int pal_wasm_target_get_reset_reason(void) { return s_esp_reset_reason; }

/**
 * @brief Complete 8-stage Reset Causality DAG (ADR-0085 / PLAN-20260930-ESP-IDF-LIFECYCLE-RESET-CRITICAL):
 *
 *  Scheduler boundary check: assert(sim_scheduler_current_id() == SIM_SCHED_NO_READY)
 *       │
 *       ▼
 *  1. esp_http_client_sim_reset() / esp_mqtt_sim_reset()  [Disconnect upper connections, drain in-flight events]
 *       │
 *       ▼
 *  2. esp_wifi_sim_reset() ───(indirect side-effect)───► sim_network_broker_reset() [Tear down air interface & network broker]
 *       │
 *       ▼
 *  3. (void)esp_netif_init()                              [Re-initialize default network interface baseline]
 *       │
 *       ▼
 *  4. esp_nimble_sim_reset()                              [Tear down BLE stack & advertise handles]
 *       │
 *       ▼
 *  5. esp_event_loop_sim_reset()                          [Drain default event queue, unregister sys_evt fiber]
 *       │
 *       ▼
 *  6. sim_scheduler_reset(0)                              [Reset scheduler, evict all application task fibers]
 *       │
 *       ▼
 *  7. esp_peripherals_reset()                             [Release hardware PALs, stop GPTimers, flush NVS cache]
 *       │
 *       ▼
 *  8. esp_freertos_pools_reset()                          [Reset FreeRTOS kernel object pools & spinlocks]
 */
void pal_wasm_target_clear_pending_reset(void) {
    assert(sim_scheduler_current_id() == SIM_SCHED_NO_READY &&
           "ESP-IDF soft reset must be applied at a scheduler boundary");
    s_esp_pending_reset = false;

    /* Stage 1: Disconnect upper application protocol clients */
    esp_http_client_sim_reset();
    sim_http_responder_reset();
    esp_mqtt_sim_reset();

    /* Stage 2: Reset Wi-Fi subsystem */
    esp_wifi_sim_reset();

    /* Stage 2.5: Reset network broker state and callback subscriptions */
    sim_network_broker_reset();

    /* Stage 3: Re-arm fresh netif baseline */
    (void)esp_netif_init();

    /* Stage 4: Reset BLE/NimBLE subsystem */
    esp_nimble_sim_reset();

    /* Stage 5: Drain system event loop and stop event task */
    esp_event_loop_sim_reset();
    esp_timer_sim_reset();

    /* Stage 5.5: Drain FreeRTOS software timer command queue and cancel active timers */
    esp_freertos_timers_sim_reset();

    /* Stage 6: Evict all application fibers and reset scheduler state */
    sim_scheduler_reset(0);

    /* Stage 7: Reset peripheral drivers (GPIO, LEDC, I2C, SPI, UART, GPTimer, NVS) */
    esp_peripherals_reset();

    /* Stage 8: Reset FreeRTOS object pools (tasks, queues, semaphores, events, spinlocks) */
    esp_freertos_pools_reset();
}

esp_reset_reason_t esp_reset_reason(void) {
    return ESP_RST_SW;
}

const char *esp_get_idf_version(void) {
    return "v6.1-dev-winksim";
}
