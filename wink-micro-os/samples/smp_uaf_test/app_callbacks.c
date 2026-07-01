/**
 * @file app_callbacks.c
 * @brief SMP UAF 测试的 Wink App 回调接口。
 *
 * 通过标准 wink_app_get_callbacks() 接口暴露给 ESP32 firmware 入口。
 * app_init() 中执行完整测试套件，然后进入空 loop。
 */

#include "wink_app.h"
#include "wink_trace.h"
#include "pal.h"       /* PAL 聚合头 */
#include "pal_debug.h"
#include "smp_uaf_test.h"

/* ─────────────────────────────────────────────────────────
 * 测试配置
 * ───────────────────────────────────────────────────────── */

#define TEST_ROUNDS              1000u    /* 测试轮数 */
#define TEST_TRIGGERS_PER_ROUND  200u     /* 每轮中断触发次数 */
#define TEST_ENABLE_SYNCHRONIZE  false    /* 是否启用 synchronize()
                                            * 注释掉再编译，验证 UAF 会发生 */

/* ─────────────────────────────────────────────────────────
 * App 回调实现
 * ───────────────────────────────────────────────────────── */

static void app_init(void) {
    test_smp_uaf_result_t result;

    pal_debug_printf("\n");
    pal_debug_printf("╔════════════════════════════════════════════════════╗\n");
    pal_debug_printf("║    SMP UAF Validation Test for pal_irq_synchronize  ║\n");
    pal_debug_printf("╚══════════════════════════════════════════════════════╝\n");
    pal_debug_printf("\n");

    /* Step 1: 验证 synchronize() 真的在阻塞 */
    uint32_t blocked_us;
    bool sync_works = test_smp_synchronize_blocks(&blocked_us);
    result.synchronize_time_us = blocked_us;
    result.synchronize_works = sync_works;

    /* Step 2: 运行主 UAF 压力测试 */
    pal_debug_printf("\n");
    test_smp_uaf_run(TEST_ROUNDS,
                     TEST_TRIGGERS_PER_ROUND,
                     TEST_ENABLE_SYNCHRONIZE,
                     &result);

    /* Step 3: 打印最终结果 */
    test_smp_uaf_print_result(&result);

    pal_debug_printf("\n");
    pal_debug_printf("╔══════════════════════════════════════════════════════╗\n");
    if (result.uaf_detected) {
        pal_debug_printf("║  ❌ CONCLUSION: synchronize() IS NECESSARY on SMP!   ║\n");
    } else if (result.passed_rounds == result.total_rounds) {
        pal_debug_printf("║  ✅ CONCLUSION: synchronize() correctly prevents UAF  ║\n");
    }
    pal_debug_printf("╚═══════════════════════════════════════════════════════╝\n");
    pal_debug_printf("\n");
    pal_debug_printf("Test completed. Entering idle loop...\n");
    pal_debug_printf("\n");
}

static void app_loop(void) {
    /* 测试完成后闲置，保持系统运行以便观察 */
}

static void app_on_fault(uint32_t fault_code) {
    wink_trace_fault(fault_code);
}

/* ─────────────────────────────────────────────────────────
 * 回调工厂（Wink App 标准入口）
 * ───────────────────────────────────────────────────────── */

const wink_app_callbacks_t *wink_app_get_callbacks(void) {
    static const wink_app_callbacks_t cb = {
        .init = app_init,
        .loop = app_loop,
        .on_fault = app_on_fault,
    };
    return &cb;
}
