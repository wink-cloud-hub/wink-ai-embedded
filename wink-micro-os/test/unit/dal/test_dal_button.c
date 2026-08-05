// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_dal_button.c
 * @brief DAL button driver unit tests.
 */
#include "unity.h"
#include "wink_status.h"
#include "dal_button.h"
#include "pal_resource.h"
#include "pal_hal.h"
#include "pal_osal.h"
#include "host_test_ctrl.h"
#include "pal_irq.h"
#include <string.h>

static const char *const OWNER = "test_dal_button";

void setUp(void) {
    pal_resource_reset();
    sim_clear_gpio_ideal();
    pal_host_reset_isr_stats();
    sim_reset_time();
}
void tearDown(void) {
    sim_clear_gpio_ideal();
}

extern void host_sim_advance_to(uint64_t us);

static wink_status_t poll_n_ticks(dal_button_t *dev, int n) {
    wink_status_t s = WINK_OK;
    for (int i = 0; i < n; i++) {
        uint64_t now = pal_os_get_us();
        host_sim_advance_to(now + 10000u);
        s = dal_button_poll(dev);
    }
    return s;
}

static wink_status_t poll_n(dal_button_t *dev, int n) {
    return poll_n_ticks(dev, n);
}

static void set_btn_pin(uint16_t pin, bool pressed, bool active_low) {
    bool level = active_low ? !pressed : pressed;
    sim_set_gpio_ideal(pin, level);
}

void test_init_null_returns_invalid_arg(void) {
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 10, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_button_init(NULL, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_button_init(NULL, NULL));
}

void test_read_before_init_returns_not_initialized(void) {
    dal_button_t dev = {0};
    bool out = false;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_button_poll(&dev));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_button_is_pressed(&dev, &out));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_button_was_pressed(&dev, &out));
}

void test_read_null_returns_invalid_arg(void) {
    bool out = false;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_button_poll(NULL));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_button_is_pressed(NULL, &out));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_button_was_pressed(NULL, &out));
}

void test_is_pressed_null_out_returns_invalid_arg(void) {
    dal_button_t dev = {0};
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 10, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_button_is_pressed(&dev, NULL));
}

void test_active_low_debounce_to_pressed(void) {
    dal_button_t dev = {0};
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 10, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));

    for (int i = 0; i < DAL_BUTTON_DEBOUNCE_THRESHOLD; i++) {
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_poll(&dev));
    }

    bool pressed = true;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_is_pressed(&dev, &pressed));
    TEST_ASSERT_FALSE(pressed);

    set_btn_pin(10, true, true);
    for (int i = 0; i < DAL_BUTTON_DEBOUNCE_THRESHOLD; i++) {
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_poll(&dev));
    }

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_is_pressed(&dev, &pressed));
    TEST_ASSERT_TRUE(pressed);
}

void test_active_high_stays_unpressed(void) {
    dal_button_t dev = {0};
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 11, .active_low = false };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));

    set_btn_pin(11, false, false);
    for (int i = 0; i < DAL_BUTTON_DEBOUNCE_THRESHOLD; i++) {
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_poll(&dev));
    }

    bool pressed = true;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_is_pressed(&dev, &pressed));
    TEST_ASSERT_FALSE(pressed);
}

void test_was_pressed_edge_once(void) {
    dal_button_t dev = {0};
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 12, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));

    bool ev = true;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_was_pressed(&dev, &ev));
    TEST_ASSERT_FALSE(ev);

    set_btn_pin(12, true, true);
    for (int i = 0; i < DAL_BUTTON_DEBOUNCE_THRESHOLD; i++) {
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_poll(&dev));
    }

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_was_pressed(&dev, &ev));
    TEST_ASSERT_TRUE(ev);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_was_pressed(&dev, &ev));
    TEST_ASSERT_FALSE(ev);

    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_poll(&dev));
    }
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_was_pressed(&dev, &ev));
    TEST_ASSERT_FALSE(ev);
}

void test_was_pressed_rearm_after_release(void) {
    dal_button_t dev = {0};
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 13, .active_low = true };
    set_btn_pin(13, false, true);
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));

    set_btn_pin(13, true, true);
    for (int i = 0; i < DAL_BUTTON_DEBOUNCE_THRESHOLD; i++) {
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_poll(&dev));
    }
    bool ev = false;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_was_pressed(&dev, &ev));
    TEST_ASSERT_TRUE(ev);

    set_btn_pin(13, false, true);
    for (int i = 0; i < DAL_BUTTON_DEBOUNCE_THRESHOLD; i++) {
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_poll(&dev));
    }
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_was_pressed(&dev, &ev));
    TEST_ASSERT_FALSE(ev);

    set_btn_pin(13, true, true);
    for (int i = 0; i < DAL_BUTTON_DEBOUNCE_THRESHOLD; i++) {
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_poll(&dev));
    }
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_was_pressed(&dev, &ev));
    TEST_ASSERT_TRUE(ev);
}

