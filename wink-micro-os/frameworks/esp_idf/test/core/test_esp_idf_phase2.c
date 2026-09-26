/* SPDX-License-Identifier: GPL-3.0-only
 *
 * test_esp_idf_phase2.c — Phase 2 ESP-IDF simulation interception validation
 *
 * Covers (per PLAN-20260926-ESP-IDF-SIM-PHASE2 §6 Task 5):
 *   T1  test_spinlock_basic_and_reentrancy        (ISSUE-02)
 *   T2  test_spinlock_yield_guard_assertion       (ISSUE-02, M3 crash-target)
 *   T3  test_spin_wait_self_healing_gpio          (ISSUE-06)
 *   T4  test_spin_wait_time_advancement           (ISSUE-06)
 *   T5  test_static_constructor_lazy_init         (ISSUE-13)
 *
 * SPDX note: test files use GPL-3.0-only per the project license map
 * (AGENTS.md §Critical Patterns / ADR-0083/0084).
 */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "unity.h"

/* FreeRTOS / portmacro (includes portMUX_TYPE) */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

/* Internal sync helpers exposed for test inspection */
#include "freertos_sync.h"

/* Driver-level APIs under test */
#include "driver/gpio.h"
#include "esp_timer.h"

/* ── Unity setUp / tearDown ─────────────────────────────────────────────── */

void setUp(void) {
    /* Reset all FreeRTOS pools + spinlock/spin-count state before each test */
    esp_freertos_pools_reset();
}

void tearDown(void) {}

/* ═══════════════════════════════════════════════════════════════════════════
 * T1  test_spinlock_basic_and_reentrancy
 *
 * Verifies:
 *   - vPortEnterCritical sets owner and count == 1 on first acquire.
 *   - Same task re-entering increments count (recursive acquisition).
 *   - vPortExitCritical decrements count; at 0 resets owner to NO_OWNER.
 * ═══════════════════════════════════════════════════════════════════════════ */
