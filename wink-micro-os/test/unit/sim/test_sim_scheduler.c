// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_sim_scheduler.c
 * @brief Simulation scheduler core unit tests.
 */
#include "unity.h"
#include "wink_sim_scheduler.h"
#include "sim_ctx.h"
#include <string.h>
#include <stdlib.h>

struct sim_ctx {
    void (*entry)(void*);
    void* arg;
    size_t stack_bytes;
};

static uint32_t s_mock_ctx_create_count = 0;
static uint32_t s_mock_ctx_destroy_count = 0;
static uint32_t s_mock_ctx_switch_count = 0;

sim_ctx_t* sim_ctx_create(void (*entry)(void*), void* arg, size_t stack_bytes) {
    s_mock_ctx_create_count++;
    sim_ctx_t* ctx = malloc(sizeof(sim_ctx_t));
    ctx->entry = entry;
    ctx->arg = arg;
    ctx->stack_bytes = stack_bytes;
    return ctx;
}

sim_ctx_t* sim_ctx_from_current(void) {
    sim_ctx_t* ctx = malloc(sizeof(sim_ctx_t));
    ctx->entry = NULL;
    ctx->arg = NULL;
    ctx->stack_bytes = 0;
    return ctx;
}

void sim_ctx_switch(sim_ctx_t* from, sim_ctx_t* to) {
    s_mock_ctx_switch_count++;
    (void)from;
    (void)to;
}

void sim_ctx_destroy(sim_ctx_t* ctx) {
    s_mock_ctx_destroy_count++;
    free(ctx);
}

void setUp(void) {
    s_mock_ctx_create_count = 0;
    s_mock_ctx_destroy_count = 0;
    s_mock_ctx_switch_count = 0;
}

void tearDown(void) {
    sim_scheduler_reset(0);
}

static void dummy_task_func(void* arg) {
    (void)arg;
}

void test_register_and_pick_round_robin(void) {
    sim_scheduler_reset(42);

    uint32_t id0, id1, id2;
    TEST_ASSERT_EQUAL(WINK_OK, sim_scheduler_register(dummy_task_func, NULL, "task0", 5, 0, 32*1024, &id0));
    TEST_ASSERT_EQUAL(WINK_OK, sim_scheduler_register(dummy_task_func, NULL, "task1", 5, 0, 32*1024, &id1));
    TEST_ASSERT_EQUAL(WINK_OK, sim_scheduler_register(dummy_task_func, NULL, "task2", 5, 0, 32*1024, &id2));

    TEST_ASSERT_EQUAL_UINT32(3, sim_scheduler_task_count());

    TEST_ASSERT_EQUAL_UINT32(id0, sim_scheduler_pick_next());
    TEST_ASSERT_EQUAL_UINT32(id1, sim_scheduler_pick_next());
    TEST_ASSERT_EQUAL_UINT32(id2, sim_scheduler_pick_next());
    TEST_ASSERT_EQUAL_UINT32(id0, sim_scheduler_pick_next());
    TEST_ASSERT_EQUAL_UINT32(id1, sim_scheduler_pick_next());
}

void test_wakeup_by_time_promotes_waiting(void) {
    sim_scheduler_reset(42);

    uint32_t id;
    TEST_ASSERT_EQUAL(WINK_OK, sim_scheduler_register(dummy_task_func, NULL, "t", 5, 0, 32*1024, &id));

    sim_scheduler_yield_timed(id, 1000, 100);
    TEST_ASSERT_EQUAL(SIM_TASK_STATE_WAITING, sim_scheduler_get(id)->state);

    TEST_ASSERT_EQUAL_UINT32(0, sim_scheduler_wakeup_by_time(1050));
    TEST_ASSERT_EQUAL(SIM_TASK_STATE_WAITING, sim_scheduler_get(id)->state);

    TEST_ASSERT_EQUAL_UINT32(1, sim_scheduler_wakeup_by_time(1150));
    TEST_ASSERT_EQUAL(SIM_TASK_STATE_READY, sim_scheduler_get(id)->state);
}

