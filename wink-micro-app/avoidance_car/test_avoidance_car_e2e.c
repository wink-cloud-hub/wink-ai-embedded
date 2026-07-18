/**
 * @file test_avoidance_car_e2e.c
 * @brief Host e2e: distance events → servo angle (ADR-0033 L1 path).
 *
 * Drives soft_timer like the unit test (re-arm echo each tick). Full
 * wink_runtime_run is used once to prove init + on_event wiring.
 */
#include "wink_runtime.h"
#include "wink_trace.h"
#include "wink_soft_timer.h"
#include "wink_event.h"
#include "wink_status.h"
#include "sensor/wink_ultrasonic_distance_events.h"
#include "device_tree.h"
#include "host_test_ctrl.h"
#include "pal_osal.h"

extern const wink_app_callbacks_t *wink_app_get_callbacks(void);
extern void host_sim_advance_to(uint64_t us);

#define E2E_PASS()      do { extern int puts(const char*); puts("E2E PASS"); return 0; } while(0)
#define E2E_FAIL(msg)   do { extern int puts(const char*); puts("E2E FAIL: " msg); return 1; } while(0)

static void arm_echo(uint32_t high_us)
{
    sim_set_echo_pin(front_radar.config.echo_pin);
    sim_set_echo_timing(100, high_us);
}

static void tick_once(void)
{
    host_sim_advance_to(pal_os_get_us() + 10000u);
    wink_soft_timer_dispatch();
}

int main(void)
{
    wink_trace_reset();
    const wink_app_callbacks_t *cb = wink_app_get_callbacks();

    /* Far (~100 cm): one runtime_run — expect stay/return to 90°. */
    sim_reset_time();
    arm_echo(5882);
    {
        wink_status_t s = wink_runtime_run(cb, 12);
        if (wink_status_is_error(s)) {
            E2E_FAIL("runtime_run far failed");
        }
    }
    if (neck_servo.current_angle != 90.0f) {
        E2E_FAIL("servo not 90 when clear");
    }

    wink_ultrasonic_disable_distance_events(&front_radar);
    wink_ultrasonic_distance_events_reset();
    wink_device_tree_deinit();

    /*
     * Near (~10 cm): drive the same callbacks without a second full
     * runtime_run (avoids soft-timer/pool re-init races). Invoke init,
     * then tick with re-armed echo and drain on_event manually.
     */
    sim_reset_time();
    WINK_IGNORE_RESULT(wink_soft_timer_init());
    WINK_IGNORE_RESULT(wink_event_queue_init(32));
    arm_echo(588);
    {
        wink_status_t st = cb->init_status();
        if (wink_status_is_error(st)) {
            E2E_FAIL("init_status near failed");
        }
    }

    for (int i = 0; i < 12; i++) {
        arm_echo(588);
        tick_once();
        if (cb->on_event != NULL) {
            wink_event_t ev;
            while (wink_event_pend(&ev, 0) == WINK_OK) {
                cb->on_event(&ev);
            }
        }
    }

    if (neck_servo.current_angle != 180.0f) {
        E2E_FAIL("servo not 180 on near obstacle");
    }

    E2E_PASS();
}
