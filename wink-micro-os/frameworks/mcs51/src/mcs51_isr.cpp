// SPDX-License-Identifier: Apache-2.0
// MCS-51 interrupt vector table + two-phase dispatch (C linkage, boundary ②).
#include "wink_mcs51_isr.h"

#include <cstdint>
#include <cstring>

#include "mcs51_trap.h"
#include "mcs51_context.h"
#include "wink_mcs51_clock.h"

namespace {
using isr_fn_t = void (*)(void);

constexpr uint8_t SFR_IE   = 0xA8u;
constexpr uint8_t SFR_IP   = 0xB8u;
constexpr uint8_t SFR_SCON = 0x98u;
constexpr uint8_t IE_EA    = 7u;

// Default mapping table for standard 8051 + CMS8S78xx
const mcs51_irq_map_entry_t s_default_irq_map[IRQ_SOURCE__COUNT] = {
    /* IRQ_SOURCE_INT0 */   { 0u,  0xA8u, 0u, 0x88u, 1u, 0xB8u, 0u, MCS51_IRQ_HW_AUTO_CLEAR }, // IE.EX0, TCON.IE0, IP.PX0
    /* IRQ_SOURCE_TIMER0 */ { 1u,  0xA8u, 1u, 0x88u, 5u, 0xB8u, 1u, MCS51_IRQ_HW_AUTO_CLEAR }, // IE.ET0, TCON.TF0, IP.PT0
    /* IRQ_SOURCE_INT1 */   { 2u,  0xA8u, 2u, 0x88u, 3u, 0xB8u, 2u, MCS51_IRQ_HW_AUTO_CLEAR }, // IE.EX1, TCON.IE1, IP.PX1
    /* IRQ_SOURCE_TIMER1 */ { 3u,  0xA8u, 3u, 0x88u, 7u, 0xB8u, 3u, MCS51_IRQ_HW_AUTO_CLEAR }, // IE.ET1, TCON.TF1, IP.PT1
    /* IRQ_SOURCE_UART0 */  { 4u,  0xA8u, 4u, 0x98u, 0u, 0xB8u, 4u, MCS51_IRQ_SW_CLEAR },      // IE.ES0, SCON.RI/TI, IP.PS0
    /* IRQ_SOURCE_ADC */    { 19u, 0xAAu, 4u, 0xB2u, 4u, 0xB9u, 4u, MCS51_IRQ_SW_CLEAR },      // EIE2.ADCIE, EIF2.ADCIF, EIP2.ADCIP
    /* IRQ_SOURCE_UART1 */  { 16u, 0xAAu, 1u, 0xFFu, 0u, 0xB9u, 1u, MCS51_IRQ_SW_CLEAR },
    /* IRQ_SOURCE_PWM */    { 18u, 0xAAu, 3u, 0xFFu, 0u, 0xB9u, 3u, MCS51_IRQ_SW_CLEAR },
    /* IRQ_SOURCE_I2C */    { 20u, 0xAAu, 5u, 0xFFu, 0u, 0xB9u, 5u, MCS51_IRQ_SW_CLEAR },
    /* IRQ_SOURCE_SPI */    { 21u, 0xAAu, 6u, 0xFFu, 0u, 0xB9u, 6u, MCS51_IRQ_SW_CLEAR },
};

mcs51_irq_map_entry_t s_irq_map[IRQ_SOURCE__COUNT];
bool                  s_irq_map_initialized = false;

void ensure_irq_map(void) {
    if (!s_irq_map_initialized) {
        std::memcpy(s_irq_map, s_default_irq_map, sizeof(s_default_irq_map));
        s_irq_map_initialized = true;
    }
}

}  // namespace

extern "C" {

void wink_mcs51_set_isr(uint8_t vector_num, void (*isr_fn)(void)) {
    if (vector_num < WINK_MCS51_NUM_VECTORS) {
        mcs51_get_context()->isr_table[vector_num] = reinterpret_cast<isr_fn_t>(isr_fn);
    }
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
    ensure_irq_map();
    if (src < IRQ_SOURCE__COUNT && entry != nullptr) {
        s_irq_map[src] = *entry;
    }
}

const mcs51_irq_map_entry_t* wink_mcs51_get_irq_map_entry(mcs51_irq_source_t src) {
    ensure_irq_map();
    if (src < IRQ_SOURCE__COUNT) {
        return &s_irq_map[src];
    }
    return nullptr;
}

void wink_mcs51_reset_irq_map(void) {
    std::memcpy(s_irq_map, s_default_irq_map, sizeof(s_default_irq_map));
    s_irq_map_initialized = true;
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
    ensure_irq_map();
    Mcu51Context* ctx = mcs51_get_context();
    const mcs51_irq_map_entry_t& entry = s_irq_map[src];
    uint8_t prio = 0;
    if (entry.prio_sfr != 0xFFu) {
        prio = (ctx->sfr_shadow[entry.prio_sfr] >> entry.prio_bit) & 1u;
    }
    WINK_TRACE_MCS51_IRQ(src, entry.vector, prio, wink_mcs51_virtual_us(), MCS51_IRQ_EVT_RAISE);

    if (entry.flag_sfr != 0xFFu && src != IRQ_SOURCE_UART0) {
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

    ensure_irq_map();

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
        const mcs51_irq_map_entry_t& entry = s_irq_map[s];

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
    const mcs51_irq_map_entry_t& entry = s_irq_map[best_src];
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
