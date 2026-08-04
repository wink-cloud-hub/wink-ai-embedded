#include "unity.h"
#include "wink_runtime.h"
#include "wink_tasks.h"
#include "wink_trace.h"
#include "wink_actuator_registry.h"
#include "pal_osal.h"
#include "host_test_ctrl.h"   /* sim_set_reset_reason（Phase 5 boot safe-lock 测试） */


/* ADR-0017 层 1 例外：本 TU 合法调用 WINK_BLOCKING API。抑制
 * -Wdeprecated-declarations 使 -Werror 下仍能编译；严格模式
 * (-DWINK_STRICT_NONBLOCKING=1) 下相关 API 声明直接消失，本 TU 会链接失败——那是设计意图。 */
#include "compat/wink_test_compat.h"
WINK_TEST_ALLOW_DEPRECATED

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

/* ---- ADR-0010：boot safe-lock 连续复位计数 + 恢复策略 ---- */

/* 连续异常复位达阈值(3)才锁死；锁死路径 trace 8001 仅一次、safe-off 一次（去重）*/
void test_boot_safe_lock_after_threshold_consecutive_abnormal(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_actuator_register(mock_actuator_off, NULL));
    pal_os_set_abnormal_boot_count(WINK_BOOT_LOCK_THRESHOLD - 1);  /* 已累计 2 次异常复位 */
    sim_set_reset_reason(PAL_OS_RESET_REASON_WATCHDOG);            /* 第 3 次 → 锁死 */
    wink_app_callbacks_t cb = { mock_init, mock_loop, mock_on_fault };
    wink_status_t s = wink_runtime_run(&cb, 1);
    TEST_ASSERT_EQUAL_INT(WINK_ERR_LOCKED, s);
    TEST_ASSERT_EQUAL_INT(0, s_init_calls);                          /* 锁死：init 不执行 */
    TEST_ASSERT_EQUAL_INT(1, s_safe_off_calls);                      /* wink_runtime_fault 内 safe-off 一次 */
    TEST_ASSERT_EQUAL_UINT32(WINK_FAULT_BOOT_AFTER_RESET, wink_trace_last());
    TEST_ASSERT_EQUAL_UINT32(1, wink_trace_count());                 /* 8001 仅 trace 一次（去重）*/
}

/* 单次异常复位（计数 0→1 < 阈值）放行恢复，init/loop 照跑 */
void test_boot_single_watchdog_recovers(void) {
    pal_os_set_abnormal_boot_count(0);
    sim_set_reset_reason(PAL_OS_RESET_REASON_WATCHDOG);
    wink_app_callbacks_t cb = { mock_init, mock_loop, mock_on_fault };
    wink_status_t s = wink_runtime_run(&cb, 5);   /* 5 tick < 健康里程碑，不清零 */
    TEST_ASSERT_EQUAL_INT(WINK_OK, s);
    TEST_ASSERT_EQUAL_INT(1, s_init_calls);              /* 恢复：init 执行 */
    TEST_ASSERT_EQUAL_INT(0, s_safe_off_calls);          /* 未锁死，不 safe-off */
    TEST_ASSERT_EQUAL_UINT32(1, pal_os_get_abnormal_boot_count());  /* 计数累加到 1 */
}

/* 恢复后跑满健康里程碑(200 tick) → 异常复位计数清零（证明已越过崩溃点）*/
void test_boot_count_clears_after_healthy_milestone(void) {
    pal_os_set_abnormal_boot_count(1);
    sim_set_reset_reason(PAL_OS_RESET_REASON_WATCHDOG);
    wink_app_callbacks_t cb = { mock_init, mock_loop, mock_on_fault };
    wink_status_t s = wink_runtime_run(&cb, WINK_BOOT_HEALTHY_TICKS + 5);
    TEST_ASSERT_EQUAL_INT(WINK_OK, s);
    TEST_ASSERT_EQUAL_UINT32(0, pal_os_get_abnormal_boot_count());  /* 跑过里程碑 → 清零 */
}

void test_boot_no_safe_lock_on_power_on_reset(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_actuator_register(mock_actuator_off, NULL));
    sim_set_reset_reason(PAL_OS_RESET_REASON_POWER_ON);
    wink_app_callbacks_t cb = { mock_init, mock_loop, mock_on_fault };
    wink_status_t s = wink_runtime_run(&cb, 1);
    TEST_ASSERT_EQUAL_INT(WINK_OK, s);
    TEST_ASSERT_EQUAL_INT(0, s_safe_off_calls);   /* 非 WDT/PANIC → 不 safe_off */
}

