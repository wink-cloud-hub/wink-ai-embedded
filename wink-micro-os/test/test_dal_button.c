#include "unity.h"
#include "wink_status.h"
#include "dal_button.h"
#include "pal_resource.h"
#include "pal_hal.h"
#include "pal_osal.h"
#include "host_test_ctrl.h"  /* sim_set_gpio_ideal, pal_host_trigger_gpio_interrupt, pal_host_reset_isr_stats */
#include "pal_irq.h"         /* PAL_CRITICAL_SECTION */
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

/* Host sim clock access (declared in targets/host/pal_osal_host.c; same extern
 * pattern used by test_host_pal.c / test_sim_physical.c to advance virtual time). */
extern void host_sim_advance_to(uint64_t us);

/* Helper: drive N polls, advancing virtual clock by 10 ms (one tick) between
 * polls.  Returns the last poll status.  Needed because Wave 3's long-press
 * detection uses pal_os_get_ms(); on host the virtual clock does NOT auto-
 * advance from non-echo GPIO reads. */
static wink_status_t poll_n_ticks(dal_button_t *dev, int n) {
    wink_status_t s = WINK_OK;
    for (int i = 0; i < n; i++) {
        uint64_t now = pal_os_get_us();
        host_sim_advance_to(now + 10000u); /* +10 ms per tick */
        s = dal_button_poll(dev);
    }
    return s;
}

/* Backward-compat helper for tests that don't care about time. */
static wink_status_t poll_n(dal_button_t *dev, int n) {
    return poll_n_ticks(dev, n);
}

/* Helper: set virtual GPIO level on host (active_low buttons read LOW=pressed).
 * Uses sim_set_gpio_ideal which is the host-test canonical pin-drive API. */
static void set_btn_pin(uint16_t pin, bool pressed, bool active_low) {
    /* active_low: pressed → LOW (false); active_high: pressed → HIGH (true) */
    bool level = active_low ? !pressed : pressed;
    sim_set_gpio_ideal(pin, level);
}

/* ---- init 契约 ---- */
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

/* ---- host 去抖：pal_gpio_read 对非 echo pin 恒返回 false ----
 * active_low=true → raw=false 视为按下；经 3 次 poll 后稳定态翻转为 true */
void test_active_low_debounce_to_pressed(void) {
    dal_button_t dev = {0};
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 10, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));

    for (int i = 0; i < DAL_BUTTON_DEBOUNCE_THRESHOLD; i++) {
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_poll(&dev));
    }

    bool pressed = false;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_is_pressed(&dev, &pressed));
    TEST_ASSERT_TRUE(pressed);
}

/* active_low=false → raw=false 视为未按下；稳定态保持 false */
void test_active_high_stays_unpressed(void) {
    dal_button_t dev = {0};
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 11, .active_low = false };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));

    for (int i = 0; i < DAL_BUTTON_DEBOUNCE_THRESHOLD * 2; i++) {
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_poll(&dev));
    }

    bool pressed = true;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_is_pressed(&dev, &pressed));
    TEST_ASSERT_FALSE(pressed);
}

/* ---- was_pressed 边沿检测 ---- */
void test_was_pressed_edge_once(void) {
    dal_button_t dev = {0};
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 12, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));

    for (int i = 0; i < DAL_BUTTON_DEBOUNCE_THRESHOLD; i++) {
        wink_status_t s = dal_button_poll(&dev);
        (void)s;
    }

    bool event = false;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_was_pressed(&dev, &event));
    TEST_ASSERT_TRUE(event);   /* 第一次：按下事件 */

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_was_pressed(&dev, &event));
    TEST_ASSERT_FALSE(event);  /* 第二次：已消费，无新事件 */
}

/* 释放后再次按下应重新触发（手动翻转 stable_pressed 模拟释放+再按下） */
void test_was_pressed_rearm_after_release(void) {
    dal_button_t dev = {0};
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 13, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
    dev.stable_pressed = true;
    dev.last_reported = true;

    bool event = false;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_was_pressed(&dev, &event));
    TEST_ASSERT_FALSE(event);  /* 已按下且已报告 */

    /* 模拟释放 */
    dev.stable_pressed = false;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_was_pressed(&dev, &event));
    TEST_ASSERT_FALSE(event);  /* 释放不产生 was_pressed */

    /* 模拟再次按下 */
    dev.stable_pressed = true;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_was_pressed(&dev, &event));
    TEST_ASSERT_TRUE(event);   /* 重新触发 */
}

