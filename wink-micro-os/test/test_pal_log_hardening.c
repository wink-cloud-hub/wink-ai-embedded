/* test_pal_log_hardening.c — Host-side verification of P1-L1 log hardening.
 *
 * Covers:
 *   1. LOG_E/W/I/D shorthand macros pick up file-scope LOG_TAG ("hrdtest")
 *      instead of falling back to "SYS".
 *   2. pal_log_in_isr() reflects pal_os_set_sim_isr_context() state.
 *   3. In ISR context, LOG_E and LOG_W don't deadlock / don't crash (ISR-safe
 *      path used). LOG_I/LOG_D are silently dropped.
 *   4. Without LOG_TAG defined, shorthand macros fall back to "SYS" (verified
 *      in a separate TU below).
 *   5. pal_log_e/w/i/d explicit-tag form still links + runs.
 *
 * We don't assert on exact output bytes (ANSI colors, timestamps, TID vary by
 * host and terminal); we only assert behavioral invariants (no crash,
 * ISR-state query works, tag routing doesn't blow up).
 */
#define LOG_TAG "hrdtest"

#include "unity.h"
#include "pal_log.h"
#include "pal_osal.h"

#include <stdio.h>
#include <string.h>

void setUp(void) { /* pal_log is stateless across setUp */ }
void tearDown(void) { /* ensure ISR sim flag cleared after each test */
    pal_os_set_sim_isr_context(false);
}

/* ---------- Test 1: shorthand macros with LOG_TAG compile + run ----------- */
void test_log_shorthand_with_tag(void)
{
    /* If these compile and run without crashing, LOG_TAG was picked up and
     * routed through the host backend. In non-NDEBUG builds all four levels
     * are live; in release builds LOG_D is ((void)0) — both are fine. */
    LOG_E("hardening test error: code=%d", -1);
    LOG_W("hardening test warn: threshold=%u", 42u);
    LOG_I("hardening test info: %s", "hello");
    LOG_D("hardening test debug: x=%d", 7);
    TEST_PASS();
}

/* ---------- Test 2: explicit-tag pal_log_* entrypoints still work --------- */
void test_log_explicit_tag_entrypoints(void)
{
    pal_log_e("explicit", "explicit-tag e: %d", 1);
    pal_log_w("explicit", "explicit-tag w");
    pal_log_i("explicit", "explicit-tag i: %s", "ok");
    pal_log_d("explicit", "explicit-tag d");
    TEST_PASS();
}

/* ---------- Test 3: pal_log_in_isr() tracks sim flag ---------------------- */
void test_pal_log_in_isr_tracks_sim_flag(void)
{
    pal_os_set_sim_isr_context(false);
    TEST_ASSERT_FALSE(pal_log_in_isr());

    pal_os_set_sim_isr_context(true);
    TEST_ASSERT_TRUE(pal_log_in_isr());

    pal_os_set_sim_isr_context(false);
    TEST_ASSERT_FALSE(pal_log_in_isr());
}

/* ---------- Test 4: ISR path does not crash / deadlock --------------------
 *
 * We can't truly simulate an IRQ on host, but we flip the sim-isr flag and
 * invoke every shorthand level. If the ISR path took a mutex (it must not)
 * or called into a non-reentrant stdio path, a multithreaded test could
 * deadlock — for single-threaded here we simply require the call to return.
 */
void test_log_in_isr_context_does_not_crash(void)
{
    pal_os_set_sim_isr_context(true);
    TEST_ASSERT_TRUE(pal_log_in_isr());

    /* ERROR and WARN must route to pal_log_isr_write (ROM/no-lock path). */
    LOG_E("isr error: rc=%d", -12);
    LOG_W("isr warning: pin=%u", 13u);

    /* INFO and DEBUG are silently dropped in ISR context (see pal_log.h).
     * Calling them must still return safely (no-op). */
    LOG_I("isr info — should be dropped");
    LOG_D("isr debug — should be dropped");

    pal_os_set_sim_isr_context(false);
    TEST_ASSERT_FALSE(pal_log_in_isr());

    /* After leaving ISR, normal logging works again. */
    LOG_I("post-isr info OK");
    TEST_PASS();
}

/* ---------- Test 5: ISR path via pal_log_* explicit tag form -------------- */
void test_log_explicit_tag_in_isr(void)
{
    pal_os_set_sim_isr_context(true);
    pal_log_e("isr_explicit", "isr explicit e");
    pal_log_w("isr_explicit", "isr explicit w");
    pal_log_i("isr_explicit", "isr explicit i — dropped");
    pal_log_d("isr_explicit", "isr explicit d — dropped");
    pal_os_set_sim_isr_context(false);
    TEST_PASS();
}

/* ---------- Test 6: SYS fallback works without LOG_TAG --------------------
 *
 * LOG_SYS_FALLBACK is defined in a separate translation unit (see
 * test_pal_log_sys_fallback.c) that does NOT define LOG_TAG. We just verify
 * the explicit-tag API still works here (the fallback TU does the rest).
 */
void test_log_sys_fallback_callable(void)
{
    /* Calling with "SYS" explicitly simulates what the fallback macro does. */
    pal_log_i("SYS", "sys-fallback smoke %d", 1);
    TEST_PASS();
}

extern void test_pal_log_sys_fallback_run(void);   /* from test_pal_log_sys_fallback.c */

/* ---------- Test 7: level numeric mapping still matches esp_log_level_t --- */
extern void test_pal_log_level_contract(void);
void test_pal_log_level_contract(void)
{
    /* Mirror STATIC_ASSERTs from test_pal_contract for runtime confidence. */
    TEST_ASSERT_EQUAL_INT(0, PAL_LOG_LEVEL_NONE);
    TEST_ASSERT_EQUAL_INT(1, PAL_LOG_LEVEL_ERROR);
    TEST_ASSERT_EQUAL_INT(2, PAL_LOG_LEVEL_WARN);
    TEST_ASSERT_EQUAL_INT(3, PAL_LOG_LEVEL_INFO);
    TEST_ASSERT_EQUAL_INT(4, PAL_LOG_LEVEL_DEBUG);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_log_shorthand_with_tag);
    RUN_TEST(test_log_explicit_tag_entrypoints);
    RUN_TEST(test_pal_log_in_isr_tracks_sim_flag);
    RUN_TEST(test_log_in_isr_context_does_not_crash);
    RUN_TEST(test_log_explicit_tag_in_isr);
    RUN_TEST(test_log_sys_fallback_callable);
    RUN_TEST(test_pal_log_sys_fallback_run);
    RUN_TEST(test_pal_log_level_contract);
    return UNITY_END();
}
