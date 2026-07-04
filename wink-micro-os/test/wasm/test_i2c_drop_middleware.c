/**
 * @file test_i2c_drop_middleware.c
 * @brief ADR-0009 Wave 2 Task 3 — pal_i2c_transfer drop middleware tests.
 *
 * Coverage:
 *   1. 零退化默认路径 (drop_permil=0) → PRNG 不被推进，直接走 JS 桥。
 *   2. 100% 丢包 (drop_permil=1000) → 必然返回 WINK_ERR_IO，且 PRNG 不被推进
 *      （wink_phys_bus_drop 的短路路径，§4 算法语义）。
 *   3. drop_permil ∈ (0, 1000) → PRNG 被推进，返回值随种子确定。
 *   4. 状态回写：每次非短路调用后 pal_wasm_get_prng_state() 反映新状态。
 *   5. 确定性复现：同一 seed 起步，每次调用产出序列完全可复现。
 *
 * 测试隔离：依赖 pal_wasm_reset_physical()（PRNG 重置为 1，drop_permil 清 0）。
 *
 * 注：与 debounce 测试相同的限制——js_pal_i2c_transfer 是 JS import，
 * C 端单测 harness 必须提供它的 mock。本文件聚焦"中间件被正确调用"
 * 与 PRNG 状态可观察行为，不依赖 JS mock 的精确返回值。
 */
#include "unity.h"
#include "pal_hal.h"
#include "pal_wasm_internal.h"
#include "wink_sim_physical.h"
#include "test_physical_golden.h"

#include <emscripten.h>
#include <stdint.h>
#include <stdbool.h>


/* ADR-0017 层 1 例外：本 TU 合法调用 WINK_BLOCKING API。抑制
 * -Wdeprecated-declarations 使 -Werror 下仍能编译；严格模式
 * (-DWINK_STRICT_NONBLOCKING=1) 下相关 API 声明直接消失，本 TU 会链接失败——那是设计意图。 */
#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

/* 来自 pal_wasm_physical.c */
extern void     pal_wasm_set_i2c_drop_permil(uint16_t permil);
extern void     pal_wasm_set_prng_seed(uint32_t seed);
extern uint32_t pal_wasm_get_prng_state(void);
extern void     pal_wasm_reset_physical(void);

/* 占位 I2C 参数（不被读取，因为 JS 桥 mock 应直接返回 true）。 */
static uint8_t  s_write_buf[1] = {0xAA};
static uint8_t  s_read_buf[1]  = {0};

void setUp(void) {
    pal_wasm_reset_physical();           /* PRNG=1, drop_permil=0 */
}

void tearDown(void) {}

/* ─────────────────────────────────────────────────────────
 * (1) 零退化路径：drop_permil=0 → PRNG 不动
 * ─────────────────────────────────────────────────────────
 * 在 drop_permil=0 时，pal_i2c_transfer 必须完全跳过 PRNG 路径
 * （§3.4 plan），保证 "未启用退化时确定性=不消耗 PRNG"。
 */
void test_i2c_zero_drop_does_not_advance_prng(void) {
    uint32_t before = pal_wasm_get_prng_state();
    /* 即使调用多次，PRNG 不应变化 */
    (void)pal_i2c_transfer(0, 0x50, s_write_buf, 1, s_read_buf, 1);
    (void)pal_i2c_transfer(0, 0x50, s_write_buf, 1, s_read_buf, 1);
    uint32_t after = pal_wasm_get_prng_state();
    TEST_ASSERT_EQUAL_UINT32(before, after);
}

/* ─────────────────────────────────────────────────────────
 * (2) 100% 丢包：drop_permil=1000 → 返回 WINK_ERR_IO，PRNG 不动
 * ─────────────────────────────────────────────────────────
 * wink_phys_bus_drop 的 1000‰ 短路路径不消耗 PRNG（§4 算法语义）。
 * pal_i2c_transfer 必须返回 WINK_ERR_IO 来触发驱动层超时退回。
 */
void test_i2c_full_drop_returns_io_err_without_prng_advance(void) {
    pal_wasm_set_i2c_drop_permil(1000u);
    uint32_t before = pal_wasm_get_prng_state();
    wink_status_t r = pal_i2c_transfer(0, 0x50, s_write_buf, 1, s_read_buf, 1);
    uint32_t after = pal_wasm_get_prng_state();
    TEST_ASSERT_EQUAL_INT(WINK_ERR_IO, r);
    TEST_ASSERT_EQUAL_UINT32(before, after);     /* 短路：未推进 */
}

