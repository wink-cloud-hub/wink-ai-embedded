#include "unity.h"
#include "pal_osal.h"
#include "wink_sim_scheduler.h"
#include <stdint.h>

static pal_os_ringbuf_handle_t rb;
static uint32_t produced = 0;
static uint32_t consumed = 0;

void producer_task(void* arg) {
    (void)arg;
    while (produced < 10) {
        uint32_t v = produced;
        if (pal_os_ringbuf_push(rb, &v, sizeof(v)) == WINK_OK) {
            produced++;
        }
        pal_os_sleep_ms(10);
    }
}

void consumer_task(void* arg) {
    (void)arg;
    while (consumed < 10) {
        uint32_t v = 0xFFFFFFFF;
        if (pal_os_ringbuf_pop(rb, &v, sizeof(v)) == WINK_OK) {
            consumed++;
        }
        pal_os_sleep_ms(15);
    }
}

void setUp(void) {
    produced = 0;
    consumed = 0;
    rb = pal_os_ringbuf_create(64);
}

void tearDown(void) {
    pal_os_ringbuf_destroy(rb);
    sim_scheduler_reset(0);
}

void test_dual_task_ringbuf_e2e(void) {
    sim_scheduler_reset(42);
    
    pal_os_task_handle_t prod_h, cons_h;
    TEST_ASSERT_EQUAL(WINK_OK, pal_os_task_create(producer_task, "prod", 32*1024, NULL, 5, PAL_OS_CORE_ANY, &prod_h));
    TEST_ASSERT_EQUAL(WINK_OK, pal_os_task_create(consumer_task, "cons", 32*1024, NULL, 5, PAL_OS_CORE_ANY, &cons_h));
    
    /* 运行仿真调度主循环，限制 max_ticks=500 */
    wink_status_t st = pal_sim_scheduler_run(SIM_SCHED_NO_READY, 500);
    TEST_ASSERT_EQUAL(WINK_OK, st);
    
    TEST_ASSERT_EQUAL_UINT32(10, produced);
    TEST_ASSERT_EQUAL_UINT32(10, consumed);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_dual_task_ringbuf_e2e);
    return UNITY_END();
}
