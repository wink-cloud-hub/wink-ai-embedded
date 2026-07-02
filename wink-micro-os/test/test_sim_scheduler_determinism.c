/**
 * @file test_sim_scheduler_determinism.c
 * @brief fixup 计划 F5 Step 1（R4 重设测试目标）—— 锁定本 wave RR 语义边界。
 *
 * 背景（R4）：F4 之后 pick_next 走 round-robin，同一注册顺序 + 同一让出模式，
 * 两次运行 pick 序列本来就 bit-exact。seed 参数当前只影响未使用的 xorshift 状态。
 * 因此本 wave 内该测试的价值是"用测试固化本 wave RR 语义边界" ——
 *   ⚠️ Task 7（Chaos Scheduling）引入 PRNG 交错扫描时此测试的 Case 2 会变红，
 *   届时把它改成 NOT_EQUAL 作为"PRNG 交错真的生效了"的反测。
 */

#include "unity.h"
#include "wink_sim_scheduler.h"
#include "sim_ctx.h"
#include <stdint.h>
#include <stdlib.h>

/* ---- mock sim_ctx（与 test_sim_scheduler.c 同构，本文件独立可运行） ---- */

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

/* ---- helpers ---- */

static void dummy(void* arg) { (void)arg; }

/* 让出模式 A：每次 pick 之后模拟 sleep（对当前 slot 打 WAITING，下一 tick 立即唤醒），
 * 制造 pick_next 每轮都有多个 READY 的场景，让 RR 语义得以被观测。 */
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
        /* 保持所有任务 READY —— 不 yield，直接观察 RR 序列。 */
    }
}

void setUp(void) {}
void tearDown(void) { sim_scheduler_reset(0); }

/* Case 1：同 seed / 同 pattern → pick 序列 bit-exact 一致（跨 reset 周期） */
void test_deterministic_across_reset_cycles(void) {
    uint32_t seq1[64] = {0};
    uint32_t seq2[64] = {0};
    capture_pick_sequence(42, 3, seq1, 64);
    capture_pick_sequence(42, 3, seq2, 64);
    TEST_ASSERT_EQUAL_UINT32_ARRAY(seq1, seq2, 64);
}

/* Case 2：本 wave RR 语义边界 —— seed 不影响 pick 序列（不走 PRNG）。
 *
 * ⚠️ Task 7 未来引入 PRNG 交错扫描后此断言会失败：届时把 EQUAL 改成 NOT_EQUAL
 * 并加注释"PRNG 交错生效反测"。 */
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
