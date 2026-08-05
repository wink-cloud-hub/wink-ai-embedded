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

extern const wink_app_callbacks_t *wink_app_get_callbacks(void);

int main(void) {
    const wink_app_callbacks_t *cb = wink_app_get_callbacks();
    return (int)wink_runtime_run(cb, 0);
}
