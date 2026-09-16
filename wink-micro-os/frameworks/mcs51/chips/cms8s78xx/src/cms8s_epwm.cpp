// SPDX-License-Identifier: LGPL-3.0-only
// CMS8S78xx on-chip Enhanced PWM (EPWM) peripheral model.
#include "cms8s_epwm.h"

#include "cms8s_priv.h"
#include "cms8s_sfr_map.h"
#include "mcs51_context.h"
#include "mcs51_family.h"
#include "wink_mcs51_clock.h"
#include "wink_mcs51_gpio.h"
#include "wink_mcs51_isr.h"

#include <algorithm>
#include <cstdint>

namespace {

constexpr uint16_t XSFR_PWMCON   = 0xF120u;
constexpr uint16_t XSFR_PWMOE    = 0xF121u;
constexpr uint16_t XSFR_PWMPINV  = 0xF122u;
constexpr uint16_t XSFR_PWM01PSC = 0xF123u;
constexpr uint16_t XSFR_PWM23PSC = 0xF124u;
constexpr uint16_t XSFR_PWMCNTE  = 0xF126u;
constexpr uint16_t XSFR_PWMCNTM  = 0xF127u;
constexpr uint16_t XSFR_PWMCNTCLR= 0xF128u;
constexpr uint16_t XSFR_PWMLOADEN= 0xF129u;
constexpr uint16_t XSFR_PWM0DIV  = 0xF12Au;
constexpr uint16_t XSFR_PWM1DIV  = 0xF12Bu;
constexpr uint16_t XSFR_PWM2DIV  = 0xF12Cu;
constexpr uint16_t XSFR_PWM3DIV  = 0xF12Du;

constexpr uint16_t XSFR_PWMP0L   = 0xF130u;
constexpr uint16_t XSFR_PWMP0H   = 0xF131u;
constexpr uint16_t XSFR_PWMD0L   = 0xF140u;
constexpr uint16_t XSFR_PWMD0H   = 0xF141u;

constexpr uint16_t XSFR_PWMBRKC  = 0xF15Cu;
constexpr uint16_t XSFR_PWMBRKRDTL= 0xF15Du;
constexpr uint16_t XSFR_PWMBRKRDTH= 0xF15Eu;
constexpr uint16_t XSFR_PWMDTE   = 0xF160u;
constexpr uint16_t XSFR_PWMFBKC  = 0xF166u;
constexpr uint16_t XSFR_PWMFBKD  = 0xF167u;
constexpr uint16_t XSFR_PWMPIE   = 0xF168u;
constexpr uint16_t XSFR_PWMZIE   = 0xF169u;
constexpr uint16_t XSFR_PWMUIE   = 0xF16Au;
constexpr uint16_t XSFR_PWMDIE   = 0xF16Bu;
constexpr uint16_t XSFR_PWMPIF   = 0xF16Cu;
constexpr uint16_t XSFR_PWMZIF   = 0xF16Du;
constexpr uint16_t XSFR_PWMUIF   = 0xF16Eu;
constexpr uint16_t XSFR_PWMDIF   = 0xF16Fu;
constexpr uint16_t XSFR_CNFBCON  = 0xF507u;

uint32_t get_channel_prescaler(const Mcu51Context* ctx, uint8_t ch) {
    const uint8_t psc_reg = (ch < 2u) ? ctx->xdata_shadow[XSFR_PWM01PSC]
                                      : ctx->xdata_shadow[XSFR_PWM23PSC];
    const uint8_t shift = psc_reg & 0x07u;
    return (shift <= 7u) ? (1u << shift) : 1u;
}

uint32_t get_channel_divider(const Mcu51Context* ctx, uint8_t ch) {
    const uint8_t div_reg = ctx->xdata_shadow[XSFR_PWM0DIV + ch];
    if (div_reg == 0xFFu) return 1u;
    if (div_reg == 0x04u) return 2u;
    if (div_reg == 0x00u) return 4u;
    if (div_reg == 0x01u) return 8u;
    if (div_reg == 0x02u) return 16u;
    if (div_reg == 0x03u) return 32u;
    return 1u;
}

uint16_t get_channel_period(const Mcu51Context* ctx, uint8_t ch) {
    const uint16_t l = ctx->xdata_shadow[XSFR_PWMP0L + 2u * ch];
    const uint16_t h = ctx->xdata_shadow[XSFR_PWMP0H + 2u * ch];
    return static_cast<uint16_t>(l | (h << 8));
}

uint8_t resolve_ps_pin_val(const Mcu51Context* ctx, uint8_t ps_val, uint8_t default_port, uint8_t default_pin) {
    if (ps_val <= 0x07u) {
        return (ctx->sfr_shadow[0x80] >> (ps_val & 0x07u)) & 0x01u; // P0
    } else if (ps_val >= 0x10u && ps_val <= 0x17u) {
        return (ctx->sfr_shadow[0x90] >> (ps_val & 0x07u)) & 0x01u; // P1
    } else if (ps_val >= 0x20u && ps_val <= 0x27u) {
        return (ctx->sfr_shadow[0xA0] >> (ps_val & 0x07u)) & 0x01u; // P2
    } else if (ps_val >= 0x30u && ps_val <= 0x37u) {
        return (ctx->sfr_shadow[0xB0] >> (ps_val & 0x07u)) & 0x01u; // P3
    }
    const uint8_t port_sfr = (default_port == 0) ? 0x80u : (default_port == 1) ? 0x90u : (default_port == 2) ? 0xA0u : 0xB0u;
    return (ctx->sfr_shadow[port_sfr] >> default_pin) & 0x01u;
}

}  // namespace

