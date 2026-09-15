// SPDX-License-Identifier: Apache-2.0
/**
 * @file wasm_entry.c
 * @brief Wasm target entry point main().
 */
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif
#include "pal_hal.h"
#include "wink_app.h"
#include "wink_runtime.h"
#include "wink_sim_scheduler.h"
#include "wasm_bridge.h"

extern const wink_app_callbacks_t *wink_app_get_callbacks(void);

static bool s_app_inited = false;

EMSCRIPTEN_KEEPALIVE int pal_wasm_app_init(void) {
    const wink_app_callbacks_t *cb = wink_app_get_callbacks();
    if (!s_app_inited) {
        s_app_inited = true;
        return (int)wink_runtime_run(cb, 1);
    }
    return WINK_OK;
}

EMSCRIPTEN_KEEPALIVE int pal_wasm_app_tick(void) {
    const wink_app_callbacks_t *cb = wink_app_get_callbacks();
    if (!s_app_inited) {
        s_app_inited = true;
        return (int)wink_runtime_run(cb, 1);
    }
    return (int)pal_sim_scheduler_run(cb, SIM_SCHED_NO_READY, 1);
}

EMSCRIPTEN_KEEPALIVE void pal_wasm_reset_app_state(void) {
    s_app_inited = false;
    pal_wasm_sim_reset_all_devices();
}

// ADR-0082 / Task 2: Exported reset query interface for Unisim TS Runner
__attribute__((weak)) bool pal_wasm_target_has_pending_reset(void) { return false; }
__attribute__((weak)) int pal_wasm_target_get_reset_reason(void) { return 0; }
__attribute__((weak)) void pal_wasm_target_clear_pending_reset(void) {}

EMSCRIPTEN_KEEPALIVE int pal_wasm_has_pending_reset(void) {
    return pal_wasm_target_has_pending_reset() ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE int pal_wasm_get_reset_reason(void) {
    return pal_wasm_target_get_reset_reason();
}

EMSCRIPTEN_KEEPALIVE void pal_wasm_clear_pending_reset(void) {
    pal_wasm_target_clear_pending_reset();
}

int main(void) {
    return pal_wasm_app_init();
}

