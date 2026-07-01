/* PAL 统一中断抽象单元测试
 * 覆盖：GPIO 中断注册、中断锁语义、嵌套锁、类型安全宏、共享中断
 */
#include "unity.h"
#include "wink_status.h"
#include "pal_irq.h"
#include "pal_hal.h"
#include "pal_shared_chain.h"  /* PLAN-20260701-PAL-TARGET-P1-MAINT Task 1: 算法层单测 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Host 平台专用测试接口（仅在 Host 构建可用） */
extern void pal_host_trigger_gpio_interrupt(wink_pin_t pin);
extern uint32_t pal_host_get_isr_call_count(wink_pin_t pin);
extern void pal_host_reset_isr_stats(void);
extern uint32_t pal_host_get_pending_count(void);
extern int pal_host_get_irq_lock_depth(void);
extern void pal_host_trigger_logical_interrupt(uint32_t irq_num);
extern uint32_t pal_host_get_logical_isr_call_count(uint32_t irq_num);

/* 测试通用 ISR 计数器 */
static volatile uint32_t s_test_isr_count = 0;
static volatile uint32_t s_test_isr_arg_val = 0;

void setUp(void)
{
    pal_host_reset_isr_stats();
    s_test_isr_count = 0;
    s_test_isr_arg_val = 0;
}

void tearDown(void) {}

/* ─────────────────────────────────────────────────────────
 * Test Group 1: GPIO 中断基本注册与触发
 * ───────────────────────────────────────────────────────── */

static void test_gpio_isr(void *arg)
{
    (void)arg;
    s_test_isr_count++;
}

void test_gpio_interrupt_registration(void)
{
    const wink_pin_t TEST_PIN = 10;

    /* 注册中断 */
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_gpio_enable_interrupt(TEST_PIN, PAL_GPIO_INTR_FALLING_EDGE,
                                   test_gpio_isr, NULL));

    /* 触发前计数为 0 */
    TEST_ASSERT_EQUAL_UINT32(0, pal_host_get_isr_call_count(TEST_PIN));
    TEST_ASSERT_EQUAL_UINT32(0, s_test_isr_count);

    /* 触发中断 */
    pal_host_trigger_gpio_interrupt(TEST_PIN);

    /* 验证 ISR 被调用 */
    TEST_ASSERT_EQUAL_UINT32(1, pal_host_get_isr_call_count(TEST_PIN));
    TEST_ASSERT_EQUAL_UINT32(1, s_test_isr_count);

    /* 禁用中断后不应再触发 */
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_gpio_disable_interrupt(TEST_PIN));
    pal_host_trigger_gpio_interrupt(TEST_PIN);
    TEST_ASSERT_EQUAL_UINT32(1, pal_host_get_isr_call_count(TEST_PIN));
}

void test_gpio_interrupt_invalid_pin(void)
{
    /* 无效引脚应返回错误 */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
        pal_gpio_enable_interrupt(-1, PAL_GPIO_INTR_FALLING_EDGE, test_gpio_isr, NULL));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
        pal_gpio_enable_interrupt(999, PAL_GPIO_INTR_FALLING_EDGE, test_gpio_isr, NULL));
}

void test_gpio_interrupt_null_callback(void)
{
    const wink_pin_t TEST_PIN = 11;
    /* NULL 回调应返回错误 */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
        pal_gpio_enable_interrupt(TEST_PIN, PAL_GPIO_INTR_FALLING_EDGE, NULL, NULL));
}

/* ─────────────────────────────────────────────────────────
 * Test Group 1.5: G3 GPIO prio 首次锁定契约（Phase 1.5，2026-07-01）
 * ─────────────────────────────────────────────────────────
 * 契约：GPIO ISR service 首次注册时锁定 prio，后续 mismatch 返回
 * WINK_ERR_INVALID_ARG；disable 不释放锁定；进程生命周期内单向锁定。
 * 参考 pal_hal.h::pal_gpio_enable_interrupt_ex v2.2 doxygen。 */

void test_gpio_prio_locked_on_first_register(void)
{
    /* 首次 HIGH 注册 → 锁定为 HIGH；再次 HIGH 注册其它 pin → OK（一致） */
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_gpio_enable_interrupt_ex(/*pin*/ 12, PAL_GPIO_INTR_FALLING_EDGE,
                                      PAL_IRQ_PRIO_HIGH, test_gpio_isr, NULL));
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_gpio_enable_interrupt_ex(/*pin*/ 13, PAL_GPIO_INTR_FALLING_EDGE,
                                      PAL_IRQ_PRIO_HIGH, test_gpio_isr, NULL));
}

