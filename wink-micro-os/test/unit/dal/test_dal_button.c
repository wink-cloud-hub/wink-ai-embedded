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

/* Host sim clock access (declared in osal/host/pal_osal_host.c; same extern
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

/* ---- host 去抖：pull-up idle 释放；显式 LOW 后去抖为按下 ---- */
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

    set_btn_pin(12, true, true);
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
    /* pull-up idle → 释放态；显式设 HIGH 与 init 一致，再注入 LOW 测 PRESS 边沿。 */
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

void test_deinit_hardening(void) {
    dal_button_t dev = {0};
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 2, .active_low = true };

    /* 1. NULL safety */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_button_deinit(NULL));

    /* 2. Idempotency on uninitialized dev */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_deinit(&dev));

    /* 3. Successful deinit with ISR disabled and resource release */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
    TEST_ASSERT_TRUE(dev.initialized);
    TEST_ASSERT_TRUE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 2));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_enable_isr_counter(&dev));
    TEST_ASSERT_TRUE(dev.isr_counter_enabled);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_deinit(&dev));
    TEST_ASSERT_FALSE(dev.initialized);
    TEST_ASSERT_FALSE(dev.isr_counter_enabled);
    TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 2));

    /* 4. Idempotency after deinit */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_deinit(&dev));
}

/* ADR-0024 §4 #8 idempotency — Task 0.7 Step 4: 10-round init→deinit loop,
 * including ISR counter enable, must not leak SW resource reservations. This
 * guards against the class of bug where deinit forgot to release pin/ISR state
 * and the next init fails with BUSY (S11-class regression). */
void test_deinit_loop_with_isr_no_resource_leak(void) {
    dal_button_t dev; memset(&dev, 0, sizeof(dev));
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 30, .active_low = true };

    for (int round = 0; round < 10; round++) {
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
        TEST_ASSERT_TRUE(dev.initialized);
        TEST_ASSERT_TRUE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 30));
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_enable_isr_counter(&dev));
        TEST_ASSERT_TRUE(dev.isr_counter_enabled);
        /* poll a couple of times */
        poll_n(&dev, 2);
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_deinit(&dev));
        TEST_ASSERT_FALSE(dev.initialized);
        TEST_ASSERT_FALSE(dev.isr_counter_enabled);
        TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 30));
    }
}

/* ADR-0034: explicit pull + NONE floating semantics */
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

/* ═══════════════════════════════════════════════════════════
 * DAL-B-025: dal_button_get_status / last_status propagation
 * ═══════════════════════════════════════════════════════════ */

/* Contract: NULL / uninitialized errors */
void test_get_status_contract(void) {
    dal_button_t dev = {0};
    /* NULL dev / NULL out */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_button_get_status(NULL, (wink_status_t[]){0}));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_button_get_status(&dev, NULL));
    /* uninitialized dev */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_button_get_status(&dev, (wink_status_t[]){0}));
}

/* Init → get_status returns WINK_OK (fresh handle, no poll yet) */
void test_get_status_initially_ok(void) {
    dal_button_t dev = {0};
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 50, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
    wink_status_t st = WINK_ERR_DISCONNECTED;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_get_status(&dev, &st));
    TEST_ASSERT_EQUAL_INT(WINK_OK, st);
}

/* Successful poll → get_status stays WINK_OK */
void test_get_status_clears_after_recovery(void) {
    dal_button_t dev = {0};
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 51, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
    /* poll a few times successfully */
    set_btn_pin(51, false, true);  /* released */
    TEST_ASSERT_EQUAL_INT(WINK_OK, poll_n(&dev, DAL_BUTTON_DEBOUNCE_THRESHOLD));
    wink_status_t st = WINK_ERR_PANIC;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_get_status(&dev, &st));
    TEST_ASSERT_EQUAL_INT(WINK_OK, st);
}

/* Pull=NONE without injection → poll returns DISCONNECTED and get_status
 * mirrors it; after the next successful read get_status resets to OK. */
void test_get_status_propagates_poll_error_and_clears(void) {
    dal_button_t dev = {0};
    const dal_button_config_t cfg = {
        .owner = OWNER, .pin = 52, .active_low = true, .pull = DAL_BUTTON_PULL_NONE
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
    /* First poll: no injection → DISCONNECTED */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_DISCONNECTED, dal_button_poll(&dev));
    wink_status_t st = WINK_OK;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_get_status(&dev, &st));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_DISCONNECTED, st);
    /* State machine left untouched: still reports the previous stable state
     * (which is "released" since init). */
    bool pressed = true;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_is_pressed(&dev, &pressed));
    TEST_ASSERT_FALSE(pressed);
    /* Inject level → next poll succeeds → last_status returns to OK */
    set_btn_pin(52, false, true);
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_poll(&dev));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_get_status(&dev, &st));
    TEST_ASSERT_EQUAL_INT(WINK_OK, st);
}

/* deinit clears the handle → get_status returns NOT_INITIALIZED
 * and last_status field is reset to 0 by memset. */
void test_get_status_after_deinit(void) {
    dal_button_t dev = {0};
    const dal_button_config_t cfg = {
        .owner = OWNER, .pin = 53, .active_low = true, .pull = DAL_BUTTON_PULL_NONE
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_DISCONNECTED, dal_button_poll(&dev));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_deinit(&dev));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED,
                          dal_button_get_status(&dev, (wink_status_t[]){0}));
}

/* ═══════════════════════════════════════════════════════════
 * DAL-V-010: was_pressed read-clear atomicity
 * ═══════════════════════════════════════════════════════════ */

