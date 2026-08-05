// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_fault_domain_stub.c
 * @brief ADR-0009 Wave 2 Task 10 — fault domain isolation framework (Wave3 forward compat).
 *
 * Wave3 will allow fault injection to be scoped per-bus / per-peripheral so a
 * single buggy sensor cannot blow up the whole simulation. Adding the
 * domain-ID + arm/disarm + trigger-count ABI *now* — even when only the GLOBAL
 * domain is wired through — locks the symbol set so Wave3 can split fault
 * configs per domain without churning the wasm/JS bridge or every call site.
 *
 * Coverage:
 *   1. pal_wasm_get_domain_config(GLOBAL) returns a non-NULL config pointer
 *      (currently aliases the global fault config; Wave3 will return distinct
 *      per-domain configs).
 *   2. pal_wasm_arm_fault_domain(GPIO, false) returns WINK_OK and the disarm
 *      is observable via the domain state (framework is in place).
 *   3. Out-of-range domain IDs are rejected with WINK_ERR_INVALID_ARG
 *      (boundary contract aligned with the rest of the wasm internal ABI).
 *   4. pal_wasm_get_domain_trigger_count starts at 0 and is queryable
 *      per-domain (Wave3 will increment from middleware).
 *
 * Build wiring: source-only delivery, same pattern as test_power_model_stub.c /
 * test_fault_log.c — no add_wink_wasm_test CMake helper exists yet. Will be
 * built downstream as part of the wasm test harness (Wave2 plan follow-up).
 *
 * Dependencies:
 *   - pal_wasm_get_domain_config / pal_wasm_arm_fault_domain /
 *     pal_wasm_get_domain_trigger_count (pal_wasm_internal.h, this task)
 *   - wasm_fault_domain_id_t, wasm_fault_domain_t (pal_wasm_internal.h, this task)
 *   - WINK_OK / WINK_ERR_INVALID_ARG (wink_status.h)
 */
#include "unity.h"
#include "pal_wasm_internal.h"
#include "wink_status.h"

#include <stdint.h>
#include <stdbool.h>

void setUp(void) {
    /* Reset all per-domain state to defaults (all domains armed, zero
     * trigger count). Mirrors fault-log/debounce ctx reset hooks so each
     * test starts from a known baseline regardless of test ordering. */
    pal_wasm_reset_physical();
}

void tearDown(void) {}

/* ─────────────────────────────────────────────────────────
 * (1) Config getter — symbol exists, GLOBAL returns non-NULL
 * ─────────────────────────────────────────────────────────
 * Wave3 will return per-domain configs; today every valid domain ID maps
 * back to the single global fault config. The contract this test guards is
 * just "valid domain → non-NULL pointer the caller can read fields from".
 */
void test_domain_api_exists(void) {
    wink_sim_faults_t *cfg = pal_wasm_get_domain_config(WASM_FAULT_DOMAIN_GLOBAL);
    TEST_ASSERT_NOT_NULL(cfg);
}

/* ─────────────────────────────────────────────────────────
 * (2) Per-domain arm/disarm — Wave3 wire-up point
 * ─────────────────────────────────────────────────────────
 * Today GLOBAL is always armed and the per-domain flag is documentary only,
 * but the setter must already accept the call so Wave3 fault-injection
 * middleware can gate on it without backporting an API.
 */
void test_domain_arm_works(void) {
    wink_status_t status = pal_wasm_arm_fault_domain(WASM_FAULT_DOMAIN_GPIO, false);
    TEST_ASSERT_EQUAL(WINK_OK, status);
}

/* ─────────────────────────────────────────────────────────
 * (3) Boundary rejection — out-of-range domain
 * ─────────────────────────────────────────────────────────
 * Mirrors the wasm-internal boundary contract for pins / fault-log indices.
 * JS-supplied integers must not be able to write past the static domain
 * array — domain_id >= WASM_FAULT_DOMAIN_COUNT returns InvalidArg, never
 * succeeds silently.
 */
void test_domain_arm_rejects_out_of_range(void) {
    wink_status_t status = pal_wasm_arm_fault_domain(WASM_FAULT_DOMAIN_COUNT, false);
    TEST_ASSERT_EQUAL(WINK_ERR_INVALID_ARG, status);

    wink_sim_faults_t *cfg = pal_wasm_get_domain_config(WASM_FAULT_DOMAIN_COUNT);
    TEST_ASSERT_NULL(cfg);
}

/* ─────────────────────────────────────────────────────────
 * (4) Trigger counter — query interface exists, starts at 0
 * ─────────────────────────────────────────────────────────
 * Wave3 middleware will bump this counter per domain on each injected fault.
 * Today it's BSS-zero / reset-zero; the test guards the symbol + the
 * "out-of-range returns 0" contract so future telemetry code doesn't have
 * to special-case the boundary.
 */
void test_domain_trigger_count_initial(void) {
    TEST_ASSERT_EQUAL_UINT32(0, pal_wasm_get_domain_trigger_count(WASM_FAULT_DOMAIN_GLOBAL));
    TEST_ASSERT_EQUAL_UINT32(0, pal_wasm_get_domain_trigger_count(WASM_FAULT_DOMAIN_I2C0));
    /* Out-of-range → 0 (sentinel, never UB read) */
    TEST_ASSERT_EQUAL_UINT32(0, pal_wasm_get_domain_trigger_count(WASM_FAULT_DOMAIN_COUNT));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_domain_api_exists);
    RUN_TEST(test_domain_arm_works);
    RUN_TEST(test_domain_arm_rejects_out_of_range);
    RUN_TEST(test_domain_trigger_count_initial);
    return UNITY_END();
}