struct btn_recorder {
    dal_button_event_t last;
    int                count;
};

static void record_event(dal_button_event_t evt, void *user_data) {
    struct btn_recorder *r = (struct btn_recorder *)user_data;
    r->last  = evt;
    r->count++;
}

void test_on_event_contract_null_args(void) {
    dal_button_t dev; memset(&dev, 0, sizeof(dev));
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 20, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
    struct btn_recorder r = {0};
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_button_on_event(NULL, record_event, &r));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_on_event(&dev, NULL, NULL));
}
void test_on_event_before_init_returns_not_initialized(void) {
    dal_button_t dev = {0};
    struct btn_recorder r = {0};
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED,
                          dal_button_on_event(&dev, record_event, &r));
}

void test_event_press_dispatched_on_debounce(void) {
    dal_button_t dev; memset(&dev, 0, sizeof(dev));
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 21, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
    struct btn_recorder r = {0};
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_on_event(&dev, record_event, &r));
    set_btn_pin(21, false, true);
    poll_n(&dev, DAL_BUTTON_DEBOUNCE_THRESHOLD);
    int count_before = r.count;

    set_btn_pin(21, true, true);
    poll_n(&dev, DAL_BUTTON_DEBOUNCE_THRESHOLD - 1);
    TEST_ASSERT_EQUAL_INT(count_before, r.count);
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_poll(&dev));
    TEST_ASSERT_EQUAL_INT(count_before + 1, r.count);
    TEST_ASSERT_EQUAL_INT(DAL_BUTTON_EVT_PRESS, r.last);
    poll_n(&dev, 10);
    TEST_ASSERT_EQUAL_INT(count_before + 1, r.count);
}

void test_event_release_dispatched_on_release(void) {
    dal_button_t dev; memset(&dev, 0, sizeof(dev));
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 22, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
    struct btn_recorder r = {0};
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_on_event(&dev, record_event, &r));

    set_btn_pin(22, false, true);
    poll_n(&dev, DAL_BUTTON_DEBOUNCE_THRESHOLD);
    int base = r.count;

    set_btn_pin(22, true, true);
    poll_n(&dev, DAL_BUTTON_DEBOUNCE_THRESHOLD);
    TEST_ASSERT_EQUAL_INT(base + 1, r.count);
    TEST_ASSERT_EQUAL_INT(DAL_BUTTON_EVT_PRESS, r.last);

    set_btn_pin(22, false, true);
    poll_n(&dev, DAL_BUTTON_DEBOUNCE_THRESHOLD - 1);
    TEST_ASSERT_EQUAL_INT(base + 1, r.count);
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_poll(&dev));
    TEST_ASSERT_EQUAL_INT(base + 2, r.count);
    TEST_ASSERT_EQUAL_INT(DAL_BUTTON_EVT_RELEASE, r.last);
}

void test_event_long_press_fires_once_after_timeout(void) {
    dal_button_t dev; memset(&dev, 0, sizeof(dev));
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 23, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
    struct btn_recorder r = {0};
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_on_event(&dev, record_event, &r));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_set_long_press_ms(&dev, 100));

    set_btn_pin(23, false, true);
    poll_n(&dev, DAL_BUTTON_DEBOUNCE_THRESHOLD);
    int base = r.count;

    set_btn_pin(23, true, true);
    poll_n(&dev, DAL_BUTTON_DEBOUNCE_THRESHOLD);
    TEST_ASSERT_EQUAL_INT(base + 1, r.count);
    TEST_ASSERT_EQUAL_INT(DAL_BUTTON_EVT_PRESS, r.last);

    poll_n(&dev, 6);
    TEST_ASSERT_EQUAL_INT(base + 1, r.count);
    poll_n(&dev, 3);
    TEST_ASSERT_EQUAL_INT(base + 1, r.count);
    poll_n(&dev, 1);
    TEST_ASSERT_EQUAL_INT(base + 2, r.count);
    TEST_ASSERT_EQUAL_INT(DAL_BUTTON_EVT_LONG_PRESS, r.last);

    poll_n(&dev, 20);
    TEST_ASSERT_EQUAL_INT(base + 2, r.count);

    set_btn_pin(23, false, true);
    poll_n(&dev, DAL_BUTTON_DEBOUNCE_THRESHOLD);
    TEST_ASSERT_EQUAL_INT(base + 3, r.count);
    TEST_ASSERT_EQUAL_INT(DAL_BUTTON_EVT_RELEASE, r.last);
}

