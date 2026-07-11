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

    /* 跑 100 tick，以执行双协程并发调度 */
    {
        wink_status_t st = wink_runtime_run(cb, 100);
        (void)st;
    }

    /* 验证：随着 mock_dist 递减到 20cm 以下，舵机角度曾被设定为 180 度 */
    extern volatile bool g_servo_was_180;
    if (!g_servo_was_180) {
        E2E_FAIL("Servo angle was never set to 180 degrees near obstacle");
    }

    /* 验证：无故障发生 */
    if (wink_trace_count() != 0) {
        E2E_FAIL("faults recorded during e2e run");
    }

    E2E_PASS();
}
