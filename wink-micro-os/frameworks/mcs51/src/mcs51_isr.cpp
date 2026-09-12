// SPDX-License-Identifier: Apache-2.0
// MCS-51 interrupt vector table + two-phase dispatch (C linkage, boundary ②).
#include "wink_mcs51_isr.h"

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#ifndef WINK_MCS51_STRICT
#include "pal_log.h"
#endif

#include "mcs51_trap.h"
#include "mcs51_context.h"
#include "mcs51_family.h"
#include "wink_event.h"
#include "wink_mcs51_clock.h"

namespace {
using isr_fn_t = void (*)(void);

constexpr uint8_t SFR_IE   = 0xA8u;
constexpr uint8_t SFR_IP   = 0xB8u;
constexpr uint8_t SFR_SCON = 0x98u;
constexpr uint8_t IE_EA    = 7u;

// Core standard profile (stage5 CPL-06): semantic sources 0..5 only.
// INT0 0, TIMER0 1, INT1 2, TIMER1 3, UART0 4, TIMER2 5 (8052); enable/
// flag/priority fields are Intel-standard SFR positions. Every other
// source stays UNMAPPED here (0xFF sentinels: vector 0xFF never dispatches
// and mcs51_raise_irq drops the source before pending latches) and is
// loaded by the owning chip package through the per-context
// irq_map_extend hook — the generic ISR never names a vendor vector.
const mcs51_irq_map_entry_t s_default_irq_map[IRQ_SOURCE__COUNT] = {
    /* IRQ_SOURCE_INT0 */   { 0u,  0xA8u, 0u, 0x88u, 1u, 0xB8u, 0u, MCS51_IRQ_HW_AUTO_CLEAR }, // IE.EX0, TCON.IE0, IP.PX0
    /* IRQ_SOURCE_TIMER0 */ { 1u,  0xA8u, 1u, 0x88u, 5u, 0xB8u, 1u, MCS51_IRQ_HW_AUTO_CLEAR }, // IE.ET0, TCON.TF0, IP.PT0
    /* IRQ_SOURCE_INT1 */   { 2u,  0xA8u, 2u, 0x88u, 3u, 0xB8u, 2u, MCS51_IRQ_HW_AUTO_CLEAR }, // IE.EX1, TCON.IE1, IP.PX1
    /* IRQ_SOURCE_TIMER1 */ { 3u,  0xA8u, 3u, 0x88u, 7u, 0xB8u, 3u, MCS51_IRQ_HW_AUTO_CLEAR }, // IE.ET1, TCON.TF1, IP.PT1
    /* IRQ_SOURCE_UART0 */  { 4u,  0xA8u, 4u, 0x98u, 0u, 0xB8u, 4u, MCS51_IRQ_SW_CLEAR },      // IE.ES0, SCON.RI/TI, IP.PS0
    /* IRQ_SOURCE_TIMER2 */ { 5u,  0xA8u, 5u, 0xC9u, 7u, 0xB8u, 5u, MCS51_IRQ_SW_CLEAR },      // IE.ET2, T2IF.T2F, IP.PT2
    /* IRQ_SOURCE_ADC */    { 0xFFu, 0xFFu, 0u, 0xFFu, 0u, 0xFFu, 0u, MCS51_IRQ_SW_CLEAR },
    /* IRQ_SOURCE_UART1 */  { 0xFFu, 0xFFu, 0u, 0xFFu, 0u, 0xFFu, 0u, MCS51_IRQ_SW_CLEAR },
    /* IRQ_SOURCE_PWM */    { 0xFFu, 0xFFu, 0u, 0xFFu, 0u, 0xFFu, 0u, MCS51_IRQ_SW_CLEAR },
    /* IRQ_SOURCE_I2C */    { 0xFFu, 0xFFu, 0u, 0xFFu, 0u, 0xFFu, 0u, MCS51_IRQ_SW_CLEAR },
    /* IRQ_SOURCE_SPI */    { 0xFFu, 0xFFu, 0u, 0xFFu, 0u, 0xFFu, 0u, MCS51_IRQ_SW_CLEAR },
    /* IRQ_SOURCE_TIMER3 */ { 0xFFu, 0xFFu, 0u, 0xFFu, 0u, 0xFFu, 0u, MCS51_IRQ_SW_CLEAR },
    /* IRQ_SOURCE_TIMER4 */ { 0xFFu, 0xFFu, 0u, 0xFFu, 0u, 0xFFu, 0u, MCS51_IRQ_SW_CLEAR },
};

// Stage5 CPL-06 insulation: a vector is reachable only when the ACTIVE
// context's family descriptor whitelists it (classic 8052: vectors 0..5).
// Unmapped rows (vector 0xFF) are never reachable by construction.
bool family_vector_reachable(const Mcu51Context* ctx, uint8_t vector) {
    if (vector >= WINK_MCS51_NUM_VECTORS) {
        return false;
    }
    const mcs51_family_desc_t* d = mcs51_family_desc(ctx->family);
    for (uint8_t i = 0; i < d->irq_count; ++i) {
        if (d->irq_vector_table[i] == vector) {
            return true;
        }
    }
    return false;
}

}  // namespace