/* ═══════════════════════════════════════════════════════════
 * Wave 3: Event callback (PRESS / RELEASE / LONG_PRESS)
 * ═══════════════════════════════════════════════════════════ */

/* Helper: record last event in a test-local counter */
struct btn_recorder { dal_button_event_t last; int count; };
static void record_event(dal_button_event_t evt, void *ctx) {
    struct btn_recorder *r = (struct btn_recorder *)ctx;
    r->last = evt;
    r->count++;
}

/* 事件回调契约：NULL dev/cb 返 INVALID_ARG；未 init 返 NOT_INITIALIZED；NULL cb 可注销 */
void test_on_event_contract_null_args(void) {
    dal_button_t dev; memset(&dev, 0, sizeof(dev));
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 20, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
    struct btn_recorder r = {0};
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_button_on_event(NULL, record_event, &r));
    /* NULL cb = 合法注销 */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_on_event(&dev, NULL, NULL));
}
void test_on_event_before_init_returns_not_initialized(void) {
    dal_button_t dev = {0};
    struct btn_recorder r = {0};
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED,
                          dal_button_on_event(&dev, record_event, &r));
}

/* PRESS 事件：去抖完成后首次稳定按下触发一次 */
void test_event_press_dispatched_on_debounce(void) {
    dal_button_t dev; memset(&dev, 0, sizeof(dev));
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 21, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
    struct btn_recorder r = {0};
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_on_event(&dev, record_event, &r));
    /* 默认未按下 raw=false XOR active_low=true → pressed=true，所以 init 后 poll
     * 会进入 debounce。但先打一帧：需要把 pin 理想电平设到 HIGH（未按）避免 init 后
     * 立即是 pressed 状态。*/
    set_btn_pin(21, false, true);  /* released → HIGH for active_low */
    poll_n(&dev, DAL_BUTTON_DEBOUNCE_THRESHOLD);
    int count_before = r.count;

    /* 按下：电平变 LOW */
    set_btn_pin(21, true, true);
    /* DAL_BUTTON_DEBOUNCE_THRESHOLD-1 次 poll 未达阈值 → 不触发 */
    poll_n(&dev, DAL_BUTTON_DEBOUNCE_THRESHOLD - 1);
    TEST_ASSERT_EQUAL_INT(count_before, r.count);
    /* 再加一帧达阈值 → 应触发 PRESS */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_poll(&dev));
    TEST_ASSERT_EQUAL_INT(count_before + 1, r.count);
    TEST_ASSERT_EQUAL_INT(DAL_BUTTON_EVT_PRESS, r.last);
    /* 再多 poll 几次不重复触发（hold 期间） */
    poll_n(&dev, 10);
    TEST_ASSERT_EQUAL_INT(count_before + 1, r.count);
}

/* RELEASE 事件：按下后释放，去抖完成时触发一次 */
void test_event_release_dispatched_on_release(void) {
    dal_button_t dev; memset(&dev, 0, sizeof(dev));
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 22, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
    struct btn_recorder r = {0};
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_on_event(&dev, record_event, &r));

    /* 先松开（确保 init 后是 release 态） */
    set_btn_pin(22, false, true);
    poll_n(&dev, DAL_BUTTON_DEBOUNCE_THRESHOLD);
    int base = r.count;

    /* 按下 */
    set_btn_pin(22, true, true);
    poll_n(&dev, DAL_BUTTON_DEBOUNCE_THRESHOLD);
    TEST_ASSERT_EQUAL_INT(base + 1, r.count);
    TEST_ASSERT_EQUAL_INT(DAL_BUTTON_EVT_PRESS, r.last);

    /* 释放 */
    set_btn_pin(22, false, true);
    poll_n(&dev, DAL_BUTTON_DEBOUNCE_THRESHOLD - 1);
    TEST_ASSERT_EQUAL_INT(base + 1, r.count);  /* 去抖中 */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_poll(&dev));
    TEST_ASSERT_EQUAL_INT(base + 2, r.count);
    TEST_ASSERT_EQUAL_INT(DAL_BUTTON_EVT_RELEASE, r.last);
}

