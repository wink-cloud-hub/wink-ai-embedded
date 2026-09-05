/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Spike-S1 PoC: cooperative-fiber yield for MCS-51 tight-poll loops.
 * Throwaway validation code (NOT production). Proves on both host (Win32
 * fibers) and wasm (emscripten fibers + asyncify) that:
 *
 *   A. A user fiber that never calls a blocking/sleep API still returns
 *      control to the master scheduler via an interception-point quota
 *      yield (modeling WinkSfrBitProxy::operator uint8_t() in `while(!TF0)`).
 *   B. Virtual time advances by MICROSTEP CHARGING inside the fiber (each
 *      intercepted SFR/nop burns functional-level us); the quota is measured
 *      in charged virtual us, NOT in a master clock (chicken-and-egg: the
 *      master clock only advances once the fiber yields).
 *   C. The master loop performs catch-up bookkeeping + timer stepping at each
 *      quota yield and enforces the 100 Hz tick boundary, so virtual time is
 *      conserved 1:1 with master ticks and the poll terminates.
 *   D. A plain timed yield (modeling delay) still works.
 *
 * The existing scheduler has NO preemption: a fiber that never switches back
 * freezes the master loop (post-hoc WCET fault 8002 cannot recover it). This
 * spike validates the interception-point quota mechanism that closes that gap.
 */
#include "wink_sim_scheduler.h"
#include "sim_ctx.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define MASTER_TICK_US   10000u   /* host app_loop: 100 Hz / 10 ms */
#define QUOTA_US          500u    /* mcs51 force-yield quota per slice (vs WCET 5000) */
#define MICROSTEP_US        5u    /* functional us charged per intercepted poll iter */
#define TIMER_PERIOD_US  50000u  /* mock Timer0 overflow: 50 ms (5 master ticks) */

static sim_ctx_t* s_main_ctx;
static uint64_t   s_virt_us;      /* slave virtual clock, advanced by microsteps */
static uint32_t   s_master_ticks;
static uint8_t    s_mock_TF0;     /* mock TCON.5 timer overflow flag */
static uint32_t   s_quota_yields;
static uint64_t   s_slice_start;  /* virt-us at which current fiber slice began */

/* ---- mock timer: functional-level Timer0 overflow (no 12-T modeling, AD-2) ---- */
static void step_timers(uint64_t now_us) {
    static uint64_t s_last_overflow = 0;
    if (now_us - s_last_overflow >= TIMER_PERIOD_US) {
        s_last_overflow = now_us;
        s_mock_TF0 = 1;   /* overflow -> set TF0; real impl also dispatches ISR */
    }
}

/* ---- interception point: models SFR bit read hook inside `while(!TF0)` ----
 * Each pass CHARGES functional virtual time (microstep) and, once the slice
 * has consumed QUOTA_US of virtual time, force-yields to master. Master does
 * catch-up + timer stepping, then re-enters the fiber.
 */
static uint8_t read_TF0_hooked(uint32_t self) {
    s_virt_us += MICROSTEP_US;                 /* charge virtual time */
    if ((uint64_t)(s_virt_us - s_slice_start) >= QUOTA_US) {
        s_quota_yields++;
        sim_scheduler_yield_timed(self, s_virt_us, 0);  /* duration 0: park for catch-up */
        sim_ctx_switch(sim_scheduler_current_ctx(), s_main_ctx);
        s_slice_start = s_virt_us;             /* resumed: begin new slice */
    }
    return s_mock_TF0;
}

/* ---- timed yield: models user delay_ms() (virtual-time sleep) ---- */
static void user_delay_us(uint32_t self, uint32_t us) {
    sim_scheduler_yield_timed(self, s_virt_us, us);
    sim_ctx_switch(sim_scheduler_current_ctx(), s_main_ctx);
}

/* ======================= scenario A: timed delay ======================= */
static void fiber_delay(void* arg) {
    uint32_t self = sim_scheduler_current_id();
    (void)arg;
    for (int i = 0; i < 3; i++) {
        user_delay_us(self, 1000); /* 1 ms virtual each */
    }
}

/* ============ scenario B: tight poll with NO explicit yield ============= */
static uint32_t s_poll_iters;
static void fiber_tight_poll(void* arg) {
    uint32_t self = sim_scheduler_current_id();
    (void)arg;
    s_slice_start = s_virt_us;
    /* Classic 8051: while(!TF0); -- no sleep, no yield point in user code.
     * Only the SFR read hook charges time and can return control. */
    while (!read_TF0_hooked(self)) {
        s_poll_iters++;
    }
    printf("[fiber] poll exited: TF0 at virt=%llu us after %u iters, %u quota yields\n",
           (unsigned long long)s_virt_us, s_poll_iters, s_quota_yields);
}

