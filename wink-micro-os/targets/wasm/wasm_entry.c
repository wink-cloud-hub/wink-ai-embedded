// SPDX-License-Identifier: Apache-2.0
/**
 * @file wasm_entry.c
 * @brief Wasm target entry point main().
 */
#ifdef EMSCRIPTEN
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif
#include "pal_hal.h"
#include "wink_app.h"
#include "wink_runtime.h"
#include "wink_sim_scheduler.h"

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
    return (int)pal_sim_scheduler_run(cb, 0, 1);
}

int main(void) {
    return pal_wasm_app_init();
}