void test_gpio_prio_mismatch_returns_invalid_arg(void)
{
    /* 首次 NORMAL 注册 → 锁定为 NORMAL；再 HIGH 注册应返回 INVALID_ARG（不是 BUSY/UNSUPPORTED） */
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_gpio_enable_interrupt_ex(/*pin*/ 14, PAL_GPIO_INTR_FALLING_EDGE,
                                      PAL_IRQ_PRIO_NORMAL, test_gpio_isr, NULL));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
        pal_gpio_enable_interrupt_ex(/*pin*/ 15, PAL_GPIO_INTR_FALLING_EDGE,
                                      PAL_IRQ_PRIO_HIGH, test_gpio_isr, NULL));
}

void test_gpio_non_ex_locks_to_normal(void)
{
    /* 非 ex 版内联到 _ex(..., NORMAL, ...)；先注册非 ex → 锁定 NORMAL；
     * 再 _ex(HIGH) 应被拒。反过来同样成立，见 test_gpio_prio_mismatch_returns_invalid_arg。 */
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_gpio_enable_interrupt(/*pin*/ 16, PAL_GPIO_INTR_FALLING_EDGE,
                                   test_gpio_isr, NULL));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
        pal_gpio_enable_interrupt_ex(/*pin*/ 17, PAL_GPIO_INTR_FALLING_EDGE,
                                      PAL_IRQ_PRIO_HIGH, test_gpio_isr, NULL));
}

void test_gpio_disable_does_not_unlock(void)
{
    /* 契约：disable 不释放 prio 锁定（拒绝 disable→uninstall 方案，见 pal_hal.h §1.2）。
     * 步骤：
     *   1. 注册 Pin A (NORMAL) → OK
     *   2. 注册 Pin B (HIGH)   → INVALID_ARG（锁定 mismatch）
     *   3. disable(Pin A)      → OK
     *   4. 再次注册 Pin B (HIGH) → 仍为 INVALID_ARG（disable 未解锁） */
    const wink_pin_t PIN_A = 18;
    const wink_pin_t PIN_B = 19;

    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_gpio_enable_interrupt_ex(PIN_A, PAL_GPIO_INTR_FALLING_EDGE,
                                      PAL_IRQ_PRIO_NORMAL, test_gpio_isr, NULL));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
        pal_gpio_enable_interrupt_ex(PIN_B, PAL_GPIO_INTR_FALLING_EDGE,
                                      PAL_IRQ_PRIO_HIGH, test_gpio_isr, NULL));

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_gpio_disable_interrupt(PIN_A));

    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
        pal_gpio_enable_interrupt_ex(PIN_B, PAL_GPIO_INTR_FALLING_EDGE,
                                      PAL_IRQ_PRIO_HIGH, test_gpio_isr, NULL));
}

/* ─────────────────────────────────────────────────────────
 * Test Group 2: 中断锁语义（核心验收项）
 * ───────────────────────────────────────────────────────── */

void test_irq_lock_pending_semantics(void)
{
    const wink_pin_t TEST_PIN = 12;

    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_gpio_enable_interrupt(TEST_PIN, PAL_GPIO_INTR_FALLING_EDGE,
                                   test_gpio_isr, NULL));

    /* 持有中断锁时触发中断 */
    uint32_t mask = pal_irq_save();

    /* 验证锁深度已增加 */
    TEST_ASSERT_TRUE(pal_host_get_irq_lock_depth() > 0);

    pal_host_trigger_gpio_interrupt(TEST_PIN);

    /* ✅ 核心语义验证：持有锁期间 ISR 不应立即执行，
     * 应加入 pending 队列延迟到锁释放后执行 */
    TEST_ASSERT_EQUAL_UINT32(0, s_test_isr_count);
    TEST_ASSERT_EQUAL_UINT32(1, pal_host_get_pending_count());

    /* 释放锁 → pending 中断应被刷新执行 */
    pal_irq_restore(mask);

    /* 验证锁深度已归零，ISR 已执行 */
    TEST_ASSERT_EQUAL_INT(0, pal_host_get_irq_lock_depth());
    TEST_ASSERT_EQUAL_UINT32(1, s_test_isr_count);
    TEST_ASSERT_EQUAL_UINT32(0, pal_host_get_pending_count());
}

