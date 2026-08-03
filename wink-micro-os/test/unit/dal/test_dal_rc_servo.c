#include "unity.h"
#include "wink_status.h"
#include "dal_rc_servo.h"
#include "pal_pwm_router.h"
#include "pal_resource.h"
#include "host_test_ctrl.h"

#include <string.h>

static const char *const OWNER = "test_dal_rc_servo";

void setUp(void) {
    sim_reset_time();
    pal_pwm_router_reset();
    pal_resource_reset();
}
void tearDown(void) {}

/* ---- init 契约 ---- */
void test_init_null_returns_invalid_arg(void) {
    dal_rc_servo_config_t cfg = {
        .owner = OWNER, .pwm_channel = 0,
        .min_pulse_us = 500, .max_pulse_us = 2500, .max_angle_ddeg = 1800
    };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_rc_servo_init(NULL, &cfg));
    dal_rc_servo_t dev = {0};
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_rc_servo_init(&dev, NULL));
}

void test_init_rejects_invalid_channel(void) {
    dal_rc_servo_t dev = {0};
    dal_rc_servo_config_t cfg = {
        .owner = OWNER, .pwm_channel = PAL_PWM_CHANNELS,
        .min_pulse_us = 500, .max_pulse_us = 2500, .max_angle_ddeg = 1800
    };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_rc_servo_init(&dev, &cfg));
}

void test_init_normalizes_default_pulse(void) {
    dal_rc_servo_t s = {0};
    dal_rc_servo_config_t cfg = {
        .owner = OWNER, .pwm_channel = 0,
        .min_pulse_us = 0, .max_pulse_us = 0, .max_angle_ddeg = 1800
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_init(&s, &cfg));
    TEST_ASSERT_EQUAL_UINT16(500, s.config.min_pulse_us);
    TEST_ASSERT_EQUAL_UINT16(2500, s.config.max_pulse_us);
}

void test_init_writes_zero_duty(void) {
    dal_rc_servo_t s = {0};
    dal_rc_servo_config_t cfg = {
        .owner = OWNER, .pwm_channel = 0,
        .min_pulse_us = 500, .max_pulse_us = 2500, .max_angle_ddeg = 1800
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_init(&s, &cfg));
    /* DAL-L-006: init must explicitly write duty=0 (limp zero-energy) */
    TEST_ASSERT_EQUAL_FLOAT(0.0f, sim_last_pwm_duty(0));
    TEST_ASSERT_EQUAL_UINT16(0, s.current_angle_ddeg);
}

void test_set_angle_before_init_returns_not_initialized(void) {
    dal_rc_servo_t dev = {0};
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_rc_servo_set_angle(&dev, 900));
}

void test_set_angle_null_returns_invalid_arg(void) {
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_rc_servo_set_angle(NULL, 900));
}

/* ---- init 后 set_angle 的角度→占空比映射（整数 ddeg）---- */
void test_set_angle_900_updates_duty(void) {
    dal_rc_servo_t s = {0};
    dal_rc_servo_config_t cfg = {
        .owner = OWNER, .pwm_channel = 0,
        .min_pulse_us = 500, .max_pulse_us = 2500, .max_angle_ddeg = 1800
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_init(&s, &cfg));
    /* 900 ddeg = 90.0° -> pulse = 500 + (900*2000)/1800 = 1500us -> duty 7.5% */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_set_angle(&s, 900));
    TEST_ASSERT_EQUAL_FLOAT(7.5f, sim_last_pwm_duty(0));
    TEST_ASSERT_EQUAL_UINT16(900, s.current_angle_ddeg);
}

void test_set_angle_clamps_overflow(void) {
    dal_rc_servo_t s = {0};
    dal_rc_servo_config_t cfg = {
        .owner = OWNER, .pwm_channel = 1,
        .min_pulse_us = 500, .max_pulse_us = 2500, .max_angle_ddeg = 1800
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_init(&s, &cfg));
    /* 2000 ddeg clamps to 1800 -> pulse 2500us -> duty 12.5% */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_set_angle(&s, 2000));
    TEST_ASSERT_EQUAL_UINT16(1800, s.current_angle_ddeg);
    TEST_ASSERT_EQUAL_FLOAT(12.5f, sim_last_pwm_duty(1));
}