void test_all_waiting_returns_no_ready(void) {
    sim_scheduler_reset(42);

    uint32_t id0, id1;
    TEST_ASSERT_EQUAL(WINK_OK, sim_scheduler_register(dummy_task_func, NULL, "t0", 5, 0, 32*1024, &id0));
    TEST_ASSERT_EQUAL(WINK_OK, sim_scheduler_register(dummy_task_func, NULL, "t1", 5, 0, 32*1024, &id1));

    sim_scheduler_yield_timed(id0, 0, 100);
    sim_scheduler_yield_timed(id1, 0, 200);

    TEST_ASSERT_EQUAL_UINT32(SIM_SCHED_NO_READY, sim_scheduler_pick_next());
    TEST_ASSERT_EQUAL_UINT64(100, sim_scheduler_next_wakeup_us());
}

void test_terminate_via_zombie(void) {
    sim_scheduler_reset(42);

    uint32_t id0, id1;
    TEST_ASSERT_EQUAL(WINK_OK, sim_scheduler_register(dummy_task_func, NULL, "t0", 5, 0, 32*1024, &id0));
    TEST_ASSERT_EQUAL(WINK_OK, sim_scheduler_register(dummy_task_func, NULL, "t1", 5, 0, 32*1024, &id1));

    sim_scheduler_mark_zombie(id0);
    TEST_ASSERT_EQUAL(SIM_TASK_STATE_ZOMBIE, sim_scheduler_get(id0)->state);

    TEST_ASSERT_EQUAL_UINT32(2, sim_scheduler_task_count());

    uint32_t old_destroy_count = s_mock_ctx_destroy_count;
    sim_scheduler_gc_zombies();

    TEST_ASSERT_EQUAL(SIM_TASK_STATE_TERMINATED, sim_scheduler_get(id0)->state);
    TEST_ASSERT_EQUAL_UINT32(old_destroy_count + 1, s_mock_ctx_destroy_count);

    for (int i = 0; i < 5; ++i) {
        TEST_ASSERT_EQUAL_UINT32(id1, sim_scheduler_pick_next());
    }
}

void test_determinism_same_seed(void) {
    uint32_t seq1[100];
    uint32_t seq2[100];

    sim_scheduler_reset(12345);
    uint32_t id0, id1, id2;
    sim_scheduler_register(dummy_task_func, NULL, "t0", 5, 0, 32*1024, &id0);
    sim_scheduler_register(dummy_task_func, NULL, "t1", 5, 0, 32*1024, &id1);
    sim_scheduler_register(dummy_task_func, NULL, "t2", 5, 0, 32*1024, &id2);

    for (int i = 0; i < 100; ++i) {
        seq1[i] = sim_scheduler_pick_next();
    }

    sim_scheduler_reset(12345);
    sim_scheduler_register(dummy_task_func, NULL, "t0", 5, 0, 32*1024, &id0);
    sim_scheduler_register(dummy_task_func, NULL, "t1", 5, 0, 32*1024, &id1);
    sim_scheduler_register(dummy_task_func, NULL, "t2", 5, 0, 32*1024, &id2);

    for (int i = 0; i < 100; ++i) {
        seq2[i] = sim_scheduler_pick_next();
    }

    for (int i = 0; i < 100; ++i) {
        TEST_ASSERT_EQUAL_UINT32(seq1[i], seq2[i]);
    }
}

void test_max_tasks_full(void) {
    sim_scheduler_reset(42);

    uint32_t id;
    for (int i = 0; i < WINK_SIM_MAX_TASKS; ++i) {
        TEST_ASSERT_EQUAL(WINK_OK, sim_scheduler_register(dummy_task_func, NULL, "t", 5, 0, 32*1024, &id));
    }

    TEST_ASSERT_EQUAL(WINK_ERR_NO_MEM, sim_scheduler_register(dummy_task_func, NULL, "fail", 5, 0, 32*1024, &id));
}

void test_single_task_direct_pass(void) {
    sim_scheduler_reset(42);

    uint32_t id;
    TEST_ASSERT_EQUAL(WINK_OK, sim_scheduler_register(dummy_task_func, NULL, "single", 5, 0, 32*1024, &id));

    for (int i = 0; i < 5; ++i) {
        TEST_ASSERT_EQUAL_UINT32(id, sim_scheduler_pick_next());
    }
}