void test_irq_lock_nesting(void)
{
    const wink_pin_t TEST_PIN = 13;

    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_gpio_enable_interrupt(TEST_PIN, PAL_GPIO_INTR_FALLING_EDGE,
                                   test_gpio_isr, NULL));

    /* 嵌套 save 两次 */
    uint32_t mask1 = pal_irq_save();
    uint32_t mask2 = pal_irq_save();

    /* 触发中断 */
    pal_host_trigger_gpio_interrupt(TEST_PIN);
    TEST_ASSERT_EQUAL_UINT32(0, s_test_isr_count);
    TEST_ASSERT_EQUAL_UINT32(1, pal_host_get_pending_count());

    /* 内层 restore → 不应释放锁，pending 仍不执行 */
    pal_irq_restore(mask2);
    TEST_ASSERT_TRUE(pal_host_get_irq_lock_depth() > 0);
    TEST_ASSERT_EQUAL_UINT32(0, s_test_isr_count);
    TEST_ASSERT_EQUAL_UINT32(1, pal_host_get_pending_count());

    /* 外层 restore → 真正释放锁，pending 被执行 */
    pal_irq_restore(mask1);
    TEST_ASSERT_EQUAL_INT(0, pal_host_get_irq_lock_depth());
    TEST_ASSERT_EQUAL_UINT32(1, s_test_isr_count);
}

void test_irq_lock_rtos_safe_same_semantics(void)
{
    const wink_pin_t TEST_PIN = 14;

    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_gpio_enable_interrupt(TEST_PIN, PAL_GPIO_INTR_FALLING_EDGE,
                                   test_gpio_isr, NULL));

    /* pal_irq_save_rtos_safe() 应具有相同的 pending 语义 */
    uint32_t mask = pal_irq_save_rtos_safe();
    pal_host_trigger_gpio_interrupt(TEST_PIN);

    TEST_ASSERT_EQUAL_UINT32(0, s_test_isr_count);
    TEST_ASSERT_EQUAL_UINT32(1, pal_host_get_pending_count());

    pal_irq_restore(mask);
    TEST_ASSERT_EQUAL_UINT32(1, s_test_isr_count);
}

/* ─────────────────────────────────────────────────────────
 * Test Group 3: PAL_DEFINE_ISR 类型安全宏
 * ───────────────────────────────────────────────────────── */

typedef struct {
    uint32_t counter;
    uint8_t  id;
    bool     flag;
} test_isr_context_t;

static test_isr_context_t s_test_ctx;

/* ✅ 使用类型安全宏定义 ISR，无需手动类型转换 */
PAL_DEFINE_ISR(test_typed_isr, test_isr_context_t, ctx)
{
    ctx->counter++;
    ctx->flag = true;
    s_test_isr_arg_val = ctx->id;
}

void test_type_safe_isr_macro(void)
{
    const wink_pin_t TEST_PIN = 15;

    memset(&s_test_ctx, 0, sizeof(s_test_ctx));
    s_test_ctx.id = 0xAB;

    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_gpio_enable_interrupt(TEST_PIN, PAL_GPIO_INTR_FALLING_EDGE,
                                   test_typed_isr, &s_test_ctx));

    pal_host_trigger_gpio_interrupt(TEST_PIN);

    /* 验证结构体字段正确访问，无类型转换错误 */
    TEST_ASSERT_EQUAL_UINT32(1, s_test_ctx.counter);
    TEST_ASSERT_TRUE(s_test_ctx.flag);
    TEST_ASSERT_EQUAL_UINT32(0xAB, s_test_isr_arg_val);
}

/* ─────────────────────────────────────────────────────────
 * Test Group 4: 逻辑中断接口
 * ───────────────────────────────────────────────────────── */

static void test_logical_isr(void *arg)
{
    s_test_isr_count++;
    s_test_isr_arg_val = (uint32_t)(uintptr_t)arg;
}