void test_set_angle_0_is_min_pulse(void) {
    dal_rc_servo_t s = {0};
    dal_rc_servo_config_t cfg = {
        .owner = OWNER, .pwm_channel = 0,
        .min_pulse_us = 500, .max_pulse_us = 2500, .max_angle_ddeg = 1800
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_init(&s, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_set_angle(&s, 0));
    /* pulse = 500us -> duty 2.5% */
    TEST_ASSERT_EQUAL_FLOAT(2.5f, sim_last_pwm_duty(0));
    TEST_ASSERT_EQUAL_UINT16(0, s.current_angle_ddeg);
}

/* F-1: cache (current_angle_ddeg) must only update after PAL success */
void test_cache_updated_after_hardware_success(void) {
    dal_rc_servo_t s = {0};
    dal_rc_servo_config_t cfg = {
        .owner = OWNER, .pwm_channel = 0,
        .min_pulse_us = 500, .max_pulse_us = 2500, .max_angle_ddeg = 1800
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_init(&s, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_set_angle(&s, 450));
    TEST_ASSERT_EQUAL_UINT16(450, s.current_angle_ddeg);
}

/* ---- max_angle_ddeg travel ---- */
void test_explicit_max_angle_2700_no_clamp_at_2000(void) {
    dal_rc_servo_t s = {0};
    dal_rc_servo_config_t cfg = {
        .owner = OWNER, .pwm_channel = 2,
        .min_pulse_us = 500, .max_pulse_us = 2500, .max_angle_ddeg = 2700,
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_init(&s, &cfg));
    /* 2000 ddeg not clamped to 1800; denominator is 2700 */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_set_angle(&s, 2000));
    TEST_ASSERT_EQUAL_UINT16(2000, s.current_angle_ddeg);
    uint32_t pulse_us = 500 + ((uint32_t)2000 * 2000u) / 2700u;
    float expected_duty = ((float)pulse_us / 20000.0f) * 100.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, expected_duty, sim_last_pwm_duty(2));
}

/* ---- safe-off ---- */
void test_safe_off_null_returns_invalid_arg(void) {
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_rc_servo_safe_off(NULL));
}

/* F-2 / DAL-L-022: safe_off on uninitialized handle returns WINK_OK */
void test_safe_off_before_init_returns_ok(void) {
    dal_rc_servo_t dev = {0};  /* !initialized */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_safe_off(&dev));
}

void test_safe_off_after_init_sets_zero_duty(void) {
    dal_rc_servo_t s = {0};
    dal_rc_servo_config_t cfg = {
        .owner = OWNER, .pwm_channel = 2,
        .min_pulse_us = 500, .max_pulse_us = 2500, .max_angle_ddeg = 1800
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_init(&s, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_set_angle(&s, 900));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_safe_off(&s));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, sim_last_pwm_duty(2));
}

/* ---- ADR-0008 Flash 覆写 apply_override ---- */
/* params (7 bytes LE): pwm_channel:u8@0, min_pulse_us:u16@1, max_pulse_us:u16@3, max_angle_ddeg:u16@5 */
static void build_servo_params(uint8_t *p, uint8_t ch, uint16_t min_us,
                              uint16_t max_us, uint16_t max_ddeg) {
    memset(p, 0, 16);
    p[0] = ch;
    memcpy(p + 1, &min_us, 2);
    memcpy(p + 3, &max_us, 2);
    memcpy(p + 5, &max_ddeg, 2);
}

void test_apply_override_writes_fields(void) {
    dal_rc_servo_t s = {0};
    uint8_t p[16];
    build_servo_params(p, 3, 600, 2400, 2700);
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_apply_override(&s, p, sizeof p));
    TEST_ASSERT_EQUAL_UINT8(3u, s.config.pwm_channel);
    TEST_ASSERT_EQUAL_UINT16(600, s.config.min_pulse_us);
    TEST_ASSERT_EQUAL_UINT16(2400, s.config.max_pulse_us);
    TEST_ASSERT_EQUAL_UINT16(2700, s.config.max_angle_ddeg);
}

void test_apply_override_rejects_invalid_pulse(void) {
    dal_rc_servo_t s = {0};
    s.config.min_pulse_us = 500;
    s.config.max_pulse_us = 2500;
    uint8_t p[16];
    build_servo_params(p, 0, 0, 2500, 1800);   /* min == 0 */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_rc_servo_apply_override(&s, p, sizeof p));
    build_servo_params(p, 0, 2500, 500, 1800); /* max <= min */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_rc_servo_apply_override(&s, p, sizeof p));
    /* illegal -> fields unchanged */
    TEST_ASSERT_EQUAL_UINT16(500, s.config.min_pulse_us);
    TEST_ASSERT_EQUAL_UINT16(2500, s.config.max_pulse_us);
}

