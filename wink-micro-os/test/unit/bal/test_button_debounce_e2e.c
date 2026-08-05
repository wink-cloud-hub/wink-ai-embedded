// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_button_debounce_e2e.c
 * @brief End-to-end button debounce simulation unit tests.
 */
#include "unity.h"
#include "dal_button.h"
#include "pal_hal.h"
#include "pal_osal.h"
#include "pal_resource.h"
#include "host_test_ctrl.h"
#include "wink_sim_physical.h"

#include "compat/wink_test_compat.h"
WINK_TEST_ALLOW_DEPRECATED

void setUp(void) { sim_reset_time(); pal_resource_reset(); }
void tearDown(void) {}

#define TICK_MS 10
#define BOUNCE_US 30000u

static void run_ticks(dal_button_t *btn, int n) {
    for (int i = 0; i < n; i++) {
        pal_os_sleep_ms(TICK_MS);
        TEST_ASSERT_EQUAL(WINK_OK, dal_button_poll(btn));
    }
}

static bool raw_pressed(uint16_t pin, bool active_low) {
    bool lvl = false;
    WINK_IGNORE_UNUSED(pal_gpio_read((wink_pin_t)pin, &lvl));
    return lvl != active_low;
}

void test_dal_button_absorbs_bounce_and_settles(void) {
    wink_sim_faults_t f = WINK_SIM_FAULTS_IDEAL;
    f.bounce_us = BOUNCE_US; f.prng_seed = 1;
    sim_set_faults(&f);

    dal_button_t btn = {0};
    const dal_button_config_t cfg = { .owner = "e2e_debounce_bounce", .pin = 7, .active_low = true };
    TEST_ASSERT_EQUAL(WINK_OK, dal_button_init(&btn, &cfg));
    sim_set_gpio_ideal(7, true);
    run_ticks(&btn, 2);
    bool released = true;
    TEST_ASSERT_EQUAL(WINK_OK, dal_button_is_pressed(&btn, &released));
    TEST_ASSERT_FALSE(released);

    sim_set_gpio_ideal(7, false);
    run_ticks(&btn, 6);

    bool pressed = false;
    TEST_ASSERT_EQUAL(WINK_OK, dal_button_is_pressed(&btn, &pressed));
    TEST_ASSERT_TRUE(pressed);
    sim_clear_gpio_ideal();
}

void test_raw_read_without_debounce_bounces(void) {
    wink_sim_faults_t f = WINK_SIM_FAULTS_IDEAL;
    f.bounce_us = BOUNCE_US; f.prng_seed = 1;
    sim_set_faults(&f);

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 9, "test"));
    sim_set_gpio_ideal(9, true);
    pal_os_sleep_ms(TICK_MS);
    sim_set_gpio_ideal(9, false);

    bool saw_pressed = false, saw_released = false;
    for (int i = 0; i < 3; i++) {
        if (raw_pressed(9, true)) { saw_pressed = true; }
        else { saw_released = true; }
        pal_os_sleep_ms(TICK_MS);
    }
    TEST_ASSERT_TRUE(saw_pressed && saw_released);
    sim_clear_gpio_ideal();
}

void test_no_bounce_config_settles_fast(void) {
    sim_set_faults(&WINK_SIM_FAULTS_IDEAL);
    dal_button_t btn;
    const dal_button_config_t cfg = { .owner = "e2e_debounce_baseline", .pin = 8, .active_low = false };
    TEST_ASSERT_EQUAL(WINK_OK, dal_button_init(&btn, &cfg));
    sim_set_gpio_ideal(8, true);
    run_ticks(&btn, 5);
    bool pressed = false;
    TEST_ASSERT_EQUAL(WINK_OK, dal_button_is_pressed(&btn, &pressed));
    TEST_ASSERT_TRUE(pressed);
    sim_clear_gpio_ideal();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_dal_button_absorbs_bounce_and_settles);
    RUN_TEST(test_raw_read_without_debounce_bounces);
    RUN_TEST(test_no_bounce_config_settles_fast);
    return UNITY_END();
}