extern "C" {

void cms8s_epwm_init(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    if (!ctx) return;
    cms8s_soc_bind(ctx);
    Cms8sPriv* priv = cms8s_priv(ctx);
    if (priv) {
        for (uint8_t i = 0; i < 4; ++i) {
            priv->epwm.counter[i] = 0;
            priv->epwm.direction[i] = 0;
        }
        priv->epwm.last_poll_us = 0;
        priv->epwm.pwmoe_prev = 0;
        priv->epwm.pwmcnte_prev = 0;
        priv->epwm.pwmcon_prev = 0;
        priv->epwm.brake_latched = false;
        priv->epwm.brake_delay_cnt = 0;
        ctx->xdata_shadow[CMS8S_XSFR_PS_FB0] = CMS8S_XSFR_PS_RESET;
        ctx->xdata_shadow[CMS8S_XSFR_PS_FB1] = CMS8S_XSFR_PS_RESET;
    }
}

void cms8s_epwm_reset(struct Mcu51Context* ctx) {
    cms8s_epwm_init(ctx);
}

void cms8s_epwm_poll(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    if (!ctx || ctx->family != MCS51_FAMILY_CMS8S78XX) return;

    Cms8sPriv* priv = cms8s_priv(ctx);
    if (!priv) return;

    const uint8_t pwmcon = ctx->xdata_shadow[XSFR_PWMCON];
    const uint8_t cnte   = ctx->xdata_shadow[XSFR_PWMCNTE];
    const bool    running = (pwmcon & 0x80u) != 0 && (cnte != 0);

    const uint64_t now_us = wink_mcs51_virtual_us();
    const uint8_t count_mode = pwmcon & 0x03u;

    const bool was_running = (priv->epwm.pwmcon_prev & 0x80u) != 0 && (priv->epwm.pwmcnte_prev != 0);
    if (running && !was_running) {
        for (uint8_t i = 0; i < 4; ++i) {
            if (cnte & (1u << i)) {
                priv->epwm.counter[i] = (count_mode == 0x02u) ? 0 : get_channel_period(ctx, i);
                priv->epwm.direction[i] = (count_mode == 0x02u) ? 0 : 1;
            }
        }
    } else if (running) {
        const uint8_t newly_enabled = cnte & ~priv->epwm.pwmcnte_prev;
        if (newly_enabled) {
            for (uint8_t i = 0; i < 4; ++i) {
                if (newly_enabled & (1u << i)) {
                    priv->epwm.counter[i] = (count_mode == 0x02u) ? 0 : get_channel_period(ctx, i);
                    priv->epwm.direction[i] = (count_mode == 0x02u) ? 0 : 1;
                }
            }
        }
    }

    const uint64_t elapsed_us = (now_us >= priv->epwm.last_poll_us) ? (now_us - priv->epwm.last_poll_us) : 0;
    if (elapsed_us == 0 && running == was_running) {
        return;
    }

    // Handle counter clear register
    const uint8_t cntclr = ctx->xdata_shadow[XSFR_PWMCNTCLR];
    if (cntclr != 0) {
        for (uint8_t i = 0; i < 4; ++i) {
            if (cntclr & (1u << i)) {
                priv->epwm.counter[i] = 0;
                priv->epwm.direction[i] = 0;
            }
        }
        ctx->xdata_shadow[XSFR_PWMCNTCLR] = 0;
    }

    // ── Brake System Handling ──
    const uint8_t pwmbrkc = ctx->xdata_shadow[XSFR_PWMBRKC];
    const uint8_t pwmfbkc = ctx->xdata_shadow[XSFR_PWMFBKC];
    const uint8_t cnfbc   = ctx->xdata_shadow[XSFR_CNFBCON];

    // Handle manual clear bit (BRKRCLR = bit 3 of PWMBRKC)
    if (pwmbrkc & 0x08u) {
        ctx->xdata_shadow[XSFR_PWMBRKC] &= ~0x88u; // Clear BRKOSF and BRKRCLR
        priv->epwm.brake_latched = false;
        priv->epwm.brake_delay_cnt = 0;
    }

    // Evaluate brake condition inputs
    const bool sw_brake = (pwmfbkc & 0x10u) != 0; // PWMFBKSW

    // Hardware FB pins (mapped via PS_FB0 / PS_FB1, default P1.4 / P1.5)
    bool fb0_brake = false;
    if (pwmfbkc & 0x01u) { // PWMFB0EN
        const uint8_t ps_fb0 = ctx->xdata_shadow[CMS8S_XSFR_PS_FB0];
        const uint8_t pin_val = resolve_ps_pin_val(ctx, ps_fb0, 1, 4);
        const uint8_t fb0_es = (pwmfbkc >> 2u) & 0x01u;
        fb0_brake = (pin_val == fb0_es);
    }
    bool fb1_brake = false;
    if (pwmfbkc & 0x02u) { // PWMFB1EN
        const uint8_t ps_fb1 = ctx->xdata_shadow[CMS8S_XSFR_PS_FB1];
        const uint8_t pin_val = resolve_ps_pin_val(ctx, ps_fb1, 1, 5);
        const uint8_t fb1_es = (pwmfbkc >> 3u) & 0x01u;
        fb1_brake = (pin_val == fb1_es);
    }

    // ACMP comparator brake linkage
    bool acmp0_brake = false;
    if (cnfbc & 0x40u) { // C0FBPEN (level)
        const uint8_t acmp0_out = priv->acmp.last_c0out ? 1u : 0u;
        const uint8_t c0_level = (cnfbc >> 4u) & 0x01u;
        acmp0_brake = (acmp0_out == c0_level);
    }
    if (cnfbc & 0x04u) { // C0FBEEN (edge)
        if (priv->acmp.last_c0out) acmp0_brake = true;
    }

    bool acmp1_brake = false;
    if (cnfbc & 0x80u) { // C1FBPEN (level)
        const uint8_t acmp1_out = priv->acmp.last_c1out ? 1u : 0u;
        const uint8_t c1_level = (cnfbc >> 5u) & 0x01u;
        acmp1_brake = (acmp1_out == c1_level);
    }
    if (cnfbc & 0x08u) { // C1FBEEN (edge)
        if (priv->acmp.last_c1out) acmp1_brake = true;
    }

    const bool brake_condition = sw_brake || fb0_brake || fb1_brake || acmp0_brake || acmp1_brake;

    if (brake_condition) {
        // Set Brake Active Flag (BRKAF = bit 5 of PWMFBKC)
        ctx->xdata_shadow[XSFR_PWMFBKC] |= 0x20u;

        // If Fault Brake is enabled (BRKEN = bit 2 of PWMBRKC)
        if (pwmbrkc & 0x04u) {
            const bool was_latched = priv->epwm.brake_latched;
            priv->epwm.brake_latched = true;
            ctx->xdata_shadow[XSFR_PWMBRKC] |= 0x80u; // Set BRKOSF (bit 7)

            // Trigger Fault Brake interrupt on leading edge
            if (!was_latched) {
                ctx->xdata_shadow[XSFR_PWMFBKC] |= 0x40u; // Set PWMFBF (bit 6)
                if ((ctx->xdata_shadow[XSFR_PWMFBKC] & 0x80u) && (ctx->sfr_shadow[0xAA] & 0x08u)) {
                    mcs51_raise_irq(IRQ_SOURCE_PWM);
                }
            }
        }
    } else {
        // Clear Brake Active Flag
        ctx->xdata_shadow[XSFR_PWMFBKC] &= ~0x20u;

        if (priv->epwm.brake_latched) {
            const uint8_t brake_mode = pwmbrkc & 0x03u;
            if (brake_mode == 0x01u) {
                // Suspend Mode: immediately release output when brake input de-asserts
                priv->epwm.brake_latched = false;
                ctx->xdata_shadow[XSFR_PWMBRKC] &= ~0x80u; // Clear BRKOSF
            }
        }
    }

    if (!running) {
        priv->epwm.last_poll_us = now_us;
        priv->epwm.pwmcon_prev = pwmcon;
        priv->epwm.pwmcnte_prev = cnte;
        return;
    }

    for (uint8_t ch = 0; ch < 4; ++ch) {
        if ((cnte & (1u << ch)) == 0) continue;

        const uint16_t period = get_channel_period(ctx, ch);
        if (period == 0) continue;

        const uint32_t prescaler = get_channel_prescaler(ctx, ch);
        const uint32_t div       = get_channel_divider(ctx, ch);
        const uint32_t factor    = prescaler * div; // factor ticks per Fsys/factor

        // System clock = 24MHz -> 24 ticks per microsecond at factor = 1
        // Total ticks in elapsed_us = elapsed_us * 24 / factor
        const uint64_t ticks = elapsed_us * 24u / factor;
        if (ticks == 0) continue;

        bool zero_event = false;
        bool period_event = false;

        if (count_mode == 0x00u) {
            // ── Down-Count Mode ──
            // Counts down from period to 0, then reloads to period.
            const uint32_t cycle = period + 1u;
            uint32_t cur = priv->epwm.counter[ch];
            if (ticks >= cur) {
                zero_event = true;
                const uint64_t rem = ticks - cur;
                cur = period - (rem % cycle);
            } else {
                cur -= static_cast<uint32_t>(ticks);
            }
            priv->epwm.counter[ch] = static_cast<uint16_t>(cur);
        } else if (count_mode == 0x02u) {
            // ── Up-Down (Center-Aligned) Mode ──
            // Counts 0 -> period (dir=0) -> 0 (dir=1)
            uint64_t rem_ticks = ticks;
            while (rem_ticks > 0) {
                if (priv->epwm.direction[ch] == 0) {
                    // Counting UP
                    const uint32_t to_period = (priv->epwm.counter[ch] < period)
                                                   ? (period - priv->epwm.counter[ch])
                                                   : 0;
                    if (rem_ticks >= to_period) {
                        priv->epwm.counter[ch] = period;
                        priv->epwm.direction[ch] = 1; // Switch to counting DOWN
                        period_event = true;
                        rem_ticks -= to_period;
                    } else {
                        priv->epwm.counter[ch] += static_cast<uint16_t>(rem_ticks);
                        rem_ticks = 0;
                    }
                } else {
                    // Counting DOWN
                    const uint32_t to_zero = priv->epwm.counter[ch];
                    if (rem_ticks >= to_zero) {
                        priv->epwm.counter[ch] = 0;
                        priv->epwm.direction[ch] = 0; // Switch to counting UP
                        zero_event = true;
                        rem_ticks -= to_zero;
                    } else {
                        priv->epwm.counter[ch] -= static_cast<uint16_t>(rem_ticks);
                        rem_ticks = 0;
                    }
                }
            }
        }

        if (zero_event) {
            ctx->xdata_shadow[XSFR_PWMZIF] |= (1u << ch);
            if ((ctx->xdata_shadow[XSFR_PWMZIE] & (1u << ch)) &&
                (ctx->sfr_shadow[0xAA] & 0x08u)) {
                mcs51_raise_irq(IRQ_SOURCE_PWM);
            }

            // Recover mode: check if this is the designated reload source channel
            const uint8_t brk_reload_ch = (ctx->xdata_shadow[XSFR_PWMBRKC] >> 4u) & 0x03u;
            if (ch == brk_reload_ch && !brake_condition && priv->epwm.brake_latched) {
                const uint8_t mode = ctx->xdata_shadow[XSFR_PWMBRKC] & 0x03u;
                if (mode == 0x02u) {
                    // Recover mode: unlatch on reload event!
                    priv->epwm.brake_latched = false;
                    ctx->xdata_shadow[XSFR_PWMBRKC] &= ~0x80u;
                } else if (mode == 0x03u) {
                    // Delay recover mode: count delay
                    const uint16_t delay_target = (static_cast<uint16_t>(ctx->xdata_shadow[XSFR_PWMBRKRDTH] & 0x03u) << 8) |
                                                   ctx->xdata_shadow[XSFR_PWMBRKRDTL];
                    if (++priv->epwm.brake_delay_cnt >= delay_target) {
                        priv->epwm.brake_latched = false;
                        priv->epwm.brake_delay_cnt = 0;
                        ctx->xdata_shadow[XSFR_PWMBRKC] &= ~0x80u;
                    }
                }
            }
        }

        if (period_event) {
            ctx->xdata_shadow[XSFR_PWMPIF] |= (1u << ch);
            if ((ctx->xdata_shadow[XSFR_PWMPIE] & (1u << ch)) &&
                (ctx->sfr_shadow[0xAA] & 0x08u)) {
                mcs51_raise_irq(IRQ_SOURCE_PWM);
            }
        }
    }

    priv->epwm.last_poll_us = now_us;
    priv->epwm.pwmcon_prev = pwmcon;
    priv->epwm.pwmcnte_prev = cnte;
}

uint64_t cms8s_epwm_next_event_us(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    if (!ctx || ctx->family != MCS51_FAMILY_CMS8S78XX) return UINT64_MAX;

    Cms8sPriv* priv = cms8s_priv(ctx);
    if (!priv) return UINT64_MAX;

    const uint8_t pwmcon = ctx->xdata_shadow[XSFR_PWMCON];
    const uint8_t cnte   = ctx->xdata_shadow[XSFR_PWMCNTE];
    if ((pwmcon & 0x80u) == 0 || cnte == 0) {
        return UINT64_MAX;
    }

    const uint8_t count_mode = pwmcon & 0x03u;
    uint64_t min_delta_us = UINT64_MAX;

    for (uint8_t ch = 0; ch < 4; ++ch) {
        if ((cnte & (1u << ch)) == 0) continue;

        const uint16_t period = get_channel_period(ctx, ch);
        if (period == 0) continue;

        const uint32_t prescaler = get_channel_prescaler(ctx, ch);
        const uint32_t div       = get_channel_divider(ctx, ch);
        const uint32_t factor    = prescaler * div;

        uint32_t ticks_to_event = 0;
        if (count_mode == 0x00u) {
            // Down-count mode: event is zero match
            ticks_to_event = (priv->epwm.counter[ch] > 0) ? priv->epwm.counter[ch] : (period + 1u);
        } else if (count_mode == 0x02u) {
            // Up-down mode: event is either period match (if up) or zero match (if down)
            if (priv->epwm.direction[ch] == 0) {
                ticks_to_event = (period > priv->epwm.counter[ch]) ? (period - priv->epwm.counter[ch]) : 1u;
            } else {
                ticks_to_event = (priv->epwm.counter[ch] > 0) ? priv->epwm.counter[ch] : 1u;
            }
        }

        const uint64_t delta_us = (static_cast<uint64_t>(ticks_to_event) * factor + 23u) / 24u;
        if (delta_us < min_delta_us) {
            min_delta_us = delta_us;
        }
    }

    if (min_delta_us == UINT64_MAX) return UINT64_MAX;
    return wink_mcs51_virtual_us() + (min_delta_us > 0 ? min_delta_us : 1u);
}

}  // extern "C"
