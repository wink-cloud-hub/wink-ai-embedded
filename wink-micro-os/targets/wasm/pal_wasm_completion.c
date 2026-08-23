// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_wasm_completion.c
 * @brief Unified asynchronous completion pull-model scheduler for Wasm simulation.
 */
#include "pal_wasm_completion.h"
#include "pal_wasm_common.h"
#include "pal_osal.h"
#include <string.h>

typedef struct {
    bool                     in_use;
    uint64_t                 deadline_us;
    pal_wasm_completion_cb_t cb;
    void                    *arg;
    wink_status_t            result;
} wasm_completion_entry_t;

static wasm_completion_entry_t s_completions[PAL_WASM_MAX_PENDING_COMPLETIONS];
static uint32_t s_pending_count = 0;

wink_status_t pal_wasm_schedule_complete_with_result(uint32_t delta_us,
                                                    pal_wasm_completion_cb_t cb,
                                                    void *arg,
                                                    wink_status_t result) {
    if (cb == NULL) return WINK_ERR_INVALID_ARG;

    for (int i = 0; i < PAL_WASM_MAX_PENDING_COMPLETIONS; i++) {
        if (!s_completions[i].in_use) {
            uint64_t now = pal_os_get_us();
            s_completions[i].in_use = true;
            s_completions[i].deadline_us = now + (uint64_t)delta_us;
            s_completions[i].cb = cb;
            s_completions[i].arg = arg;
            s_completions[i].result = result;
            s_pending_count++;
            return WINK_OK;
        }
    }
    return WINK_ERR_RESOURCE_EXHAUSTED;
}

wink_status_t pal_wasm_schedule_complete_us(uint32_t delta_us,
                                            pal_wasm_completion_cb_t cb,
                                            void *arg) {
    return pal_wasm_schedule_complete_with_result(delta_us, cb, arg, WINK_OK);
}

void pal_wasm_drain_completions(void) {
    if (s_pending_count == 0) return;
    uint64_t now = pal_os_get_us();

    for (int i = 0; i < PAL_WASM_MAX_PENDING_COMPLETIONS; i++) {
        if (s_completions[i].in_use && s_completions[i].deadline_us <= now) {
            pal_wasm_completion_cb_t cb = s_completions[i].cb;
            void *arg = s_completions[i].arg;
            wink_status_t res = s_completions[i].result;

            s_completions[i].in_use = false;
            if (s_pending_count > 0) s_pending_count--;

            if (cb != NULL) {
                cb(arg, res);
            }
        }
    }
}

void pal_wasm_reset_completions(void) {
    memset(s_completions, 0, sizeof(s_completions));
    s_pending_count = 0;
}

uint32_t pal_wasm_get_pending_completions_count(void) {
    return s_pending_count;
}