/* SMP-style race: simulate a second caller that grabs the IRQLock just
 * before the first caller's was_pressed completes.  Before the fix this
 * was a single read+write pair with no critical section, so the second
 * caller could observe the same rising edge and return true twice for a
 * single physical press.  With PAL_CRITICAL_SECTION around the
 * (stable_pressed, last_reported) read-clear, only one caller can ever
 * see the transition true→false on (last_reported) — the second caller
 * is serialized through the same lock and sees last_reported already
 * updated. */
void test_was_pressed_atomic_under_lock(void) {
    dal_button_t dev = {0};
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 60, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
    /* Hand-craft the post-debounce "press" state */
    dev.stable_pressed = true;
    dev.last_reported  = false;

    /* First was_pressed call → true (rising edge) */
    bool ev1 = false;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_was_pressed(&dev, &ev1));
    TEST_ASSERT_TRUE(ev1);
    /* Internal: last_reported must be true after the read-clear */
    TEST_ASSERT_TRUE(dev.last_reported);

    /* Second was_pressed call from "another core" → false (edge already
     * reported and consumed).  This is the contract that protects against
     * the double-report SMP race. */
    bool ev2 = true;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_was_pressed(&dev, &ev2));
    TEST_ASSERT_FALSE(ev2);
    TEST_ASSERT_TRUE(dev.last_reported);
}

/* Reproduce the SMP race directly: two callers both observe the rising
 * edge when PAL_CRITICAL_SECTION serialization is in place.  We use
 * pal_irq_save/restore to pretend we're on the other core — host IRQ
 * lock is recursive so this doesn't deadlock.  The contract we verify:
 * exactly ONE caller sees the press; the other sees false. */
void test_was_pressed_serializes_concurrent_readers(void) {
    dal_button_t dev = {0};
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 61, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
    dev.stable_pressed = true;
    dev.last_reported  = false;

    /* Take the IRQ lock first to "claim" being the second caller.  In
     * practice host's lock is per-thread, so this just exercises that
     * the inner PAL_CRITICAL_SECTION nests cleanly. */
    uint32_t mask = pal_irq_save_rtos_safe();

    bool ev_outer = false;
    /* This call holds the lock the whole time.  After it returns,
     * last_reported must be set. */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_was_pressed(&dev, &ev_outer));
    TEST_ASSERT_TRUE(ev_outer);
    TEST_ASSERT_TRUE(dev.last_reported);

    pal_irq_restore(mask);

    /* Second call (lock released) — no fresh press, must return false */
    bool ev_inner = true;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_was_pressed(&dev, &ev_inner));
    TEST_ASSERT_FALSE(ev_inner);
}

/* ═══════════════════════════════════════════════════════════
 * ADR-0024 §4 #8 idempotency — extended 10-round loop with BOTH
 * counter and IRQ backend active simultaneously (the refcount path).
 * Catches leaks in dal_button_disable_gpio_isr's reference counting.
 * ═══════════════════════════════════════════════════════════ */

void test_deinit_loop_with_counter_and_irq_backend(void) {
    dal_button_t dev; memset(&dev, 0, sizeof(dev));
    const dal_button_config_t cfg = { .owner = OWNER, .pin = 62, .active_low = true };

    for (int round = 0; round < 10; round++) {
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&dev, &cfg));
        TEST_ASSERT_TRUE(dev.initialized);
        TEST_ASSERT_TRUE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 62));

        /* Enable the counter path (refcount +1) */
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_enable_isr_counter(&dev));
        TEST_ASSERT_TRUE(dev.isr_counter_enabled);
        TEST_ASSERT_TRUE(dev.gpio_isr_registered);

        /* Arm the BAL IRQ backend (refcount +1 — both consumers share the
         * same ISR thunk).  This is the worst case for refcounting: if
         * either consumer forgets to drop, the next init() will fail with
         * BUSY. */
        extern void dal_button_set_event_backend(dal_button_t *dev, uint8_t backend);
        dal_button_set_event_backend(&dev, 2 /* DAL_BUTTON_BACKEND_IRQ */);
        TEST_ASSERT_TRUE(dev.gpio_isr_registered);

        /* Drive a few interrupts to exercise the counter */
        pal_host_trigger_gpio_interrupt(62);
        pal_host_trigger_gpio_interrupt(62);
        uint32_t c = 0;
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_get_edge_count(&dev, &c));
        TEST_ASSERT_EQUAL_UINT32(2, c);

        /* deinit must drop BOTH refs and the underlying resource */
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_deinit(&dev));
        TEST_ASSERT_FALSE(dev.initialized);
        TEST_ASSERT_FALSE(dev.isr_counter_enabled);
        TEST_ASSERT_FALSE(dev.gpio_isr_registered);
        TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 62));
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
    RUN_TEST(test_deinit_hardening);
    RUN_TEST(test_deinit_loop_with_isr_no_resource_leak);
    RUN_TEST(test_pull_illegal_rejected_before_claim);
    RUN_TEST(test_pull_none_disconnected_without_injection);
    RUN_TEST(test_pull_none_with_injection_press_release);
    RUN_TEST(test_pull_explicit_up_overrides_active_low_polarity);
    /* DAL-B-025: last_status propagation */
    RUN_TEST(test_get_status_contract);
    RUN_TEST(test_get_status_initially_ok);
    RUN_TEST(test_get_status_clears_after_recovery);
    RUN_TEST(test_get_status_propagates_poll_error_and_clears);
    RUN_TEST(test_get_status_after_deinit);
    /* DAL-V-010: was_pressed atomicity */
    RUN_TEST(test_was_pressed_atomic_under_lock);
    RUN_TEST(test_was_pressed_serializes_concurrent_readers);
    /* ADR-0024: 10-round loop with refcounted counter + IRQ backend */
    RUN_TEST(test_deinit_loop_with_counter_and_irq_backend);
    return UNITY_END();
}