void test_logical_irq_enable_disable(void)
{
    const uint32_t TEST_IRQ = 5;

    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_irq_enable(TEST_IRQ, PAL_IRQ_PRIO_NORMAL, test_logical_isr, (void *)0x1234));

    pal_host_trigger_logical_interrupt(TEST_IRQ);
    TEST_ASSERT_EQUAL_UINT32(1, s_test_isr_count);
    TEST_ASSERT_EQUAL_UINT32(0x1234, s_test_isr_arg_val);

    /* disable 后不应再触发 */
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_irq_disable(TEST_IRQ));
    s_test_isr_count = 0;
    pal_host_trigger_logical_interrupt(TEST_IRQ);
    TEST_ASSERT_EQUAL_UINT32(0, s_test_isr_count);
}

void test_logical_irq_invalid_number(void)
{
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
        pal_irq_enable(999, PAL_IRQ_PRIO_NORMAL, test_logical_isr, NULL));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
        pal_irq_disable(999));
}

void test_direct_connect_irq(void)
{
    /* 复用专门的无参 direct ISR，详见 test_direct_connect_calls_handler */
    extern void test_direct_isr_void(void);
    const uint32_t TEST_IRQ = 7;

    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_irq_direct_connect(TEST_IRQ, test_direct_isr_void));

    pal_host_trigger_logical_interrupt(TEST_IRQ);
    /* test_direct_isr_void 共享 s_test_isr_count（在文件顶部声明） */
    TEST_ASSERT_EQUAL_UINT32(1, s_test_isr_count);
}

/* ─────────────────────────────────────────────────────────
 * v2.1 G1 验收：direct_connect 的无参签名 trampoline
 * 旧版 `(pal_direct_isr_t)test_logical_isr` 是 void(void*) → void(void) 的非法 cast，
 * 经 trampoline 后会真的以无参方式调用 handler，那种 cast 现在会段错误/UB。
 * 此 test 用真正的 void(void) handler 验证 v2.1 trampoline 正确桥接。
 * ───────────────────────────────────────────────────────── */

/* 文件级 ISR，给 test_direct_connect_irq 与 test_direct_connect_calls_handler 共用 */
void test_direct_isr_void(void)
{
    s_test_isr_count++;
}

void test_direct_connect_calls_handler(void)
{
    const uint32_t TEST_IRQ = 9;

    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_irq_direct_connect(TEST_IRQ, test_direct_isr_void));

    /* 触发逻辑中断 → trampoline → test_direct_isr_void */
    pal_host_trigger_logical_interrupt(TEST_IRQ);
    TEST_ASSERT_EQUAL_UINT32(1, s_test_isr_count);

    /* disable 应同步清除 direct 槽位 */
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_irq_disable(TEST_IRQ));
    s_test_isr_count = 0;
    pal_host_trigger_logical_interrupt(TEST_IRQ);
    TEST_ASSERT_EQUAL_UINT32(0, s_test_isr_count);
}

void test_direct_connect_invalid_args(void)
{
    /* NULL handler */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
        pal_irq_direct_connect(5, NULL));
    /* irq 越界（host 上限 32） */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
        pal_irq_direct_connect(999, test_direct_isr_void));
}

/* ─────────────────────────────────────────────────────────
 * Test Group 5: 中断优先级枚举边界检查
 * ───────────────────────────────────────────────────────── */

void test_irq_priority_enum_bounds(void)
{
    const uint32_t TEST_IRQ = 8;

    /* 所有合法优先级都应能成功注册 */
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_irq_enable(TEST_IRQ, PAL_IRQ_PRIO_LOWEST, test_logical_isr, NULL));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_irq_disable(TEST_IRQ));

    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_irq_enable(TEST_IRQ, PAL_IRQ_PRIO_HIGHEST, test_logical_isr, NULL));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_irq_disable(TEST_IRQ));

    /* v2.2（Phase 1.5，2026-07-01）：REALTIME 在所有 target 上默认拒接，
     * 与 ESP32 对齐（ADR-0012 契约诚实）。opt-in 通过
     * WINK_HOST_ALLOW_REALTIME_FOR_TESTING 编译宏放行 —— 见
     * test_irq_realtime_accepted_when_opt_in。 */
#if defined(WINK_HOST_ALLOW_REALTIME_FOR_TESTING)
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_irq_enable(TEST_IRQ, PAL_IRQ_PRIO_REALTIME, test_logical_isr, NULL));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_irq_disable(TEST_IRQ));
#else
    TEST_ASSERT_EQUAL_INT(WINK_ERR_UNSUPPORTED,
        pal_irq_enable(TEST_IRQ, PAL_IRQ_PRIO_REALTIME, test_logical_isr, NULL));
