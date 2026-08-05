// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_fault_log.c
 * @brief ADR-0009 Wave 2 Task 8 — fault audit log (causal-chain tracing) tests.
 *
 * The fault log is a 256-entry ring buffer in pal_wasm_physical.c that records
 * every physical-degradation event (GPIO bounce entry, I2C drop, future RC
 * noise / clock drift). It is the diagnostic SSOT for CI replay: a failing
 * scenario can be reconstructed by reading back the recorded sequence.
 *
 * Coverage:
 *   1. Fresh BSS / post-reset → count == 0, get_event(0) returns false.
 *   2. log_fault(GPIO_BOUNCE, pin=5) → count == 1, retrieval round-trips
 *      all fields and assigns sequence = 1.
 *   3. log_fault N>256 times → count saturates at 256 (no overflow,
 *      ring buffer semantics hold).
 *
 * Build wiring: source-only, same pattern as test_clock_overflow.c /
 * test_wasm_physical.c — no add_wink_wasm_test CMake helper exists yet.
 * Built downstream as part of the wasm test harness (Task 5/6 plan item).
 *
 * Dependencies:
 *   - pal_wasm_reset_fault_log / pal_wasm_log_fault / pal_wasm_get_fault_log_count
 *     / pal_wasm_get_fault_event (pal_wasm_internal.h, Task 8)
 *   - wasm_fault_event_t, wasm_fault_type_t (pal_wasm_internal.h, Task 8)
 */
#include "unity.h"
#include "pal_wasm_internal.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

void setUp(void) {
    pal_wasm_reset_fault_log();
}

void tearDown(void) {}

/* ─────────────────────────────────────────────────────────
 * (1) Empty initially: post-reset / BSS state
 * ─────────────────────────────────────────────────────────
 * After pal_wasm_reset_fault_log() (or fresh BSS via Emscripten loader),
 * count must be 0 and get_event(0) must return false. This guards against
 * stale state leaking across CI scenarios.
 */
void test_fault_log_empty_initially(void) {
    TEST_ASSERT_EQUAL_UINT32(0, pal_wasm_get_fault_log_count());

    wasm_fault_event_t event;
    bool ok = pal_wasm_get_fault_event(0, &event);
    TEST_ASSERT_FALSE(ok);
}

/* ─────────────────────────────────────────────────────────
 * (2) Single event round-trip
 * ─────────────────────────────────────────────────────────
 * log_fault must persist (type, pin) verbatim and assign a monotonically
 * increasing sequence (first event = 1). Sequence numbering is the diff
 * marker for "did the log advance since I last checked it" in CI replay.
 */
void test_fault_log_records_bounce_event(void) {
    pal_wasm_log_fault(FAULT_TYPE_GPIO_BOUNCE, 5);

    TEST_ASSERT_EQUAL_UINT32(1, pal_wasm_get_fault_log_count());

    wasm_fault_event_t event;
    bool ok = pal_wasm_get_fault_event(0, &event);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT8(FAULT_TYPE_GPIO_BOUNCE, event.fault_type);
    TEST_ASSERT_EQUAL_UINT16(5, event.pin_or_bus);
    TEST_ASSERT_EQUAL_UINT32(1, event.sequence);
}

/* ─────────────────────────────────────────────────────────
 * (3) Wrap-around / overflow saturation
 * ─────────────────────────────────────────────────────────
 * The ring buffer holds the most-recent WASM_FAULT_LOG_SIZE (=256)
 * events. After logging 300 events the count must clamp at 256 — and
 * crucially, no out-of-bounds write may corrupt adjacent BSS (verified
 * implicitly by relying on the test harness's ASAN / wasm bounds checks
 * not flagging a fault).
 *
 * Eviction order is also asserted: entries 0..43 (sequences 1..44) must
 * have been overwritten, so slot 0 now holds the oldest surviving event
 * with sequence == 45 (= 300 - 256 + 1). A buggy impl that merely clamps
 * writes would leave slot 0 at sequence 1 and fail this check.
 */
void test_fault_log_wraps_around(void) {
    for (int i = 0; i < 300; i++) {
        pal_wasm_log_fault(FAULT_TYPE_I2C_DROP, 0);
    }
    TEST_ASSERT_EQUAL_UINT32(256, pal_wasm_get_fault_log_count());

    wasm_fault_event_t evt;
    bool ok = pal_wasm_get_fault_event(0, &evt);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT32(45, evt.sequence);  /* 300 - 256 + 1 — oldest surviving */
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_fault_log_empty_initially);
    RUN_TEST(test_fault_log_records_bounce_event);
    RUN_TEST(test_fault_log_wraps_around);
    return UNITY_END();
}