void test_set_long_press_ms_validates(void) {
    dal_button_t dev = {0};
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_button_set_long_press_ms(&dev, 500));
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 24, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_button_set_long_press_ms(&dev, 0));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_set_long_press_ms(&dev, 1));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_set_long_press_ms(&dev, 60000));
}

void test_on_event_null_cb_unsubscribes(void) {
    dal_button_t dev; memset(&dev, 0, sizeof(dev));
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 25, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
    struct btn_recorder r = {0};
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_on_event(&dev, record_event, &r));

    set_btn_pin(25, true, true);
    poll_n(&dev, DAL_BUTTON_DEBOUNCE_THRESHOLD);
    TEST_ASSERT_EQUAL_INT(1, r.count);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_on_event(&dev, NULL, NULL));
    set_btn_pin(25, false, true);
    poll_n(&dev, DAL_BUTTON_DEBOUNCE_THRESHOLD);
    TEST_ASSERT_EQUAL_INT(1, r.count);
}

void test_isr_counter_contract(void) {
    dal_button_t dev = {0};
    uint32_t c = 0xDEAD;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_button_enable_isr_counter(&dev));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_button_get_edge_count(&dev, &c));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_button_reset_edge_count(&dev));

    const dal_button_config_t cfg = { .owner = OWNER, .pin = 26, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_button_get_edge_count(&dev, NULL));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_get_edge_count(&dev, &c));
    TEST_ASSERT_EQUAL_UINT32(0, c);
}

void test_isr_counter_increments_on_interrupt(void) {
    dal_button_t dev; memset(&dev, 0, sizeof(dev));
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 27, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_enable_isr_counter(&dev));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_enable_isr_counter(&dev));

    uint32_t c = 0;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_get_edge_count(&dev, &c));
    TEST_ASSERT_EQUAL_UINT32(0, c);

    pal_host_trigger_gpio_interrupt(27);
    pal_host_trigger_gpio_interrupt(27);
    pal_host_trigger_gpio_interrupt(27);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_get_edge_count(&dev, &c));
    TEST_ASSERT_EQUAL_UINT32(3, c);
}

void test_isr_counter_reset_is_atomic(void) {
    dal_button_t dev; memset(&dev, 0, sizeof(dev));
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 28, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_enable_isr_counter(&dev));

    pal_host_trigger_gpio_interrupt(28);
    pal_host_trigger_gpio_interrupt(28);
    uint32_t c = 0;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_get_edge_count(&dev, &c));
    TEST_ASSERT_EQUAL_UINT32(2, c);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_reset_edge_count(&dev));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_get_edge_count(&dev, &c));
    TEST_ASSERT_EQUAL_UINT32(0, c);

    pal_host_trigger_gpio_interrupt(28);
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_get_edge_count(&dev, &c));
    TEST_ASSERT_EQUAL_UINT32(1, c);
}

void test_isr_counter_no_lost_edges_during_reset(void) {
    dal_button_t dev; memset(&dev, 0, sizeof(dev));
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 29, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_enable_isr_counter(&dev));

    uint32_t mask = pal_irq_save_rtos_safe();
    pal_host_trigger_gpio_interrupt(29);
    uint32_t c = 0;
    pal_irq_restore(mask);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_get_edge_count(&dev, &c));
    TEST_ASSERT_EQUAL_UINT32(1, c);
}

void test_deinit_hardening(void) {
    dal_button_t dev = {0};
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 2, .active_low = true };

    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_button_deinit(NULL));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_deinit(&dev));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
    TEST_ASSERT_TRUE(dev.initialized);
    TEST_ASSERT_TRUE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 2));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_enable_isr_counter(&dev));
    TEST_ASSERT_TRUE(dev.isr_counter_enabled);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_deinit(&dev));
    TEST_ASSERT_FALSE(dev.initialized);
    TEST_ASSERT_FALSE(dev.isr_counter_enabled);
    TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 2));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_deinit(&dev));
}

void test_deinit_loop_with_isr_no_resource_leak(void) {
    dal_button_t dev; memset(&dev, 0, sizeof(dev));
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 30, .active_low = true };

    for (int round = 0; round < 10; round++) {
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
        TEST_ASSERT_TRUE(dev.initialized);
        TEST_ASSERT_TRUE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 30));
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_enable_isr_counter(&dev));
        TEST_ASSERT_TRUE(dev.isr_counter_enabled);
        poll_n(&dev, 2);
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_deinit(&dev));
        TEST_ASSERT_FALSE(dev.initialized);
        TEST_ASSERT_FALSE(dev.isr_counter_enabled);
        TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 30));
    }
}