#endif

    /* 越界优先级应返回错误 */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
        pal_irq_enable(TEST_IRQ, (pal_irq_prio_t)999, test_logical_isr, NULL));
}

/* v2.2 G2（Phase 1.5，2026-07-01）：REALTIME 全 target 默认拒接的契约锚点 */

void test_irq_realtime_rejected_on_all_targets(void)
{
    /* v2.2：默认编译（无 WINK_HOST_ALLOW_REALTIME_FOR_TESTING）时，
     * pal_irq_enable(..., REALTIME, ...) 在所有 target 上一致返回
     * WINK_ERR_UNSUPPORTED，让"仿真通过 → 真机通过"关系严格成立。 */
#if defined(WINK_HOST_ALLOW_REALTIME_FOR_TESTING)
    TEST_IGNORE_MESSAGE("opt-in 编译，REALTIME 放行；见 test_irq_realtime_accepted_when_opt_in");
#else
    const uint32_t TEST_IRQ = 6;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_UNSUPPORTED,
        pal_irq_enable(TEST_IRQ, PAL_IRQ_PRIO_REALTIME, test_logical_isr, NULL));
#endif
}

void test_gpio_realtime_rejected_on_all_targets(void)
{
    /* v2.2：GPIO 路径同样在所有 target 上拒接 REALTIME（不受 opt-in 宏影响 —— GPIO
     * 无 per-pin 抢占，REALTIME 无处映射，无论何种编译都返回 UNSUPPORTED）。 */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_UNSUPPORTED,
        pal_gpio_enable_interrupt_ex(/*pin*/ 4, PAL_GPIO_INTR_FALLING_EDGE,
                                      PAL_IRQ_PRIO_REALTIME, test_gpio_isr, NULL));
}

void test_irq_realtime_accepted_when_opt_in(void)
{
    /* v2.2 opt-in：定义 WINK_HOST_ALLOW_REALTIME_FOR_TESTING 后，host/wasm 上
     * pal_irq_enable REALTIME 被受控放行；首次调用会打 warn（未在 unit test 内 capture，
     * 覆盖 warn 输出留给 stderr 目视/CI 日志）。 */
#if defined(WINK_HOST_ALLOW_REALTIME_FOR_TESTING)
    const uint32_t TEST_IRQ = 6;
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_irq_enable(TEST_IRQ, PAL_IRQ_PRIO_REALTIME, test_logical_isr, NULL));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_irq_disable(TEST_IRQ));
#else
    TEST_IGNORE_MESSAGE("需要 -DWINK_HOST_ALLOW_REALTIME_FOR_TESTING 编译；默认 build 跳过");
#endif
}

void test_realtime_priority_rejected_on_esp32(void)
{
    /* 保留 ESP32 侧的历史锚点（v2.1 引入）。v2.2 起 host/wasm 也拒接，
     * 见 test_irq_realtime_rejected_on_all_targets / test_gpio_realtime_rejected_on_all_targets。 */
#if !defined(ESP_PLATFORM)
    TEST_IGNORE_MESSAGE("仅在 ESP32 target 上有意义；host build 跳过");
#else
    const uint32_t TEST_IRQ = 6;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_UNSUPPORTED,
        pal_irq_enable(TEST_IRQ, PAL_IRQ_PRIO_REALTIME, test_logical_isr, NULL));

    /* GPIO 路径同样拒接 */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_UNSUPPORTED,
        pal_gpio_enable_interrupt_ex(/*pin*/ 4, PAL_GPIO_INTR_FALLING_EDGE,
                                      PAL_IRQ_PRIO_REALTIME, test_gpio_isr, NULL));
#endif
}

/* ─────────────────────────────────────────────────────────
 * Test Group 5.5: G3 并发首次注册竞态（Phase 1.5，host only，需 pthread）
 * ─────────────────────────────────────────────────────────
 * 契约：两条线程同时首次注册不同 prio，实现必须以 mutex 保护 double-checked
 * initialization —— 一条获得锁定为其 prio，另一条应返回 WINK_ERR_INVALID_ARG，
 * 且不出现 UB / crash / 双重初始化。 */

#if !defined(ESP_PLATFORM) && !defined(__EMSCRIPTEN__)
#include <pthread.h>

