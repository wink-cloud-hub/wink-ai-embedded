/* test_pal_log_sys_fallback.c — Companion TU for test_pal_log_hardening.
 *
 * Deliberately does NOT #define LOG_TAG before including pal_log.h, so the
 * shorthand macros LOG_E/W/I/D must fall back to the default "SYS" tag.
 * Exposed entrypoint is called from main() in test_pal_log_hardening.c.
 */
#include "unity.h"
#include "pal_log.h"

void test_pal_log_sys_fallback_run(void)
{
    /* These compile and run without an explicit tag — "SYS" is used. */
    LOG_I("sys-fallback message: %d", 1);
    LOG_W("sys-fallback warn");
    LOG_E("sys-fallback err: %s", "probe");
    LOG_D("sys-fallback debug");
    TEST_PASS();
}
