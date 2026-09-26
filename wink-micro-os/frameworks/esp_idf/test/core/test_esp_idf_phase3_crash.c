/* SPDX-License-Identifier: GPL-3.0-only
 *
 * test_esp_idf_phase3_crash.c — Negative crash test for ISR blocking call rejection
 *
 * This test is expected to CRASH via assert() / abort() when vTaskDelay() is
 * invoked inside an active simulated ISR context (pal_os_in_isr() == true).
 *
 * CTest registers this with WILL_FAIL TRUE so the non-zero exit (SIGABRT)
 * is interpreted as the expected PASS signal for the guard assertion test.
 *
 * Per PLAN-20260927-ESP-IDF-SIM-PHASE3 §6 Task 5.
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos_sync.h"
#include "osal/pal_osal.h"

#include <signal.h>
#include <stdlib.h>

#if defined(_MSC_VER)
#include <crtdbg.h>
#endif

static void abort_handler(int sig) {
    (void)sig;
    exit(1);
}

int main(void) {
#if defined(_MSC_VER)
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG);
#endif
    signal(SIGABRT, abort_handler);

    /* Ensure framework pools are ready */
    esp_freertos_pools_reset();

    /* Enter simulated ISR context */
    pal_os_set_sim_isr_context(true);

    /* This call MUST trigger esp_freertos_assert_not_in_isr("vTaskDelay")
     * -> assert(!pal_os_in_isr()) -> abort() -> SIGABRT -> non-zero exit.
     * If the guard is missing, the process exits 0 and CTest fails (WILL_FAIL). */
    vTaskDelay(1);

    /* Should never reach here if the guard is working */
    pal_os_set_sim_isr_context(false);
    return 0;
}