typedef struct {
    pal_irq_prio_t prio;
    wink_pin_t     pin;
    wink_status_t  result;
} race_ctx_t;

static void race_isr(void *arg) { (void)arg; }

static void *race_worker(void *arg)
{
    race_ctx_t *ctx = (race_ctx_t *)arg;
    ctx->result = pal_gpio_enable_interrupt_ex(
        ctx->pin, PAL_GPIO_INTR_FALLING_EDGE, ctx->prio, race_isr, NULL);
    return NULL;
}

void test_gpio_concurrent_first_register_race(void)
{
    /* 两条线程同时首次注册不同 prio；重复 100 次以增加碰撞概率。 */
    for (int iter = 0; iter < 100; ++iter) {
        pal_host_reset_isr_stats();   /* 重置 GPIO service 锁定状态 */

        race_ctx_t a = { .prio = PAL_IRQ_PRIO_NORMAL, .pin = 22, .result = WINK_OK };
        race_ctx_t b = { .prio = PAL_IRQ_PRIO_HIGH,   .pin = 23, .result = WINK_OK };

        pthread_t ta, tb;
        pthread_create(&ta, NULL, race_worker, &a);
        pthread_create(&tb, NULL, race_worker, &b);
        pthread_join(ta, NULL);
        pthread_join(tb, NULL);

        /* 恰好一条成功、一条被拒（不允许双成功——那意味着 mutex 失效或
         * lock 提交/检查在临界区外；也不允许双失败——那意味着首次注册被误判）。 */
        int ok_count = 0, invalid_count = 0;
        if (a.result == WINK_OK)              ok_count++;
        if (b.result == WINK_OK)              ok_count++;
        if (a.result == WINK_ERR_INVALID_ARG) invalid_count++;
        if (b.result == WINK_ERR_INVALID_ARG) invalid_count++;

        TEST_ASSERT_EQUAL_INT_MESSAGE(1, ok_count,
            "expected exactly one thread to win the first-register race");
        TEST_ASSERT_EQUAL_INT_MESSAGE(1, invalid_count,
            "the losing thread must return WINK_ERR_INVALID_ARG (not BUSY/UNSUPPORTED)");
    }
}
#else
void test_gpio_concurrent_first_register_race(void)
{
    TEST_IGNORE_MESSAGE("需要 pthread（host build）；ESP32/WASM 跳过");
}
#endif

/* ─────────────────────────────────────────────────────────
 * Test Group 6: 共享中断机制（v2.0 核心特性）
 * ───────────────────────────────────────────────────────── */

static volatile uint32_t s_shared_handler1_count = 0;
static volatile uint32_t s_shared_handler2_count = 0;

static bool test_shared_handler1(void *arg)
{
    (void)arg;
    s_shared_handler1_count++;
    return true;  /* 认领中断 */
}

static bool test_shared_handler2(void *arg)
{
    (void)arg;
    s_shared_handler2_count++;
    return true;  /* 认领中断 */
}

/* ✅ v2.0 核心语义验证：双 handler 同时触发时，两个都应被调用
 * （旧版 v1.x 语义：第一个返回 true 后终止遍历，导致第二个不被调用） */
void test_shared_irq_both_handlers_called(void)
{
    const uint32_t TEST_IRQ = 10;
    s_shared_handler1_count = 0;
    s_shared_handler2_count = 0;

    /* 注册第一个 handler */
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_irq_shared_register(TEST_IRQ, PAL_IRQ_PRIO_NORMAL,
                                 test_shared_handler1, NULL));

    /* 注册第二个 handler（共享同一中断） */
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_irq_shared_register(TEST_IRQ, PAL_IRQ_PRIO_NORMAL,
                                 test_shared_handler2, NULL));

    /* 触发一次中断 */
    pal_host_trigger_logical_interrupt(TEST_IRQ);

    /* ✅ v2.0 核心验收：两个 handler 都应被调用
     * 这是 v2.0 的关键语义修正，参考 Linux 内核 Shared IRQ 行为 */
    TEST_ASSERT_EQUAL_UINT32(1, s_shared_handler1_count);
    TEST_ASSERT_EQUAL_UINT32(1, s_shared_handler2_count);
}

