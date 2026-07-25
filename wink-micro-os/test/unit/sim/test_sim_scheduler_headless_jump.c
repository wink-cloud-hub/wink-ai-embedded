#include "unity.h"
#include "pal_osal.h"
#include "wink_sim_scheduler.h"
#include <stdint.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
static uint64_t get_wall_us(void) {
    return (uint64_t)(emscripten_get_now() * 1000.0);
}
#elif defined(_WIN32)
#include <windows.h>
static uint64_t get_wall_us(void) {
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (uint64_t)(counter.QuadPart * 1000000 / freq.QuadPart);
}
#else
#include <time.h>
static uint64_t get_wall_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}
#endif

#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

static uint32_t task_runs = 0;

void sleep_task(void* arg) {
    (void)arg;
    pal_os_sleep_ms(3000); // 3 seconds virtual sleep
    task_runs++;
}

void setUp(void) {
    task_runs = 0;
}

void tearDown(void) {
    sim_scheduler_reset(0);
}

void test_sim_scheduler_headless_jump(void) {
    sim_scheduler_reset(42);
    
    // Explicitly set simulation mode to HEADLESS
    wink_sim_set_mode(WINK_SIM_MODE_HEADLESS);
    TEST_ASSERT_EQUAL(WINK_SIM_MODE_HEADLESS, wink_sim_get_mode());

    pal_os_task_handle_t t1, t2, t3;
    TEST_ASSERT_EQUAL(WINK_OK, pal_os_task_create(sleep_task, "sleep1", 32*1024, NULL, 5, PAL_OS_CORE_ANY, &t1));
    TEST_ASSERT_EQUAL(WINK_OK, pal_os_task_create(sleep_task, "sleep2", 32*1024, NULL, 5, PAL_OS_CORE_ANY, &t2));
    TEST_ASSERT_EQUAL(WINK_OK, pal_os_task_create(sleep_task, "sleep3", 32*1024, NULL, 5, PAL_OS_CORE_ANY, &t3));

    uint64_t start_virtual_us = pal_os_get_us();
    uint64_t start_wall_us = get_wall_us();

    // Run the scheduler. With HEADLESS mode, this should finish immediately by jumping the clock
    wink_status_t st = pal_sim_scheduler_run(NULL, SIM_SCHED_NO_READY, 500);
    TEST_ASSERT_EQUAL(WINK_OK, st);

    uint64_t end_virtual_us = pal_os_get_us();
    uint64_t end_wall_us = get_wall_us();

    uint64_t elapsed_virtual_ms = (end_virtual_us - start_virtual_us) / 1000;
    uint64_t elapsed_wall_ms = (end_wall_us - start_wall_us) / 1000;

    // Verify virtual time has advanced by at least 3 seconds
    TEST_ASSERT_GREATER_OR_EQUAL_UINT64(3000, elapsed_virtual_ms);
    TEST_ASSERT_EQUAL_UINT32(3, task_runs);

    // Verify wall clock elapsed time is extremely small (less than 200ms, usually < 10ms)
    // We allow a bit of buffer (200ms) for slow CI runners, but it should be far below 3 seconds.
    TEST_ASSERT_LESS_THAN_UINT64(200, elapsed_wall_ms);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_sim_scheduler_headless_jump);
    return UNITY_END();
}