void test_stack_clamp_warns(void) {
    sim_scheduler_reset(42);

    uint32_t id;
    uint32_t initial_create_count = s_mock_ctx_create_count;
    TEST_ASSERT_EQUAL(WINK_OK, sim_scheduler_register(dummy_task_func, NULL, "tiny", 5, 0, 1024, &id));
    TEST_ASSERT_EQUAL_UINT32(initial_create_count + 1, s_mock_ctx_create_count);

    const sim_task_t* t = sim_scheduler_get(id);
    TEST_ASSERT_NOT_NULL(t);
    TEST_ASSERT_NOT_NULL(t->ctx);
    TEST_ASSERT_EQUAL_UINT32(WINK_SIM_STACK_MIN, t->ctx->stack_bytes);
}

void test_block_with_timeout_wakes_by_time(void) {
    sim_scheduler_reset(42);

    uint32_t id;
    sim_scheduler_register(dummy_task_func, NULL, "b", 5, 0, 32*1024, &id);

    sim_scheduler_block(id, 1, 1000, 100);
    TEST_ASSERT_EQUAL(SIM_TASK_STATE_BLOCKED, sim_scheduler_get(id)->state);
    TEST_ASSERT_EQUAL_UINT32(1, sim_scheduler_get(id)->blocked_on);
    TEST_ASSERT_FALSE(sim_scheduler_get(id)->timeout_fired);

    TEST_ASSERT_EQUAL_UINT32(0, sim_scheduler_wakeup_by_time(1050));
    TEST_ASSERT_EQUAL(SIM_TASK_STATE_BLOCKED, sim_scheduler_get(id)->state);

    TEST_ASSERT_EQUAL_UINT32(1, sim_scheduler_wakeup_by_time(1150));
    TEST_ASSERT_EQUAL(SIM_TASK_STATE_READY, sim_scheduler_get(id)->state);
    TEST_ASSERT_TRUE(sim_scheduler_get(id)->timeout_fired);
    TEST_ASSERT_EQUAL_UINT32(0, sim_scheduler_get(id)->blocked_on);
}

void test_block_infinite_only_resume(void) {
    sim_scheduler_reset(42);

    uint32_t id;
    sim_scheduler_register(dummy_task_func, NULL, "inf", 5, 0, 32*1024, &id);

    sim_scheduler_block(id, 1, 1000, 0);
    TEST_ASSERT_EQUAL(SIM_TASK_STATE_BLOCKED, sim_scheduler_get(id)->state);
    TEST_ASSERT_EQUAL_UINT32(1, sim_scheduler_get(id)->blocked_on);
    TEST_ASSERT_EQUAL_UINT64(0, sim_scheduler_get(id)->wakeup_us);

    TEST_ASSERT_EQUAL_UINT32(0, sim_scheduler_wakeup_by_time(1000000));
    TEST_ASSERT_EQUAL(SIM_TASK_STATE_BLOCKED, sim_scheduler_get(id)->state);

    sim_scheduler_resume(id);
    TEST_ASSERT_EQUAL(SIM_TASK_STATE_READY, sim_scheduler_get(id)->state);
    TEST_ASSERT_FALSE(sim_scheduler_get(id)->timeout_fired);
    TEST_ASSERT_EQUAL_UINT32(0, sim_scheduler_get(id)->blocked_on);
}

void test_gc_zombies_releases_ctx(void) {
    sim_scheduler_reset(42);

    uint32_t id;
    sim_scheduler_register(dummy_task_func, NULL, "t", 5, 0, 32*1024, &id);

    uint32_t old_destroy = s_mock_ctx_destroy_count;
    sim_scheduler_mark_zombie(id);
    sim_scheduler_gc_zombies();

    TEST_ASSERT_EQUAL_UINT32(old_destroy + 1, s_mock_ctx_destroy_count);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_register_and_pick_round_robin);
    RUN_TEST(test_wakeup_by_time_promotes_waiting);
    RUN_TEST(test_all_waiting_returns_no_ready);
    RUN_TEST(test_terminate_via_zombie);
    RUN_TEST(test_determinism_same_seed);
    RUN_TEST(test_max_tasks_full);
    RUN_TEST(test_single_task_direct_pass);
    RUN_TEST(test_stack_clamp_warns);
    RUN_TEST(test_block_with_timeout_wakes_by_time);
    RUN_TEST(test_block_infinite_only_resume);
    RUN_TEST(test_gc_zombies_releases_ctx);
    return UNITY_END();
}
