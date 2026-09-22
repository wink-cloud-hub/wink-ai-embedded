// SPDX-License-Identifier: LGPL-3.0-only
// CMS8S78xx on-chip SPI master model — synchronous completion + two-step
// read-clear (T1.2, PLAN-20260921-CMS8S78XX-I2C-SPI-DEADLOCK §5.2).
//
// Silicon behavior modeled against the vendor StdDriver spi.c anchor:
//   * SPDR write starts the transfer and, in this sandbox, completes it
//     synchronously: SPISIF auto-asserts inside the write (same paradigm as
//     the UART TI auto-assert, ADR-0081), so `while(!SPI_GetTransferIntFlag())`
//     exits on its first poll and the firmware never spins forever (§2.2).
//   * Standard read-clear sequence is read SPSR (arm) then read SPDR (clear):
//     reading SPSR alone latches the pending clear but leaves SPISIF set, so
//     the vendor SPI_ClearTransferIntFlag() (`temp = SPSR; temp = SPDR;`)
//     clears it and SPI_Transmit's `return SPDR` ends the transaction (E-04).
//   * SPISIF/WCOL are hardware-owned: software writes to SPSR only land in
//     SSCEN. WCOL sets when SPDR is overwritten before SPISIF was consumed.
//   * Per-byte charge = ceil(8 / Fspi) with Fspi = Fsys / SPIClkDiv
//     (SPCR.SPR2:SPRn); Fsys comes from the system-clock model, never a
//     hardcoded constant (§5.2.5).
//   * Phase 1 is protocol-agnostic: every transfer returns the injected mock
//     byte (0xFF = MISO idle high). M95256 command semantics belong to the
//     Phase 2 UniSim plugin; SSCR.NSSO1 edges are recorded for CTest only
//     until the SPI line-level session ADR-0087 lands (§5.2.3/§5.2.4).
#include "cms8s_spi.h"

#include "cms8s_priv.h"
#include "cms8s_sfr_map.h"
#include "mcs51_context.h"
#include "mcs51_family.h"
#include "mcs51_trap.h"
#include "wink_mcs51_clock.h"
#include "wink_mcs51_isr.h"

#include <cstdint>

