/* SPDX-License-Identifier: LGPL-3.0-only */
#include "wink_app.h"
#include "pal_log.h"
#include "wink_sim_scheduler.h"
#include "freertos_sync.h"
#include <stddef.h>
#include <stdatomic.h>

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

/* ── Phase 2 Task 4.1: Idempotent lazy cold-start (ISSUE-13) ───────────────
 * Uses atomic CAS so that the very first caller — even a C++ global
 * constructor running before app_main — safely initialises the FreeRTOS pools.
 * Subsequent callers see s_framework_inited == true and skip immediately.
 *
 * Thread-safety: atomic_bool degrades to a plain read/write on wasm32
 * (single-threaded), so there is zero overhead in the Wasm target.
 * On host POSIX the CAS prevents TOCTOU races (P1 fix from plan review).   */
static atomic_bool s_framework_inited = ATOMIC_VAR_INIT(false);

void esp_idf_ensure_framework_ready(void) {
    bool expected = false;
    /* Only the first thread that succeeds in false→true runs the init body */
    if (atomic_compare_exchange_strong(&s_framework_inited, &expected, true)) {
        esp_freertos_pools_reset();
        pal_log_i("ESP_IDF",
                  "Framework lazily auto-initialized (C++ static constructor safe)");
    }
    /* All subsequent callers: CAS failed (expected became true) → instant return */
}

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

    /* Mark framework as ready via atomic store so ensure_framework_ready()
     * called from C++ constructors after this point skips reinitialisation. */
    atomic_store(&s_framework_inited, true);
    esp_freertos_pools_reset();

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
