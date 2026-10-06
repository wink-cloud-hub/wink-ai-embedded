/* SPDX-License-Identifier: GPL-3.0-only */
/* Test adapter: public SDK/PAL APIs and the ISR hook only. */
#include "esp_task_wdt.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pal_osal.h"
#include <emscripten.h>
#include <stdio.h>
#include <stdint.h>

#define PROBE_USERS (2u)
#define PROBE_MESSAGE_BYTES (512u)
#define PROBE_FOREIGN_HANDLE (0xdeadbeefu)
static esp_task_wdt_user_handle_t s_users[PROBE_USERS];
static esp_task_wdt_user_handle_t s_old_user;
static unsigned s_isr_count;
static bool s_hook_in_isr;
static uint64_t s_last_isr_us;
static int s_cpus;
static char s_message[PROBE_MESSAGE_BYTES];
static size_t s_message_used;

void app_main(void) {
    printf("TWDT SDK probe ready\n");
}

void esp_task_wdt_isr_user_handler(void) {
    ++s_isr_count;
    s_hook_in_isr = pal_os_in_isr();
    s_last_isr_us = pal_os_get_us();
}

EMSCRIPTEN_KEEPALIVE int twdt_probe_init(unsigned timeout_ms, int panic) {
    s_old_user = s_users[0];
    s_users[0] = NULL;
    s_users[1] = NULL;
    const esp_task_wdt_config_t config = {
        .timeout_ms = timeout_ms, .idle_core_mask = 0,
        .trigger_panic = panic != 0};
    return esp_task_wdt_init(&config);
}
EMSCRIPTEN_KEEPALIVE int twdt_probe_add(int index) {
    if (index < 0 || index >= (int)PROBE_USERS) return ESP_ERR_INVALID_ARG;
    return esp_task_wdt_add_user(index == 0 ? "probe_a" : "probe_b",
                                 &s_users[index]);
}
EMSCRIPTEN_KEEPALIVE int twdt_probe_feed(int index) {
    if (index < 0 || index >= (int)PROBE_USERS) return ESP_ERR_INVALID_ARG;
    return esp_task_wdt_reset_user(s_users[index]);
}
EMSCRIPTEN_KEEPALIVE int twdt_probe_delete(int index) {
    if (index < 0 || index >= (int)PROBE_USERS) return ESP_ERR_INVALID_ARG;
    return esp_task_wdt_delete_user(s_users[index]);
}
EMSCRIPTEN_KEEPALIVE int twdt_probe_feed_old(void) {
    return esp_task_wdt_reset_user(s_old_user);
}
EMSCRIPTEN_KEEPALIVE int twdt_probe_feed_foreign(void) {
    return esp_task_wdt_reset_user(
        (esp_task_wdt_user_handle_t)(uintptr_t)PROBE_FOREIGN_HANDLE);
}
EMSCRIPTEN_KEEPALIVE int twdt_probe_deinit(void) { return esp_task_wdt_deinit(); }
EMSCRIPTEN_KEEPALIVE unsigned twdt_probe_isr_count(void) {
    return s_isr_count;
}
EMSCRIPTEN_KEEPALIVE int twdt_probe_hook_in_isr(void) {
    return s_hook_in_isr;
}
EMSCRIPTEN_KEEPALIVE uint64_t twdt_probe_last_isr_us(void) {
    return s_last_isr_us;
}
EMSCRIPTEN_KEEPALIVE int twdt_probe_reset_reason(void) {
    return esp_reset_reason();
}

static void collect_message(void *opaque, const char *text) {
    (void)opaque;
    size_t remaining = sizeof(s_message) - s_message_used;
    int count = snprintf(s_message + s_message_used, remaining, "%s\n", text);
    if (count > 0 && (size_t)count < remaining) {
        s_message_used += (size_t)count;
    }
}
EMSCRIPTEN_KEEPALIVE int twdt_probe_diagnose(void) {
    s_message[0] = '\0';
    s_message_used = 0;
    s_cpus = 0;
    return esp_task_wdt_print_triggered_tasks(collect_message, NULL, &s_cpus);
}
EMSCRIPTEN_KEEPALIVE int twdt_probe_cpus(void) { return s_cpus; }
EMSCRIPTEN_KEEPALIVE const char *twdt_probe_message(void) {
    return s_message;
}
