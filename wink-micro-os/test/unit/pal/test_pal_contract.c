/* PAL 契约完整性编译探针（防符号漂移门禁；P0-1 对策）。
 *
 * 背景：ESP32 target 代码在 `#if defined(ESP_PLATFORM)` 内引用 PAL 契约符号，而 host 构建
 * 不编译 ESP32 .c，曾导致 WINK_ERR_HARDWARE / WINK_MUTEX_WAIT_FOREVER / PAL_RESET_REASON_*
 * 等「被引用却未在头中定义」的符号长期未被发现（2026-06 评审 P0-1，ESP32 在 idf.py 下无法编译）。
 *
 * 本测试 include 全部 PAL 契约头，并对每一个【跨 target 被使用】的枚举/宏符号做编译期断言。
 * 任何符号缺失或取值漂移 → 编译失败 → run-tests 立即拦截。这是 host 端对 ESP32 契约完整性
 * 能给出的最强自检（ESP32 .c 函数体的真机行为仍需 idf.py + 硬件验证，不在本探针范围）。
 */
#include "unity.h"
#include "wink_status.h"
#include "pal_hal.h"
#include "pal_osal.h"
#include "pal_resource.h"
#include "pal_log.h"

/* ---- 编译期：契约符号必须存在且取值正确（核心门禁）---- */
/* C99 compatible static assert to prevent MSVC compilation failures and encoding quirks */
#define STATIC_ASSERT_CONCAT(a, b) a##b
#define STATIC_ASSERT_CONCAT2(a, b) STATIC_ASSERT_CONCAT(a, b)
#define STATIC_ASSERT(cond) typedef char STATIC_ASSERT_CONCAT2(static_assertion_at_line_, __LINE__)[(cond) ? 1 : -1]

STATIC_ASSERT(WINK_OK == 0);
STATIC_ASSERT(WINK_ERR_HARDWARE == -12);
STATIC_ASSERT(WINK_MUTEX_WAIT_FOREVER == 0xFFFFFFFFu);
STATIC_ASSERT(PAL_OS_RESET_REASON_WATCHDOG == 2);
STATIC_ASSERT(PAL_OS_RESET_REASON_PANIC    == 3);
STATIC_ASSERT(PAL_OS_RESET_REASON_SOFTWARE == 4);
STATIC_ASSERT(PAL_OS_RESET_REASON_BROWNOUT == 5);

/* 分级日志级别数值必须与 ESP-IDF esp_log_level_t 一一对应，
 * 否则 ESP32 后端强转映射会错位（P1-L1 门禁）。 */
STATIC_ASSERT(PAL_LOG_LEVEL_NONE  == 0);
STATIC_ASSERT(PAL_LOG_LEVEL_ERROR == 1);
STATIC_ASSERT(PAL_LOG_LEVEL_WARN  == 2);
STATIC_ASSERT(PAL_LOG_LEVEL_INFO  == 3);
STATIC_ASSERT(PAL_LOG_LEVEL_DEBUG == 4);
/* 默认编译期级别：debug 构建≥DEBUG, release 构建≤INFO（debug 必须被裁剪）。 */
#ifndef NDEBUG
STATIC_ASSERT(PAL_LOG_COMPILE_LEVEL >= PAL_LOG_LEVEL_DEBUG);
#endif

void setUp(void) { pal_resource_reset(); }
void tearDown(void) {}

void test_contract_error_codes_and_macros_exist(void) {
    /* 引用符号到 volatile 防 DCE，并断言取值——符号缺失会在编译期（而非此处）失败 */
    static volatile wink_status_t code = WINK_ERR_HARDWARE;
    volatile uint32_t forever = WINK_MUTEX_WAIT_FOREVER;
    TEST_ASSERT_EQUAL_INT(-12, (int)code);
    TEST_ASSERT_EQUAL_INT(0xFFFFFFFFu, forever);
}

void test_contract_release_roundtrip(void) {
    /* 顺带证明新增 pal_resource_release 在 host 链接且行为正确（P2-1） */
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 13, "probe"));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_BUSY, pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 13, "other"));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
                          pal_resource_release(PAL_RESOURCE_GPIO_PIN, 13, "other"));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_resource_release(PAL_RESOURCE_GPIO_PIN, 13, "probe"));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 13, "other"));
}

void test_contract_pal_log_macros_link_and_run(void) {
    /* P1-L1：验证四个日志宏在 host 后端可链接、可调用、不崩溃。
     * 不校验 stderr 输出内容（避免 ANSI 转义/平台差异），只确认调用安全。
     * debug 宏在 release 下展开为 ((void)0)，但在当前 host 测试构建（无 NDEBUG）
     * 会实际调用 pal_log_vprintf——两种情况此处都应编译且不崩溃。 */
    static const char *TAG = "contract";
    pal_log_e(TAG, "contract err test: %d", -1);
    pal_log_w(TAG, "contract warn test");
    pal_log_i(TAG, "contract info test: %s", "ok");
    pal_log_d(TAG, "contract debug test");
    TEST_PASS();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_contract_error_codes_and_macros_exist);
    RUN_TEST(test_contract_release_roundtrip);
    RUN_TEST(test_contract_pal_log_macros_link_and_run);
    return UNITY_END();
}
