/**
 * @file test_power_model_stub.c
 * @brief ADR-0009 Wave 2 Task 9 — power-model API stubs (Wave3 forward compat).
 *
 * Wave3 will introduce a per-pin energy/power model that lives downstream of
 * the same fault-injection pipeline driven by pal_wasm_physical.c.  Adding the
 * setter/getter ABI *now* — even as zero-effect stubs — locks the symbol set
 * in place so Wave3 can light up the real calculation behind these symbols
 * without churning the wasm/JS bridge or every call site.
 *
 * Coverage:
 *   1. pal_wasm_get_total_energy_mj() is callable, returns a sensible value
 *      (>= 0 by type; stub documented to return 0).
 *   2. pal_wasm_set_pin_power_model() accepts a well-formed model and reports
 *      WINK_OK.  Validates the type wasm_pin_power_model_t exists with the
 *      committed field set (active_current_ua, leakage_current_ua,
 *      transition_energy_nj).
 *
 * Build wiring: source-only delivery, same pattern as test_fault_log.c /
 * test_wasm_physical.c — no add_wink_wasm_test CMake helper exists yet.
 * Built downstream as part of the wasm test harness (Wave2 plan follow-up).
 *
 * Dependencies:
 *   - pal_wasm_set_pin_power_model / pal_wasm_get_total_energy_mj
 *     (pal_wasm_internal.h, this task)
 *   - wasm_pin_power_model_t (pal_wasm_internal.h, this task)
 *   - WINK_OK (wink_status.h, transitively via wink_sim_physical.h)
 */
#include "unity.h"
#include "pal_wasm_internal.h"

#include <stdint.h>

void setUp(void) {}
void tearDown(void) {}

/* ─────────────────────────────────────────────────────────
 * (1) API surface exists — stub returns sentinel zero
 * ─────────────────────────────────────────────────────────
 * The getter is callable from C-side tests (and therefore from the JS Worker
 * via cwrap once exported in wasm_bridge.h).  Wave3 will replace the body
 * with an integrated P=I*V accumulator; this test guards against the symbol
 * disappearing or changing signature.
 */
void test_power_model_api_exists(void) {
    /* 接口应该存在（即使是 stub 实现） */
    uint64_t energy = pal_wasm_get_total_energy_mj();
    TEST_ASSERT_TRUE(energy >= 0);
}

/* ─────────────────────────────────────────────────────────
 * (2) Per-pin setter compiles with the committed model struct
 * ─────────────────────────────────────────────────────────
 * Stub does not persist the model (no storage allocated yet), but it must
 * validate inputs and return WINK_OK on the happy path.  Field names are
 * load-bearing — Wave3's calculation will read these exact names from
 * device-tree-supplied power models.
 */
void test_power_model_pin_api_compiles(void) {
    wasm_pin_power_model_t model = {
        .active_current_ua = 1000,
        .leakage_current_ua = 10,
        .transition_energy_nj = 100
    };
    /* 设置应返回 OK（即使 stub 不真正生效） */
    wink_status_t status = pal_wasm_set_pin_power_model(5, &model);
    TEST_ASSERT_EQUAL(WINK_OK, status);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_power_model_api_exists);
    RUN_TEST(test_power_model_pin_api_compiles);
    return UNITY_END();
}
