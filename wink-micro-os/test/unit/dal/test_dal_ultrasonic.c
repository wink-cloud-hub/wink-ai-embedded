#include "unity.h"
#include "wink_status.h"
#include "dal_ultrasonic.h"
#include "pal_resource.h"
#include "host_test_ctrl.h"
#include <time.h>
#include <string.h>   /* ADR-0008 apply_override params 构造 */

/* ADR-0017：dal_ultrasonic_read 挂上 WINK_BLOCKING（=deprecated 属性）后，
 * 本文件对该 API 的契约守卫调用（5 处）会在 -Wall -Wextra -Werror 下变为
 * -Werror=deprecated-declarations 硬错。这是 blocking-API 深度防御的**过渡期例外**
 * （见 ADR-0017 §Consequences「保留过渡期能力：host 单测继续可用」）——
 * 单测本就是契约守卫，deprecation 告警对它无意义；协作式调度器构建路径经
 * -DWINK_STRICT_NONBLOCKING=1 从符号表剔除后，此单测自动不参与那条链，无 gap。
 * MSVC/其它编译器无 -Wdeprecated-declarations，编译期告警本就退化为空，pragma 无副作用。 */
#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

static const char *const OWNER = "test_dal_ultrasonic";

void setUp(void) { sim_reset_time(); pal_resource_reset(); }
void tearDown(void) {}

/* ---- init 契约（Phase 2 Task 2-2）---- */
void test_ultrasonic_init_null_returns_invalid_arg(void) {
    const dal_ultrasonic_config_t cfg = { .owner = OWNER, .trig_pin = 4, .echo_pin = 5, .use_rmt = false };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_ultrasonic_init(NULL, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_ultrasonic_init(NULL, NULL));
}

void test_ultrasonic_init_rejects_same_pin(void) {
    dal_ultrasonic_t dev = {0};
    const dal_ultrasonic_config_t cfg = { .owner = OWNER, .trig_pin = 5, .echo_pin = 5, .use_rmt = false };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_ultrasonic_init(&dev, &cfg));
}

void test_ultrasonic_read_before_init_returns_not_initialized(void) {
    /* initialized 默认 false（未 init） */
    dal_ultrasonic_t dev = { .config.trig_pin = 4, .config.echo_pin = 5, .last_distance = 0.0f };
    float dist = 0.0f;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_ultrasonic_read(&dev, &dist));
}

void test_read_null_returns_invalid_arg(void) {
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_ultrasonic_read(NULL, (float[]){0}));
}

void test_read_null_out_returns_invalid_arg(void) {
    dal_ultrasonic_t dev = {0};
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_ultrasonic_read(&dev, NULL));
}

/* ---- 共享换算纯函数 ---- */
extern float dal_pulse_us_to_cm(uint32_t pulse_us);

void test_pulse_to_cm_100cm(void) {
    /* 100cm -> 往返 200cm -> ≈5882us；0.017*5882 ≈ 99.994 */
    TEST_ASSERT_EQUAL_FLOAT(99.994f, dal_pulse_us_to_cm(5882));
}

/* ---- 真机分支脉宽测量集成（init 后；host 协作式时间）---- */
void test_ultrasonic_init_then_read_real_measure_pulse(void) {
    dal_ultrasonic_t dev = {0};
    const dal_ultrasonic_config_t cfg = { .owner = OWNER, .trig_pin = 4, .echo_pin = 5, .use_rmt = false };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ultrasonic_init(&dev, &cfg));
    sim_set_echo_pin(5);
    sim_set_echo_timing(100, 5882);   /* rise@100us, high 5882us ≈100cm */
    float dist = 0.0f;
    wink_status_t s = dal_ultrasonic_read(&dev, &dist);
    TEST_ASSERT_EQUAL_INT(WINK_OK, s);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 99.994f, dist);
}

void test_ultrasonic_init_then_read_real_timeout(void) {
    dal_ultrasonic_t dev = {0};
    const dal_ultrasonic_config_t cfg = { .owner = OWNER, .trig_pin = 4, .echo_pin = 5, .use_rmt = false };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ultrasonic_init(&dev, &cfg));
    sim_set_echo_pin(5);
    sim_set_echo_timing(100000, 1000);  /* rise > 30ms 上限 */
    float dist = 0.0f;
    wink_status_t s = dal_ultrasonic_read(&dev, &dist);
    TEST_ASSERT_EQUAL_INT(WINK_ERR_TIMEOUT, s);
}

