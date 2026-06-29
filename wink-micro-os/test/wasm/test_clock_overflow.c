/**
 * @file test_clock_overflow.c
 * @brief WASM 虚拟时钟溢出预警单测（Wave2 P1 Task 6）。
 *
 * 背景：uint64_t 虚拟时钟看似可用 ~584 年（微秒），但 1000x 加速仿真下
 * 每天可推进 ~2.7 年，CI 连续运行 ~200 天即可能溢出。Task 6 在 50% 量程
 * （UINT64 中点，约 292 年）处插入一次性早期警告标志：JS 侧每个 tick 边界
 * 轮询 pal_wasm_is_clock_warning_fired()，触发后输出 console.warn 提示，
 * 让用户在真正回绕前重置仿真环境。
 *
 * 本文件验证：
 *   1. _Static_assert 已生效（编译期保证 64 位），运行期占位 PASS；
 *   2. 启动状态：警告标志为 false（BSS 零初始化）；
 *   3. 阈值以下推进：不触发警告，accessor 仍返回 false；
 *   4. 跨越阈值推进：accessor 返回 true，且仅触发一次（幂等性）；
 *   5. get_virtual_clock_us 与 pal_get_us 读出一致（同一 SSOT 状态）。
 *
 * 构建接线（与 test_virtual_clock.c / test_wasm_physical.c 同样源码先行）：
 *   现阶段 add_wink_wasm_test CMake helper 未落地，本文件以源码形式交付，
 *   待 wasm 端测试运行器完善后纳入 CI。host 端无 emscripten.h，本文件
 *   不参与 host 编译。
 */
#include "unity.h"
#include "pal_osal.h"
#include "pal_wasm_internal.h"
#include <emscripten.h>
#include <stdint.h>
#include <stdbool.h>

void setUp(void) {
    /* WASM 单测每个用例独立实例化，s_virtual_us 与 s_clock_warning_fired
     * 由 BSS 零初始化保证为 0/false。本文件用例间共享同一实例时，注意
     * 警告一旦触发就保持，故跨阈值用例必须放在「初始/未触发」用例之后。 */
}

void tearDown(void) {}

/* ─────────────────────────────────────────────────────────
 * (1) 编译期 64 位静态断言占位
 *     真正的 _Static_assert 在 pal_osal_wasm.c 顶部，编译期已强制；
 *     此处仅做运行时 PASS，作为「构建成功 = 静态断言通过」的证据。
 * ───────────────────────────────────────────────────────── */
void test_clock_64bit_static_assert(void) {
    TEST_PASS();
}

/* ─────────────────────────────────────────────────────────
 * (2) 启动态：警告标志未触发
 * ───────────────────────────────────────────────────────── */
void test_clock_warning_not_fired_initially(void) {
    TEST_ASSERT_FALSE(pal_wasm_is_clock_warning_fired());
    /* 同时验证 accessor 与 SSOT 读出一致 */
    TEST_ASSERT_EQUAL_UINT64(pal_get_us(), pal_wasm_get_virtual_clock_us());
}

/* ─────────────────────────────────────────────────────────
 * (3) 阈值以下：不触发
 *     CLOCK_WARNING_THRESHOLD 为 UINT64 中点 0x8000000000000000。
 *     此处推进 1 秒（1_000_000 us），远小于阈值，警告不应触发。
 * ───────────────────────────────────────────────────────── */
void test_clock_warning_not_fired_below_threshold(void) {
    pal_wasm_advance_virtual_clock(1000000ULL);  /* 1 秒 */
    TEST_ASSERT_FALSE(pal_wasm_is_clock_warning_fired());
}

/* ─────────────────────────────────────────────────────────
 * (4) 跨越阈值：触发一次，幂等
 *     直接推进到中点之上。注意：本用例与前序用例共享同一 wasm 实例
 *     时累计值仍远小于 UINT64_MAX，不会回绕。
 * ───────────────────────────────────────────────────────── */
void test_clock_warning_fires_when_crossing_threshold(void) {
    /* 阈值为 0x8000000000000000；一次性推进到阈值之上 */
    pal_wasm_advance_virtual_clock(0x8000000000000001ULL);
    TEST_ASSERT_TRUE(pal_wasm_is_clock_warning_fired());

    /* 幂等性：再次推进，accessor 仍返回 true，不会"再次触发"——
     * 业务上 JS 侧只关心 false→true 一次性沿，重复读不应有副作用。 */
    pal_wasm_advance_virtual_clock(1000ULL);
    TEST_ASSERT_TRUE(pal_wasm_is_clock_warning_fired());
}

/* ─────────────────────────────────────────────────────────
 * 主入口
 * ───────────────────────────────────────────────────────── */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_clock_64bit_static_assert);
    RUN_TEST(test_clock_warning_not_fired_initially);
    RUN_TEST(test_clock_warning_not_fired_below_threshold);
    RUN_TEST(test_clock_warning_fires_when_crossing_threshold);
    return UNITY_END();
}
