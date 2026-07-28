#include "unity.h"
#include "wink_status.h"
#include "dal_rc_servo.h"
#include "pal_pwm_router.h"
#include "pal_resource.h"
#include "host_test_ctrl.h"

#include <string.h>   /* ADR-0008 apply_override params 构造 */

static const char *const OWNER = "test_dal_rc_servo";

void setUp(void) {
    sim_reset_time();
    pal_pwm_router_reset();
    pal_resource_reset();
}
void tearDown(void) {}

/* ---- init 契约（Phase 2 Task 2-1）---- */
void test_init_null_returns_invalid_arg(void) {
    dal_rc_servo_config_t cfg = { .owner = OWNER, .pwm_channel = 0, .min_pulse_ms = 0.5f, .max_pulse_ms = 2.5f };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_rc_servo_init(NULL, &cfg));
    dal_rc_servo_t dev = {0};
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_rc_servo_init(&dev, NULL));
}

void test_init_rejects_invalid_pulse_range(void) {
    dal_rc_servo_t dev = {0};
    dal_rc_servo_config_t zero_min  = { .owner = OWNER, .pwm_channel = 0, .min_pulse_ms = 0.0f, .max_pulse_ms = 2.5f };
    dal_rc_servo_config_t inverted  = { .owner = OWNER, .pwm_channel = 0, .min_pulse_ms = 2.5f, .max_pulse_ms = 0.5f };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_rc_servo_init(&dev, &zero_min));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_rc_servo_init(&dev, &inverted));
}

void test_set_angle_before_init_returns_not_initialized(void) {
    /* initialized 默认 false（未 init） */
    dal_rc_servo_t dev = { .config.pwm_channel = 0, .current_angle = 0.0f,
                        .config.min_pulse_ms = 0.5f, .config.max_pulse_ms = 2.5f };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_rc_servo_set_angle(&dev, 90.0f));
}

void test_set_angle_null_returns_invalid_arg(void) {
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_rc_servo_set_angle(NULL, 90.0f));
}

/* ---- init 后 set_angle 的角度→占空比映射（继承 Phase 0 常量等值校验）---- */
void test_init_then_set_angle_updates_duty(void) {
    dal_rc_servo_t s = {0};
    dal_rc_servo_config_t cfg = { .owner = OWNER, .pwm_channel = 0, .min_pulse_ms = 0.5f, .max_pulse_ms = 2.5f };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_init(&s, &cfg));
    /* 90° -> 脉宽 0.5+0.5*(2.5-0.5)=1.5ms -> 占空比 (1.5/20)*100 = 7.5% */
    wink_status_t st = dal_rc_servo_set_angle(&s, 90.0f);
    TEST_ASSERT_EQUAL_INT(WINK_OK, st);
    TEST_ASSERT_EQUAL_FLOAT(7.5f, sim_last_pwm_duty(0));
    TEST_ASSERT_EQUAL_FLOAT(90.0f, s.current_angle);
}

void test_init_then_set_angle_clamps_overflow(void) {
    dal_rc_servo_t s = {0};
    dal_rc_servo_config_t cfg = { .owner = OWNER, .pwm_channel = 1, .min_pulse_ms = 0.5f, .max_pulse_ms = 2.5f };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_init(&s, &cfg));
    /* 200° 钳到 180 -> 脉宽 2.5ms -> 占空比 12.5% */
    wink_status_t st_overflow = dal_rc_servo_set_angle(&s, 200.0f);
    TEST_ASSERT_EQUAL_INT(WINK_OK, st_overflow);
    TEST_ASSERT_EQUAL_FLOAT(12.5f, sim_last_pwm_duty(1));
}

/* ---- safe-off（Phase 5 Task 5-2）---- */
void test_safe_off_null_returns_invalid_arg(void) {
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_rc_servo_safe_off(NULL));
}

void test_safe_off_before_init_returns_not_initialized(void) {
    dal_rc_servo_t dev = { .config.pwm_channel = 0, .config.min_pulse_ms = 0.5f, .config.max_pulse_ms = 2.5f };  /* !initialized */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_rc_servo_safe_off(&dev));
}

void test_safe_off_after_init_sets_zero_duty(void) {
    dal_rc_servo_t s = {0};
    dal_rc_servo_config_t cfg = { .owner = OWNER, .pwm_channel = 2, .min_pulse_ms = 0.5f, .max_pulse_ms = 2.5f };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_init(&s, &cfg));
    wink_status_t sa = dal_rc_servo_set_angle(&s, 90.0f);   /* 先设非零占空比 */
    (void)sa;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_safe_off(&s));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, sim_last_pwm_duty(2));   /* duty 归零 = 舵机 limp = 安全 */
}