/* ---- master run loop: tick-bounded, catch-up at each yield ---- */
static void run_master(uint32_t task_id, uint32_t max_ticks) {
    s_main_ctx = sim_ctx_from_current();
    uint64_t tick_end = s_virt_us + MASTER_TICK_US;
    uint32_t ticks = 0;

    while (ticks < max_ticks) {
        const sim_task_t* mt = sim_scheduler_get(task_id);
        if (mt && (mt->state == SIM_TASK_STATE_TERMINATED ||
                   mt->state == SIM_TASK_STATE_ZOMBIE)) break;

        sim_scheduler_wakeup_by_time(s_virt_us);
        uint32_t next = sim_scheduler_pick_next();
        if (next == SIM_SCHED_NO_READY) {
            uint64_t wake = sim_scheduler_next_wakeup_us();
            s_virt_us = (wake == UINT64_MAX || wake > tick_end) ? tick_end : wake;
        } else {
            sim_scheduler_set_current(next);
            const sim_task_t* t = sim_scheduler_get(next);
            s_slice_start = s_virt_us;
            sim_ctx_switch(s_main_ctx, t->ctx);    /* enter fiber */
            sim_scheduler_set_current(SIM_SCHED_NO_READY);
        }

        /* fiber yielded back (or idle): catch-up bookkeeping + step timers */
        sim_scheduler_gc_zombies();
        step_timers(s_virt_us);

        const sim_task_t* after = sim_scheduler_get(task_id);
        if (after->state == SIM_TASK_STATE_ZOMBIE ||
            after->state == SIM_TASK_STATE_TERMINATED) break;

        if (s_virt_us >= tick_end) {               /* master tick boundary */
            ticks++;
            s_master_ticks = ticks;
            tick_end = s_virt_us + MASTER_TICK_US;
        }
    }
}

/* pal_os_task_delete: sim_ctx trampoline calls it when a fiber entry returns. */
void pal_os_task_delete(void* handle) {
    (void)handle;
    uint32_t cur = sim_scheduler_current_id();
    sim_scheduler_mark_zombie(cur);
    sim_ctx_switch(sim_scheduler_current_ctx(), s_main_ctx);
}

int main(void) {
    int fail = 0;

    /* ---- Scenario A: timed delay keeps clock 1:1 ---- */
    sim_scheduler_reset(42);
    s_virt_us = 0; s_master_ticks = 0; s_mock_TF0 = 0; s_quota_yields = 0;
    s_poll_iters = 0;
    uint32_t idA;
    if (sim_scheduler_register(fiber_delay, NULL, "delay", 0, 0, 32 * 1024, &idA) != WINK_OK) {
        printf("[A] register failed\n"); return 1;
    }
    run_master(idA, 5);
    printf("[A] after 3x1ms delay: virt=%llu us, master_ticks=%u\n",
           (unsigned long long)s_virt_us, s_master_ticks);
    if (s_virt_us < 3000) { printf("[A] FAIL: virtual clock did not reach 3ms\n"); fail++; }

    /* ---- Scenario B: tight poll does NOT freeze; timer via catch-up ---- */
    sim_scheduler_reset(42);
    s_virt_us = 0; s_master_ticks = 0; s_mock_TF0 = 0; s_quota_yields = 0;
    s_poll_iters = 0;
    uint32_t idB;
    if (sim_scheduler_register(fiber_tight_poll, NULL, "poll", 0, 0, 32 * 1024, &idB) != WINK_OK) {
        printf("[B] register failed\n"); return 1;
    }
    run_master(idB, 20); /* 20 master ticks = 200 virtual ms budget */
    printf("[B] tight-poll: ticks=%u, quota_yields=%u, virt=%llu us, TF0=%u\n",
           s_master_ticks, s_quota_yields, (unsigned long long)s_virt_us, s_mock_TF0);

    if (s_master_ticks < 5) { printf("[B] FAIL: master loop frozen (ticks=%u)\n", s_master_ticks); fail++; }
    if (s_quota_yields == 0) { printf("[B] FAIL: quota yield never fired\n"); fail++; }
    if (s_mock_TF0 != 1)    { printf("[B] FAIL: timer never set TF0 (catch-up broke)\n"); fail++; }
    if (s_virt_us < TIMER_PERIOD_US) {
        printf("[B] FAIL: virtual clock %llu < timer period %u (time not conserved)\n",
               (unsigned long long)s_virt_us, TIMER_PERIOD_US); fail++;
    }
    if (s_master_ticks < 5 || s_master_ticks > 7) {
        printf("[B] FAIL: 50ms timer should land ~5 ticks, got %u\n", s_master_ticks); fail++;
    }

    if (fail == 0) printf("\nS1 SPIKE PASS: tight poll non-freezing + quota catch-up + 1:1 clock\n");
    else           printf("\nS1 SPIKE FAIL: %d assertion(s)\n", fail);
    return fail ? 1 : 0;
}