/* ---- 非阻塞状态机（Phase 4 Task 4-3；host 单 tick 同步 ready）---- */
void test_nonblocking_get_cached_before_request_returns_empty(void) {
    dal_ultrasonic_t dev = {0};
    const dal_ultrasonic_config_t cfg = { .owner = OWNER, .trig_pin = 4, .echo_pin = 5, .use_rmt = false };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ultrasonic_init(&dev, &cfg));
    float dist = 0.0f;
    /* DAL-B-024: state == IDLE (no request yet) -> WINK_ERR_EMPTY ("无数据"语义)
     * 不得返回 WINK_ERR_BUSY（BUSY 仅保留给 MEASURING 传输中） */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_EMPTY, dal_ultrasonic_get_cached_distance(&dev, &dist));
}

void test_nonblocking_request_then_get_cached_returns_distance(void) {
    dal_ultrasonic_t dev = {0};
    const dal_ultrasonic_config_t cfg = { .owner = OWNER, .trig_pin = 4, .echo_pin = 5, .use_rmt = false };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ultrasonic_init(&dev, &cfg));
    sim_set_echo_pin(5);
    sim_set_echo_timing(100, 5882);   /* rise@100us, high 5882us ≈100cm */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ultrasonic_request_measurement(&dev));
    float dist = 0.0f;
    wink_status_t s = dal_ultrasonic_get_cached_distance(&dev, &dist);
    TEST_ASSERT_EQUAL_INT(WINK_OK, s);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 99.994f, dist);
}

void test_nonblocking_request_timeout_returns_error_status(void) {
    dal_ultrasonic_t dev = {0};
    const dal_ultrasonic_config_t cfg = { .owner = OWNER, .trig_pin = 4, .echo_pin = 5, .use_rmt = false };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ultrasonic_init(&dev, &cfg));
    sim_set_echo_pin(5);
    sim_set_echo_timing(100000, 1000);   /* rise > 30ms → pulse_in TIMEOUT */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ultrasonic_request_measurement(&dev));
    float dist = 0.0f;
    wink_status_t s = dal_ultrasonic_get_cached_distance(&dev, &dist);
    TEST_ASSERT_EQUAL_INT(WINK_ERR_TIMEOUT, s);
}

/* Phase 4 Task 4-6 墙钟守卫：单 tick 超声波路径用虚拟时间，无真实阻塞泄漏到墙钟。
 * 阈值取 100ms（>> clock 粒度，且远小于旧 blocking worst-case ≈60ms 的真实阻塞风险面）。 */
void test_nonblocking_single_tick_wallclock_is_small(void) {
    dal_ultrasonic_t dev = {0};
    const dal_ultrasonic_config_t cfg = { .owner = OWNER, .trig_pin = 4, .echo_pin = 5, .use_rmt = false };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ultrasonic_init(&dev, &cfg));
    sim_set_echo_pin(5);
    sim_set_echo_timing(100, 5882);
    clock_t t0 = clock();
    for (int i = 0; i < 1000; i++) {   /* 重复 1000 次放大可测性 */
        wink_status_t rq = dal_ultrasonic_request_measurement(&dev); (void)rq;
        float dist = 0.0f;
        wink_status_t gc = dal_ultrasonic_get_cached_distance(&dev, &dist); (void)gc;
    }
    clock_t dt = clock() - t0;
    /* 1000 次单 tick 路径应远 < 100ms（即每次 < 100us 量级）；防止真实阻塞泄漏 */
    TEST_ASSERT(dt < (clock_t)(CLOCKS_PER_SEC / 10));
}

/* ---- ADR-0008 Flash 覆写 apply_override（init 前引脚改写 + 轻校验）---- */
/* params 布局（小端）：trig_pin:u16@0, echo_pin:u16@2 (buf=16B) */
static void build_radar_params(uint8_t *p, uint16_t trig, uint16_t echo) {
    memset(p, 0, 16);
    memcpy(p + 0, &trig, 2);
    memcpy(p + 2, &echo, 2);
}

void test_apply_override_writes_pins(void) {
    dal_ultrasonic_t u = { .config.trig_pin = 4, .config.echo_pin = 5 };
    uint8_t p[16];
    build_radar_params(p, 6, 7);
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ultrasonic_apply_override(&u, p, sizeof p));
    TEST_ASSERT_EQUAL_UINT16(6, u.config.trig_pin);
    TEST_ASSERT_EQUAL_UINT16(7, u.config.echo_pin);
}

void test_apply_override_rejects_same_pin(void) {
    dal_ultrasonic_t u = { .config.trig_pin = 4, .config.echo_pin = 5 };
    uint8_t p[16];
    build_radar_params(p, 8, 8);
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_ultrasonic_apply_override(&u, p, sizeof p));
    /* 非法 → 字段保持不变 */
    TEST_ASSERT_EQUAL_UINT16(4, u.config.trig_pin);
    TEST_ASSERT_EQUAL_UINT16(5, u.config.echo_pin);
}