/* ---- ADR-0008 Flash 覆写 apply_override（init 前字段改写 + 轻校验）---- */
/* params 布局（小端）：pwm_channel:u8@0, min_pulse_ms:f32@1, max_pulse_ms:f32@5 (buf=16B) */
static void build_servo_params(uint8_t *p, uint8_t ch, float min_ms, float max_ms) {
    memset(p, 0, 16);
    p[0] = ch;
    memcpy(p + 1, &min_ms, 4);
    memcpy(p + 5, &max_ms, 4);
}

void test_apply_override_writes_fields(void) {
    dal_rc_servo_t s = { .config.pwm_channel = 0, .config.min_pulse_ms = 0.5f, .config.max_pulse_ms = 2.5f };
    uint8_t p[16];
    build_servo_params(p, 3, 0.6f, 2.4f);
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_apply_override(&s, p, sizeof p));
    TEST_ASSERT_EQUAL_UINT8(3u, s.config.pwm_channel);
    TEST_ASSERT_EQUAL_FLOAT(0.6f, s.config.min_pulse_ms);
    TEST_ASSERT_EQUAL_FLOAT(2.4f, s.config.max_pulse_ms);
}

void test_apply_override_rejects_invalid_pulse(void) {
    dal_rc_servo_t s = { .config.pwm_channel = 0, .config.min_pulse_ms = 0.5f, .config.max_pulse_ms = 2.5f };
    uint8_t p[16];
    build_servo_params(p, 0, 0.0f, 2.5f);            /* min == 0 */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_rc_servo_apply_override(&s, p, sizeof p));
    build_servo_params(p, 0, 2.5f, 0.5f);            /* max <= min */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_rc_servo_apply_override(&s, p, sizeof p));
    /* 非法 → 字段保持不变（绝不写半状态） */
    TEST_ASSERT_EQUAL_FLOAT(0.5f, s.config.min_pulse_ms);
    TEST_ASSERT_EQUAL_FLOAT(2.5f, s.config.max_pulse_ms);
}

void test_apply_override_rejects_bad_channel(void) {
    dal_rc_servo_t s = { .config.pwm_channel = 0, .config.min_pulse_ms = 0.5f, .config.max_pulse_ms = 2.5f };
    uint8_t p[16];
    build_servo_params(p, PAL_PWM_CHANNELS, 0.5f, 2.5f);   /* channel 越界 */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_rc_servo_apply_override(&s, p, sizeof p));
}

void test_apply_override_null_returns_invalid_arg(void) {
    uint8_t p[16] = {0};
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_rc_servo_apply_override(NULL, p, sizeof p));
    dal_rc_servo_t s = {0};
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_rc_servo_apply_override(&s, NULL, sizeof p));
}

void test_deinit_hardening(void) {
    dal_rc_servo_t dev = {0};
    const dal_rc_servo_config_t cfg = { .owner = OWNER, .pwm_channel = 0, .min_pulse_ms = 0.5f, .max_pulse_ms = 2.5f };

    /* 1. NULL safety */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_rc_servo_deinit(NULL));

    /* 2. Idempotency on uninitialized dev */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_deinit(&dev));

    /* 3. Successful deinit and resource release */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_init(&dev, &cfg));
    TEST_ASSERT_TRUE(dev.initialized);
    TEST_ASSERT_TRUE(pal_resource_is_claimed(PAL_RESOURCE_PWM_CHANNEL, 0));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_deinit(&dev));
    TEST_ASSERT_FALSE(dev.initialized);
    TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_PWM_CHANNEL, 0));

    /* 4. Idempotency after deinit */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_deinit(&dev));
}

/* ADR-0024 §4 #8 idempotency — Task 0.7 Step 4: 10-round init→deinit loop
 * guards against PWM channel reservation leak across re-init cycles. */
void test_deinit_loop_pwm_channel_no_resource_leak(void) {
    dal_rc_servo_t dev = {0};
    const dal_rc_servo_config_t cfg = {
        .owner = "servo_loop", .pwm_channel = 1,
        .min_pulse_ms = 0.5f, .max_pulse_ms = 2.5f,
    };
    for (int round = 0; round < 10; round++) {
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_init(&dev, &cfg));
        TEST_ASSERT_TRUE(dev.initialized);
        TEST_ASSERT_TRUE(pal_resource_is_claimed(PAL_RESOURCE_PWM_CHANNEL, 1));
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_deinit(&dev));
        TEST_ASSERT_FALSE(dev.initialized);
        TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_PWM_CHANNEL, 1));
    }
}