namespace {

constexpr uint8_t SFR_SPCR = CMS8S_SFR_SPCR;
constexpr uint8_t SFR_SPSR = CMS8S_SFR_SPSR;
constexpr uint8_t SFR_SPDR = CMS8S_SFR_SPDR;
constexpr uint8_t SFR_SSCR = CMS8S_SFR_SSCR;

constexpr uint8_t SPCR_SPR2_Pos    = 5u;
constexpr uint8_t SPCR_SPR2_Msk    = 0x20u;
constexpr uint8_t SPCR_SPRn_Msk    = 0x03u;

constexpr uint8_t SPSR_SPISIF_Msk  = 0x80u;
constexpr uint8_t SPSR_WCOL_Msk    = 0x40u;
constexpr uint8_t SPSR_SSCEN_Msk   = 0x01u;

constexpr uint8_t SSCR_NSSO1_Msk   = 0x02u;
// SSCR power-on reset value: NSSx deasserted ("unselected"), assumption
// recorded in the plan §5.1 note 3 and locked by the CTest reset case.
constexpr uint8_t SSCR_RESET_VALUE = 0x02u;

constexpr uint8_t EIE2_SPIIE_Msk   = 0x80u;
constexpr uint8_t EIF2_SPIIF_Msk   = 0x80u;

// SPCR SPR2:SPRn -> Fsys divider (vendor StdDriver spi.h SPI_CLK_DIV_4..512).
// Index = (SPR2 << 2) | SPRn.
constexpr uint32_t SPI_DIV_TABLE[8] = {
    4u, 8u, 16u, 32u, 64u, 128u, 256u, 512u
};

inline Cms8sSpiState* spi_state(Mcu51Context* ctx) {
    Cms8sPriv* priv = cms8s_priv(ctx);
    return (priv != nullptr) ? &priv->spi : nullptr;
}

// Completes one byte synchronously: charge, mock RX load, SPISIF auto-assert
// (mirrored into sfr_shadow so the very next read sees it), optional vector
// 22 for the interrupt-driven path.
void spi_complete_transfer(Mcu51Context* ctx, uint8_t tx) {
    Cms8sSpiState* spi = spi_state(ctx);
    if (spi == nullptr) {
        return;
    }

    const uint8_t div_idx = static_cast<uint8_t>(
        (((spi->spcr & SPCR_SPR2_Msk) != 0u) ? 0x04u : 0x00u) |
        (spi->spcr & SPCR_SPRn_Msk));
    const uint32_t div = SPI_DIV_TABLE[div_idx];
    const uint32_t fsys = wink_mcs51_get_clock_hz();
    uint32_t byte_us = 0u;
    if (fsys != 0u) {
        const uint32_t fspi = fsys / div;
        if (fspi != 0u) {
            byte_us = (8u * 1000000u + fspi - 1u) / fspi;
            wink_mcs51_charge_us(byte_us);
        }
    }
    spi->last_charge_us = byte_us;

    if ((spi->spsr & SPSR_SPISIF_Msk) != 0u) {
        spi->spsr |= SPSR_WCOL_Msk;  // overwrite before the flag was consumed
    }
    spi->tx_last = tx;
    spi->spdr_rx = spi->rx_value;
    spi->spsr |= SPSR_SPISIF_Msk;
    ++spi->transfer_count;

    ctx->sfr_shadow[SFR_SPSR] = spi->spsr;  // synchronous auto-assert

    if ((ctx->sfr_shadow[CMS8S_SFR_EIE2] & EIE2_SPIIE_Msk) != 0u) {
        ctx->sfr_shadow[CMS8S_SFR_EIF2] =
            static_cast<uint8_t>(ctx->sfr_shadow[CMS8S_SFR_EIF2] |
                                 EIF2_SPIIF_Msk);
        mcs51_raise_irq(IRQ_SOURCE_SPI);
    }
}

// ── Hooks (M3: C language linkage for the C-ABI hook table) ────────────────

extern "C" void on_spcr_write(Mcu51Context* ctx, uint8_t addr,
                              uint8_t old_val, uint8_t new_val) {
    (void)addr;
    (void)old_val;
    if (!ctx) ctx = mcs51_get_context();
    if (!cms8s_hook_armed(ctx)) return;  // S2-1: stale hook on another family
    Cms8sSpiState* spi = spi_state(ctx);
    if (spi == nullptr) return;
    spi->spcr = new_val;
    ctx->sfr_shadow[SFR_SPCR] = new_val;  // no read-only bits on SPCR
}

extern "C" void on_spcr_read(Mcu51Context* ctx, uint8_t addr) {
    (void)addr;
    if (!ctx) ctx = mcs51_get_context();
    if (!cms8s_hook_armed(ctx)) return;
    Cms8sSpiState* spi = spi_state(ctx);
    if (spi != nullptr) {
        ctx->sfr_shadow[SFR_SPCR] = spi->spcr;
    }
}

extern "C" void on_spsr_write(Mcu51Context* ctx, uint8_t addr,
                              uint8_t old_val, uint8_t new_val) {
    (void)addr;
    (void)old_val;
    if (!ctx) ctx = mcs51_get_context();
    if (!cms8s_hook_armed(ctx)) return;
    Cms8sSpiState* spi = spi_state(ctx);
    if (spi == nullptr) return;
    // SPISIF/WCOL are hardware-owned; only SSCEN follows the software write.
    spi->spsr = static_cast<uint8_t>((spi->spsr & ~SPSR_SSCEN_Msk) |
                                     (new_val & SPSR_SSCEN_Msk));
    ctx->sfr_shadow[SFR_SPSR] = spi->spsr;
}

extern "C" void on_spsr_read(Mcu51Context* ctx, uint8_t addr) {
    (void)addr;
    if (!ctx) ctx = mcs51_get_context();
    if (!cms8s_hook_armed(ctx)) return;
    Cms8sSpiState* spi = spi_state(ctx);
    if (spi == nullptr) return;
    if ((spi->spsr & (SPSR_SPISIF_Msk | SPSR_WCOL_Msk)) != 0u) {
        spi->spsr_read_latched = true;  // arm the SPDR-side clear
    }
    ctx->sfr_shadow[SFR_SPSR] = spi->spsr;
}

extern "C" void on_spdr_write(Mcu51Context* ctx, uint8_t addr,
                              uint8_t old_val, uint8_t new_val) {
    (void)addr;
    (void)old_val;
    if (!ctx) ctx = mcs51_get_context();
    if (!cms8s_hook_armed(ctx)) return;
    spi_complete_transfer(ctx, new_val);
}

extern "C" void on_spdr_read(Mcu51Context* ctx, uint8_t addr) {
    (void)addr;
    if (!ctx) ctx = mcs51_get_context();
    if (!cms8s_hook_armed(ctx)) return;
    Cms8sSpiState* spi = spi_state(ctx);
    if (spi == nullptr) return;
    if (spi->spsr_read_latched) {
        // Vendor read-clear pair: SPSR read then SPDR read clears both the
        // transfer flag and the write-collision flag.
        spi->spsr &= static_cast<uint8_t>(~(SPSR_SPISIF_Msk | SPSR_WCOL_Msk));
        spi->spsr_read_latched = false;
    }
    ctx->sfr_shadow[SFR_SPDR] = spi->spdr_rx;
}

extern "C" void on_sscr_write(Mcu51Context* ctx, uint8_t addr,
                              uint8_t old_val, uint8_t new_val) {
    (void)addr;
    (void)old_val;
    if (!ctx) ctx = mcs51_get_context();
    if (!cms8s_hook_armed(ctx)) return;
    Cms8sSpiState* spi = spi_state(ctx);
    if (spi == nullptr) return;
    const bool was_deasserted = (spi->sscr & SSCR_NSSO1_Msk) != 0u;
    const bool now_deasserted = (new_val & SSCR_NSSO1_Msk) != 0u;
    if (was_deasserted && !now_deasserted) {
        ++spi->frame_start_count;  // NSSx 1 -> 0: frame begins
    } else if (!was_deasserted && now_deasserted) {
        ++spi->frame_end_count;    // NSSx 0 -> 1: frame ends
    }
    spi->sscr = new_val;
    ctx->sfr_shadow[SFR_SSCR] = new_val;
}

extern "C" void on_sscr_read(Mcu51Context* ctx, uint8_t addr) {
    (void)addr;
    if (!ctx) ctx = mcs51_get_context();
    if (!cms8s_hook_armed(ctx)) return;
    Cms8sSpiState* spi = spi_state(ctx);
    if (spi != nullptr) {
        ctx->sfr_shadow[SFR_SSCR] = spi->sscr;
    }
}

}  // namespace