void test_apply_override_null_returns_invalid_arg(void) {
    uint8_t p[16] = {0};
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_ultrasonic_apply_override(NULL, p, sizeof p));
    dal_ultrasonic_t u = {0};
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_ultrasonic_apply_override(&u, NULL, sizeof p));
}

/* DAL-BC-012: v1 wire format with explicit schema_version byte. */
void test_apply_override_v1_writes_pins(void) {
    dal_ultrasonic_t u = { .config.trig_pin = 4, .config.echo_pin = 5 };
    uint8_t p[16] = {0};
    p[0] = 0x01u;                        /* schema_version = v1 */
    uint16_t trig = 6, echo = 7;
    memcpy(p + 1, &trig, 2);
    memcpy(p + 3, &echo, 2);
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ultrasonic_apply_override(&u, p, sizeof p));
    TEST_ASSERT_EQUAL_UINT16(6, u.config.trig_pin);
    TEST_ASSERT_EQUAL_UINT16(7, u.config.echo_pin);
}

/* DAL-BC-012: too-short payload rejected for both v0 and v1. */
void test_apply_override_too_short_rejected(void) {
    dal_ultrasonic_t u = { .config.trig_pin = 4, .config.echo_pin = 5 };
    uint8_t p[3] = {0x06, 0x00, 0x07};   /* 3B < v0 minimum 4B */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_ultrasonic_apply_override(&u, p, sizeof p));
    /* 字段保持不变 */
    TEST_ASSERT_EQUAL_UINT16(4, u.config.trig_pin);
    TEST_ASSERT_EQUAL_UINT16(5, u.config.echo_pin);
}

void test_deinit_hardening(void) {
    dal_ultrasonic_t dev = {0};
    const dal_ultrasonic_config_t cfg = { .owner = "radar0", .trig_pin = 4, .echo_pin = 5, .use_rmt = false };

    /* 1. NULL safety */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_ultrasonic_deinit(NULL));

    /* 2. Idempotency on uninitialized dev */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ultrasonic_deinit(&dev));

    /* 3. Successful deinit and resource release */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ultrasonic_init(&dev, &cfg));
    TEST_ASSERT_TRUE(dev.initialized);
    TEST_ASSERT_TRUE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 4));
    TEST_ASSERT_TRUE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 5));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ultrasonic_deinit(&dev));
    TEST_ASSERT_FALSE(dev.initialized);
    TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 4));
    TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 5));

    /* 4. Idempotency after deinit */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ultrasonic_deinit(&dev));
}

/* ADR-0024 §4 #8 idempotency — Task 0.7 Step 4: 10-round init→deinit loop.
 * Ultrasonic owns TWO GPIO pins (trig+echo); a leak on either side would
 * surface as BUSY on the next init. Guard for S11 regression. */
void test_deinit_loop_two_pins_no_resource_leak(void) {
    dal_ultrasonic_t dev = {0};
    const dal_ultrasonic_config_t cfg = {
        .owner = "radar_loop", .trig_pin = 6, .echo_pin = 7, .use_rmt = false,
    };
    for (int round = 0; round < 10; round++) {
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ultrasonic_init(&dev, &cfg));
        TEST_ASSERT_TRUE(dev.initialized);
        TEST_ASSERT_TRUE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 6));
        TEST_ASSERT_TRUE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 7));
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ultrasonic_deinit(&dev));
        TEST_ASSERT_FALSE(dev.initialized);
        TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 6));
        TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 7));
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ultrasonic_init_null_returns_invalid_arg);
    RUN_TEST(test_ultrasonic_init_rejects_same_pin);
    RUN_TEST(test_ultrasonic_read_before_init_returns_not_initialized);
    RUN_TEST(test_read_null_returns_invalid_arg);
    RUN_TEST(test_read_null_out_returns_invalid_arg);
    RUN_TEST(test_pulse_to_cm_100cm);
    RUN_TEST(test_ultrasonic_init_then_read_real_measure_pulse);
    RUN_TEST(test_ultrasonic_init_then_read_real_timeout);
    RUN_TEST(test_nonblocking_get_cached_before_request_returns_empty);
    RUN_TEST(test_nonblocking_request_then_get_cached_returns_distance);
    RUN_TEST(test_nonblocking_request_timeout_returns_error_status);
    RUN_TEST(test_nonblocking_single_tick_wallclock_is_small);
    RUN_TEST(test_apply_override_writes_pins);
    RUN_TEST(test_apply_override_rejects_same_pin);
    RUN_TEST(test_apply_override_null_returns_invalid_arg);
    RUN_TEST(test_apply_override_v1_writes_pins);
    RUN_TEST(test_apply_override_too_short_rejected);
    RUN_TEST(test_deinit_hardening);
    RUN_TEST(test_deinit_loop_two_pins_no_resource_leak);
    return UNITY_END();
}

#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic pop
#endif