void test_pull_illegal_rejected_before_claim(void) {
    dal_button_t dev = {0};
    dal_button_config_t bad = {
        .owner = OWNER, .pin = 40, .active_low = true, .pull = (dal_button_pull_t)4
    };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_button_init(&dev, &bad));
    TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 40));

    dal_button_t victim = {0};
    const dal_button_config_t ok = { .owner = "pull_victim", .pin = 40, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&victim, &ok));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_deinit(&victim));
}

void test_pull_none_disconnected_without_injection(void) {
    dal_button_t dev = {0};
    const dal_button_config_t cfg = {
        .owner = OWNER, .pin = 41, .active_low = true, .pull = DAL_BUTTON_PULL_NONE
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_DISCONNECTED, dal_button_poll(&dev));

    bool pressed = true;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_is_pressed(&dev, &pressed));
    TEST_ASSERT_FALSE(pressed);
}

void test_pull_none_with_injection_press_release(void) {
    dal_button_t dev = {0};
    const dal_button_config_t cfg = {
        .owner = OWNER, .pin = 42, .active_low = true, .pull = DAL_BUTTON_PULL_NONE
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));

    set_btn_pin(42, false, true);
    TEST_ASSERT_EQUAL_INT(WINK_OK, poll_n(&dev, DAL_BUTTON_DEBOUNCE_THRESHOLD));
    bool pressed = false;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_is_pressed(&dev, &pressed));
    TEST_ASSERT_FALSE(pressed);

    set_btn_pin(42, true, true);
    TEST_ASSERT_EQUAL_INT(WINK_OK, poll_n(&dev, DAL_BUTTON_DEBOUNCE_THRESHOLD));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_is_pressed(&dev, &pressed));
    TEST_ASSERT_TRUE(pressed);
}

void test_pull_explicit_up_overrides_active_low_polarity(void) {
    dal_button_t dev = {0};
    const dal_button_config_t cfg = {
        .owner = OWNER, .pin = 43, .active_low = false, .pull = DAL_BUTTON_PULL_UP
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
    set_btn_pin(43, false, false);
    TEST_ASSERT_EQUAL_INT(WINK_OK, poll_n(&dev, DAL_BUTTON_DEBOUNCE_THRESHOLD));
    bool pressed = true;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_is_pressed(&dev, &pressed));
    TEST_ASSERT_FALSE(pressed);
}

void test_get_status_contract(void) {
    dal_button_t dev = {0};
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_button_get_status(NULL, (wink_status_t[]){0}));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_button_get_status(&dev, NULL));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_button_get_status(&dev, (wink_status_t[]){0}));
}

void test_get_status_initially_ok(void) {
    dal_button_t dev = {0};
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 14, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
    wink_status_t st = WINK_ERR_DISCONNECTED;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_get_status(&dev, &st));
    TEST_ASSERT_EQUAL_INT(WINK_OK, st);
}

void test_get_status_clears_after_recovery(void) {
    dal_button_t dev = {0};
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 15, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
    set_btn_pin(15, false, true);
    TEST_ASSERT_EQUAL_INT(WINK_OK, poll_n(&dev, DAL_BUTTON_DEBOUNCE_THRESHOLD));
    wink_status_t st = WINK_ERR_PANIC;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_get_status(&dev, &st));
    TEST_ASSERT_EQUAL_INT(WINK_OK, st);
}

void test_get_status_propagates_poll_error_and_clears(void) {
    dal_button_t dev = {0};
    const dal_button_config_t cfg = {
        .owner = OWNER, .pin = 16, .active_low = true, .pull = DAL_BUTTON_PULL_NONE
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_DISCONNECTED, dal_button_poll(&dev));
    wink_status_t st = WINK_OK;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_get_status(&dev, &st));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_DISCONNECTED, st);
    bool pressed = true;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_is_pressed(&dev, &pressed));
    TEST_ASSERT_FALSE(pressed);
    set_btn_pin(16, false, true);
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_poll(&dev));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_get_status(&dev, &st));
    TEST_ASSERT_EQUAL_INT(WINK_OK, st);
}

void test_get_status_after_deinit(void) {
    dal_button_t dev = {0};
    const dal_button_config_t cfg = {
        .owner = OWNER, .pin = 17, .active_low = true, .pull = DAL_BUTTON_PULL_NONE
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_DISCONNECTED, dal_button_poll(&dev));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_deinit(&dev));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED,
                          dal_button_get_status(&dev, (wink_status_t[]){0}));
}