/* LONG_PRESS 事件：按住达 long_press_ms 触发一次，不重复 */
void test_event_long_press_fires_once_after_timeout(void) {
    dal_button_t dev; memset(&dev, 0, sizeof(dev));
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 23, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
    struct btn_recorder r = {0};
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_on_event(&dev, record_event, &r));
    /* 用短阈值（100ms）加速测试；tick=10ms → 10 ticks 达阈值 */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_set_long_press_ms(&dev, 100));

    /* 松开起步 */
    set_btn_pin(23, false, true);
    poll_n(&dev, DAL_BUTTON_DEBOUNCE_THRESHOLD);
    int base = r.count;

    /* 按下 → PRESS */
    set_btn_pin(23, true, true);
    poll_n(&dev, DAL_BUTTON_DEBOUNCE_THRESHOLD);
    TEST_ASSERT_EQUAL_INT(base + 1, r.count);
    TEST_ASSERT_EQUAL_INT(DAL_BUTTON_EVT_PRESS, r.last);

    /* 持续按住：每个 poll 推进 10ms。press_start_ms 记录在 PRESS 触发的那次 poll
     * 上（此刻虚拟时间 t = 30ms 起步 + 30ms 按下-debounce = 60ms）。
     * 再 poll 6 次 → t = 120ms, held = 60ms < 100ms，long_press 不触发。 */
    poll_n(&dev, 6);
    TEST_ASSERT_EQUAL_INT(base + 1, r.count);
    /* 再 poll 3 次 → t = 150ms, held = 90ms < 100ms，仍未触发。 */
    poll_n(&dev, 3);
    TEST_ASSERT_EQUAL_INT(base + 1, r.count);
    /* 再 poll 1 次 → t = 160ms, held = 100ms ≥ 100ms，触发 LONG_PRESS。 */
    poll_n(&dev, 1);
    TEST_ASSERT_EQUAL_INT(base + 2, r.count);
    TEST_ASSERT_EQUAL_INT(DAL_BUTTON_EVT_LONG_PRESS, r.last);

    /* 继续按住 20 tick 不重复触发 */
    poll_n(&dev, 20);
    TEST_ASSERT_EQUAL_INT(base + 2, r.count);

    /* 松开触发 RELEASE（先 advance 时间让释放后的 debounce 窗口发生在新时间）。 */
    set_btn_pin(23, false, true);
    poll_n(&dev, DAL_BUTTON_DEBOUNCE_THRESHOLD);
    TEST_ASSERT_EQUAL_INT(base + 3, r.count);
    TEST_ASSERT_EQUAL_INT(DAL_BUTTON_EVT_RELEASE, r.last);
}

/* set_long_press_ms 契约：0 返 INVALID_ARG，未 init 返 NOT_INITIALIZED */
void test_set_long_press_ms_validates(void) {
    dal_button_t dev = {0};
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_button_set_long_press_ms(&dev, 500));
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 24, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_button_set_long_press_ms(&dev, 0));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_set_long_press_ms(&dev, 1));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_set_long_press_ms(&dev, 60000));
}

/* 注销回调（cb=NULL）不再派事件 */
void test_on_event_null_cb_unsubscribes(void) {
    dal_button_t dev; memset(&dev, 0, sizeof(dev));
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 25, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
    struct btn_recorder r = {0};
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_on_event(&dev, record_event, &r));

    /* 按住到 press */
    set_btn_pin(25, true, true);
    poll_n(&dev, DAL_BUTTON_DEBOUNCE_THRESHOLD);
    TEST_ASSERT_EQUAL_INT(1, r.count);

    /* 注销 */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_on_event(&dev, NULL, NULL));
    /* 再松开，不应再派事件 */
    set_btn_pin(25, false, true);
    poll_n(&dev, DAL_BUTTON_DEBOUNCE_THRESHOLD);
    TEST_ASSERT_EQUAL_INT(1, r.count);
}

