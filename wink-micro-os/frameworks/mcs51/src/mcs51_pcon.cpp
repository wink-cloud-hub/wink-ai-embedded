// SPDX-License-Identifier: LGPL-3.0-only
// MCS-51 PCON low-power modes (IDLE / Power-Down) dual-mode scheduling (Task R6).
#include "mcs51_pcon.h"

#include "mcs51_context.h"
#include "mcs51_peripheral.h"
#include "wink_event.h"
#include "wink_mcs51_clock.h"
#include "pal_osal.h"

extern "C" {

uint64_t mcs51_calc_next_event_us(struct Mcu51Context* ctx, bool is_pd) {
    if (!ctx) ctx = mcs51_get_context();
    uint64_t earliest = UINT64_MAX;

    if (!is_pd) {
        for (uint8_t i = 0; i < g_mcs51_num_peripherals; ++i) {
            if (!mcs51_peripheral_active_for(&g_mcs51_peripherals[i], ctx->family)) {
                continue;
            }
            if (g_mcs51_peripherals[i].next_event_us != nullptr) {
                uint64_t t = g_mcs51_peripherals[i].next_event_us(ctx);
                if (t < earliest) {
                    earliest = t;
                }
            }
        }
        // Stage4 CPL-10: chip registry next-events join the same minimum.
        for (uint8_t i = 0; i < mcs51_peripheral_registered_count(); ++i) {
            const mcs51_peripheral_desc_t* d = mcs51_peripheral_registered(i);
            if (!mcs51_peripheral_active_for(d, ctx->family)) {
                continue;
            }
            if (d->next_event_us != nullptr) {
                uint64_t t = d->next_event_us(ctx);
                if (t < earliest) {
                    earliest = t;
                }
            }
        }
    }

    if (ctx->edge_head != ctx->edge_tail) {
        uint64_t edge_t = ctx->edge_queue[ctx->edge_tail].fire_us;
        if (edge_t < earliest) {
            earliest = edge_t;
        }
    }

    return earliest;
}

void wink_mcs51_scenario_terminate_timeout(struct Mcu51Context* ctx) {
    (void)ctx;
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
    pal_os_sleep_ms(0);
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif
}

void mcs51_on_pcon_write(struct Mcu51Context* ctx, uint8_t addr, uint8_t old_val, uint8_t new_val) {
    (void)addr;
    if (!ctx) ctx = mcs51_get_context();

    // ── 1. IDLE standby mode (PCON.0 = 1) ──────────────────────────────────
    if ((new_val & 0x01u) && !(old_val & 0x01u)) {
        uint64_t next_us = mcs51_calc_next_event_us(ctx, false);
        if (next_us != UINT64_MAX && next_us > ctx->virtual_us) {
            // Mode A: Active scheduled events — step-pump without wall-clock delay
            while (ctx->virtual_us < next_us && ctx->pending_interrupts == 0) {
                wink_mcs51_microstep();
            }
        } else {
            // Mode B: True silent state — wait on event queue
            wink_event_t evt;
            wink_status_t st = wink_event_pend(&evt, WINK_MCS51_SCENARIO_HORIZON_MS);
            if (st == WINK_ERR_TIMEOUT) {
                wink_mcs51_scenario_terminate_timeout(ctx);
                return;
            }
        }
        ctx->sfr_shadow[0x87] &= ~0x01u; // Exit IDLE
    }

    // ── 2. Power Down mode (PCON.1 = 1) ────────────────────────────────────
    if ((new_val & 0x02u) && !(old_val & 0x02u)) {
        wink_event_t evt;
        wink_status_t st = wink_event_pend(&evt, WINK_MCS51_SCENARIO_HORIZON_MS);
        if (st == WINK_ERR_TIMEOUT) {
            wink_mcs51_scenario_terminate_timeout(ctx);
            return;
        }
        ctx->sfr_shadow[0x87] &= ~0x02u; // Exit PD
    }
}

} // extern "C"
