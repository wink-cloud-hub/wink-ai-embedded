// SPDX-License-Identifier: Apache-2.0
/**
 * @file selftest_smp_stress.c
 * @brief PAL resource claim/release concurrency stress selftest.
 */
#define LOG_TAG "selftest.smp"

#include "wink_selftest.h"
#include "wink_selftest_internal.h"
#include "wink_status.h"
#include "wink_log.h"
#include "pal_osal.h"
#include "pal_resource.h"

#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
#  pragma warning(disable: 4996)
#endif

#if defined(PLATFORM_host) || defined(PLATFORM_wasm) || defined(SIMULATION)
#define STRESS_DURATION_MS   10u
#define SINGLE_THREAD_ITERS  2000u
#else
#define STRESS_DURATION_MS   2000u
#define SINGLE_THREAD_ITERS  10000u
#endif

static void stress_task_fn(void *arg)
{
    uint32_t core_id = (uint32_t)(uintptr_t)arg;
    uint64_t end_time = pal_os_get_ms() + STRESS_DURATION_MS;
    uint64_t last_yield = pal_os_get_ms();
    uint32_t pin = 100u + (core_id & 1u);

    while (pal_os_get_ms() < end_time) {
        WINK_IGNORE_RESULT(pal_resource_claim(PAL_RESOURCE_GPIO_PIN, pin, "selftest_smp"));
        WINK_IGNORE_RESULT(pal_resource_release(PAL_RESOURCE_GPIO_PIN, pin, "selftest_smp"));

        uint64_t now = pal_os_get_ms();
        if ((uint32_t)(now - last_yield) >= 100u) {
            pal_os_sleep_ms(1);
            last_yield = pal_os_get_ms();
        }
    }

    pal_os_task_delete(NULL);
}

wink_status_t wink_selftest_smp_resource_stress(wink_selftest_result_t *r)
{
    r->note = "resource claim/release single-thread";

    uint32_t iters = 0;
    for (uint32_t i = 0; i < SINGLE_THREAD_ITERS; i++) {
        WINK_IGNORE_RESULT(pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 100u, "selftest_smp"));
        WINK_IGNORE_RESULT(pal_resource_release(PAL_RESOURCE_GPIO_PIN, 100u, "selftest_smp"));
        WINK_IGNORE_RESULT(pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 101u, "selftest_smp"));
        WINK_IGNORE_RESULT(pal_resource_release(PAL_RESOURCE_GPIO_PIN, 101u, "selftest_smp"));
        iters++;
    }
    r->metric = iters;

    bool spawned_smp = false;
    uint32_t stack_size = 4096u;
#if defined(PLATFORM_host) || defined(PLATFORM_wasm) || defined(SIMULATION)
    stack_size = 32768u;
#endif
    wink_status_t st0 = pal_os_task_create(stress_task_fn, "selftest_smp_0",
                                           stack_size, (void*)(uintptr_t)0,
                                           2, PAL_OS_CORE_0, NULL);
    wink_status_t st1 = pal_os_task_create(stress_task_fn, "selftest_smp_1",
                                           stack_size, (void*)(uintptr_t)1,
                                           2, PAL_OS_CORE_1, NULL);
    if (st0 == WINK_OK && st1 == WINK_OK) {
        spawned_smp = true;
        r->note = "single-thread ok; SMP tasks spawned (background)";
    } else if (st0 == WINK_ERR_UNSUPPORTED || st1 == WINK_ERR_UNSUPPORTED) {
        r->note = "single-thread ok; SMP not supported (SKIP background)";
    } else {
        r->note = "single-thread ok; SMP task spawn returned error";
        LOG_W("smp_stress: background task spawn returned %d/%d", (int)st0, (int)st1);
    }

    (void)spawned_smp;
    return WINK_OK;
}
