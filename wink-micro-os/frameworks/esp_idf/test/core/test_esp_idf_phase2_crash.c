/* SPDX-License-Identifier: GPL-3.0-only
 *
 * test_esp_idf_phase2_crash.c — M3 crash-target for spinlock yield-guard test
 *
 * This is a standalone process (no Unity) that is expected to CRASH via
 * assert() when vTaskDelay() is called while holding a portMUX_TYPE spinlock.
 *
 * CTest registers this with WILL_FAIL TRUE so the non-zero exit (SIGABRT)
 * is interpreted as the expected PASS signal for the guard assertion test.
 *
 * Per PLAN-20260926-ESP-IDF-SIM-PHASE2 §6 Task 5 Step 5.1 (M3 option A).
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos_sync.h"

#include <signal.h>
#include <stdlib.h>

static void abort_handler(int sig) {
    (void)sig;
    exit(1);
}

int main(void) {
    signal(SIGABRT, abort_handler);

    /* Ensure framework pools are ready */
    esp_freertos_pools_reset();

    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

    /* Acquire the spinlock — depth becomes 1 for this (no-scheduler) context */
    vPortEnterCritical(&mux);

    /* This call MUST trigger esp_freertos_assert_not_in_critical("vTaskDelay")
     * → assert(0) → SIGABRT → non-zero exit.
     * If the guard is missing, the process exits 0 and CTest fails (WILL_FAIL). */
    vTaskDelay(1);

    /* Should never reach here if the guard is working */
    vPortExitCritical(&mux);
    return 0;
}
