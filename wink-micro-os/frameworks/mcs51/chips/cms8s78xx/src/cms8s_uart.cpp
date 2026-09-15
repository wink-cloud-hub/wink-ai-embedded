// SPDX-License-Identifier: GPL-3.0-only
// CMS8S78xx UART source-selection model (Stage4 S4-2 Step 1,
// PLAN-20260911-MCS51-S4, CPL-04).
//
// Standard 8051 serial runs on fixed pins (TXD=P3.1, RXD=P3.0) off Timer1.
// This silicon adds a clock-select register (FUNCCR: Timer1/TMR4/TMR2/BRT),
// pin-share muxes (PxxCFG) and an RXD input selector (PS_RXD). The model
// serves the generic TX engine through the per-context uart_hooks (S4-D3);
// the core fast-paths standard parts on caps_cache. Moved verbatim from the
// generic UART TU (same formulas, same predicates).
#include "mcs51_context.h"
#include "mcs51_family.h"
#include "mcs51_trap.h"
#include "mcs51_sfr_map.h"
#include "cms8s_priv.h"
#include "cms8s_sfr_map.h"
#include "wink_mcs51_clock.h"
#include "wink_mcs51_uart.h"

#include <stdint.h>

namespace {

constexpr uint8_t SFR_SCON = 0x98;
constexpr uint8_t SFR_PCON = 0x87;
constexpr uint8_t SFR_TH1  = 0x8D;
constexpr uint8_t SFR_TMOD = 0x89;
constexpr uint8_t SFR_TCON = 0x88;
constexpr uint8_t SFR_TH4  = 0xE3;
constexpr uint8_t SFR_RLDL = 0xCA;
constexpr uint8_t SFR_RLDH = 0xCB;
constexpr uint8_t SFR_FUNCCR = 0x91;  // UART0 clock source select
// M5: shared addresses alias the single source.
constexpr uint8_t SFR_T2CON  = MCS51_SFR_T2CON;
constexpr uint8_t SFR_CKCON  = MCS51_SFR_CKCON;
constexpr uint8_t SFR_T34MOD = CMS8S_SFR_T34MOD;

// XSFR (MOVX window, read via xdata_shadow): pin mux + BRT + RXD selector.
constexpr uint16_t XSFR_P13CFG  = 0xF013u;
constexpr uint16_t XSFR_P21CFG  = 0xF021u;
constexpr uint16_t XSFR_BRT_CON = 0xF5C0u;
constexpr uint16_t XSFR_BRTDL   = 0xF5C1u;
constexpr uint16_t XSFR_BRTDH   = 0xF5C2u;
constexpr uint16_t XSFR_PS_RXD  = 0xF69Fu;

constexpr uint8_t SCON_SM1 = 6u;  // SCON.6: async modes 1/3 iff set
constexpr uint8_t SCON_REN = 4u;  // SCON.4 receive enable
constexpr uint8_t PCON_SMOD0 = 7u;  // PCON.7: baud doubler
constexpr uint8_t CKCON_T1M = 4u;   // CKCON.4: Timer1 1T select
constexpr uint8_t T34MOD_T4M = 6u;  // T34MOD.6: Timer4 1T select
constexpr uint8_t T2CON_T2PS = 7u;  // T2CON.7: Timer2 prescale select
constexpr uint8_t TCON_TR1 = 6u;  // TCON.6 Timer1 run control
constexpr uint8_t T34MOD_TR4 = 7u;

// FUNCCR UART0_CKS values (StdDriver uart.h: UART_BAUD_TMR1/TMR4/TMR2/BRT).
// 4..7 are reserved on silicon.
constexpr uint8_t CKS_TMR1 = 0u;
constexpr uint8_t CKS_TMR4 = 1u;
constexpr uint8_t CKS_TMR2 = 2u;
constexpr uint8_t CKS_BRT  = 3u;

// Timer1 as UART baud source: TR1 running + TMOD mode 2 (8-bit auto-reload).
// The reload VALUE (TH1) only affects the rate, not readiness, so it is
// deliberately not validated here (rate accuracy is out of scope).
inline bool timer1_baud_ready(const Mcu51Context* ctx) {
    const uint8_t tcon = ctx->sfr_shadow[SFR_TCON];
    const uint8_t tmod = ctx->sfr_shadow[SFR_TMOD];
    return ((tcon >> TCON_TR1) & 1u) != 0 &&
           ((tmod >> 4u) & 0x03u) == 0x02u;
}

// M3: C language linkage for the C-ABI hook table.
extern "C" uint32_t cms8s_uart_notready_mask(struct Mcu51Context* ctx) {
    if (!ctx) {
        ctx = mcs51_get_context();
    }
    if (!ctx) {
        return 0u;
    }
    uint32_t mask = 0u;
    const uint8_t scon = ctx->sfr_shadow[SFR_SCON];
    if (((scon >> SCON_SM1) & 1u) == 0) {
        // Modes 0/2 (shift register / 9-bit sync): no async framing.
        mask |= WINK_MCS51_UART_NOTREADY_MODE;
    }

    bool baud_ok = false;
    switch (ctx->sfr_shadow[SFR_FUNCCR] & 0x07u) {
        case CKS_TMR1:
            baud_ok = timer1_baud_ready(ctx);
            break;
        case CKS_TMR4:
            // T34MOD.TR4 run bit (same definition as the timer model).
            baud_ok = ((ctx->sfr_shadow[SFR_T34MOD] >> T34MOD_TR4) & 1u) != 0;
            break;
        case CKS_TMR2:
            // T2CON T2I interval != 0 means running (same definition as
            // the T2 model: the timer T2CON write hook).
            baud_ok = (ctx->sfr_shadow[SFR_T2CON] & 0x03u) != 0;
            break;
        case CKS_BRT:
            baud_ok = ((ctx->xdata_shadow[XSFR_BRT_CON] >> 7u) & 1u) != 0;
            break;
        default:
            // Reserved CKS 4..7: no defined baud source on silicon.
            // Never pretend ready (GAP-02 review note).
            baud_ok = false;
            break;
    }
    if (!baud_ok) {
        mask |= WINK_MCS51_UART_NOTREADY_BAUD;
    }

    // TXD: P3.1 is the hardwired default (no CFG needed); P1.4/P2.2 are
    // additive alternates (mux 0x03 each). Silicon drives P3.1
    // unconditionally, so at functional level TXD cannot be disconnected —
    // no TXD bit is ever set (the WINK_MCS51_UART_NOTREADY_TXD reason stays
    // reserved for parts that can disable the pin).

    if (((scon >> SCON_REN) & 1u) != 0) {
        // RXD input selector: only an explicit alt selection can break
        // the link (selector points at a pin whose mux is not RXD).
        // Any other PS_RXD value falls back to the hardwired default
        // P3.0 — this deliberately does NOT depend on the PS_RXD reset
        // value (unseeded in the model), so a future reset-seed addition
        // cannot change verdicts.
        const uint8_t ps = ctx->xdata_shadow[XSFR_PS_RXD];
        if (ps == 0x13u) {  // P1.3
            if (ctx->xdata_shadow[XSFR_P13CFG] != 0x03u) {
                mask |= WINK_MCS51_UART_NOTREADY_RXD;
            }
        } else if (ps == 0x21u) {  // P2.1
            if (ctx->xdata_shadow[XSFR_P21CFG] != 0x03u) {
                mask |= WINK_MCS51_UART_NOTREADY_RXD;
            }
        }
    }
    return mask;
}

extern "C" uint32_t cms8s_uart_baud_hz(struct Mcu51Context* ctx) {
    if (!ctx) {
        ctx = mcs51_get_context();
    }
    if (!ctx) {
        return 0u;
    }
    const uint32_t fsys = wink_mcs51_get_clock_hz();
    if (fsys == 0u) {
        return 0u;
    }
    const uint32_t smod =
        (((ctx->sfr_shadow[SFR_PCON] >> PCON_SMOD0) & 1u) != 0u) ? 2u : 1u;
    const uint8_t cks =
        static_cast<uint8_t>(ctx->sfr_shadow[SFR_FUNCCR] & 0x07u);
    switch (cks) {
        case CKS_TMR1: {
            // A-01 guarantees TMOD mode 2 + TR1.
            uint32_t t = 3u;
            if ((((ctx->sfr_shadow[SFR_CKCON] >> CKCON_T1M) & 1u) != 0u)) {
                t = 1u;
            }
            const uint32_t n = 256u - ctx->sfr_shadow[SFR_TH1];
            if (n == 0u) {
                return 0u;
            }
            return (fsys * smod) / (128u * t * n);  // 32*K, K=4
        }
        case CKS_TMR4: {
            // Assumes 8-bit auto-reload (T4 mode 2).
            if (((ctx->sfr_shadow[SFR_T34MOD] >> 4u) & 0x03u) != 0x02u) {
                return 0u;
            }
            const uint32_t t =
                (((ctx->sfr_shadow[SFR_T34MOD] >> T34MOD_T4M) & 1u) != 0u)
                    ? 1u
                    : 3u;
            const uint32_t n = 256u - ctx->sfr_shadow[SFR_TH4];
            if (n == 0u) {
                return 0u;
            }
            return (fsys * smod) / (128u * t * n);
        }
        case CKS_TMR2: {
            const uint32_t t =
                (((ctx->sfr_shadow[SFR_T2CON] >> T2CON_T2PS) & 1u) != 0u)
                    ? 2u
                    : 1u;
            const uint32_t reload =
                (static_cast<uint32_t>(ctx->sfr_shadow[SFR_RLDH]) << 8) |
                ctx->sfr_shadow[SFR_RLDL];
            const uint32_t n = 65536u - reload;
            if (n == 0u) {
                return 0u;
            }
            return (fsys * smod) / (384u * t * n);  // 32*K, K=12
        }
        case CKS_BRT: {
            const uint32_t div =
                1u << (ctx->xdata_shadow[XSFR_BRT_CON] & 0x07u);
            const uint32_t reload =
                (static_cast<uint32_t>(ctx->xdata_shadow[XSFR_BRTDH]) << 8) |
                ctx->xdata_shadow[XSFR_BRTDL];
            const uint32_t n = 65536u - reload;
            if (n == 0u) {
                return 0u;
            }
            return (fsys * smod) / (32u * div * n);
        }
        default:
            return 0u;
    }
}

void install_hooks(struct Mcu51Context* ctx) {
    ctx->uart_hooks.notready_mask = cms8s_uart_notready_mask;
    ctx->uart_hooks.baud_hz = cms8s_uart_baud_hz;
}

}  // namespace

extern "C" {

void cms8s_uart_init(struct Mcu51Context* ctx) {
    if (!ctx) {
        ctx = mcs51_get_context();
    }
    if (!ctx) {
        return;
    }
    if (ctx->family != MCS51_FAMILY_CMS8S78XX) {
        return;  // belt-and-braces: the registry mask already filters
    }
    cms8s_soc_bind(ctx);
    install_hooks(ctx);
}

void cms8s_uart_reset(struct Mcu51Context* ctx) {
    if (!ctx) {
        ctx = mcs51_get_context();
    }
    if (!ctx) {
        return;
    }
    if (ctx->family != MCS51_FAMILY_CMS8S78XX) {
        return;
    }
    cms8s_soc_bind(ctx);  // defensive: standalone resets bind too
    install_hooks(ctx);  // reset memset cleared the table: reinstall
}

}  // extern "C"