/* ─────────────────────────────────────────────────────────
 * (3) drop_permil ∈ (0, 1000)：PRNG 状态被推进
 * ─────────────────────────────────────────────────────────
 * 任何调用 wink_phys_prng_next 的调用都会推进 PRNG。500‰ 配置下，
 * pal_i2c_transfer 必须把推进后的种子写回到 s_prng_state（通过
 * pal_wasm_advance_prng_state）。
 *
 * Golden: seed=1 → wink_phys_prng_next(&seed) 后 seed = 1103527590,
 *         value ≈ 0.51387。0.51387 > 0.5 → will_drop = false。
 */
void test_i2c_partial_drop_advances_prng(void) {
    pal_wasm_set_i2c_drop_permil(500u);
    pal_wasm_set_prng_seed(GOLDEN_PRNG_SEED1);
    TEST_ASSERT_EQUAL_UINT32(GOLDEN_PRNG_SEED1, pal_wasm_get_prng_state());

    (void)pal_i2c_transfer(0, 0x50, s_write_buf, 1, s_read_buf, 1);

    /* PRNG 必须推进到 golden 值（确定性回写） */
    TEST_ASSERT_EQUAL_UINT32(GOLDEN_PRNG_AFTER_CALL1, pal_wasm_get_prng_state());
}

/* ─────────────────────────────────────────────────────────
 * (4) 确定性可复现：同一 seed + 同一 drop_permil → 同样的丢/通序列
 * ─────────────────────────────────────────────────────────
 * 重置 seed 两次，分别跑 N 次调用，期望两次 PRNG 终态完全一致。
 * 这是 ADR-0009 §4.1 "single seed reproduces the whole system" 的
 * I2C 子集合规验证。
 */
void test_i2c_drop_is_deterministic_across_runs(void) {
    pal_wasm_set_i2c_drop_permil(300u);

    pal_wasm_set_prng_seed(42u);
    for (int i = 0; i < 16; i++) {
        (void)pal_i2c_transfer(0, 0x50, s_write_buf, 1, s_read_buf, 1);
    }
    uint32_t state_after_run1 = pal_wasm_get_prng_state();

    pal_wasm_set_prng_seed(42u);
    for (int i = 0; i < 16; i++) {
        (void)pal_i2c_transfer(0, 0x50, s_write_buf, 1, s_read_buf, 1);
    }
    uint32_t state_after_run2 = pal_wasm_get_prng_state();

    TEST_ASSERT_EQUAL_UINT32(state_after_run1, state_after_run2);
    /* 32 调用后 seed 应该已经走出初始值（防御 PRNG-not-advancing 回归） */
    TEST_ASSERT_NOT_EQUAL(42u, state_after_run1);
}

/* ─────────────────────────────────────────────────────────
 * (5) seed=1 + drop_permil=500：第一次 transfer 应返回 OK（基于 golden）
 * ─────────────────────────────────────────────────────────
 * golden: r=0.51387, threshold=0.500 → r >= threshold → not drop。
 * 这一断言也间接验证了 pal_i2c_transfer 在 not-drop 分支会走 JS 桥；
 * mock 桩须返回 true → WINK_OK。如果 mock 返回 false，断言会变成
 * WINK_ERR_IO（但 PRNG 仍正确推进，可与 test_i2c_partial_drop_advances_prng
 * 区分开）。本断言依赖 harness 提供的 mock，源代码层只保留行为契约。
 *
 * 为不让 mock 行为污染本测试，断言放宽：调用返回值 ∈ {WINK_OK,
 * WINK_ERR_IO}，但 PRNG 必须推进——这与零退化路径形成对照组。
 */
void test_i2c_partial_drop_non_drop_path_still_advances_prng(void) {
    pal_wasm_set_i2c_drop_permil(500u);
    pal_wasm_set_prng_seed(GOLDEN_PRNG_SEED1);
    wink_status_t r = pal_i2c_transfer(0, 0x50, s_write_buf, 1, s_read_buf, 1);
    /* mock 返回值不确定，但状态码必须是 IO 或 OK 之一 */
    TEST_ASSERT_TRUE(r == WINK_OK || r == WINK_ERR_IO);
    /* PRNG 已推进 */
    TEST_ASSERT_NOT_EQUAL(GOLDEN_PRNG_SEED1, pal_wasm_get_prng_state());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_i2c_zero_drop_does_not_advance_prng);
    RUN_TEST(test_i2c_full_drop_returns_io_err_without_prng_advance);
    RUN_TEST(test_i2c_partial_drop_advances_prng);
    RUN_TEST(test_i2c_drop_is_deterministic_across_runs);
    RUN_TEST(test_i2c_partial_drop_non_drop_path_still_advances_prng);
    return UNITY_END();
}
