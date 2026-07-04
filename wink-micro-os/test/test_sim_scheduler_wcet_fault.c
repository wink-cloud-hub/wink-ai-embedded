/**
 * @file test_sim_scheduler_wcet_fault.c
 * @brief WCET 8002 fault 触发门禁测试（fixup 计划 F2 Step 4 / R2 契约）。
 *
 * 门禁契约（与 ADR-0013 §"已知保真度边界" 第 1 条对齐）：
 *   1. 纯 CPU 忙循环（无 yield 点）应触发 WCET fault (code=8002)；
 *   2. `pal_os_busy_wait_us(N)` 只推进虚拟时钟，物理耗时微秒级，绝不能误报；
 *   3. WCET fault 路径必须携带非 NULL callbacks（指向调用方传入的 s_test_callbacks），
 *      App on_fault(8002) 必须被调（红线 16 / P1-6 回归门禁）。
 *
 * 依赖：物理墙钟 helper `host_wall_clock_us`（fixup R6）。
 */

#include "unity.h"
#include "pal_osal.h"
#include "wink_sim_scheduler.h"
#include "host_wall_clock.h"
#include <stdint.h>
#include <stdlib.h>

#include "wink_app.h"


/* ADR-0017 层 1 例外：本 TU 合法调用 WINK_BLOCKING API。抑制
 * -Wdeprecated-declarations 使 -Werror 下仍能编译；严格模式
 * (-DWINK_STRICT_NONBLOCKING=1) 下相关 API 声明直接消失，本 TU 会链接失败——那是设计意图。 */
#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

/* ---- test-local hook：强符号覆盖 weak wink_runtime_fault ----
 *
 * pal_osal_host.c 已提供 __attribute__((weak)) wink_runtime_fault 的 stub 用于
 * scheduler 触发 WCET。本文件用强符号覆盖，真实透传 cb->on_fault(code) ——
 * 这样可以同时验证：
 *   (a) fault 被触发；
 *   (b) cb 指针非 NULL 且指向传入的 s_test_callbacks（P1-6 回归门禁）；
 *   (c) App on_fault(code) 被真实路径调用（而非测试手动 invoke）。 */
static bool s_fault_fired = false;
static uint32_t s_fault_code = 0;
static bool s_app_on_fault_called = false;
static const wink_app_callbacks_t* s_captured_cb = NULL;

void wink_runtime_fault(const struct wink_app_callbacks* cb, uint32_t code) {
    s_fault_fired = true;
    s_fault_code = code;
    s_captured_cb = cb;
    if (cb && cb->on_fault) {
        cb->on_fault(code);
    }
}

/* 测试用 App callbacks —— on_fault 走本地 hook 记录。 */
static void test_app_on_fault(uint32_t code) {
    (void)code;
    s_app_on_fault_called = true;
}

static const wink_app_callbacks_t s_test_callbacks = {
    .init = NULL,
    .loop = NULL,
    .on_fault = test_app_on_fault,
};

/* ---- tasks ---- */

/* 精准物理忙等 15ms —— 稳定超越默认 5ms / CI 放宽 50ms 阈值下的默认判定。
 * 但注意本测试默认关闭 CI 环境变量以避免阈值被自动放大。 */
static void cpu_hog_task(void* arg) {
    (void)arg;
    /* 用共享墙钟 helper 精准忙等（不使用虚拟时钟）；确保 WCET slice > 阈值。 */
    uint64_t start = host_wall_clock_us();
    while ((host_wall_clock_us() - start) < 15000ULL) {
        /* spin */
    }
    pal_os_task_delete(NULL);
}

/* busy_wait_us 只推进虚拟时钟；物理耗时应保持微秒级，不应误报。 */
static void busy_wait_task(void* arg) {
    (void)arg;
    pal_os_busy_wait_us(50000);   /* virtual 50ms → 但 CPU 只花微秒 */
    pal_os_task_delete(NULL);
}

/* ---- fixtures ---- */

void setUp(void) {
    s_fault_fired = false;
    s_fault_code = 0;
    s_app_on_fault_called = false;
    s_captured_cb = NULL;
    /* 显式清除 CI env，让测试用固定 5ms 默认阈值（不被 10x 放宽干扰） */
    _putenv("CI=");
    _putenv("WINK_SIM_BYPASS_WCET=");
}

void tearDown(void) {
    sim_scheduler_reset(0);
}

/* ---- cases ---- */

void test_cpu_hog_triggers_8002(void) {
    sim_scheduler_reset(42);

    pal_os_task_handle_t h;
    TEST_ASSERT_EQUAL(WINK_OK, pal_os_task_create(
        cpu_hog_task, "hog", 32*1024, NULL, 5, PAL_OS_CORE_ANY, &h));

    /* 走真 pal_sim_scheduler_run —— 传 App callbacks 让 R2 契约生效。 */
    (void)pal_sim_scheduler_run(&s_test_callbacks, SIM_SCHED_NO_READY, 100);

    TEST_ASSERT_TRUE_MESSAGE(s_fault_fired,
        "WCET fault must fire when task busy-loops > threshold (C2 门禁)");
    TEST_ASSERT_EQUAL_UINT32(8002, s_fault_code);

    /* P1-6：scheduler 必须把调用方提供的 cb 指针透传给 wink_runtime_fault，
     * 不得传 NULL、不得传野指针。 */
    TEST_ASSERT_NOT_NULL_MESSAGE(s_captured_cb,
        "wink_runtime_fault must receive a non-NULL cb pointer (P1-6)");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(&s_test_callbacks, s_captured_cb,
        "wink_runtime_fault cb must match the callbacks passed to pal_sim_scheduler_run (P1-6)");

    /* R2 契约：强符号 wink_runtime_fault 内部直接透传 on_fault，
     * 所以 s_app_on_fault_called 被真实路径置位，而非测试手动 invoke。 */
    TEST_ASSERT_TRUE_MESSAGE(s_app_on_fault_called,
        "App on_fault must be invoked with WCET fault code (red-line 16 / R2)");
}

void test_busy_wait_us_does_not_trigger(void) {
    sim_scheduler_reset(42);

    pal_os_task_handle_t h;
    TEST_ASSERT_EQUAL(WINK_OK, pal_os_task_create(
        busy_wait_task, "bw", 32*1024, NULL, 5, PAL_OS_CORE_ANY, &h));

    (void)pal_sim_scheduler_run(&s_test_callbacks, SIM_SCHED_NO_READY, 100);

    TEST_ASSERT_FALSE_MESSAGE(s_fault_fired,
        "pal_os_busy_wait_us must NOT trigger 8002 (virtual clock only)");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_cpu_hog_triggers_8002);
    RUN_TEST(test_busy_wait_us_does_not_trigger);
    return UNITY_END();
}
