// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_devkitc_smoke_e2e.c
 * @brief DevKitC smoke test host E2E runner.
 */
#include "wink_runtime.h"
#include "wink_trace.h"
#include "device_tree.h"
#include "host_test_ctrl.h"

extern const wink_app_callbacks_t *wink_app_get_callbacks(void);

#define E2E_PASS() do { extern int puts(const char*); puts("E2E PASS"); return 0; } while(0)
#define E2E_FAIL(msg) do { extern int puts(const char*); puts("E2E FAIL: " msg); return 1; } while(0)

int main(void)
{
    wink_trace_reset();
    sim_reset_time();
    const wink_app_callbacks_t *cb = wink_app_get_callbacks();

    {
        wink_status_t st = wink_runtime_run(cb, 5);
        (void)st;
    }

    if (!board_led.is_on) {
        E2E_FAIL("LED not on after ticks (led blink)");
    }

    if (sim_last_pwm_duty(1) < 49.0f || sim_last_pwm_duty(1) > 51.0f) {
        E2E_FAIL("PWM ch1 duty not 50%");
    }

    if (wink_trace_count() != 0) {
        E2E_FAIL("faults recorded during run");
    }

    for (int i = 0; i < 5; i++) {
        wink_device_tree_deinit();
        wink_status_t st = wink_device_tree_init();
        if (wink_status_is_error(st)) {
            E2E_FAIL("S11: failed to reinitialize device tree during deinit loop");
        }
    }

    E2E_PASS();
}
