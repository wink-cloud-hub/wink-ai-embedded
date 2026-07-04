/**
 * @file test_sim_scheduler_stack_clamp.c
 * @brief fixup 计划 F5 Step 2 —— 栈下限 clamp 的 host 集成层验证。
 *
 * 与 test_sim_scheduler.c case 8（用 mock ctx 验证 clamp 逻辑）互补：
 * 本文件走真 pal_host + 真 Win32 fiber，验证"传 1024 stack → clamp 到
 * WINK_SIM_STACK_MIN（32KB）后 task 能真跑不崩栈"。
 */

#include "unity.h"
#include "pal_osal.h"
#include "wink_sim_scheduler.h"
#include <stdint.h>


/* ADR-0017 层 1 例外：本 TU 合法调用 WINK_BLOCKING API。抑制
 * -Wdeprecated-declarations 使 -Werror 下仍能编译；严格模式
 * (-DWINK_STRICT_NONBLOCKING=1) 下相关 API 声明直接消失，本 TU 会链接失败——那是设计意图。 */
#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

static volatile bool s_task_ran = false;

static void tiny_stack_task(void* arg) {
    (void)arg;
    /* 使用 ~4KB 栈空间探测 clamp 是否真的把 1024 提到 ≥32KB。
     * 若 clamp 失败，此数组会溢出用户栈，进而导致 CreateFiber
     * 上的守护页触发访问违例。 */
    volatile uint8_t buf[4096];
    for (int i = 0; i < 4096; ++i) buf[i] = (uint8_t)i;
    /* 读一次强制 volatile 生效，避免 gcc -Wunused-but-set-variable。 */
    volatile uint8_t sink = buf[0];
    (void)sink;
    s_task_ran = true;
    pal_os_task_delete(NULL);
}

void setUp(void) { s_task_ran = false; }
void tearDown(void) { sim_scheduler_reset(0); }

void test_1024_stack_is_clamped_and_task_runs(void) {
    sim_scheduler_reset(42);

    pal_os_task_handle_t h;
    /* stack_depth=1024 —— 远低于 WINK_SIM_STACK_MIN=32KB；预期被 clamp 到 32KB
     * 后 4KB 栈使用不会崩。 */
    TEST_ASSERT_EQUAL(WINK_OK, pal_os_task_create(
        tiny_stack_task, "tiny", 1024, NULL, 5, PAL_OS_CORE_ANY, &h));

    /* 无 App callbacks，允许 task 自删；max_ticks=50 兜底 */
    (void)pal_sim_scheduler_run(NULL, SIM_SCHED_NO_READY, 50);

    TEST_ASSERT_TRUE_MESSAGE(s_task_ran,
        "task with clamped 1KB→32KB stack must run to completion");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_1024_stack_is_clamped_and_task_runs);
    return UNITY_END();
}