void test_apply_override_rejects_bad_channel(void) {
    dal_rc_servo_t s = {0};
    uint8_t p[16];
    build_servo_params(p, PAL_PWM_CHANNELS, 500, 2500, 1800);
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_rc_servo_apply_override(&s, p, sizeof p));
}

/* F-9 / DAL-S-015: override after init must fail */
void test_apply_override_rejects_after_init(void) {
    dal_rc_servo_t s = {0};
    dal_rc_servo_config_t cfg = {
        .owner = OWNER, .pwm_channel = 0,
        .min_pulse_us = 500, .max_pulse_us = 2500, .max_angle_ddeg = 1800
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_init(&s, &cfg));
    uint8_t p[16];
    build_servo_params(p, 3, 600, 2400, 2700);
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_rc_servo_apply_override(&s, p, sizeof p));
}

void test_apply_override_null_returns_invalid_arg(void) {
    uint8_t p[16] = {0};
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_rc_servo_apply_override(NULL, p, sizeof p));
    dal_rc_servo_t s = {0};
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_rc_servo_apply_override(&s, NULL, sizeof p));
}

/* ---- deinit hardening ---- */
void test_deinit_hardening(void) {
    dal_rc_servo_t dev = {0};
    dal_rc_servo_config_t cfg = {
        .owner = OWNER, .pwm_channel = 0,
        .min_pulse_us = 500, .max_pulse_us = 2500, .max_angle_ddeg = 1800
    };

    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_rc_servo_deinit(NULL));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_deinit(&dev));  /* idempotent on uninit */

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_init(&dev, &cfg));
    TEST_ASSERT_TRUE(dev.initialized);
    TEST_ASSERT_TRUE(pal_resource_is_claimed(PAL_RESOURCE_PWM_CHANNEL, 0));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_deinit(&dev));
    TEST_ASSERT_FALSE(dev.initialized);
    TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_PWM_CHANNEL, 0));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_deinit(&dev));  /* idempotent after deinit */
}

void test_deinit_loop_no_resource_leak(void) {
    dal_rc_servo_t dev = {0};
    dal_rc_servo_config_t cfg = {
        .owner = "servo_loop", .pwm_channel = 1,
        .min_pulse_us = 500, .max_pulse_us = 2500, .max_angle_ddeg = 1800,
    };
    for (int round = 0; round < 10; round++) {
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_init(&dev, &cfg));
        TEST_ASSERT_TRUE(dev.initialized);
        TEST_ASSERT_TRUE(pal_resource_is_claimed(PAL_RESOURCE_PWM_CHANNEL, 1));
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_set_angle(&dev, 900));
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_deinit(&dev));
        TEST_ASSERT_FALSE(dev.initialized);
        TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_PWM_CHANNEL, 1));
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_init_null_returns_invalid_arg);
    RUN_TEST(test_init_rejects_invalid_channel);
    RUN_TEST(test_init_normalizes_default_pulse);
    RUN_TEST(test_init_writes_zero_duty);
    RUN_TEST(test_set_angle_before_init_returns_not_initialized);
    RUN_TEST(test_set_angle_null_returns_invalid_arg);
    RUN_TEST(test_set_angle_900_updates_duty);
    RUN_TEST(test_set_angle_clamps_overflow);
    RUN_TEST(test_set_angle_0_is_min_pulse);
    RUN_TEST(test_cache_updated_after_hardware_success);
    RUN_TEST(test_explicit_max_angle_2700_no_clamp_at_2000);
    RUN_TEST(test_safe_off_null_returns_invalid_arg);
    RUN_TEST(test_safe_off_before_init_returns_ok);
    RUN_TEST(test_safe_off_after_init_sets_zero_duty);
    RUN_TEST(test_apply_override_writes_fields);
    RUN_TEST(test_apply_override_rejects_invalid_pulse);
    RUN_TEST(test_apply_override_rejects_bad_channel);
    RUN_TEST(test_apply_override_rejects_after_init);
    RUN_TEST(test_apply_override_null_returns_invalid_arg);
    RUN_TEST(test_deinit_hardening);
    RUN_TEST(test_deinit_loop_no_resource_leak);
    return UNITY_END();
}