void test_shared_irq_chain_full_returns_no_mem(void)
{
    const uint32_t TEST_IRQ = 11;

    /* 注册 4 个 handler（达到 MAX_SHARED_HANDLERS 上限） */
    for (int i = 0; i < 4; i++) {
        wink_status_t st = pal_irq_shared_register(TEST_IRQ, PAL_IRQ_PRIO_NORMAL,
                                                    test_shared_handler1, NULL);
        if (i == 0) {
            TEST_ASSERT_EQUAL_INT(WINK_OK, st);
        }
        /* 后续注册可能 OK 或 NO_MEM，取决于实现的上限
         * 此处只验证第 5 个一定会失败 */
    }

    /* 第 5 个 handler 应返回 NO_MEM */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NO_MEM,
        pal_irq_shared_register(TEST_IRQ, PAL_IRQ_PRIO_NORMAL,
                                 test_shared_handler2, NULL));
}

/* ─────────────────────────────────────────────────────────
 * Test Group 7: CRITICAL_SECTION RAII 宏
 * ───────────────────────────────────────────────────────── */

void test_critical_section_macro(void)
{
    const wink_pin_t TEST_PIN = 20;

    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_gpio_enable_interrupt(TEST_PIN, PAL_GPIO_INTR_FALLING_EDGE,
                                   test_gpio_isr, NULL));

    /* ✅ RAII 临界区：进入时自动加锁，退出时自动解锁 */
    PAL_CRITICAL_SECTION({
        /* 临界区内触发中断应 pending */
        pal_host_trigger_gpio_interrupt(TEST_PIN);
        TEST_ASSERT_EQUAL_UINT32(0, s_test_isr_count);
        TEST_ASSERT_TRUE(pal_host_get_pending_count() > 0);
    });

    /* 退出临界区后 pending 应被执行 */
    TEST_ASSERT_EQUAL_UINT32(1, s_test_isr_count);
    TEST_ASSERT_EQUAL_INT(0, pal_host_get_irq_lock_depth());
}

void test_critical_section_strict_macro(void)
{
    const wink_pin_t TEST_PIN = 21;

    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_gpio_enable_interrupt(TEST_PIN, PAL_GPIO_INTR_FALLING_EDGE,
                                   test_gpio_isr, NULL));

    /* STRICT 版本使用全屏蔽锁，语义上应与普通版一致（Host 平台无区别） */
    PAL_CRITICAL_SECTION_STRICT({
        pal_host_trigger_gpio_interrupt(TEST_PIN);
        TEST_ASSERT_EQUAL_UINT32(0, s_test_isr_count);
    });

    TEST_ASSERT_EQUAL_UINT32(1, s_test_isr_count);
}

/* ─────────────────────────────────────────────────────────
 * Test Group 8: pal_irq_synchronize 无崩溃验证
 * ───────────────────────────────────────────────────────── */

void test_irq_synchronize_no_crash(void)
{
    const uint32_t TEST_IRQ = 15;

    /* 调用 synchronize 不应崩溃，即使没有正在执行的 ISR */
    pal_irq_synchronize(TEST_IRQ);
    pal_irq_synchronize(~0U);  /* ~0 表示等待所有中断 */

    /* 如果执行到这里，测试通过 */
    TEST_PASS();
}

/* ─────────────────────────────────────────────────────────
 * Test Group 9: pal_shared_chain 算法层单测（PLAN-20260701-PAL-TARGET-P1-MAINT Task 1）
 * ─────────────────────────────────────────────────────────
 * 直接对 pal_shared_chain_append/dispatch 做算法级测试，不依赖
 * pal_host_trigger_logical_interrupt 路径，也不依赖 pal_irq_enable/register。
 * 单线程简化路径（ops == NULL）即可覆盖 host / wasm 语义。 */

void test_shared_chain_append_full(void)
{
    pal_shared_chain_t *slot = NULL;
    bool first = false;
    for (int i = 0; i < PAL_SHARED_CHAIN_MAX_HANDLERS; i++) {
        TEST_ASSERT_EQUAL_INT(WINK_OK,
            pal_shared_chain_append(&slot, NULL, /*irq*/ 99,
                                    test_shared_handler1, NULL, &first));
        /* 首次追加应返回 became_first=true，之后应为 false */
        TEST_ASSERT_EQUAL_INT((i == 0) ? 1 : 0, first ? 1 : 0);
    }
    /* 第 5 次应满 */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NO_MEM,
        pal_shared_chain_append(&slot, NULL, 99,
                                test_shared_handler1, NULL, &first));
    free(slot);
}

