/* SPDX-License-Identifier: LGPL-3.0-only */
#include "wink_app.h"
#include "pal_log.h"
#include "wink_sim_scheduler.h"
#include "freertos_sync.h"
#include <stddef.h>

#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak)) void app_main(void) {}
#elif defined(_MSC_VER)
void __cdecl app_main_default(void) {}
#pragma comment(linker, "/alternatename:app_main=app_main_default")
void app_main(void);
#else
void app_main(void);
#endif

static uint32_t s_app_main_slot = SIM_SCHED_NO_READY;

static void app_main_trampoline(void* arg) {
    (void)arg;
    app_main();
    uint32_t self = sim_scheduler_current_id();
    if (self != SIM_SCHED_NO_READY) {
        sim_scheduler_mark_zombie(self);
        sim_scheduler_yield_context();
        for (;;) {}
    }
}

static void esp_idf_framework_init(void) {
    pal_log_i("ESP_IDF", "Framework initialized in simulation mode");
    uint32_t slot = UINT32_MAX;
    wink_status_t st = sim_scheduler_register(
        app_main_trampoline,
        NULL,
        "app_main",
        1,
        0,
        32 * 1024,
        &slot
    );
    if (st == WINK_OK) {
        s_app_main_slot = slot;
        esp_freertos_register_task_slot(slot, 1, "app_main");
    } else {
        pal_log_e("ESP_IDF", "Failed to register app_main fiber");
    }
}

static void esp_idf_app_loop(void) {
    /* app_main runs as an independent fiber; app_loop is no-op (ADR-0070) */
}

uint32_t esp_idf_get_app_main_task_id(void) {
    return s_app_main_slot;
}

static const wink_app_callbacks_t s_esp_idf_callbacks = {
    esp_idf_framework_init,
    esp_idf_app_loop,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

const wink_app_callbacks_t* wink_app_get_callbacks(void) {
    return &s_esp_idf_callbacks;
}
