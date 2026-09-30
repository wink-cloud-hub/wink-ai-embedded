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
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos_sync.h"

#include <signal.h>
#include <stdlib.h>
#include <string.h>

extern void esp_idf_ensure_framework_ready(void);

static void abort_handler(int sig) {
    (void)sig;
    exit(1);
}

int main(int argc, char *argv[]) {
    signal(SIGABRT, abort_handler);

    /* Ensure framework pools are ready */
    esp_idf_ensure_framework_ready();

    const char *mode = (argc > 1) ? argv[1] : "delay";

    SemaphoreHandle_t sem = NULL;
    QueueHandle_t q = NULL;
    if (strcmp(mode, "sem") == 0) {
        sem = xSemaphoreCreateBinary();
    } else if (strcmp(mode, "queue") == 0) {
        q = xQueueCreate(1, sizeof(int));
    }

    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

    /* Acquire the spinlock — depth becomes 1 for this (no-scheduler) context */
    vPortEnterCritical(&mux);

    if (strcmp(mode, "sem") == 0) {
        /* Blocking take inside critical section MUST assert */
        xSemaphoreTake(sem, 10);
    } else if (strcmp(mode, "queue") == 0) {
        int dummy = 0;
        /* Blocking receive on empty queue inside critical section MUST assert */
        xQueueReceive(q, &dummy, 10);
    } else {
        /* Default mode: vTaskDelay */
        vTaskDelay(1);
    }

    /* Should never reach here if the guard is working */
    vPortExitCritical(&mux);
    return 0;
}
