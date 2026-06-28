#include "unity.h"
#include "wink_status.h"
#include "dal_servo.h"
#include "pal_pwm_router.h"
#include "host_test_ctrl.h"

#include <string.h>   /* ADR-0008 apply_override params 构造 */

void setUp(void) {
    sim_reset_time();
    pal_pwm_router_reset();
}
void tearDown(void) {}

/* ---- init 契约（Phase 2 Task 2-1）---- */
void test_init_null_returns_invalid_arg(void) {
    dal_servo_config_t cfg = { .pwm_channel = 0, .min_pulse_ms = 0.5f, .max_pulse_ms = 2.5f };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_servo_init(NULL, &cfg));
    dal_servo_t dev = {0};
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_servo_init(&dev, NULL));
}

void test_init_rejects_invalid_pulse_range(void) {
    dal_servo_t dev = {0};
    dal_servo_config_t zero_min  = { .pwm_channel = 0, .min_pulse_ms = 0.0f, .max_pulse_ms = 2.5f };
    dal_servo_config_t inverted  = { .pwm_channel = 0, .min_pulse_ms = 2.5f, .max_pulse_ms = 0.5f };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_servo_init(&dev, &zero_min));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_servo_init(&dev, &inverted));
}

void test_set_angle_before_init_returns_not_initialized(void) {
    /* initialized 默认 false（未 init） */
    dal_servo_t dev = { .pwm_channel = 0, .current_angle = 0.0f,
                        .min_pulse_ms = 0.5f, .max_pulse_ms = 2.5f };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_servo_set_angle(&dev, 90.0f));
}

void test_set_angle_null_returns_invalid_arg(void) {
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_servo_set_angle(NULL, 90.0f));
}

/* ---- init 后 set_angle 的角度→占空比映射（继承 Phase 0 常量等值校验）---- */
void test_init_then_set_angle_updates_duty(void) {
    dal_servo_t s = {0};
    dal_servo_config_t cfg = { .pwm_channel = 0, .min_pulse_ms = 0.5f, .max_pulse_ms = 2.5f };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_servo_init(&s, &cfg));
    /* 90° -> 脉宽 0.5+0.5*(2.5-0.5)=1.5ms -> 占空比 (1.5/20)*100 = 7.5% */
    wink_status_t st = dal_servo_set_angle(&s, 90.0f);
    TEST_ASSERT_EQUAL_INT(WINK_OK, st);
    TEST_ASSERT_EQUAL_FLOAT(7.5f, sim_last_pwm_duty(0));
    TEST_ASSERT_EQUAL_FLOAT(90.0f, s.current_angle);
}

void test_init_then_set_angle_clamps_overflow(void) {
    dal_servo_t s = {0};
    dal_servo_config_t cfg = { .pwm_channel = 1, .min_pulse_ms = 0.5f, .max_pulse_ms = 2.5f };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_servo_init(&s, &cfg));
    /* 200° 钳到 180 -> 脉宽 2.5ms -> 占空比 12.5% */
    wink_status_t st_overflow = dal_servo_set_angle(&s, 200.0f);
    TEST_ASSERT_EQUAL_INT(WINK_OK, st_overflow);
    TEST_ASSERT_EQUAL_FLOAT(12.5f, sim_last_pwm_duty(1));
}

/* ---- safe-off（Phase 5 Task 5-2）---- */
void test_safe_off_null_returns_invalid_arg(void) {
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_servo_safe_off(NULL));
}

void test_safe_off_before_init_returns_not_initialized(void) {
    dal_servo_t dev = { .pwm_channel = 0, .min_pulse_ms = 0.5f, .max_pulse_ms = 2.5f };  /* !initialized */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_servo_safe_off(&dev));
}

void test_safe_off_after_init_sets_zero_duty(void) {
    dal_servo_t s = {0};
    dal_servo_config_t cfg = { .pwm_channel = 2, .min_pulse_ms = 0.5f, .max_pulse_ms = 2.5f };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_servo_init(&s, &cfg));
    wink_status_t sa = dal_servo_set_angle(&s, 90.0f);   /* 先设非零占空比 */
    (void)sa;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_servo_safe_off(&s));
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
    dal_servo_t s = { .pwm_channel = 0, .min_pulse_ms = 0.5f, .max_pulse_ms = 2.5f };
    uint8_t p[16];
    build_servo_params(p, 3, 0.6f, 2.4f);
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_servo_apply_override(&s, p, sizeof p));
    TEST_ASSERT_EQUAL_UINT8(3u, s.pwm_channel);
    TEST_ASSERT_EQUAL_FLOAT(0.6f, s.min_pulse_ms);
    TEST_ASSERT_EQUAL_FLOAT(2.4f, s.max_pulse_ms);
}

void test_apply_override_rejects_invalid_pulse(void) {
    dal_servo_t s = { .pwm_channel = 0, .min_pulse_ms = 0.5f, .max_pulse_ms = 2.5f };
    uint8_t p[16];
    build_servo_params(p, 0, 0.0f, 2.5f);            /* min == 0 */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_servo_apply_override(&s, p, sizeof p));
    build_servo_params(p, 0, 2.5f, 0.5f);            /* max <= min */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_servo_apply_override(&s, p, sizeof p));
    /* 非法 → 字段保持不变（绝不写半状态） */
    TEST_ASSERT_EQUAL_FLOAT(0.5f, s.min_pulse_ms);
    TEST_ASSERT_EQUAL_FLOAT(2.5f, s.max_pulse_ms);
}

void test_apply_override_rejects_bad_channel(void) {
    dal_servo_t s = { .pwm_channel = 0, .min_pulse_ms = 0.5f, .max_pulse_ms = 2.5f };
    uint8_t p[16];
    build_servo_params(p, PAL_PWM_CHANNELS, 0.5f, 2.5f);   /* channel 越界 */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_servo_apply_override(&s, p, sizeof p));
}

void test_apply_override_null_returns_invalid_arg(void) {
    uint8_t p[16] = {0};
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_servo_apply_override(NULL, p, sizeof p));
    dal_servo_t s = {0};
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_servo_apply_override(&s, NULL, sizeof p));
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
    return UNITY_END();
}
