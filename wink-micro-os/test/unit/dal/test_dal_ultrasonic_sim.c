/* 核心：证明仿真分支同样调用 dal_pulse_us_to_cm，输出 == 真机分支对同一脉宽的换算。
 * 这是 ADR-0003 决策2「两端同源」的回归守卫——host 真机测试只覆盖 #else。
 * Phase 2：sim 分支 dal_ultrasonic_init 跳过物理 GPIO，仅置 initialized=true。 */
#include "unity.h"
#include "wink_status.h"
#include "dal_ultrasonic.h"
#include "pal_resource.h"
#include "js_sim_host_stub.h"

/* ADR-0017：dal_ultrasonic_read 挂 WINK_BLOCKING 后，本文件对其调用属过渡期例外，
 * 见 test_dal_ultrasonic.c 顶部同款说明。 */
#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

static const char *const OWNER = "test_dal_ultrasonic_sim";

extern float dal_pulse_us_to_cm(uint32_t pulse_us);

void setUp(void) { sim_set_echo_pulse_us(0); pal_resource_reset(); }
void tearDown(void) {}

void test_sim_read_uses_shared_conversion(void) {
    sim_set_echo_pulse_us(5882);
    dal_ultrasonic_t dev = {0};
    const dal_ultrasonic_config_t cfg = { .owner = OWNER, .trig_pin = 4, .echo_pin = 5, .use_rmt = false };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ultrasonic_init(&dev, &cfg));
    float dist = 0.0f;
    wink_status_t s = dal_ultrasonic_read(&dev, &dist);
    TEST_ASSERT_EQUAL_INT(WINK_OK, s);
    /* 与真机分支 test_ultrasonic_init_then_read_real_measure_pulse 同一脉宽 → 同一距离（两端同源铁证） */
    TEST_ASSERT_EQUAL_FLOAT(dal_pulse_us_to_cm(5882), dist);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 99.994f, dist);
}

void test_sim_read_timeout_when_pulse_exceeds_limit(void) {
    /* RMT backend idle_thres=25ms + max-valid pulse=25ms forced ULTRASONIC_TIMEOUT_US
     * to 60ms (see dal_ultrasonic.c). Set the simulated pulse to 61ms so it still
     * exceeds the timeout and exercises the TIMEOUT return path. */
    sim_set_echo_pulse_us(61000);   /* ≥ ULTRASONIC_TIMEOUT_US (60ms) */
    dal_ultrasonic_t dev = {0};
    const dal_ultrasonic_config_t cfg = { .owner = OWNER, .trig_pin = 4, .echo_pin = 5, .use_rmt = false };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ultrasonic_init(&dev, &cfg));
    float dist = 0.0f;
    wink_status_t s = dal_ultrasonic_read(&dev, &dist);
    TEST_ASSERT_EQUAL_INT(WINK_ERR_TIMEOUT, s);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_sim_read_uses_shared_conversion);
    RUN_TEST(test_sim_read_timeout_when_pulse_exceeds_limit);
    return UNITY_END();
}

#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic pop
#endif
