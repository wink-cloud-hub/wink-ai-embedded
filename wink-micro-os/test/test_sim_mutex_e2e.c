/**
 * @file test_sim_mutex_e2e.c
 * @brief P0 E5-part2：wasm/host 协作调度下 mutex BLOCKED 路径 e2e 单测。
 *
 * 覆盖：
 *   1. 基本 lock/unlock 无竞争
 *   2. 多 task 争用 → FIFO 顺序获得锁
 *   3. timeout=0（WAIT_FOREVER 等价非0长时阻塞）最终能拿到锁
 *   4. 短超时：锁被持有时 lock 返 TIMEOUT
 *   5. NULL handle 返 INVALID_ARG
 *   6. 非 owner unlock 返 PERM（契约诚实）
 *   7. 递归加锁（同 task relock）返 BUSY（非递归锁）
 *
 * wasm/host 共享 wink_sim_scheduler，host 执行即覆盖 wasm 侧同构实现。
 */
#include "unity.h"
#include "pal_osal.h"
#include "wink_sim_scheduler.h"
#include <stdint.h>
#include <string.h>


/* ADR-0017 层 1 例外：本 TU 合法调用 WINK_BLOCKING API。抑制
 * -Wdeprecated-declarations 使 -Werror 下仍能编译；严格模式
 * (-DWINK_STRICT_NONBLOCKING=1) 下相关 API 声明直接消失，本 TU 会链接失败——那是设计意图。 */
#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

/* ── 共享状态 ─────────────────────────────────────── */
static pal_os_mutex_t g_mtx;
static volatile uint32_t g_owner_order[4];
static volatile uint32_t g_owner_count;
static volatile uint32_t g_critical_section_guarded;

void setUp(void) {
    sim_scheduler_reset(0xBADF00D);
    g_mtx = pal_os_mutex_create();
    TEST_ASSERT_NOT_NULL(g_mtx);
    g_owner_count = 0;
    memset((void*)g_owner_order, 0, sizeof(g_owner_order));
    g_critical_section_guarded = 0;
}

void tearDown(void) {
    pal_os_mutex_destroy(g_mtx);
    g_mtx = NULL;
    sim_scheduler_reset(0);
}

/* ── Case 1: 无竞争 lock/unlock ──────────────────── */
void task_no_contention(void* arg) {
    (void)arg;
    TEST_ASSERT_EQUAL(WINK_OK, pal_os_mutex_lock(g_mtx, WINK_MUTEX_WAIT_FOREVER));
    g_critical_section_guarded = 1;
    pal_os_sleep_ms(1);
    g_critical_section_guarded = 0;
    TEST_ASSERT_EQUAL(WINK_OK, pal_os_mutex_unlock(g_mtx));
    pal_os_task_delete(NULL);
}

void test_mutex_no_contention(void) {
    pal_os_task_handle_t h;
    TEST_ASSERT_EQUAL(WINK_OK,
        pal_os_task_create(task_no_contention, "nocont", 16*1024, NULL, 5, PAL_OS_CORE_ANY, &h));
    TEST_ASSERT_EQUAL(WINK_OK, pal_sim_scheduler_run(NULL, 0, 200));
    TEST_ASSERT_EQUAL_UINT32(0, g_critical_section_guarded);
}

/* ── Case 2: 三 task FIFO 争用 ─────────────────────
 * main task 先持锁 → 创建 A/B/C 三个争用者 → 释放锁 →
 * 观察三个 task 按 FIFO 顺序依次进入临界区，顺序 0→1→2 */
static void contender_task(void* arg) {
    uint32_t id = (uint32_t)(uintptr_t)arg;
    TEST_ASSERT_EQUAL(WINK_OK, pal_os_mutex_lock(g_mtx, WINK_MUTEX_WAIT_FOREVER));
    /* 记录获得锁的顺序 */
    uint32_t idx = g_owner_count++;
    TEST_ASSERT(idx < 4);
    g_owner_order[idx] = id;
    /* 临界区独占：在临界区内 g_critical_section_guarded 必须为 0（只有我在里面）*/
    TEST_ASSERT_EQUAL_UINT32(0, g_critical_section_guarded);
    g_critical_section_guarded = id + 1;
    pal_os_sleep_ms(1);  /* 切出，让其他 task 有机会争抢（但应被 mutex 挡住）*/
    g_critical_section_guarded = 0;
    TEST_ASSERT_EQUAL(WINK_OK, pal_os_mutex_unlock(g_mtx));
    pal_os_task_delete(NULL);
}

void task_fifo_holder(void* arg) {
    (void)arg;
    /* 启动先持锁 */
    TEST_ASSERT_EQUAL(WINK_OK, pal_os_mutex_lock(g_mtx, WINK_MUTEX_WAIT_FOREVER));
    /* 此时创建 3 个争用者（从该 task 内创建即可，scheduler 支持运行中 register）*/
    for (uint32_t i = 0; i < 3; ++i) {
        char name[8] = { 'c', (char)('0'+i), 0 };
        pal_os_task_handle_t h;
        TEST_ASSERT_EQUAL(WINK_OK,
            pal_os_task_create(contender_task, name, 16*1024, (void*)(uintptr_t)i, 4, PAL_OS_CORE_ANY, &h));
    }
    /* sleep 1ms 给新创建的 task 机会运行并 block */
    pal_os_sleep_ms(1);
    /* 此时 contender 应都被 block，g_owner_count 仍 0 */
    TEST_ASSERT_EQUAL_UINT32(0, g_owner_count);
    /* 释放锁 → A/B/C 按 FIFO 依次获得 */
    TEST_ASSERT_EQUAL(WINK_OK, pal_os_mutex_unlock(g_mtx));
    /* 让出，让 contender 们依次跑 */
    pal_os_sleep_ms(10);
    pal_os_task_delete(NULL);
}