// GAP-12: duplicate-vector registration. Keil/SDCC reject two ISRs on the
// same vector at link time; the sim used to silently overwrite the table
// entry. STRICT aborts, release warns once and counts (runner-visible).
static uint32_t s_duplicate_vector_count = 0;

extern "C" {

void wink_mcs51_set_isr(uint8_t vector_num, void (*isr_fn)(void)) {
    if (vector_num < WINK_MCS51_NUM_VECTORS) {
        if (mcs51_get_context()->isr_table[vector_num] != nullptr) {
#ifdef WINK_MCS51_STRICT
            assert(0 && "duplicate ISR registration on one vector "
                        "(link error on Keil/SDCC; WINK_MCS51_STRICT)");
            std::abort();
#else
            if (s_duplicate_vector_count == 0) {
                pal_log_w("MCS51",
                          "duplicate ISR registration on vector %u: previous "
                          "handler silently overwritten (GAP-12)",
                          static_cast<unsigned>(vector_num));
            }
            ++s_duplicate_vector_count;
#endif
        }
        mcs51_get_context()->isr_table[vector_num] = reinterpret_cast<isr_fn_t>(isr_fn);
    }
}

uint32_t wink_mcs51_duplicate_vector_count(void) {
    return s_duplicate_vector_count;
}

void (*wink_mcs51_get_isr(uint8_t vector_num))(void) {
    if (vector_num < WINK_MCS51_NUM_VECTORS) {
        return mcs51_get_context()->isr_table[vector_num];
    }
    return nullptr;
}

void wink_mcs51_isr_enable(void)  { mcs51_get_context()->interrupts_enabled = true; }
void wink_mcs51_isr_disable(void) { mcs51_get_context()->interrupts_enabled = false; }

bool wink_mcs51_in_isr(void) { return mcs51_get_context()->in_service_depth > 0; }

uint8_t wink_mcs51_dispatch_vector(uint8_t vector_num) {
    Mcu51Context* ctx = mcs51_get_context();
    if (!ctx->interrupts_enabled || vector_num >= WINK_MCS51_NUM_VECTORS) {
        return 0;
    }
    // Stage5 CPL-06 insulation: an ISR registered for an extended vector is
    // unreachable on a family whose descriptor does not whitelist it.
    if (!family_vector_reachable(ctx, vector_num)) {
        return 0;
    }
    isr_fn_t fn = ctx->isr_table[vector_num];
    if (fn == nullptr) {
        return 0;
    }
    ++ctx->isr_dispatch_count[vector_num];

    bool local_depth_pushed = false;
    if (ctx->in_service_depth == 0) {
        ctx->in_service_prio_stack[0] = 0;
        ctx->in_service_depth = 1;
        local_depth_pushed = true;
    }

    fn();

    if (local_depth_pushed) {
        ctx->in_service_depth = 0;
    }
    return 1;
}

uint32_t wink_mcs51_isr_dispatch_count(uint8_t vector_num) {
    if (vector_num >= WINK_MCS51_NUM_VECTORS) {
        return 0;
    }
    return mcs51_get_context()->isr_dispatch_count[vector_num];
}

void wink_mcs51_reset_irq_state(void) {
    Mcu51Context* ctx = mcs51_get_context();
    ctx->pending_interrupts = 0;
    ctx->in_service_depth = 0;
    ctx->reti_suppress_one = false;
    for (uint8_t i = 0; i < 4; ++i) {
        ctx->in_service_prio_stack[i] = 0;
    }
    wink_mcs51_reset_irq_map();
}

void wink_mcs51_reset_isrs(void) {
    Mcu51Context* ctx = mcs51_get_context();
    for (uint32_t i = 0; i < WINK_MCS51_NUM_VECTORS; ++i) {
        ctx->isr_table[i] = nullptr;
        ctx->isr_dispatch_count[i] = 0;
    }
    ctx->interrupts_enabled = false;
    wink_mcs51_reset_irq_state();
}

void wink_mcs51_set_irq_map_entry(mcs51_irq_source_t src, const mcs51_irq_map_entry_t* entry) {
    Mcu51Context* ctx = mcs51_get_context();
    if (ctx != nullptr && src < IRQ_SOURCE__COUNT && entry != nullptr) {
        ctx->irq_map[src] = *entry;
    }
}

const mcs51_irq_map_entry_t* wink_mcs51_get_irq_map_entry(mcs51_irq_source_t src) {
    Mcu51Context* ctx = mcs51_get_context();
    if (ctx != nullptr && src < IRQ_SOURCE__COUNT) {
        return &ctx->irq_map[src];
    }
    return nullptr;
}

void wink_mcs51_reset_irq_map(void) {
    Mcu51Context* ctx = mcs51_get_context();
    if (ctx == nullptr) {
        return;
    }
    // Core standard rows first, then the chip package re-applies its
    // extended rows (stage5 CPL-06): a state reset must never silently
    // demote a chip-family context to the bare standard profile.
    std::memcpy(ctx->irq_map, s_default_irq_map, sizeof(s_default_irq_map));
    if (ctx->irq_map_extend != nullptr) {
        ctx->irq_map_extend(ctx);
    }
}

void wink_mcs51_suppress_next_irq(void) {
    mcs51_get_context()->reti_suppress_one = true;
}

void wink_mcs51_clear_reti_suppress(void) {
    mcs51_get_context()->reti_suppress_one = false;
}

void wink_mcs51_clear_irq(mcs51_irq_source_t src) {
    if (src < IRQ_SOURCE__COUNT) {
        mcs51_get_context()->pending_interrupts &= ~(1u << src);
    }
}

uint32_t wink_mcs51_get_pending_interrupts(void) {
    return mcs51_get_context()->pending_interrupts;
}

uint8_t wink_mcs51_get_in_service_depth(void) {
    return mcs51_get_context()->in_service_depth;
}

uint8_t wink_mcs51_get_in_service_prio(uint8_t depth_index) {
    Mcu51Context* ctx = mcs51_get_context();
    if (depth_index < ctx->in_service_depth) {
        return ctx->in_service_prio_stack[depth_index];
    }
    return 0;
}

void mcs51_raise_irq(mcs51_irq_source_t src) {
    if (src >= IRQ_SOURCE__COUNT) {
        return;
    }
    Mcu51Context* ctx = mcs51_get_context();
    if (ctx == nullptr) {
        return;
    }
    const mcs51_irq_map_entry_t& entry = ctx->irq_map[src];
    // Stage5 CPL-06 insulation: an extended source on a family without the
    // vector (e.g. an ADC request on classic) never latches pending.
    if (!family_vector_reachable(ctx, entry.vector)) {
        return;
    }
    uint8_t prio = 0;
    if (entry.prio_sfr != 0xFFu) {
        prio = (ctx->sfr_shadow[entry.prio_sfr] >> entry.prio_bit) & 1u;
    }
    WINK_TRACE_MCS51_IRQ(src, entry.vector, prio, wink_mcs51_virtual_us(), MCS51_IRQ_EVT_RAISE);

    if (entry.flag_sfr != 0xFFu && src != IRQ_SOURCE_UART0 && src != IRQ_SOURCE_TIMER2) {
        if (src == IRQ_SOURCE_INT0) {
            bool edge = (ctx->sfr_shadow[0x88] & (1u << 0)) != 0;
            if (edge) {
                ctx->sfr_shadow[entry.flag_sfr] |= (1u << entry.flag_bit);
            }
        } else if (src == IRQ_SOURCE_INT1) {
            bool edge = (ctx->sfr_shadow[0x88] & (1u << 2)) != 0;
            if (edge) {
                ctx->sfr_shadow[entry.flag_sfr] |= (1u << entry.flag_bit);
            }
        } else {
            ctx->sfr_shadow[entry.flag_sfr] |= (1u << entry.flag_bit);
        }
    }
    ctx->pending_interrupts |= (1u << src);

    // Task R6: Low-power wake
    if ((ctx->sfr_shadow[0x87] & 0x01u) != 0) {
        // IDLE mode: post wake event to unblock silent pend
        wink_event_t evt = {0};
        (void)wink_event_post(&evt);
    } else if ((ctx->sfr_shadow[0x87] & 0x02u) != 0) {
        // Power-down mode: external interrupts INT0/INT1 with EA=1 wake the CPU
        bool ea = (ctx->sfr_shadow[0xA8] & (1u << 7)) != 0;
        if (ea && (src == IRQ_SOURCE_INT0 || src == IRQ_SOURCE_INT1)) {
            wink_event_t evt = {0};
            (void)wink_event_post(&evt);
        }
    }
}

uint8_t mcs51_irq_scan_and_dispatch(void) {
    Mcu51Context* ctx = mcs51_get_context();
    if (!ctx->interrupts_enabled) {
        return 0;
    }
    if (ctx->reti_suppress_one) {
        ctx->reti_suppress_one = false;
        return 0;
    }
    if ((ctx->sfr_shadow[SFR_IE] & (1u << IE_EA)) == 0) {
        return 0;
    }
    if (ctx->pending_interrupts == 0) {
        return 0;
    }

    // Determine current running priority (-1 if not in ISR)
    int current_prio = -1;
    if (ctx->in_service_depth > 0) {
        current_prio = (int)ctx->in_service_prio_stack[ctx->in_service_depth - 1];
    }

    int     best_src    = -1;
    int     best_prio   = -1;
    uint8_t best_vector = 0xFFu;

    for (uint8_t s = 0; s < IRQ_SOURCE__COUNT; ++s) {
        if ((ctx->pending_interrupts & (1u << s)) == 0) {
            continue;
        }
        const mcs51_irq_map_entry_t& entry = ctx->irq_map[s];

        // Stage5 CPL-06: unmapped / non-whitelisted vectors never dispatch
        // (defense in depth behind the raise-time family drop).
        if (!family_vector_reachable(ctx, entry.vector)) {
            ctx->pending_interrupts &= ~(1u << s);
            continue;
        }

        // Check individual IE
        if (entry.ie_sfr != 0xFFu) {
            if ((ctx->sfr_shadow[entry.ie_sfr] & (1u << entry.ie_bit)) == 0) {
                continue;
            }
        }

        // Check flag validity: if flag is already cleared, drop the pending request
        if (entry.flag_sfr != 0xFFu) {
            bool flag_set = false;
            if (s == IRQ_SOURCE_UART0 && entry.flag_sfr == SFR_SCON) {
                flag_set = (ctx->sfr_shadow[SFR_SCON] & 0x03u) != 0;
            } else if (s == IRQ_SOURCE_INT0) {
                bool edge = (ctx->sfr_shadow[0x88] & (1u << 0)) != 0;
                flag_set = !edge || ((ctx->sfr_shadow[0x88] & (1u << 1)) != 0);
            } else if (s == IRQ_SOURCE_INT1) {
                bool edge = (ctx->sfr_shadow[0x88] & (1u << 2)) != 0;
                flag_set = !edge || ((ctx->sfr_shadow[0x88] & (1u << 3)) != 0);
            } else if (ctx->irq_flag_predicate != nullptr) {
                // Stage5 CPL-06 (S5-1 Step 1b): chip-owned multi-flag
                // semantics (e.g. a capture/compare flag set while T2F=0).
                // The chip hook must reproduce the standard single-bit
                // check for every source it does not own; standard parts
                // (null slot) take the plain single-bit path below.
                flag_set = ctx->irq_flag_predicate(
                    ctx, static_cast<mcs51_irq_source_t>(s), &entry);
            } else {
                flag_set = (ctx->sfr_shadow[entry.flag_sfr] & (1u << entry.flag_bit)) != 0;
            }
            if (!flag_set) {
                ctx->pending_interrupts &= ~(1u << s);
                continue;
            }
        }

        // Check priority
        uint8_t prio = 0;
        if (entry.prio_sfr != 0xFFu) {
            prio = (ctx->sfr_shadow[entry.prio_sfr] >> entry.prio_bit) & 1u;
        }

        // In-service masking rule: must be strictly higher than current_prio to preempt
        if ((int)prio <= current_prio) {
            continue;
        }

        // Selection rule: highest priority first; tie-breaker: lowest vector number (natural scan order)
        if (best_src < 0 || (int)prio > best_prio ||
            ((int)prio == best_prio && entry.vector < best_vector)) {
            best_src    = s;
            best_prio   = (int)prio;
            best_vector = entry.vector;
        }
    }

    if (best_src < 0) {
        return 0;
    }

    // Best candidate found
    const mcs51_irq_map_entry_t& entry = ctx->irq_map[best_src];
    ctx->pending_interrupts &= ~(1u << best_src);

    // Auto-clear hardware flag
    if (entry.clear_mode == MCS51_IRQ_HW_AUTO_CLEAR && entry.flag_sfr != 0xFFu) {
        ctx->sfr_shadow[entry.flag_sfr] &= ~(1u << entry.flag_bit);
    }

    // Push in-service priority
    if (ctx->in_service_depth < 4) {
        ctx->in_service_prio_stack[ctx->in_service_depth++] = (uint8_t)best_prio;
    }

    WINK_TRACE_MCS51_IRQ((mcs51_irq_source_t)best_src, entry.vector, (uint8_t)best_prio,
                         wink_mcs51_virtual_us(), MCS51_IRQ_EVT_DISPATCH);

    // Dispatch vector
    wink_mcs51_dispatch_vector(entry.vector);

    // Pop in-service priority (RETI)
    if (ctx->in_service_depth > 0) {
        --ctx->in_service_depth;
    }
    ctx->reti_suppress_one = true;

    WINK_TRACE_MCS51_IRQ((mcs51_irq_source_t)best_src, entry.vector, (uint8_t)best_prio,
                         wink_mcs51_virtual_us(), MCS51_IRQ_EVT_RETI);

    return 1;
}

}  // extern "C"