static void mock_loop_wcet_exceeded(void) {
    pal_os_busy_wait_us(6000);   /* 6000us = 6ms, 超过了 10ms tick 的一半 (5ms) */
}

void test_wcet_exceeded_logs_warning_in_trace(void) {
    wink_app_callbacks_t cb = { NULL, mock_loop_wcet_exceeded, NULL };
    wink_status_t s = wink_runtime_run(&cb, 1);
    TEST_ASSERT_EQUAL_INT(WINK_OK, s);
    /* WCET 超限走 wink_trace_warn（非致命） —— 不入 fault 环，只增 warn 计数。 */
    TEST_ASSERT_EQUAL_UINT32(0u, wink_trace_last());
    TEST_ASSERT_EQUAL_UINT32(0u, wink_trace_count());
    TEST_ASSERT_TRUE(wink_warn_count() >= 1u);
}

static void mock_loop_wcet_normal(void) {
    pal_os_busy_wait_us(2000);   /* 2000us = 2ms, 低于限额 */
}

void test_wcet_normal_does_not_log_warning_in_trace(void) {
    wink_app_callbacks_t cb = { NULL, mock_loop_wcet_normal, NULL };
    wink_status_t s = wink_runtime_run(&cb, 1);
    TEST_ASSERT_EQUAL_INT(WINK_OK, s);
    TEST_ASSERT_EQUAL_UINT32(0u, wink_trace_last());
    TEST_ASSERT_EQUAL_UINT32(0u, wink_warn_count());
}

/* ============================================================
 *  wink_periodic_start / wink_periodic_stop (Wave 2)
 * ============================================================
 * Covers the LIGHT (soft-timer-backed) execution path.
 *
 * The MAY_BLOCK path is NOT covered on host: under non-SIMULATION host
 * builds the runtime's main loop does not drive the cooperative fiber
 * scheduler, so a MAY_BLOCK task registered via pal_os_task_create is
 * never scheduled and its callback body would never run.  A MAY_BLOCK
 * test would therefore either hang (on pal_os_sem_take inside stop) or
 * silently record zero firings.  On the real target (ESP-IDF FreeRTOS)
 * the task path executes independently of the main loop and can be
 * exercised end-to-end; that coverage belongs in target-side smoke
 * tests / on-hardware S<N> checks, not here. */

/* File-scope counter incremented by the LIGHT periodic callback. */
static volatile uint32_t s_periodic_light_calls = 0;
static wink_periodic_handle_t s_periodic_light_h = WINK_PERIODIC_INVALID;

static void periodic_light_cb_test(void *ctx) {
    (void)ctx;
    s_periodic_light_calls++;
}

/* Registered as init_status so the periodic starts BEFORE the runtime
 * enters its tick loop — otherwise the soft-timer dispatcher has
 * nothing to walk. */
static wink_status_t init_start_periodic_light(void) {
    /* period 5ms will be rounded up to one WINK_RUNTIME_TICK_MS tick
     * (default 10ms) inside wink_periodic_start's LIGHT branch. */
    wink_periodic_handle_t h = wink_periodic_start(
        "unit_light", 0u, 5u, periodic_light_cb_test, NULL, WINK_PERIODIC_LIGHT);
    s_periodic_light_h = h;
    if (h <= 0) {
        /* Bubble the negative status back as an init failure so the
         * test can pinpoint the failure without racing the loop. */
        return (wink_status_t)h;
    }
    return WINK_OK;
}

