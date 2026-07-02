#include "unity.h"
#include "dal_button.h"
#include "pal_hal.h"           /* pal_gpio_read for raw reads */
#include "pal_osal.h"          /* pal_os_sleep_ms 推进虚拟时钟 */
#include "pal_resource.h"
#include "host_test_ctrl.h"
#include "wink_sim_physical.h"

void setUp(void) { sim_reset_time(); pal_resource_reset(); }
void tearDown(void) {}

/* 场景参数（强制交替模型，采样周期无关）：
 *   TICK_MS=10（对齐系统 WINK_RUNTIME_TICK_MS=10，wink_status.h:63），bounce_us=30000（30ms 抖动窗，窗内 3 个采样点）。
 *   active_low 按键：释放 raw=true，按下 raw=false。dal_button 计数去抖阈值=3（dal_button.h:13）。 */
#define TICK_MS 10
#define BOUNCE_US 30000u

static void run_ticks(dal_button_t *btn, int n) {
    for (int i = 0; i < n; i++) {
        pal_os_sleep_ms(TICK_MS);
        TEST_ASSERT_EQUAL(WINK_OK, dal_button_poll(btn));
    }
}

/* 负对照 helper：无去抖的裸采样（模拟「开发者没写去抖」），与 dal_button.c:6 button_raw_pressed 同语义。 */
static bool raw_pressed(uint16_t pin, bool active_low) {
    return pal_gpio_read(pin) != active_low;
}

/* 【主线·正】电平跃变 → dal_button 计数去抖吸收抖动 → 稳定 pressed。
 * golden（强制交替；跃变 set 后首次 poll now=30000，bounce 窗 [30000,60000)）：
 *   窗内每 tick raw 强制翻转 → pressed 在 true/false 间跳 → dal_button counter 反复清零；
 *   出窗（now=60000 起）raw 稳定=false→pressed=true 连续，counter 累积到 3（now=80000）→ stable_pressed=true。
 *   故 run_ticks(6) = 3 窗内采样 + 3 出窗去抖。 */
void test_dal_button_absorbs_bounce_and_settles(void) {
    wink_sim_faults_t f = WINK_SIM_FAULTS_IDEAL;
    f.bounce_us = BOUNCE_US; f.prng_seed = 1;
    sim_set_faults(&f);

    dal_button_t btn;
    const dal_button_config_t cfg = { .owner = "e2e_debounce_bounce", .pin = 7, .active_low = true };
    TEST_ASSERT_EQUAL(WINK_OK, dal_button_init(&btn, &cfg));   /* active_low */
    sim_set_gpio_ideal(7, true);                                   /* ① 上电态=释放(raw=true)，不抖 */
    run_ticks(&btn, 2);                                            /* now=20000，稳定到「未按下」 */
    bool released = true;
    TEST_ASSERT_EQUAL(WINK_OK, dal_button_is_pressed(&btn, &released));
    TEST_ASSERT_FALSE(released);

    sim_set_gpio_ideal(7, false);                                  /* ② 跃变=按下(raw=false) → 抖动窗 */
    run_ticks(&btn, 6);                                            /* now=30000..80000：3 窗内 + 3 出窗去抖 */

    bool pressed = false;
    TEST_ASSERT_EQUAL(WINK_OK, dal_button_is_pressed(&btn, &pressed));
    TEST_ASSERT_TRUE(pressed);                                     /* 去抖吸收抖动，稳定按下 */
    sim_clear_gpio_ideal();
}

/* 【主线·负对照】同一抖动电平序列，无去抖裸采样 → 抖动窗内必跳变（误触发）。
 * 证明 ADR-0009 §3.1 核心论点「不写去抖则多次误触发」。强制交替保证窗内既采到 pressed 又 released。 */
void test_raw_read_without_debounce_bounces(void) {
    wink_sim_faults_t f = WINK_SIM_FAULTS_IDEAL;
    f.bounce_us = BOUNCE_US; f.prng_seed = 1;
    sim_set_faults(&f);

    sim_set_gpio_ideal(9, true);                                   /* 上电=释放（pin9，避耦合） */
    pal_os_sleep_ms(TICK_MS);                                         /* now=10000 */
    sim_set_gpio_ideal(9, false);                                  /* 跃变=按下 → 窗 [10000,40000) */

    bool saw_pressed = false, saw_released = false;
    for (int i = 0; i < 3; i++) {                                  /* 窗内 3 次裸采样（强制交替必跳变） */
        if (raw_pressed(9, true)) { saw_pressed = true; }
        else { saw_released = true; }
        pal_os_sleep_ms(TICK_MS);                                     /* +10ms，仍在窗内（10000→20000→30000） */
    }
    TEST_ASSERT_TRUE(saw_pressed && saw_released);                 /* 无去抖 → 既「按下」又「释放」=误触发 */
    sim_clear_gpio_ideal();
}

/* 【基线】无退化（bounce_us=0）→ 快速稳定，无抖动 */
void test_no_bounce_config_settles_fast(void) {
    sim_set_faults(&WINK_SIM_FAULTS_IDEAL);                        /* bounce_us=0 */
    dal_button_t btn;
    const dal_button_config_t cfg = { .owner = "e2e_debounce_baseline", .pin = 8, .active_low = false };
    TEST_ASSERT_EQUAL(WINK_OK, dal_button_init(&btn, &cfg));   /* active_high */
    sim_set_gpio_ideal(8, true);                                   /* 按下 raw=true */
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