/* ADR-0034: advanced PWM profile fields */
void test_init_explicit_resolution_bits_ok(void) {
    dal_rc_servo_t s = {0};
    const dal_rc_servo_config_t cfg = {
        .owner = OWNER, .pwm_channel = 0,
        .resolution_bits = 10u,
        .clock_requirement = DAL_RC_SERVO_CLOCK_AUTO,
        .min_pulse_ms = 0.5f, .max_pulse_ms = 2.5f,
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_init(&s, &cfg));
    TEST_ASSERT_EQUAL_UINT8(10u, s.config.resolution_bits);
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_set_angle(&s, 90.0f));
    TEST_ASSERT_EQUAL_FLOAT(7.5f, sim_last_pwm_duty(0));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_deinit(&s));
}

void test_init_stable_required_unsupported_on_host(void) {
    dal_rc_servo_t s = {0};
    const dal_rc_servo_config_t cfg = {
        .owner = OWNER, .pwm_channel = 3,
        .clock_requirement = DAL_RC_SERVO_CLOCK_STABLE_REQUIRED,
        .min_pulse_ms = 0.5f, .max_pulse_ms = 2.5f,
    };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_UNSUPPORTED, dal_rc_servo_init(&s, &cfg));
    TEST_ASSERT_FALSE(s.initialized);
    TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_PWM_CHANNEL, 3));
}

void test_init_illegal_clock_rejected_before_claim(void) {
    dal_rc_servo_t s = {0};
    dal_rc_servo_config_t cfg = {
        .owner = OWNER, .pwm_channel = 4,
        .clock_requirement = (dal_rc_servo_clock_requirement_t)2,
        .min_pulse_ms = 0.5f, .max_pulse_ms = 2.5f,
    };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_rc_servo_init(&s, &cfg));
    TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_PWM_CHANNEL, 4));
}

void test_apply_override_preserves_advanced_fields(void) {
    dal_rc_servo_t s = {
        .config.pwm_channel = 0,
        .config.resolution_bits = 12u,
        .config.clock_requirement = DAL_RC_SERVO_CLOCK_STABLE_REQUIRED,
        .config.min_pulse_ms = 0.5f,
        .config.max_pulse_ms = 2.5f,
    };
    uint8_t p[16];
    build_servo_params(p, 2, 0.6f, 2.4f);
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_apply_override(&s, p, 9u));
    TEST_ASSERT_EQUAL_UINT8(2u, s.config.pwm_channel);
    TEST_ASSERT_EQUAL_FLOAT(0.6f, s.config.min_pulse_ms);
    TEST_ASSERT_EQUAL_FLOAT(2.4f, s.config.max_pulse_ms);
    /* Flash wire v1 is 9 bytes — advanced fields must not be touched. */
    TEST_ASSERT_EQUAL_UINT8(12u, s.config.resolution_bits);
    TEST_ASSERT_EQUAL_UINT8(DAL_RC_SERVO_CLOCK_STABLE_REQUIRED, s.config.clock_requirement);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_init_null_returns_invalid_arg);
    RUN_TEST(test_init_rejects_invalid_pulse_range);
    RUN_TEST(test_set_angle_before_init_returns_not_initialized);
    RUN_TEST(test_set_angle_null_returns_invalid_arg);
    RUN_TEST(test_init_then_set_angle_updates_duty);
    RUN_TEST(test_init_then_set_angle_clamps_overflow);
    RUN_TEST(test_safe_off_null_returns_invalid_arg);
    RUN_TEST(test_safe_off_before_init_returns_not_initialized);
    RUN_TEST(test_safe_off_after_init_sets_zero_duty);
    RUN_TEST(test_apply_override_writes_fields);
    RUN_TEST(test_apply_override_rejects_invalid_pulse);
    RUN_TEST(test_apply_override_rejects_bad_channel);
    RUN_TEST(test_apply_override_null_returns_invalid_arg);
    RUN_TEST(test_deinit_hardening);
    RUN_TEST(test_deinit_loop_pwm_channel_no_resource_leak);
    RUN_TEST(test_init_explicit_resolution_bits_ok);
    RUN_TEST(test_init_stable_required_unsupported_on_host);
    RUN_TEST(test_init_illegal_clock_rejected_before_claim);
    RUN_TEST(test_apply_override_preserves_advanced_fields);
    return UNITY_END();
}