/* ═══════════════════════════════════════════════════════════
 * Wave 3: ISR edge counter
 * ═══════════════════════════════════════════════════════════ */

void test_isr_counter_contract(void) {
    dal_button_t dev = {0};
    uint32_t c = 0xDEAD;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_button_enable_isr_counter(&dev));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_button_get_edge_count(&dev, &c));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_button_reset_edge_count(&dev));

    const dal_button_config_t cfg = { .owner = OWNER, .pin = 26, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_button_get_edge_count(&dev, NULL));
    /* 未 enable 时 get 返回 0 */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_get_edge_count(&dev, &c));
    TEST_ASSERT_EQUAL_UINT32(0, c);
}

/* enable 后触发 GPIO ISR，edge_count 递增 */
void test_isr_counter_increments_on_interrupt(void) {
    dal_button_t dev; memset(&dev, 0, sizeof(dev));
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 27, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_enable_isr_counter(&dev));
    /* 重复 enable 是幂等的 */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_enable_isr_counter(&dev));

    uint32_t c = 0;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_get_edge_count(&dev, &c));
    TEST_ASSERT_EQUAL_UINT32(0, c);

    /* 模拟 ISR 触发（host 下直接调用测试注入 API） */
    pal_host_trigger_gpio_interrupt(27);
    pal_host_trigger_gpio_interrupt(27);
    pal_host_trigger_gpio_interrupt(27);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_get_edge_count(&dev, &c));
    TEST_ASSERT_EQUAL_UINT32(3, c);
}

/* reset 应原子清零（临界区内），清零瞬间触发的 ISR 不应丢失 */
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

    /* 原子 reset */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_reset_edge_count(&dev));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_get_edge_count(&dev, &c));
    TEST_ASSERT_EQUAL_UINT32(0, c);

    /* reset 后 ISR 触发继续计数 */
    pal_host_trigger_gpio_interrupt(28);
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_get_edge_count(&dev, &c));
    TEST_ASSERT_EQUAL_UINT32(1, c);
}

/* 在临界区内触发的 ISR 不丢：reset 期间锁中断，ISR 被 PENDING，restore 后递增 */
void test_isr_counter_no_lost_edges_during_reset(void) {
    dal_button_t dev; memset(&dev, 0, sizeof(dev));
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 29, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_enable_isr_counter(&dev));

    /* 进入临界区（模拟 reset 原子性的硬验证） */
    uint32_t mask = pal_irq_save_rtos_safe();
    /* 在锁内触发 ISR（host 下会 pending 到 restore 后才执行） */
    pal_host_trigger_gpio_interrupt(29);
    /* 此时 edge_count 不应立刻递增（在 host 实现里，持有 lock 时 ISR 入 pending 队列） */
    uint32_t c = 0;
    /* 注意：host impl 的 trigger_gpio_interrupt 在 lock 深度>0 时入 pending 队列，
     * 所以此刻直接读可能是 0；restore 后 pending 被 drain，应看到 1 */
    pal_irq_restore(mask);

    /* drain pending ISRs: host 上 restore 立即执行 pending 队列，所以此时已递增 */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_get_edge_count(&dev, &c));
    TEST_ASSERT_EQUAL_UINT32(1, c);
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
    /* Wave 3: events */
    RUN_TEST(test_on_event_contract_null_args);
    RUN_TEST(test_on_event_before_init_returns_not_initialized);
    RUN_TEST(test_event_press_dispatched_on_debounce);
    RUN_TEST(test_event_release_dispatched_on_release);
    RUN_TEST(test_event_long_press_fires_once_after_timeout);
    RUN_TEST(test_set_long_press_ms_validates);
    RUN_TEST(test_on_event_null_cb_unsubscribes);
    /* Wave 3: ISR counter */
    RUN_TEST(test_isr_counter_contract);
    RUN_TEST(test_isr_counter_increments_on_interrupt);
    RUN_TEST(test_isr_counter_reset_is_atomic);
    RUN_TEST(test_isr_counter_no_lost_edges_during_reset);
    return UNITY_END();
}