void test_shared_chain_dispatch_order(void)
{
    pal_shared_chain_t *slot = NULL;
    bool first = false;
    s_shared_handler1_count = 0;
    s_shared_handler2_count = 0;
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_shared_chain_append(&slot, NULL, 100,
                                test_shared_handler1, NULL, &first));
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_shared_chain_append(&slot, NULL, 100,
                                test_shared_handler2, NULL, &first));
    uint32_t claimed = pal_shared_chain_dispatch(slot);
    /* v2.0 语义：两个 handler 都被调用，不因第一个返回 true 而提前终止 */
    TEST_ASSERT_EQUAL_UINT32(1, s_shared_handler1_count);
    TEST_ASSERT_EQUAL_UINT32(1, s_shared_handler2_count);
    /* 两个 handler 都 return true → claimed == 2 */
    TEST_ASSERT_EQUAL_UINT32(2, claimed);
    free(slot);
}

void test_shared_chain_free_null_safe(void)
{
    /* free(NULL) 已由 stdlib 保证；本用例只是把 dispatch(NULL) 语义固化 */
    TEST_ASSERT_EQUAL_UINT32(0, pal_shared_chain_dispatch(NULL));
}

/* ─────────────────────────────────────────────────────────
 * Main: 运行所有测试
 * ───────────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();

    /* Group 1: GPIO 中断基本功能 */
    RUN_TEST(test_gpio_interrupt_registration);
    RUN_TEST(test_gpio_interrupt_invalid_pin);
    RUN_TEST(test_gpio_interrupt_null_callback);

    /* Group 1.5: G3 GPIO prio 首次锁定契约（Phase 1.5，2026-07-01） */
    RUN_TEST(test_gpio_prio_locked_on_first_register);
    RUN_TEST(test_gpio_prio_mismatch_returns_invalid_arg);
    RUN_TEST(test_gpio_non_ex_locks_to_normal);
    RUN_TEST(test_gpio_disable_does_not_unlock);

    /* Group 2: 中断锁语义（核心验收） */
    RUN_TEST(test_irq_lock_pending_semantics);
    RUN_TEST(test_irq_lock_nesting);
    RUN_TEST(test_irq_lock_rtos_safe_same_semantics);

    /* Group 3: 类型安全宏 */
    RUN_TEST(test_type_safe_isr_macro);

    /* Group 4: 逻辑中断 */
    RUN_TEST(test_logical_irq_enable_disable);
    RUN_TEST(test_logical_irq_invalid_number);
    RUN_TEST(test_direct_connect_irq);
    /* v2.1 G1 新增：trampoline 验证 */
    RUN_TEST(test_direct_connect_calls_handler);
    RUN_TEST(test_direct_connect_invalid_args);

    /* Group 5: 优先级边界 */
    RUN_TEST(test_irq_priority_enum_bounds);
    /* v2.2 G2 新增（Phase 1.5，2026-07-01）：REALTIME 全 target 一致契约 */
    RUN_TEST(test_irq_realtime_rejected_on_all_targets);
    RUN_TEST(test_gpio_realtime_rejected_on_all_targets);
    RUN_TEST(test_irq_realtime_accepted_when_opt_in);
    RUN_TEST(test_realtime_priority_rejected_on_esp32);

    /* Group 5.5: G3 并发首次注册竞态（host only） */
    RUN_TEST(test_gpio_concurrent_first_register_race);

    /* Group 6: 共享中断（v2.0 核心特性） */
    RUN_TEST(test_shared_irq_both_handlers_called);
    RUN_TEST(test_shared_irq_chain_full_returns_no_mem);

    /* Group 7: RAII 临界区宏 */
    RUN_TEST(test_critical_section_macro);
    RUN_TEST(test_critical_section_strict_macro);

    /* Group 8: synchronize API */
    RUN_TEST(test_irq_synchronize_no_crash);

    /* Group 9: pal_shared_chain 算法层单测（PLAN-20260701-PAL-TARGET-P1-MAINT Task 1） */
    RUN_TEST(test_shared_chain_append_full);
    RUN_TEST(test_shared_chain_dispatch_order);
    RUN_TEST(test_shared_chain_free_null_safe);

    return UNITY_END();
}
