// SPDX-License-Identifier: LGPL-3.0-only
// CMS8S78xx on-chip Enhanced PWM (EPWM) peripheral model.
// Verified against CMS8S78xx Reference Manual V1.1.1 Chapter 16.
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
#include <cstdio>
#include "osal/pal_osal.h"

extern "C" void js_pal_log(uint8_t level, const char* msg);

namespace {

// ── Control & Timing Registers ──
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

// ── Pin MUX Configuration Registers ──
constexpr uint16_t XSFR_P00CFG   = 0xF000u;
constexpr uint16_t XSFR_P01CFG   = 0xF001u;
constexpr uint16_t XSFR_P02CFG   = 0xF002u;
constexpr uint16_t XSFR_P03CFG   = 0xF003u;
constexpr uint16_t XSFR_P14CFG   = 0xF014u;
constexpr uint16_t XSFR_P15CFG   = 0xF015u;
constexpr uint16_t XSFR_P16CFG   = 0xF016u;
constexpr uint16_t XSFR_P17CFG   = 0xF017u;
constexpr uint16_t XSFR_P20CFG   = 0xF020u;
constexpr uint16_t XSFR_P21CFG   = 0xF021u;
constexpr uint16_t XSFR_P22CFG   = 0xF022u;
constexpr uint16_t XSFR_P23CFG   = 0xF023u;

// Clock MUX per manual Section 16.5.3 ~ 16.5.5
uint32_t get_channel_clock_hz(const Mcu51Context* ctx, uint8_t ch, uint32_t fsys) {
    const uint8_t psc_reg = (ch < 2u) ? ctx->xdata_shadow[XSFR_PWM01PSC]
                                      : ctx->xdata_shadow[XSFR_PWM23PSC];
    if (psc_reg == 0) return 0; // 00H: 预分频停止且计数停止

    const uint8_t div_reg = ctx->xdata_shadow[XSFR_PWM0DIV + ch];
    if (div_reg == 0xFFu) {
        // EPWM_CLK_DIV_1 (0xFF): 直通系统主频 Fsys，旁路第一级预分频！
        return fsys;
    }
    const uint32_t f_psc = fsys / (static_cast<uint32_t>(psc_reg) + 1u);
    if (div_reg == 0x04u) return f_psc;       // /1 (EPWM_CLK_DIV_2)
    if (div_reg == 0x00u) return f_psc / 2u;  // /2 (EPWM_CLK_DIV_4)
    if (div_reg == 0x01u) return f_psc / 4u;  // /4 (EPWM_CLK_DIV_8)
    if (div_reg == 0x02u) return f_psc / 8u;  // /8 (EPWM_CLK_DIV_16)
    if (div_reg == 0x03u) return f_psc / 16u; // /16 (EPWM_CLK_DIV_32)
    return fsys; // 其它值直通 Fsys
}

uint16_t get_channel_period(const Mcu51Context* ctx, uint8_t ch) {
    const uint16_t l = ctx->xdata_shadow[XSFR_PWMP0L + 2u * ch];
    const uint16_t h = ctx->xdata_shadow[XSFR_PWMP0H + 2u * ch];
    return static_cast<uint16_t>(l | (h << 8));
}

uint16_t get_channel_duty(const Mcu51Context* ctx, uint8_t ch) {
    const uint16_t l = ctx->xdata_shadow[XSFR_PWMD0L + 2u * ch];
    const uint16_t h = ctx->xdata_shadow[XSFR_PWMD0H + 2u * ch];
    return static_cast<uint16_t>(l | (h << 8));
}

uint8_t resolve_ps_pin_val(const Mcu51Context* ctx, uint8_t ps_val, uint8_t default_port, uint8_t default_pin) {
    (void)ctx;
    uint8_t port = default_port;
    uint8_t bit = default_pin;
    if (ps_val <= 0x07u) {
        port = 0; bit = ps_val & 0x07u;
    } else if (ps_val >= 0x10u && ps_val <= 0x17u) {
        port = 1; bit = ps_val & 0x07u;
    } else if (ps_val >= 0x20u && ps_val <= 0x27u) {
        port = 2; bit = ps_val & 0x07u;
    } else if (ps_val >= 0x30u && ps_val <= 0x37u) {
        port = 3; bit = ps_val & 0x07u;
    }
    return mcs51_gpio_bit_read_pin(port, bit);
}

uint16_t resolve_pg_physical_pin(const Mcu51Context* ctx, uint8_t ch) {
    if (ch == 0) {
        if (ctx->xdata_shadow[XSFR_P20CFG] == 0x04u) return 16u; // P2.0
        if (ctx->xdata_shadow[XSFR_P00CFG] == 0x04u) return 0u;  // P0.0
        if (ctx->xdata_shadow[XSFR_P17CFG] == 0x04u) return 15u; // P1.7
        return 16u;
    } else if (ch == 1) {
        if (ctx->xdata_shadow[XSFR_P21CFG] == 0x04u) return 17u; // P2.1
        if (ctx->xdata_shadow[XSFR_P01CFG] == 0x04u) return 1u;  // P0.1
        if (ctx->xdata_shadow[XSFR_P16CFG] == 0x04u) return 14u; // P1.6
        return 17u;
    } else if (ch == 2) {
        if (ctx->xdata_shadow[XSFR_P22CFG] == 0x04u) return 18u; // P2.2
        if (ctx->xdata_shadow[XSFR_P02CFG] == 0x04u) return 2u;  // P0.2
        if (ctx->xdata_shadow[XSFR_P15CFG] == 0x04u) return 13u; // P1.5
        return 18u;
    } else if (ch == 3) {
        if (ctx->xdata_shadow[XSFR_P23CFG] == 0x04u) return 19u; // P2.3
        if (ctx->xdata_shadow[XSFR_P03CFG] == 0x04u) return 3u;  // P0.3
        if (ctx->xdata_shadow[XSFR_P14CFG] == 0x04u) return 12u; // P1.4
        return 19u;
    }
    return 0xFFFFu;
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
            priv->epwm.pg_pin_level[i] = 0;
            priv->epwm.tick_fraction_rem[i] = 0;
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
    // PWMRUN (bit 6): 0 = 使能分频时钟, 1 = 禁止 (PSC/DIV清零)
    const bool clock_enabled = (pwmcon & 0x40u) == 0;
    const bool running = clock_enabled && (cnte != 0);

    const uint64_t now_us = wink_mcs51_virtual_us();
    const uint8_t count_mode = pwmcon & 0x02u; // bit 1: CNTTYPE (0=Down, 1=Up-Down)

    const bool was_running = (priv->epwm.pwmcnte_prev != 0) && ((priv->epwm.pwmcon_prev & 0x40u) == 0);
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

    // ── Brake System Handling (Section 16.4.6) ──
    const uint8_t pwmbrkc = ctx->xdata_shadow[XSFR_PWMBRKC];
    const uint8_t pwmfbkc = ctx->xdata_shadow[XSFR_PWMFBKC];
    const uint8_t cnfbc   = ctx->xdata_shadow[XSFR_CNFBCON];

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

    // Handle manual clear bit (BRKRCLR = bit 3 of PWMBRKC). Only effective when BRKAF == 0!
    if ((pwmbrkc & 0x08u) && !brake_condition) {
        ctx->xdata_shadow[XSFR_PWMBRKC] &= ~0x88u; // Clear BRKOSF and BRKRCLR
        priv->epwm.brake_latched = false;
        priv->epwm.brake_delay_cnt = 0;
    }

    if (brake_condition) {
        // Set Brake Active Flag (BRKAF = bit 5 of PWMFBKC)
        ctx->xdata_shadow[XSFR_PWMFBKC] |= 0x20u;

        // If Fault Brake is enabled (BRKEN = bit 2 of PWMBRKC)
        if (pwmbrkc & 0x04u) {
            const bool was_latched = priv->epwm.brake_latched;
            priv->epwm.brake_latched = true;
            ctx->xdata_shadow[XSFR_PWMBRKC] |= 0x80u; // Set BRKOSF (bit 7)

            const uint8_t brake_mode = pwmbrkc & 0x03u;
            if (brake_mode == 0x00u) {
                // Stop Mode: 故障发生时硬件清零 PWMCNTE 停止运行！(手册第 126 页)
                ctx->xdata_shadow[XSFR_PWMCNTE] = 0;
            }

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
    }

    const uint32_t fsys = wink_mcs51_get_clock_hz();

    // ── Channel Counter Progression ──
    if (running && elapsed_us > 0) {
        for (uint8_t ch = 0; ch < 4; ++ch) {
            if ((cnte & (1u << ch)) == 0) continue;

            const uint16_t period = get_channel_period(ctx, ch);
            if (period == 0) continue;

            const uint32_t f_ch = get_channel_clock_hz(ctx, ch, fsys);
            if (f_ch == 0) continue;

            // Fractional microsecond remainder accumulation
            const uint64_t dividend = elapsed_us * static_cast<uint64_t>(f_ch) + priv->epwm.tick_fraction_rem[ch];
            const uint64_t ticks = dividend / 1000000ULL;
            priv->epwm.tick_fraction_rem[ch] = static_cast<uint32_t>(dividend % 1000000ULL);
            if (ticks == 0) continue;

            bool zero_event = false;
            bool period_event = false;

            if (count_mode == 0x00u) {
                // ── Down-Count Mode (边沿对齐向下计数) ──
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
                // ── Up-Down Mode (中心对齐上下计数) ──
                uint64_t rem_ticks = ticks;
                while (rem_ticks > 0) {
                    if (priv->epwm.direction[ch] == 0) {
                        // Counting UP
                        const uint32_t to_period = (priv->epwm.counter[ch] < period)
                                                       ? (period - priv->epwm.counter[ch])
                                                       : 0;
                        if (rem_ticks >= to_period) {
                            priv->epwm.counter[ch] = period;
                            priv->epwm.direction[ch] = 1; // Turn around to DOWN
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
                            priv->epwm.direction[ch] = 0; // Turn around to UP
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

                // Recover mode: check reload source channel
                const uint8_t brk_reload_ch = (ctx->xdata_shadow[XSFR_PWMBRKC] >> 4u) & 0x03u;
                if (ch == brk_reload_ch && !brake_condition && priv->epwm.brake_latched) {
                    const uint8_t mode = ctx->xdata_shadow[XSFR_PWMBRKC] & 0x03u;
                    if (mode == 0x02u) {
                        // Recover mode: 刹车信号撤销后在下一个加载点自动恢复！
                        priv->epwm.brake_latched = false;
                        ctx->xdata_shadow[XSFR_PWMBRKC] &= ~0x80u;
                    } else if (mode == 0x03u) {
                        // Delay recover mode
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
    }

    // ── Output Waveform Generation for PG0..PG3 ──
    const uint8_t pwmoe   = ctx->xdata_shadow[XSFR_PWMOE];
    const uint8_t pwmpinv = ctx->xdata_shadow[XSFR_PWMPINV];
    const uint8_t pwmfbkd = ctx->xdata_shadow[XSFR_PWMFBKD];
    const uint8_t pwm_mode = (pwmcon >> 4u) & 0x03u; // 00: Independent, 01: Complementary, 10: Synchronized

    uint8_t raw_levels[4] = {0, 0, 0, 0};
    for (uint8_t ch = 0; ch < 4; ++ch) {
        const uint16_t period = get_channel_period(ctx, ch);
        const uint16_t duty   = get_channel_duty(ctx, ch);
        const uint16_t cur    = priv->epwm.counter[ch];

        if (count_mode == 0x00u) {
            // Down-Count: CNT in [CMP, 0) is HIGH; CNT > CMP or CNT == 0 is LOW (Section 16.4.2)
            if (duty == 0) {
                raw_levels[ch] = 0;
            } else if (cur <= duty && cur > 0) {
                raw_levels[ch] = 1;
            } else {
                raw_levels[ch] = 0;
            }
        } else {
            // Up-Down Count: Section 16.4.3
            // Counting UP: CNT >= CMP is HIGH; Counting DOWN: CNT > CMP is HIGH
            if (duty == 0) {
                raw_levels[ch] = 1; // 100% duty when CMP = 0
            } else if (priv->epwm.direction[ch] == 0) {
                raw_levels[ch] = (cur >= duty) ? 1u : 0u;
            } else {
                raw_levels[ch] = (cur > duty) ? 1u : 0u;
            }
        }
    }

    // Mode resolution (Complementary / Synchronized)
    uint8_t mode_levels[4];
    if (pwm_mode == 0x01u) {
        // Complementary: PG1 = !PG0, PG3 = !PG2
        mode_levels[0] = raw_levels[0];
        mode_levels[1] = raw_levels[0] ? 0u : 1u;
        mode_levels[2] = raw_levels[2];
        mode_levels[3] = raw_levels[2] ? 0u : 1u;
    } else if (pwm_mode == 0x02u) {
        // Synchronized: PG1 = PG0, PG3 = PG2
        mode_levels[0] = raw_levels[0];
        mode_levels[1] = raw_levels[0];
        mode_levels[2] = raw_levels[2];
        mode_levels[3] = raw_levels[2];
    } else {
        // Independent
        for (uint8_t i = 0; i < 4; ++i) mode_levels[i] = raw_levels[i];
    }

    // Polarity inversion and Fault Brake Override
    for (uint8_t ch = 0; ch < 4; ++ch) {
        uint8_t out_level = mode_levels[ch];
        if (pwmpinv & (1u << ch)) {
            out_level = out_level ? 0u : 1u;
        }

        // Fault Brake latch forces output to PWMFBKD setting
        if (priv->epwm.brake_latched) {
            out_level = (pwmfbkd >> ch) & 0x01u;
        }

        priv->epwm.pg_pin_level[ch] = out_level;


        // If channel output is enabled in PWMOE, drive physical pin
        if (pwmoe & (1u << ch)) {
            const uint16_t phys_pin = resolve_pg_physical_pin(ctx, ch);
            if (phys_pin != 0xFFFFu) {
                js_pal_gpio_write(phys_pin, out_level != 0, MCS51_DRIVE_SUPPLY);
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
    if ((pwmcon & 0x40u) != 0 || cnte == 0) {
        return UINT64_MAX; // PWMRUN=1 or CNTE=0: stopped
    }

    const uint8_t count_mode = pwmcon & 0x02u;
    const uint32_t fsys = wink_mcs51_get_clock_hz();
    uint64_t min_delta_us = UINT64_MAX;

    for (uint8_t ch = 0; ch < 4; ++ch) {
        if ((cnte & (1u << ch)) == 0) continue;

        const uint16_t period = get_channel_period(ctx, ch);
        if (period == 0) continue;

        const uint32_t f_ch = get_channel_clock_hz(ctx, ch, fsys);
        if (f_ch == 0) continue;

        const uint16_t duty = get_channel_duty(ctx, ch);
        const uint16_t cur  = priv->epwm.counter[ch];

        uint32_t ticks_to_event = 0;
        if (count_mode == 0x00u) {
            // Down-count mode:
            // If cur > duty: next transition is reaching duty (CMP match)
            // If 0 < cur <= duty: next transition is reaching 0 (zero match / reload)
            if (cur > duty) {
                ticks_to_event = cur - duty;
            } else if (cur > 0) {
                ticks_to_event = cur;
            } else {
                ticks_to_event = (period > duty) ? (period - duty + 1u) : 1u;
            }
        } else {
            // Up-down mode:
            if (priv->epwm.direction[ch] == 0) {
                // Counting UP: next is duty (if below), or period peak
                if (cur < duty) {
                    ticks_to_event = duty - cur;
                } else if (cur < period) {
                    ticks_to_event = period - cur;
                } else {
                    ticks_to_event = 1u;
                }
            } else {
                // Counting DOWN: next is duty (if above), or zero bottom
                if (cur > duty) {
                    ticks_to_event = cur - duty;
                } else if (cur > 0) {
                    ticks_to_event = cur;
                } else {
                    ticks_to_event = 1u;
                }
            }
        }

        if (ticks_to_event == 0) ticks_to_event = 1u;
        const uint64_t delta_us = (static_cast<uint64_t>(ticks_to_event) * 1000000ULL + (f_ch - 1u)) / f_ch;
        if (delta_us < min_delta_us) {
            min_delta_us = delta_us;
        }
    }

    if (min_delta_us == UINT64_MAX) return UINT64_MAX;
    return wink_mcs51_virtual_us() + (min_delta_us > 0 ? min_delta_us : 1u);
}

}  // extern "C"
