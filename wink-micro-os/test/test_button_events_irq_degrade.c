/**
 * @file test_button_events_irq_degrade.c
 * @brief S4 host test — GPIO_IRQ drive requested on a non-ESP32 target
 *        must transparently degrade to SOFT_POLL, raise the
 *        `WINK_WARN_BUTTON_IRQ_DEGRADED` warn code, and still deliver
 *        `WINK_EVENT_BUTTON_PRESSED` via the periodic fallback.
 *
 * When compiled with `-DWINK_BUTTON_IRQ_STRICT=1` (see companion CMake
 * target `test_button_events_irq_strict`), the same misconfiguration
 * must instead hard-fail with `WINK_ERR_UNSUPPORTED` so codegen contract
 * violations can be gated at build time.
 *
 * Copyright (c) 2026 Wink-AI.
 */
#define LOG_TAG "tst_btn_ev_irq"

#include "unity.h"
#include "wink_button_events.h"
#include "wink_soft_timer.h"
#include "wink_tasks.h"
#include "wink_event.h"
#include "wink_status.h"
#include "wink_fault.h"
#include "wink_trace.h"
#include "dal_button.h"
#include "pal_osal.h"
#include "pal_resource.h"
#include "pal_log.h"
#include "host_test_ctrl.h"

#include <string.h>

/* Host sim clock advance (declared in targets/host/pal_osal_host.c). */
extern void host_sim_advance_to(uint64_t us);

static dal_button_t s_btn;
static const uint16_t BTN_PIN = 0;

/* Advance virtual time by one 10 ms runtime tick and dispatch soft_timers. */
#ifndef WINK_BUTTON_IRQ_STRICT
static void tick_once(void) {
    uint64_t now = pal_os_get_us();
    host_sim_advance_to(now + 10000u);   /* +10 ms */
    wink_soft_timer_dispatch();
}

static void tick_n(int n) {
    for (int i = 0; i < n; i++) { tick_once(); }
}

static void press_button(void)   { sim_set_gpio_ideal(BTN_PIN, false); } /* active_low LOW=press */
#endif
static void release_button(void) { sim_set_gpio_ideal(BTN_PIN, true);  } /* active_low HIGH=idle */

void setUp(void) {
    /* Drain any lingering slot before test isolation reset. */
    wink_button_events_stop(&s_btn);

    pal_resource_reset();
    sim_clear_gpio_ideal();
    sim_reset_time();

    WINK_IGNORE_RESULT(wink_soft_timer_init());
    WINK_IGNORE_RESULT(wink_event_queue_init(16));
    wink_trace_reset();

    memset(&s_btn, 0, sizeof(s_btn));
    release_button();  /* first sim_set_gpio_ideal = power-on level, no bounce */

    const dal_button_config_t cfg = {
        .owner = "test_btn_ev_irq",
        .pin = BTN_PIN,
        .active_low = true,
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&s_btn, &cfg));
}

void tearDown(void) {
    wink_button_events_stop(&s_btn);
    WINK_IGNORE_RESULT(dal_button_deinit(&s_btn));
    wink_event_queue_deinit();
    sim_clear_gpio_ideal();
}

/* Contract precondition: host build must report the IRQ backend as
 * unsupported so the degrade branch is actually exercised below. */
void test_irq_unsupported_on_host(void) {
    TEST_ASSERT_FALSE(wink_button_events_irq_supported());
}

#ifndef WINK_BUTTON_IRQ_STRICT
/* Permissive (default) mode: GPIO_IRQ + host → degrade + warn + events. */
void test_gpio_irq_degrades_to_soft_poll_on_host(void) {
    const uint32_t warn_before = wink_warn_count();

    const wink_button_event_config_t cfg = {
        .drive         = WINK_BUTTON_DRIVE_GPIO_IRQ,
        .auto_poll_ms  = 0,      /* exercise the default poll_ms formula */
        .debounce_ms   = 20,
        .wake_from_sleep = false,
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_button_events_start(&s_btn, &cfg));

    /* Warn recorded exactly once by the degrade branch. */
    TEST_ASSERT_EQUAL_UINT32(warn_before + 1u, wink_warn_count());

    /* Settle in released state, then simulate a press and let the soft
     * poll fallback drive the debounce FSM until the PRESS event lands
     * on the event queue. Effective poll period = max(debounce_ms, 10) =
     * 20 ms → one poll every 2 ticks. Give generous headroom. */
    tick_n(DAL_BUTTON_DEBOUNCE_THRESHOLD + 4);

    /* No PRESS while idle. */
    wink_event_t ev;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_EMPTY, wink_event_pend(&ev, 0));

    press_button();
    tick_n(20);   /* 200 ms virtual — well above 20 ms poll * DEBOUNCE_THRESHOLD. */

    memset(&ev, 0, sizeof(ev));
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_event_pend(&ev, 0));
    TEST_ASSERT_EQUAL_UINT32(WINK_EVENT_BUTTON_PRESSED, ev.type);
    TEST_ASSERT_EQUAL_PTR(&s_btn, ev.device);
}
#endif /* !WINK_BUTTON_IRQ_STRICT */

#ifdef WINK_BUTTON_IRQ_STRICT
/* Strict mode: GPIO_IRQ + host → hard-fail (no degrade, no warn). */
void test_gpio_irq_strict_returns_unsupported(void) {
    const uint32_t warn_before = wink_warn_count();

    const wink_button_event_config_t cfg = {
        .drive         = WINK_BUTTON_DRIVE_GPIO_IRQ,
        .auto_poll_ms  = 0,
        .debounce_ms   = 20,
        .wake_from_sleep = false,
    };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_UNSUPPORTED,
        wink_button_events_start(&s_btn, &cfg));

    /* No warn should be raised on the hard-fail path. */
    TEST_ASSERT_EQUAL_UINT32(warn_before, wink_warn_count());
}
#endif /* WINK_BUTTON_IRQ_STRICT */

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_irq_unsupported_on_host);
#ifndef WINK_BUTTON_IRQ_STRICT
    RUN_TEST(test_gpio_irq_degrades_to_soft_poll_on_host);
#else
    RUN_TEST(test_gpio_irq_strict_returns_unsupported);
#endif
    return UNITY_END();
}
