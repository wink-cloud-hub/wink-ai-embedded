// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_sim_scheduler_determinism.c
 * @brief Unit tests for simulation scheduler determinism.
 */
#include "unity.h"
#include "wink_sim_scheduler.h"
#include "sim_ctx.h"
#include <stdint.h>
#include <stdlib.h>

struct sim_ctx {
    void (*entry)(void*);
    void* arg;
    size_t stack_bytes;
};

sim_ctx_t* sim_ctx_create(void (*entry)(void*), void* arg, size_t stack_bytes) {
    sim_ctx_t* ctx = malloc(sizeof(*ctx));
    ctx->entry = entry; ctx->arg = arg; ctx->stack_bytes = stack_bytes;
    return ctx;
}
sim_ctx_t* sim_ctx_from_current(void) {
    sim_ctx_t* ctx = malloc(sizeof(*ctx));
    ctx->entry = NULL; ctx->arg = NULL; ctx->stack_bytes = 0;
    return ctx;
}
void sim_ctx_switch(sim_ctx_t* from, sim_ctx_t* to) { (void)from; (void)to; }
void sim_ctx_destroy(sim_ctx_t* ctx) { free(ctx); }

static void dummy(void* arg) { (void)arg; }

static void capture_pick_sequence(uint32_t seed, uint32_t task_count,
                                  uint32_t* out_seq, uint32_t len) {
    sim_scheduler_reset(seed);
    for (uint32_t i = 0; i < task_count; ++i) {
        uint32_t id;
        char name[16]; snprintf(name, sizeof(name), "t%u", i);
        sim_scheduler_register(dummy, NULL, name, 5, 0, 32u * 1024u, &id);
    }
    for (uint32_t i = 0; i < len; ++i) {
        uint32_t p = sim_scheduler_pick_next();
        out_seq[i] = p;
    }
}

void setUp(void) {}
void tearDown(void) { sim_scheduler_reset(0); }

void test_deterministic_across_reset_cycles(void) {
    uint32_t seq1[64] = {0};
    uint32_t seq2[64] = {0};
    capture_pick_sequence(42, 3, seq1, 64);
    capture_pick_sequence(42, 3, seq2, 64);
    TEST_ASSERT_EQUAL_UINT32_ARRAY(seq1, seq2, 64);
}

void test_wave_3_seed_does_not_affect_rr_sequence(void) {
    uint32_t seq_42[64] = {0};
    uint32_t seq_99[64] = {0};
    capture_pick_sequence(42, 3, seq_42, 64);
    capture_pick_sequence(99, 3, seq_99, 64);
    TEST_ASSERT_EQUAL_UINT32_ARRAY(seq_42, seq_99, 64);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_deterministic_across_reset_cycles);
    RUN_TEST(test_wave_3_seed_does_not_affect_rr_sequence);
    return UNITY_END();
}
