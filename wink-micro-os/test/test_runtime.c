#include "unity.h"
#include "wink_runtime.h"
#include "wink_trace.h"
#include "wink_actuator_registry.h"
#include "pal_osal.h"
#include "host_test_ctrl.h"   /* sim_set_reset_reason（Phase 5 boot safe-lock 测试） */

/* 测试用 mock 计数器（静态分配，§6.1 约束1） */
static int s_init_calls = 0;
static int s_loop_calls = 0;
static int s_fault_calls = 0;
static uint32_t s_last_fault = 0;

/* Phase 5：mock 执行器 + on_fault 时捕获 safe_off 已调用次数（验证顺序） */
static int s_safe_off_calls = 0;
static int s_safe_off_calls_at_fault = -1;

static void mock_init(void) { s_init_calls++; }
static void mock_loop(void) {
    s_loop_calls++;
    /* 第 3 次 loop 模拟触发故障并主动上报 trace */
    if (s_loop_calls == 3) {
        wink_trace_fault(7001);
    }
}
static void mock_on_fault(uint32_t code) {
    s_fault_calls++; s_last_fault = code;
    s_safe_off_calls_at_fault = s_safe_off_calls;   /* 捕获 on_fault 时刻 safe_off 已跑次数 */
}
static wink_status_t mock_actuator_off(void *ctx) { (void)ctx; s_safe_off_calls++; return WINK_OK; }

void setUp(void) {
    s_init_calls = s_loop_calls = s_fault_calls = 0;
    s_last_fault = 0;
    s_safe_off_calls = 0;
    s_safe_off_calls_at_fault = -1;
    wink_trace_reset();
    wink_actuator_registry_reset();
    sim_reset_time();   /* 复位 reset reason 为 POWER_ON */
}
void tearDown(void) {}

void test_run_calls_init_once_then_loops_n_times(void) {
    wink_app_callbacks_t cb = { mock_init, mock_loop, mock_on_fault };
    wink_status_t s = wink_runtime_run(&cb, 5);
    TEST_ASSERT_EQUAL_INT(WINK_OK, s);
    TEST_ASSERT_EQUAL_INT(1, s_init_calls);
    TEST_ASSERT_EQUAL_INT(5, s_loop_calls);
}

void test_run_null_callbacks_returns_invalid_arg(void) {
    wink_status_t s = wink_runtime_run(NULL, 5);
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, s);
}

void test_null_init_callback_treated_as_ok(void) {
    /* init/loop 允许 NULL（runtime 跳过），on_fault 允许 NULL */
    wink_app_callbacks_t cb = { NULL, NULL, NULL };
    wink_status_t s = wink_runtime_run(&cb, 3);
    TEST_ASSERT_EQUAL_INT(WINK_OK, s);
}

/* ---- Phase 5 Task 5-3：fault 路径 trace → safe_off_all → on_fault ---- */
void test_fault_path_safe_off_before_on_fault(void) {
    wink_app_callbacks_t cb = { NULL, NULL, mock_on_fault };
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_actuator_register(mock_actuator_off, NULL));
    wink_runtime_fault(&cb, 7500);
    TEST_ASSERT_EQUAL_INT(1, s_safe_off_calls);          /* safe_off 被调用 */
    TEST_ASSERT_EQUAL_INT(1, s_safe_off_calls_at_fault); /* 且在 on_fault 之前（捕获值为 1） */
    TEST_ASSERT_EQUAL_INT(1, s_fault_calls);             /* on_fault 被调用 */
    TEST_ASSERT_EQUAL_UINT32(7500, s_last_fault);
}

/* ---- Phase 5 Task 5-5：boot safe-lock ---- */
void test_boot_safe_lock_on_watchdog_reset(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_actuator_register(mock_actuator_off, NULL));
    sim_set_reset_reason(PAL_RESET_REASON_WATCHDOG);
    wink_app_callbacks_t cb = { mock_init, mock_loop, mock_on_fault };
    wink_status_t s = wink_runtime_run(&cb, 1);
    TEST_ASSERT_EQUAL_INT(WINK_ERR_LOCKED, s);
    TEST_ASSERT_EQUAL_INT(2, s_safe_off_calls);   /* boot 时先关断执行器，进入 fault 时再关断一次 */
    TEST_ASSERT_EQUAL_INT(0, s_init_calls);        /* ADR-0007: init 不执行 */
    TEST_ASSERT_EQUAL_UINT32(WINK_FAULT_BOOT_AFTER_RESET, wink_trace_last());
}

void test_boot_no_safe_lock_on_power_on_reset(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_actuator_register(mock_actuator_off, NULL));
    sim_set_reset_reason(PAL_RESET_REASON_POWER_ON);
    wink_app_callbacks_t cb = { mock_init, mock_loop, mock_on_fault };
    wink_status_t s = wink_runtime_run(&cb, 1);
    TEST_ASSERT_EQUAL_INT(WINK_OK, s);
    TEST_ASSERT_EQUAL_INT(0, s_safe_off_calls);   /* 非 WDT/PANIC → 不 safe_off */
}

static void mock_loop_wcet_exceeded(void) {
    pal_delay_us(6000);   /* 6000us = 6ms, 超过了 10ms tick 的一半 (5ms) */
}

void test_wcet_exceeded_logs_warning_in_trace(void) {
    wink_app_callbacks_t cb = { NULL, mock_loop_wcet_exceeded, NULL };
    wink_status_t s = wink_runtime_run(&cb, 1);
    TEST_ASSERT_EQUAL_INT(WINK_OK, s);
    TEST_ASSERT_EQUAL_UINT32(WINK_WARN_WCET_EXCEEDED, wink_trace_last());
}

static void mock_loop_wcet_normal(void) {
    pal_delay_us(2000);   /* 2000us = 2ms, 低于限额 */
}

void test_wcet_normal_does_not_log_warning_in_trace(void) {
    wink_app_callbacks_t cb = { NULL, mock_loop_wcet_normal, NULL };
    wink_status_t s = wink_runtime_run(&cb, 1);
    TEST_ASSERT_EQUAL_INT(WINK_OK, s);
    TEST_ASSERT_EQUAL_UINT32(0u, wink_trace_last());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_run_calls_init_once_then_loops_n_times);
    RUN_TEST(test_run_null_callbacks_returns_invalid_arg);
    RUN_TEST(test_null_init_callback_treated_as_ok);
    RUN_TEST(test_fault_path_safe_off_before_on_fault);
    RUN_TEST(test_boot_safe_lock_on_watchdog_reset);
    RUN_TEST(test_boot_no_safe_lock_on_power_on_reset);
    RUN_TEST(test_wcet_exceeded_logs_warning_in_trace);
    RUN_TEST(test_wcet_normal_does_not_log_warning_in_trace);
    return UNITY_END();
}
