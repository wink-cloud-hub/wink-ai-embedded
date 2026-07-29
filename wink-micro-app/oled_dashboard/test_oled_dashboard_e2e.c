/**
 * @file test_oled_dashboard_e2e.c
 * @brief OLED Dashboard host e2e：pull-up idle → 注入按下 → 验证 LED + OLED。
 */
#include "wink_runtime.h"
#include "wink_trace.h"
#include "wink_soft_timer.h"
#include "device_tree.h"
#include "host_test_ctrl.h"
#include "pal_osal.h"
#include "wink_event.h"

extern const wink_app_callbacks_t *wink_app_get_callbacks(void);
extern void host_sim_advance_to(uint64_t us);

#define E2E_PASS() do { extern int puts(const char*); puts("E2E PASS"); return 0; } while(0)
#define E2E_FAIL(msg) do { extern int puts(const char*); puts("E2E FAIL: " msg); return 1; } while(0)

static void advance_runtime_ticks(const wink_app_callbacks_t *cb, int n)
{
    for (int i = 0; i < n; i++) {
        uint64_t now = pal_os_get_us();
        host_sim_advance_to(now + 10000u);
        wink_soft_timer_dispatch();
        if (cb->loop) {
            cb->loop();
        }
        if (cb->on_event) {
            wink_event_t event;
            while (wink_event_pend(&event, 0) == WINK_OK) {
                cb->on_event(&event);
            }
        }
    }
}

int main(void) {
    wink_trace_reset();
    sim_reset_time();
    const wink_app_callbacks_t *cb = wink_app_get_callbacks();

    {
        wink_status_t s = wink_runtime_run(cb, 5);
        (void)s;
    }

    /* Re-initialize event queue for the manual ticking phase */
    WINK_IGNORE_RESULT(wink_event_queue_init(WINK_EVENT_QUEUE_DEFAULT_CAPACITY));

    if (status_led.is_on) {
        E2E_FAIL("LED on while button released (pull-up idle)");
    }

    int nonzero = 0;
    for (int i = 0; i < MONO_OLED_FB_SIZE; i++) {
        if (status_oled.framebuffer[i] != 0) {
            nonzero++;
        }
    }
    if (nonzero != 0) {
        E2E_FAIL("framebuffer non-empty while button released");
    }

    sim_set_gpio_ideal(user_button.config.pin, false);
    advance_runtime_ticks(cb, 8);

    if (!status_led.is_on) {
        E2E_FAIL("LED not on after button press");
    }

    if (sim_i2c_transfer_count() == 0) {
        E2E_FAIL("no I2C transfers (OLED not flushed)");
    }
    if (sim_last_i2c_addr() != 0x3C) {
        E2E_FAIL("I2C addr mismatch");
    }

    nonzero = 0;
    for (int i = 0; i < MONO_OLED_FB_SIZE; i++) {
        if (status_oled.framebuffer[i] != 0) {
            nonzero++;
        }
    }
    if (nonzero == 0) {
        E2E_FAIL("framebuffer empty after button press");
    }

    if (wink_trace_count() != 0) {
        E2E_FAIL("faults recorded during run");
    }

    E2E_PASS();
}
