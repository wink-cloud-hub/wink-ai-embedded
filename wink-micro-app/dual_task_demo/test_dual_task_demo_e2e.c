// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_dual_task_demo_e2e.c
 * @brief Dual task demo host E2E runner.
 */
#include "wink_runtime.h"
#include "wink_trace.h"
#include "device_tree.h"
#include "host_test_ctrl.h"
#include <stdio.h>

extern const wink_app_callbacks_t *wink_app_get_callbacks(void);

#define E2E_PASS() do { extern int puts(const char*); puts("E2E PASS"); return 0; } while(0)
#define E2E_FAIL(msg) do { extern int puts(const char*); puts("E2E FAIL: " msg); return 1; } while(0)

int main(void)
{
    wink_trace_reset();
    sim_reset_time();
    const wink_app_callbacks_t *cb = wink_app_get_callbacks();

    {
        wink_status_t st = wink_runtime_run(cb, 100);
        (void)st;
    }

    extern volatile bool g_servo_was_180;
    if (!g_servo_was_180) {
        E2E_FAIL("Servo angle was never set to 180 degrees near obstacle");
    }

    if (wink_trace_count() != 0) {
        E2E_FAIL("faults recorded during e2e run");
    }

    E2E_PASS();
}
