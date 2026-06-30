/**
 * @file test_smp_uaf_e2e.c
 * @brief SMP UAF 测试的 host 端到端验证。
 *
 * ⚠️  重要提示：host 环境是单线程的，永远测不出 SMP 并发问题！
 *
 * 本测试的目的仅为：
 *   1. 验证代码可以在 host 下正确编译链接
 *   2. 验证 PAL API 调用链路正确
 *   3. 验证测试逻辑自洽（无 crash、无死循环）
 *
 * 真正的 SMP 并发验证必须在 ESP32 真机上运行！
 */

#include <stdio.h>
#include "test_smp_uaf.h"
#include "pal.h"       /* PAL 聚合头 */
#include "pal_debug.h"

int main(void) {
    pal_debug_printf("================================================\n");
    pal_debug_printf("  SMP UAF Test - Host E2E Build Validation\n");
    pal_debug_printf("================================================\n");
    pal_debug_printf("\n");
    pal_debug_printf("⚠️  IMPORTANT: This is host, SINGLE-THREADED environment!\n");
    pal_debug_printf("   Real SMP UAF validation requires ESP32 dual-core hardware.\n");
    pal_debug_printf("   This test ONLY verifies code compiles and runs without crash.\n");
    pal_debug_printf("\n");

    /* 在 host 下运行一个简化版本（轮数少一点，验证 API 链路） */
    test_smp_uaf_result_t result;
    test_smp_uaf_run(100,   /* 少量轮数，仅验证不 crash */
                     10,    /* 少量触发 */
                     true,  /* 启用 synchronize */
                     &result);

    /* host 下单线程，synchronize 应该永远不会触发 UAF */
    if (result.uaf_detected) {
        pal_debug_printf("\n❌ UNEXPECTED: UAF detected on host (should never happen!)\n");
        return 1;
    }

    if (result.passed_rounds != result.total_rounds) {
        pal_debug_printf("\n❌ Test did not complete all rounds\n");
        return 1;
    }

    pal_debug_printf("\n");
    pal_debug_printf("✅ Host E2E validation PASSED\n");
    pal_debug_printf("   Code structure is correct, APIs work.\n");
    pal_debug_printf("   Run on ESP32 hardware for REAL SMP validation!\n");
    pal_debug_printf("\n");

    return 0;
}