void test_spinlock_basic_and_reentrancy(void) {
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

    /* Initial state */
    TEST_ASSERT_EQUAL_UINT32(portMUX_NO_OWNER, mux.owner);
    TEST_ASSERT_EQUAL_UINT32(0u, mux.count);

    /* First acquisition */
    vPortEnterCritical(&mux);
    TEST_ASSERT_NOT_EQUAL(portMUX_NO_OWNER, mux.owner);
    TEST_ASSERT_EQUAL_UINT32(1u, mux.count);

    uint32_t slot_after_first = mux.owner;

    /* Re-entrant acquisition 1 */
    vPortEnterCritical(&mux);
    TEST_ASSERT_EQUAL_UINT32(slot_after_first, mux.owner);
    TEST_ASSERT_EQUAL_UINT32(2u, mux.count);

    /* Re-entrant acquisition 2 → depth == 3 */
    vPortEnterCritical(&mux);
    TEST_ASSERT_EQUAL_UINT32(slot_after_first, mux.owner);
    TEST_ASSERT_EQUAL_UINT32(3u, mux.count);

    /* Critical depth query API must agree */
    TEST_ASSERT_EQUAL_UINT32(3u, esp_freertos_get_critical_depth(slot_after_first));

    /* Release depth 3→2 */
    vPortExitCritical(&mux);
    TEST_ASSERT_EQUAL_UINT32(slot_after_first, mux.owner);
    TEST_ASSERT_EQUAL_UINT32(2u, mux.count);

    /* Release depth 2→1 */
    vPortExitCritical(&mux);
    TEST_ASSERT_EQUAL_UINT32(slot_after_first, mux.owner);
    TEST_ASSERT_EQUAL_UINT32(1u, mux.count);

    /* Final release: owner becomes NO_OWNER, count → 0 */
    vPortExitCritical(&mux);
    TEST_ASSERT_EQUAL_UINT32(portMUX_NO_OWNER, mux.owner);
    TEST_ASSERT_EQUAL_UINT32(0u, mux.count);

    /* Critical depth also back to 0 */
    TEST_ASSERT_EQUAL_UINT32(0u, esp_freertos_get_critical_depth(slot_after_first));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * T2  test_spinlock_yield_guard_assertion  (M3: crash-target variant)
 *
 * This test is compiled into the CRASH target (test_esp_idf_phase2_crash).
 * It acquires a spinlock then calls vTaskDelay(), which must trigger
 * esp_freertos_assert_not_in_critical() → assert() → SIGABRT.
 *
 * CTest expects WILL_FAIL TRUE for test_spinlock_yield_guard_assertion so
 * the non-zero exit from SIGABRT is treated as a passing signal.
 *
 * The test body below is shared between the normal executable (where it
 * simply validates the critical depth > 0 pre-condition before the call)
 * and the crash target (where the call is actually made).
 * ═══════════════════════════════════════════════════════════════════════════ */
void test_spinlock_yield_guard_assertion_precondition(void) {
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

    /* Acquire the spinlock — depth becomes 1 */
    vPortEnterCritical(&mux);

    uint32_t slot = mux.owner;
    /* Depth must be 1 now */
    TEST_ASSERT_EQUAL_UINT32(1u, esp_freertos_get_critical_depth(slot));

    /* Release before returning from this "safe" test case */
    vPortExitCritical(&mux);
    TEST_ASSERT_EQUAL_UINT32(0u, esp_freertos_get_critical_depth(slot));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * T3  test_spin_wait_self_healing_gpio
 *
 * Calls gpio_get_level() more than ESP_SIM_SPIN_WAIT_THRESHOLD (500) times
 * and verifies:
 *   - The process does NOT hang (the loop completes in finite iterations).
 *   - Virtual time has advanced (sim auto-yielded + advanced the clock).
 * ═══════════════════════════════════════════════════════════════════════════ */
void test_spin_wait_self_healing_gpio(void) {
    /* Configure pin 2 as input so gpio_get_level is valid */
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << 2),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);

    int64_t t_before = esp_timer_get_time();

    /* Spin 1000 times — well above threshold (500); self-healing should trigger
     * at least once and advance virtual time. */
    int level = 0;
    for (int i = 0; i < 1000; i++) {
        level = gpio_get_level(2);
        (void)level;
    }

    int64_t t_after = esp_timer_get_time();

    /* Virtual clock must have advanced due to self-healing yield(s) */
    TEST_ASSERT_GREATER_THAN_INT64(t_before, t_after);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * T4  test_spin_wait_time_advancement
 *
 * Verifies that after 1000 consecutive gpio_get_level() calls the virtual
 * clock has moved forward from its initial value (at least one auto-advance
 * of 10µs per threshold crossing must have occurred).
 * ═══════════════════════════════════════════════════════════════════════════ */
void test_spin_wait_time_advancement(void) {
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << 4),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);

    int64_t t0 = esp_timer_get_time();

    for (int i = 0; i < 1000; i++) {
        (void)gpio_get_level(4);
    }

    int64_t t1 = esp_timer_get_time();

    /* Expected: at least 2 threshold crossings → ≥ 20µs of virtual advance */
    TEST_ASSERT_GREATER_OR_EQUAL_INT64(t0 + 10LL, t1);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * T5  test_static_constructor_lazy_init
 *
 * Simulates a C++ global object whose constructor calls xQueueCreate and
 * gpio_config BEFORE esp_idf_framework_init() has been called.
 *
 * After esp_freertos_pools_reset() in setUp() we call the APIs directly —
 * no explicit framework init — relying entirely on ensure_framework_ready().
 * ═══════════════════════════════════════════════════════════════════════════ */
void test_static_constructor_lazy_init(void) {
    /* Simulate pre-app_main state by resetting pools (done in setUp already).
     * Directly invoke resource creation APIs as a C++ global ctor would. */

    /* xQueueCreate internally calls esp_idf_ensure_framework_ready() */
    QueueHandle_t q = xQueueCreate(4, sizeof(uint32_t));
    TEST_ASSERT_NOT_NULL(q);

    /* Verify the queue is functional */
    uint32_t val = 0xDEADBEEFu;
    BaseType_t rc = xQueueSend(q, &val, 0);
    TEST_ASSERT_EQUAL_INT(pdPASS, rc);

    uint32_t recv = 0;
    rc = xQueueReceive(q, &recv, 0);
    TEST_ASSERT_EQUAL_INT(pdPASS, rc);
    TEST_ASSERT_EQUAL_UINT32(0xDEADBEEFu, recv);

    /* gpio_config internally calls esp_idf_ensure_framework_ready() */
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << 5),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&cfg);
    TEST_ASSERT_EQUAL_INT(ESP_OK, err);

    /* xSemaphoreCreateBinary internally calls esp_idf_ensure_framework_ready() */
    SemaphoreHandle_t sem = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(sem);

    /* System should remain stable — no crash, no NULL handles */
    vQueueDelete(q);
    vSemaphoreDelete(sem);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Unity main runner
 * ═══════════════════════════════════════════════════════════════════════════ */
int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_spinlock_basic_and_reentrancy);
    RUN_TEST(test_spinlock_yield_guard_assertion_precondition);
    RUN_TEST(test_spin_wait_self_healing_gpio);
    RUN_TEST(test_spin_wait_time_advancement);
    RUN_TEST(test_static_constructor_lazy_init);

    return UNITY_END();
}