void test_mutex_fifo_contention(void) {
    pal_os_task_handle_t h;
    TEST_ASSERT_EQUAL(WINK_OK,
        pal_os_task_create(task_fifo_holder, "holder", 16*1024, NULL, 5, PAL_OS_CORE_ANY, &h));
    TEST_ASSERT_EQUAL(WINK_OK, pal_sim_scheduler_run(NULL, 0, 500));
    TEST_ASSERT_EQUAL_UINT32(3, g_owner_count);
    TEST_ASSERT_EQUAL_UINT32(0, g_owner_order[0]);
    TEST_ASSERT_EQUAL_UINT32(1, g_owner_order[1]);
    TEST_ASSERT_EQUAL_UINT32(2, g_owner_order[2]);
}

/* ── Case 3: 超时返 TIMEOUT ────────────────────── */
void task_lock_holder_short(void* arg) {
    (void)arg;
    TEST_ASSERT_EQUAL(WINK_OK, pal_os_mutex_lock(g_mtx, WINK_MUTEX_WAIT_FOREVER));
    /* 持锁 10ms 不释放 */
    pal_os_sleep_ms(10);
    TEST_ASSERT_EQUAL(WINK_OK, pal_os_mutex_unlock(g_mtx));
    pal_os_task_delete(NULL);
}

void task_timeout_try(void* arg) {
    volatile wink_status_t* rs = (volatile wink_status_t*)arg;
    /* sleep 1ms 等 holder 先拿锁 */
    pal_os_sleep_ms(1);
    /* 用 1ms 超时尝试——应 TIMEOUT */
    *rs = pal_os_mutex_lock(g_mtx, 1);
    pal_os_task_delete(NULL);
}

void test_mutex_timeout_returns_timeout(void) {
    volatile wink_status_t try_result = WINK_OK;
    pal_os_task_handle_t h1, h2;
    TEST_ASSERT_EQUAL(WINK_OK,
        pal_os_task_create(task_lock_holder_short, "holder", 16*1024, NULL, 5, PAL_OS_CORE_ANY, &h1));
    TEST_ASSERT_EQUAL(WINK_OK,
        pal_os_task_create(task_timeout_try, "tryr", 16*1024, (void*)&try_result, 5, PAL_OS_CORE_ANY, &h2));
    TEST_ASSERT_EQUAL(WINK_OK, pal_sim_scheduler_run(NULL, 0, 500));
    TEST_ASSERT_EQUAL(WINK_ERR_TIMEOUT, try_result);
}

/* ── Case 4: 参数错误 & 非 owner unlock ─────────── */
void task_wrong_unlocker(void* arg) {
    (void)arg;
    /* 没持锁就 unlock → INVALID_STATE */
    TEST_ASSERT_EQUAL(WINK_ERR_INVALID_STATE, pal_os_mutex_unlock(g_mtx));
    pal_os_task_delete(NULL);
}

void test_mutex_non_owner_unlock_rejected(void) {
    pal_os_task_handle_t h;
    TEST_ASSERT_EQUAL(WINK_OK,
        pal_os_task_create(task_wrong_unlocker, "wrongu", 16*1024, NULL, 5, PAL_OS_CORE_ANY, &h));
    TEST_ASSERT_EQUAL(WINK_OK, pal_sim_scheduler_run(NULL, 0, 100));
}

void test_mutex_null_handle_invalid_arg(void) {
    TEST_ASSERT_EQUAL(WINK_ERR_INVALID_ARG, pal_os_mutex_lock(NULL, WINK_MUTEX_WAIT_FOREVER));
    TEST_ASSERT_EQUAL(WINK_ERR_INVALID_ARG, pal_os_mutex_unlock(NULL));
    pal_os_mutex_destroy(NULL);  /* 无崩 */
}

/* ── Case 5: 递归重入（非递归锁）返 BUSY ───────── */
void task_recursive_try(void* arg) {
    (void)arg;
    TEST_ASSERT_EQUAL(WINK_OK, pal_os_mutex_lock(g_mtx, WINK_MUTEX_WAIT_FOREVER));
    /* 同一 task 再加锁必须返 BUSY（本 mutex 非递归） */
    TEST_ASSERT_EQUAL(WINK_ERR_BUSY, pal_os_mutex_lock(g_mtx, WINK_MUTEX_WAIT_FOREVER));
    TEST_ASSERT_EQUAL(WINK_OK, pal_os_mutex_unlock(g_mtx));
    pal_os_task_delete(NULL);
}

void test_mutex_recursive_relock_rejected(void) {
    pal_os_task_handle_t h;
    TEST_ASSERT_EQUAL(WINK_OK,
        pal_os_task_create(task_recursive_try, "recur", 16*1024, NULL, 5, PAL_OS_CORE_ANY, &h));
    TEST_ASSERT_EQUAL(WINK_OK, pal_sim_scheduler_run(NULL, 0, 200));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_mutex_no_contention);
    RUN_TEST(test_mutex_fifo_contention);
    RUN_TEST(test_mutex_timeout_returns_timeout);
    RUN_TEST(test_mutex_non_owner_unlock_rejected);
    RUN_TEST(test_mutex_null_handle_invalid_arg);
    RUN_TEST(test_mutex_recursive_relock_rejected);
    return UNITY_END();
}