void test_was_pressed_atomic_under_lock(void) {
    dal_button_t dev = {0};
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 18, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
    dev.stable_pressed = true;
    dev.last_reported  = false;

    bool ev1 = false;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_was_pressed(&dev, &ev1));
    TEST_ASSERT_TRUE(ev1);
    TEST_ASSERT_TRUE(dev.last_reported);

    bool ev2 = true;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_was_pressed(&dev, &ev2));
    TEST_ASSERT_FALSE(ev2);
    TEST_ASSERT_TRUE(dev.last_reported);
}

void test_was_pressed_serializes_concurrent_readers(void) {
    dal_button_t dev = {0};
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 19, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
    dev.stable_pressed = true;
    dev.last_reported  = false;

    uint32_t mask = pal_irq_save_rtos_safe();

    bool ev_outer = false;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_was_pressed(&dev, &ev_outer));
    TEST_ASSERT_TRUE(ev_outer);
    TEST_ASSERT_TRUE(dev.last_reported);

    pal_irq_restore(mask);

    bool ev_inner = true;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_was_pressed(&dev, &ev_inner));
    TEST_ASSERT_FALSE(ev_inner);
}

void test_deinit_loop_with_counter_and_irq_backend(void) {
    dal_button_t dev; memset(&dev, 0, sizeof(dev));
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 44, .active_low = true };

    for (int round = 0; round < 10; round++) {
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
        TEST_ASSERT_TRUE(dev.initialized);
        TEST_ASSERT_TRUE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 44));

        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_enable_isr_counter(&dev));
        TEST_ASSERT_TRUE(dev.isr_counter_enabled);
        TEST_ASSERT_TRUE(dev.gpio_isr_registered);

        extern void dal_button_set_event_backend(dal_button_t *dev, uint8_t backend);
        dal_button_set_event_backend(&dev, 2);
        TEST_ASSERT_TRUE(dev.gpio_isr_registered);

        pal_host_trigger_gpio_interrupt(44);
        pal_host_trigger_gpio_interrupt(44);
        uint32_t c = 0;
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_get_edge_count(&dev, &c));
        TEST_ASSERT_EQUAL_UINT32(2, c);

        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_deinit(&dev));
        TEST_ASSERT_FALSE(dev.initialized);
        TEST_ASSERT_FALSE(dev.isr_counter_enabled);
        TEST_ASSERT_FALSE(dev.gpio_isr_registered);
        TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 44));
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_init_null_returns_invalid_arg);
    RUN_TEST(test_read_before_init_returns_not_initialized);
    RUN_TEST(test_read_null_returns_invalid_arg);
    RUN_TEST(test_is_pressed_null_out_returns_invalid_arg);
    RUN_TEST(test_active_low_debounce_to_pressed);
    RUN_TEST(test_active_high_stays_unpressed);
    RUN_TEST(test_was_pressed_edge_once);
    RUN_TEST(test_was_pressed_rearm_after_release);
    RUN_TEST(test_on_event_contract_null_args);
    RUN_TEST(test_on_event_before_init_returns_not_initialized);
    RUN_TEST(test_event_press_dispatched_on_debounce);
    RUN_TEST(test_event_release_dispatched_on_release);
    RUN_TEST(test_event_long_press_fires_once_after_timeout);
    RUN_TEST(test_set_long_press_ms_validates);
    RUN_TEST(test_on_event_null_cb_unsubscribes);
    RUN_TEST(test_isr_counter_contract);
    RUN_TEST(test_isr_counter_increments_on_interrupt);
    RUN_TEST(test_isr_counter_reset_is_atomic);
    RUN_TEST(test_isr_counter_no_lost_edges_during_reset);
    RUN_TEST(test_deinit_hardening);
    RUN_TEST(test_deinit_loop_with_isr_no_resource_leak);
    RUN_TEST(test_pull_illegal_rejected_before_claim);
    RUN_TEST(test_pull_none_disconnected_without_injection);
    RUN_TEST(test_pull_none_with_injection_press_release);
    RUN_TEST(test_pull_explicit_up_overrides_active_low_polarity);
    RUN_TEST(test_get_status_contract);
    RUN_TEST(test_get_status_initially_ok);
    RUN_TEST(test_get_status_clears_after_recovery);
    RUN_TEST(test_get_status_propagates_poll_error_and_clears);
    RUN_TEST(test_get_status_after_deinit);
    RUN_TEST(test_was_pressed_atomic_under_lock);
    RUN_TEST(test_was_pressed_serializes_concurrent_readers);
    RUN_TEST(test_deinit_loop_with_counter_and_irq_backend);
    return UNITY_END();
}
