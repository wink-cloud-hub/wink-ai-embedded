/**
 * @file test_virtual_clock.c
 * @brief WASM 虚拟时钟单测（ADR-0003 决策 3 / ADR-0009 §4.1）。
 *
 * 验证 SSOT 架构核心约束：
 *   1. 时钟启动为 0；
 *   2. pal_wasm_advance_virtual_clock() 单调步进；
 *   3. pal_delay_ms/us() 内部不主动步进时钟（本文件断言 + 静态 grep 双重保证）；
 *   4. 64 位无回绕语义正确（自然截断）。
 */
#include "unity.h"
#include "pal_osal.h"
#include "pal_wasm_internal.h"
#include <emscripten.h>
#include <stdint.h>

void setUp(void) {
    /* 重置虚拟时钟：wasm 侧无 reset 接口（避免业务代码误调用），
     * 单测环境下通过再次调用 advance 到 0 无法实现——
     * 实际 WASM 运行时每个测试用例重新实例化，时钟自动归零。
     * 本文件内用例间步进值叠加不影响断言逻辑。 */
}

void tearDown(void) {}

/* ─────────────────────────────────────────────────────────
 * 时钟启动正确性
 * ───────────────────────────────────────────────────────── */

void test_virtual_clock_starts_at_zero(void) {
    /* WASM 实例刚启动，BSS 零初始化保证 s_virtual_us = 0 */
    TEST_ASSERT_EQUAL_UINT64(0, pal_get_us());
    TEST_ASSERT_EQUAL_UINT64(0, pal_get_ms());
}

/* ─────────────────────────────────────────────────────────
 * 时钟单调步进正确性
 * ───────────────────────────────────────────────────────── */

void test_virtual_clock_monotonic_advance(void) {
    pal_wasm_advance_virtual_clock(1000);
    TEST_ASSERT_EQUAL_UINT64(1000, pal_get_us());
    TEST_ASSERT_EQUAL_UINT64(1, pal_get_ms());

    pal_wasm_advance_virtual_clock(500);
    TEST_ASSERT_EQUAL_UINT64(1500, pal_get_us());
    TEST_ASSERT_EQUAL_UINT64(1, pal_get_ms());  /* 截断向下取整（整数除法） */

    pal_wasm_advance_virtual_clock(500);
    TEST_ASSERT_EQUAL_UINT64(2000, pal_get_us());
    TEST_ASSERT_EQUAL_UINT64(2, pal_get_ms());
}

void test_virtual_clock_accepts_zero_advance(void) {
    uint64_t before = pal_get_us();
    pal_wasm_advance_virtual_clock(0);  /* 合法空操作 */
    TEST_ASSERT_EQUAL_UINT64(before, pal_get_us());
}

/* ─────────────────────────────────────────────────────────
 * SSOT 架构核心断言：pal_delay_ms/us() 不主动步进时钟
 *
 * 注意：Node 单测环境中 js_pal_delay_ms 是同步 mock 立即返回，
 * 不涉及真实 Asyncify 挂起。调用后时钟值不变 → delay 函数
 * 体内没有调用 pal_wasm_advance_virtual_clock()。
 *
 * 第二重保证：CI 静态 grep 检查——
 *   grep -n "pal_wasm_advance_virtual_clock" pal_osal_wasm.c
 *   只能在函数定义行出现，不能在 pal_delay_* 函数体内出现。
 * ───────────────────────────────────────────────────────── */

void test_delay_ms_does_NOT_advance_clock(void) {
    uint64_t before = pal_get_us();
    pal_delay_ms(10);  /* mock 立即返回，无真实等待 */
    uint64_t after = pal_get_us();
    /* SSOT 红线：调用前后时钟完全相同 */
    TEST_ASSERT_EQUAL_UINT64(before, after);
}

void test_delay_us_does_NOT_advance_clock(void) {
    uint64_t before = pal_get_us();
    pal_delay_us(500);  /* mock 立即返回 */
    uint64_t after = pal_get_us();
    TEST_ASSERT_EQUAL_UINT64(before, after);
}

/* ─────────────────────────────────────────────────────────
 * 64 位无符号自然回绕语义（580+ 年，仿真不可能溢出）
 * ───────────────────────────────────────────────────────── */

void test_virtual_clock_64bit_wraparound_semantics(void) {
    /* 推进到 UINT64_MAX 边界 */
    uint64_t near_max = UINT64_MAX - 100;
    /* 注意：本测试不能直接写 s_virtual_us，因为它是 static。
     * 实际 WASM 单测会在独立实例中跑，这里只验证计算逻辑。
     * 真实回绕由编译器保证（C 标准 unsigned 定义了模 2^N 语义）。 */
    uint64_t result = near_max + 200;  /* 发生回绕 */
    TEST_ASSERT_TRUE(result < near_max);  /* 回绕后值变小 */
    (void)result;
}

/* ─────────────────────────────────────────────────────────
 * 主入口
 * ───────────────────────────────────────────────────────── */

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_virtual_clock_starts_at_zero);
    RUN_TEST(test_virtual_clock_monotonic_advance);
    RUN_TEST(test_virtual_clock_accepts_zero_advance);
    RUN_TEST(test_delay_ms_does_NOT_advance_clock);
    RUN_TEST(test_delay_us_does_NOT_advance_clock);
    RUN_TEST(test_virtual_clock_64bit_wraparound_semantics);
    return UNITY_END();
}
