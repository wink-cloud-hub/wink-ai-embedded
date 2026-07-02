#include "unity.h"
#include "pal_osal.h"
#include "wink_sim_scheduler.h"
#include <stdint.h>

static uint32_t s_self_delete_count = 0;

void self_deleter_task(void* arg) {
    (void)arg;
    pal_os_sleep_ms(1);
    s_self_delete_count++;
    pal_os_task_delete(NULL);
    /* 崩溃保护，如果删除失败回到这里说明有bug */
    TEST_FAIL_MESSAGE("pal_os_task_delete(NULL) should not return!");
}

void setUp(void) {
    s_self_delete_count = 0;
}

void tearDown(void) {
    sim_scheduler_reset(0);
}

void test_self_delete_reaches_zombie_gc(void) {
    sim_scheduler_reset(42);
    
    pal_os_task_handle_t t_h;
    TEST_ASSERT_EQUAL(WINK_OK, pal_os_task_create(self_deleter_task, "sd", 32*1024, NULL, 5, PAL_OS_CORE_ANY, &t_h));
    
    /* 运行调度循环，主任务 id 设为 0（即注册的唯一任务）；测试无 App，传 NULL。 */
    wink_status_t st = pal_sim_scheduler_run(NULL, 0, 50);
    TEST_ASSERT_EQUAL(WINK_OK, st);
    
    TEST_ASSERT_EQUAL_UINT32(1, s_self_delete_count);
    
    /* 验证任务已被回收并置为 TERMINATED 状态 */
    const sim_task_t* t = sim_scheduler_get(0);
    TEST_ASSERT_NOT_NULL(t);
    TEST_ASSERT_EQUAL(SIM_TASK_STATE_TERMINATED, t->state);
    TEST_ASSERT_NULL(t->ctx);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_self_delete_reaches_zombie_gc);
    return UNITY_END();
}
