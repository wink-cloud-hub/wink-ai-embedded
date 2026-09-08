// SPDX-License-Identifier: Apache-2.0
// MCS-51 virtual slave clock + cooperative quota engine (ADR-0072).
#include "wink_mcs51_clock.h"
#include "wink_mcs51_isr.h"
#include "mcs51_context.h"

#include "pal_osal.h"
#include "wink_sim_scheduler.h"

#include <cstdint>

namespace {

// Catch-up hook (timer models); null until registered.
wink_mcs51_catchup_fn_t s_catchup = nullptr;

bool in_fiber(void) {
    return sim_scheduler_current_id() != SIM_SCHED_NO_READY;
}

void do_catchup(void) {
    if (s_catchup != nullptr) {
        s_catchup(mcs51_get_context()->virtual_us);
    }
}

void cooperative_yield(void) {
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
    pal_os_sleep_ms(0u);
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif
}

void bill_master(uint32_t us) {
    pal_os_busy_wait_us(us);
}

}  // namespace

extern "C" {

void wink_mcs51_clock_reset(void) {
    Mcu51Context* ctx = mcs51_get_context();
    ctx->virtual_us = 0;
    ctx->slice_start_us = 0;
    ctx->quota_yields = 0;
}

void wink_mcs51_set_catchup_hook(wink_mcs51_catchup_fn_t hook) {
    s_catchup = hook;
}

uint64_t wink_mcs51_virtual_us(void) {
    return mcs51_get_context()->virtual_us;
}

uint32_t wink_mcs51_quota_yield_count(void) {
    return mcs51_get_context()->quota_yields;
}

void wink_mcs51_test_advance_virtual_us(uint32_t us) {
    Mcu51Context* ctx = mcs51_get_context();
    ctx->virtual_us += static_cast<uint64_t>(us);
    wink_mcs51_clear_reti_suppress();
}

uint32_t wink_mcs51_master_tick_count(void) {
    return static_cast<uint32_t>(mcs51_get_context()->virtual_us / 10000u);
}

void wink_mcs51_charge_us(uint32_t us) {
    if (us == 0 || !in_fiber()) {
        return;
    }
    Mcu51Context* ctx = mcs51_get_context();
    ctx->virtual_us += us;

    if (wink_mcs51_in_isr()) {
        return;
    }

    wink_mcs51_clear_reti_suppress();

    if (static_cast<uint64_t>(ctx->virtual_us - ctx->slice_start_us) >= WINK_MCS51_QUOTA_US) {
        bill_master(static_cast<uint32_t>(ctx->virtual_us - ctx->slice_start_us));
        ++ctx->quota_yields;
        cooperative_yield();
        ctx->slice_start_us = ctx->virtual_us;
        do_catchup();
        mcs51_irq_scan_and_dispatch();
    }
}

void wink_mcs51_delay_ms(uint32_t ms) {
    if (!in_fiber() || ms == 0) {
        return;
    }
    Mcu51Context* ctx = mcs51_get_context();
    const uint64_t target = ctx->virtual_us + static_cast<uint64_t>(ms) * 1000u;
    while (ctx->virtual_us < target) {
        uint64_t remaining = target - ctx->virtual_us;
        uint32_t step = (remaining >= WINK_MCS51_QUOTA_US)
                            ? WINK_MCS51_QUOTA_US
                            : static_cast<uint32_t>(remaining);
        ctx->virtual_us += step;
        bill_master(step);
        ++ctx->quota_yields;
        cooperative_yield();
        ctx->slice_start_us = ctx->virtual_us;
        do_catchup();
        mcs51_irq_scan_and_dispatch();
    }
}

}  // extern "C"