extern "C" {

void cms8s_spi_reset(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    if (!ctx) return;
    if (ctx->family != MCS51_FAMILY_CMS8S78XX) {
        return;  // belt-and-braces: the registry mask already filters
    }
    cms8s_soc_bind(ctx);  // defensive: standalone resets bind too
    Cms8sSpiState* spi = spi_state(ctx);
    if (spi != nullptr) {
        Cms8sSpiState fresh = {};
        fresh.spdr_rx = 0xFFu;  // MISO idle high
        fresh.rx_value = 0xFFu;
        fresh.sscr = SSCR_RESET_VALUE;
        *spi = fresh;
    }
    ctx->sfr_shadow[SFR_SPCR] = 0x00u;
    ctx->sfr_shadow[SFR_SPSR] = 0x00u;
    ctx->sfr_shadow[SFR_SPDR] = 0x00u;
    ctx->sfr_shadow[SFR_SSCR] = SSCR_RESET_VALUE;
    // S4-H2: reset rebuilds the registration the context reset just wiped.
    mcs51_trap_register_sfr_write(SFR_SPCR, on_spcr_write);
    mcs51_trap_register_sfr_read(SFR_SPCR, on_spcr_read);
    mcs51_trap_register_sfr_write(SFR_SPSR, on_spsr_write);
    mcs51_trap_register_sfr_read(SFR_SPSR, on_spsr_read);
    mcs51_trap_register_sfr_write(SFR_SPDR, on_spdr_write);
    mcs51_trap_register_sfr_read(SFR_SPDR, on_spdr_read);
    mcs51_trap_register_sfr_write(SFR_SSCR, on_sscr_write);
    mcs51_trap_register_sfr_read(SFR_SSCR, on_sscr_read);
}

void cms8s_spi_init(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    if (!ctx) return;
    if (ctx->family != MCS51_FAMILY_CMS8S78XX) {
        return;
    }
    cms8s_soc_bind(ctx);  // bind BEFORE any pool deref (ordering invariant)
    cms8s_spi_reset(ctx);
}

uint32_t cms8s_spi_transfer_count(void) {
    Cms8sSpiState* spi = spi_state(nullptr);
    return (spi != nullptr) ? spi->transfer_count : 0u;
}

uint32_t cms8s_spi_frame_start_count(void) {
    Cms8sSpiState* spi = spi_state(nullptr);
    return (spi != nullptr) ? spi->frame_start_count : 0u;
}

uint32_t cms8s_spi_frame_end_count(void) {
    Cms8sSpiState* spi = spi_state(nullptr);
    return (spi != nullptr) ? spi->frame_end_count : 0u;
}

uint8_t cms8s_spi_last_tx(void) {
    Cms8sSpiState* spi = spi_state(nullptr);
    return (spi != nullptr) ? spi->tx_last : 0x00u;
}

uint32_t cms8s_spi_last_charge_us(void) {
    Cms8sSpiState* spi = spi_state(nullptr);
    return (spi != nullptr) ? spi->last_charge_us : 0u;
}

void cms8s_spi_set_rx_value(uint8_t value) {
    Cms8sSpiState* spi = spi_state(nullptr);
    if (spi != nullptr) {
        spi->rx_value = value;
    }
}

uint8_t cms8s_spi_rx_value(void) {
    Cms8sSpiState* spi = spi_state(nullptr);
    return (spi != nullptr) ? spi->rx_value : 0xFFu;
}

}  // extern "C"
