// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_app_e2e.c
 * @brief PAL->DAL->runtime->App end-to-end integration test.
 */
#include "wink_runtime.h"
#include "wink_trace.h"
#include "dal_rc_servo.h"
#include "device_tree.h"
#include "host_test_ctrl.h"

extern const wink_app_callbacks_t *wink_app_get_callbacks(void);

#define E2E_PASS()      do { extern int puts(const char*); puts("E2E PASS"); return 0; } while(0)
#define E2E_FAIL(msg)   do { extern int puts(const char*); puts("E2E FAIL: " msg); return 1; } while(0)

int main(void) {
    wink_trace_reset();
    const wink_app_callbacks_t *cb = wink_app_get_callbacks();

    sim_reset_time();
    sim_set_echo_pin(front_radar.config.echo_pin);
    sim_set_echo_timing(100, 5882);
    {
        wink_status_t s = wink_runtime_run(cb, 1);
        (void)s;
    }
    if (neck_servo.current_angle != 90.0f) E2E_FAIL("servo not 90 when clear");

    sim_reset_time();
    sim_set_echo_pin(front_radar.config.echo_pin);
    sim_set_echo_timing(100, 588);
    {
        wink_status_t s = wink_runtime_run(cb, 1);
        (void)s;
    }
    if (neck_servo.current_angle != 180.0f) E2E_FAIL("servo not 180 on near obstacle");

    E2E_PASS();
}