void test_periodic_start_stop_light(void) {
    s_periodic_light_calls = 0;
    s_periodic_light_h = WINK_PERIODIC_INVALID;

    /* Minimal no-op callbacks; use init_status so we can return an
     * error from wink_periodic_start without needing extra plumbing. */
    wink_app_callbacks_t cb;
    memset(&cb, 0, sizeof(cb));
    cb.init_status = init_start_periodic_light;

    /* Enough ticks to see several firings of a 1-tick-period timer.
     * host_pal advances the virtual clock inside pal_os_sleep_ms
     * called by wink_app_delay_ms between ticks, so no separate
     * pal_sim_simulate step is needed here. */
    wink_status_t s = wink_runtime_run(&cb, 20u);
    TEST_ASSERT_EQUAL_INT(WINK_OK, s);

    /* Handle returned by wink_periodic_start must be a positive
     * handle (>=1); 0 is reserved for INVALID. */
    TEST_ASSERT_TRUE(s_periodic_light_h > 0);

    /* Core assertion: the periodic callback actually fired at least
     * once during the tick loop. */
    TEST_ASSERT_TRUE_MESSAGE(s_periodic_light_calls > 0u,
        "LIGHT periodic callback never fired during runtime tick loop");

    /* Stop the periodic, then confirm it no longer fires.  Use a fresh
     * callbacks struct with NO init callback so the second runtime_run
     * doesn't start a *new* periodic on top of the stopped one. */
    uint32_t before_stop = s_periodic_light_calls;
    wink_periodic_stop(s_periodic_light_h);

    wink_app_callbacks_t cb_noinit;
    memset(&cb_noinit, 0, sizeof(cb_noinit));
    wink_status_t s2 = wink_runtime_run(&cb_noinit, 10u);
    TEST_ASSERT_EQUAL_INT(WINK_OK, s2);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(before_stop, s_periodic_light_calls,
        "LIGHT periodic callback still firing after wink_periodic_stop");
}

/* MAY_BLOCK path is only meaningful when a real preemptive scheduler
 * drives the task body.  On host non-SIM builds pal_os_task_create
 * merely registers with the cooperative fiber scheduler that
 * wink_runtime_run does not drive, so the task body never executes.
 * Guard the test so it compiles only where the semantics hold. */
#if defined(ESP_PLATFORM)
static volatile uint32_t s_periodic_blk_calls = 0;
static wink_periodic_handle_t s_periodic_blk_h = 0;

static void periodic_blk_cb_test(void *ctx) {
    (void)ctx;
    s_periodic_blk_calls++;
}

static wink_status_t init_start_periodic_blk(void) {
    wink_periodic_handle_t h = wink_periodic_start(
        "unit_blk", 2048u, 5u, periodic_blk_cb_test, NULL, WINK_PERIODIC_MAY_BLOCK);
    s_periodic_blk_h = h;
    if (h <= 0) {
        return (wink_status_t)h;
    }
    return WINK_OK;
}

void test_periodic_start_stop_may_block(void) {
    s_periodic_blk_calls = 0;
    s_periodic_blk_h = 0;

    wink_app_callbacks_t cb;
    memset(&cb, 0, sizeof(cb));
    cb.init_status = init_start_periodic_blk;

    wink_status_t s = wink_runtime_run(&cb, 20u);
    TEST_ASSERT_EQUAL_INT(WINK_OK, s);
    TEST_ASSERT_TRUE(s_periodic_blk_h > 0);
    TEST_ASSERT_TRUE_MESSAGE(s_periodic_blk_calls > 0u,
        "MAY_BLOCK periodic callback never fired");

    wink_periodic_stop(s_periodic_blk_h);
    uint32_t after_stop = s_periodic_blk_calls;

    wink_app_callbacks_t cb_noinit;
    memset(&cb_noinit, 0, sizeof(cb_noinit));
    wink_status_t s3 = wink_runtime_run(&cb_noinit, 10u);
    TEST_ASSERT_EQUAL_INT(WINK_OK, s3);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(after_stop, s_periodic_blk_calls,
        "MAY_BLOCK periodic callback still firing after wink_periodic_stop");
}
#endif /* ESP_PLATFORM */

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_run_calls_init_once_then_loops_n_times);
    RUN_TEST(test_run_null_callbacks_returns_invalid_arg);
    RUN_TEST(test_null_init_callback_treated_as_ok);
    RUN_TEST(test_fault_path_safe_off_before_on_fault);
    RUN_TEST(test_boot_safe_lock_after_threshold_consecutive_abnormal);
    RUN_TEST(test_boot_single_watchdog_recovers);
    RUN_TEST(test_boot_count_clears_after_healthy_milestone);
    RUN_TEST(test_boot_no_safe_lock_on_power_on_reset);
    RUN_TEST(test_wcet_exceeded_logs_warning_in_trace);
    RUN_TEST(test_wcet_normal_does_not_log_warning_in_trace);
    RUN_TEST(test_periodic_start_stop_light);
#if defined(ESP_PLATFORM)
    RUN_TEST(test_periodic_start_stop_may_block);
#endif
    return UNITY_END();
}
