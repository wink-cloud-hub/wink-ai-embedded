// SPDX-License-Identifier: LGPL-3.0-only
// CMS8S78xx on-chip I2C master model — 0xF5 command state machine + charging
// (T1.3, PLAN-20260921-CMS8S78XX-I2C-SPI-DEADLOCK §5.3).
//
// Silicon behavior modeled against the vendor StdDriver i2c.c / demo_i2c.c
// anchors and the ADR-0086 v1.3 session contract:
//   * 0xF5 is write-I2CMCR / read-I2CMSR: the write hook decodes a command,
//     the read hook serves the model status image (the register split).
//   * Commands (vendor demo macros): START|RUN (open or repeated START with
//     address phase + one byte), RUN (one more byte), START|RUN|ACK
//     (repeated START + read byte), RUN|ACK (read byte), STOP.
//   * Address NACK enters ADDR_NACKED (ADR-0086 FSM): a session handle
//     exists, I2CMIF/ERROR/ADD_ACK latch, and data commands are skipped until
//     STOP or a repeated START re-resolves the address.
//   * Writing 0x00 to 0xF5 is I2C_ClearMasterIntFlag / I2C_EnableMasterMode:
//     it clears I2CMIF and is idempotent (never a command).
//   * STOP does NOT set I2CMIF: the vendor demo never polls after STOP, so
//     this is a deliberate behavioral-level choice (plan §5.3.2, locked by
//     CTest; upgrade condition recorded there).
//   * Charge per command = units of 9 SCL (start condition/address/data/stop),
//     SCL = 30*Tsys (I2CMTP=0) or 20*(1+I2CMTP)*Tsys, ceil to microseconds;
//     Fsys comes from the system-clock model, never a hardcoded constant.
//   * Phase 1 mocks every address/data phase as ACK and returns an injectable
//     receive byte (§5.3.4); tWR/ACK-polling belongs to the Phase 2 plugin.
//   * Scope out (§5.3.6): 9-clock recovery, slave-mode behavior (0xF2 writes
//     are tolerated, never asserted), RSTS full semantics, multi-master,
//     10-bit addressing, SMBus PEC.
#include "cms8s_i2c.h"

#include "cms8s_priv.h"
#include "cms8s_sfr_map.h"
#include "mcs51_bus_abi.h"
#include "mcs51_context.h"
#include "mcs51_family.h"
#include "mcs51_trap.h"
#include "wink_mcs51_clock.h"

#include <cstdint>

namespace {

constexpr uint8_t SFR_I2CSCR  = CMS8S_SFR_I2CSCR;  // 0xF2 write view
constexpr uint8_t SFR_I2CSSR  = CMS8S_SFR_I2CSSR;  // 0xF2 read view
constexpr uint8_t SFR_I2CMSA  = CMS8S_SFR_I2CMSA;
constexpr uint8_t SFR_I2CMCR  = CMS8S_SFR_I2CMCR;  // 0xF5 write view
constexpr uint8_t SFR_I2CMSR  = CMS8S_SFR_I2CMSR;  // 0xF5 read view
constexpr uint8_t SFR_I2CMBUF = CMS8S_SFR_I2CMBUF;
constexpr uint8_t SFR_I2CMTP  = CMS8S_SFR_I2CMTP;

// I2CMSR bits (vendor macros).
constexpr uint8_t SR_I2CMIF   = 0x80u;
constexpr uint8_t SR_BUS_BUSY = 0x40u;
constexpr uint8_t SR_IDLE     = 0x20u;
constexpr uint8_t SR_DATA_ACK = 0x08u;
constexpr uint8_t SR_ADD_ACK  = 0x04u;
constexpr uint8_t SR_ERROR    = 0x02u;
constexpr uint8_t SR_BUSY     = 0x01u;

// I2CMCR bits (vendor macros). The ACK bit (0x08) selects the master ACK for
// a received byte; Phase 1 models the bus as ideal, so it is accepted but not
// acted on (per-byte ACK/NACK is a Phase 2 session-ABI concern, ADR-0086).
constexpr uint8_t CMD_RSTS  = 0x80u;
constexpr uint8_t CMD_ACK   = 0x08u;
constexpr uint8_t CMD_STOP  = 0x04u;
constexpr uint8_t CMD_START = 0x02u;
constexpr uint8_t CMD_RUN   = 0x01u;

constexpr uint8_t I2CMSA_RS_Msk = 0x01u;

// Reset status: master idle, no flags, no pending interrupt.
constexpr uint8_t I2CMSR_RESET_VALUE = SR_IDLE;

// One charge unit = 9 SCL (8 data bits + ACK); a START/STOP condition is
// estimated at the same unit cost (plan §5.3.3).
constexpr uint32_t SCL_PER_UNIT = 9u;

// ── Phase 2 (T2.3-D): ADR-0086 session routing ─────────────────────────────
//
// Every I2CMCR command is expressed on the ADR-0086 session stream:
// START|RUN -> session_open (session_restart when a session is active),
// RUN -> session_write/read(len=1), STOP/RSTS -> session_close. ADD_ACK and
// DATA_ACK come from the pal_i2c_result_t the engine fills; an engine
// ADDR_NACKED answer maps to the register reservation exactly as the Phase 1
// mock did, and a bus-level failure latches ERROR without faking success.
//
// Host fallback: the host build has no JS bus engine. The compat library
// (mcs51_uni_bridge.cpp) answers WINK_ERR_UNSUPPORTED until a test enables
// its scriptable bus mock; only then does this model fall back to the Phase 1
// in-chip mock. Wasm NEVER falls back - an engine UNSUPPORTED answer is
// surfaced as a latched bus error, so no bus data is ever faked in
// simulation (ADR-0012).
constexpr uint8_t I2C_PORT_LOGICAL = 0u;  // first on-chip I2C bus
constexpr uint8_t SESSION_NONE = 0xFFu;

#if !defined(__EMSCRIPTEN__)
#define CMS8S_I2C_HOST_FALLBACK 1
#else
#define CMS8S_I2C_HOST_FALLBACK 0
#endif

inline Cms8sI2cState* i2c_state(Mcu51Context* ctx) {
    Cms8sPriv* priv = cms8s_priv(ctx);
    return (priv != nullptr) ? &priv->i2c : nullptr;
}

// Charges `units` 9-SCL units using the SCL formula from the vendor i2c.c
// comment; rounds up to whole microseconds.
void i2c_charge(Mcu51Context* ctx, Cms8sI2cState* i2c, uint32_t units) {
    const uint32_t scl_cycles =
        (i2c->i2cmtp == 0u) ? 30u : (20u * (1u + i2c->i2cmtp));
    const uint32_t fsys = wink_mcs51_get_clock_hz();
    uint32_t charge_us = 0u;
    if (fsys != 0u) {
        const uint64_t cycles = static_cast<uint64_t>(units) * SCL_PER_UNIT *
                                scl_cycles;
        charge_us = static_cast<uint32_t>(
            (cycles * 1000000ull + fsys - 1ull) / fsys);
        wink_mcs51_charge_us(charge_us);
    }
    i2c->last_charge_us = charge_us;
}

inline uint8_t i2c_addr7(uint8_t i2cmsa) {
    return static_cast<uint8_t>((i2cmsa >> 1) & 0x7Fu);
}

// Latches a bus-level failure without faking ACK/session success: I2CMIF
// stays set (the firmware poll exits), ERROR reports the fault, and
// ADD_ACK/DATA_ACK are left untouched (plan §5.3.2 error mapping).
void i2c_abi_fail(Cms8sI2cState* i2c) {
    ++i2c->abi_error_count;
    i2c->session_active = false;
    i2c->session_read = false;
    i2c->addr_nacked = false;
    i2c->i2cmsr = static_cast<uint8_t>(
        (i2c->i2cmsr & ~(SR_BUS_BUSY | SR_IDLE | SR_BUSY | SR_ADD_ACK |
                         SR_DATA_ACK)) |
        SR_I2CMIF | SR_ERROR);
}

// Releases the ADR-0086 handle if one is held (idempotent on the engine).
void i2c_abi_close(Cms8sI2cState* i2c) {
    if (i2c->session_id != SESSION_NONE) {
        (void)js_pal_i2c_session_close(i2c->session_id);
        i2c->session_id = SESSION_NONE;
    }
}

#if CMS8S_I2C_HOST_FALLBACK
// Phase 1 in-chip mock (host-only fallback): address phase result mapping
// shared by open/restart. NACK -> ADDR_NACKED reservation, ACK -> clear error
// and run the payload.
void i2c_exec_address_phase(Mcu51Context* ctx, Cms8sI2cState* i2c) {
    const bool read_dir = (i2c->i2cmsa & I2CMSA_RS_Msk) != 0u;
    i2c->session_active = true;
    i2c->session_read = read_dir;

    if (i2c->addr_ack_inject != 0u) {
        // ADDR_NACKED: keep the interrupt latched and never run a data phase
        // (ADR-0086 §2: only close/restart may follow).
        ++i2c->addr_nack_count;
        i2c->addr_nacked = true;
        i2c->i2cmsr = static_cast<uint8_t>(
            (i2c->i2cmsr & ~(SR_BUS_BUSY | SR_IDLE | SR_BUSY | SR_ADD_ACK |
                             SR_ERROR)) |
            SR_I2CMIF | SR_ADD_ACK | SR_ERROR);
        i2c_charge(ctx, i2c, 2u);  // START condition + address byte
        return;
    }

    i2c->addr_nacked = false;
    i2c->i2cmsr = static_cast<uint8_t>(
        (i2c->i2cmsr & ~(SR_BUS_BUSY | SR_IDLE | SR_BUSY | SR_ADD_ACK |
                         SR_ERROR | SR_DATA_ACK)) |
        SR_I2CMIF);
    if (read_dir) {
        // START|RUN|ACK / repeated START with R: receive one byte.
        i2c->i2cmbuf_rx = i2c->rx_value;
    }
    // Write direction sends I2CMBUF; the mock ACK leaves DATA_ACK clear.
    i2c_charge(ctx, i2c, 3u);  // START + address + one data byte
}

void i2c_exec_data_phase(Mcu51Context* ctx, Cms8sI2cState* i2c) {
    if (i2c->addr_nacked) {
        // ADDR_NACKED: data commands are skipped; I2CMIF stays latched and
        // DATA_ACK is never updated (ADR-0086 §2).
        ++i2c->illegal_cmd_count;
        return;
    }
    if (!i2c->session_active) {
        // RUN without an open session: never fake success (contract honesty).
        ++i2c->illegal_cmd_count;
        return;
    }
    i2c->i2cmsr = static_cast<uint8_t>(
        (i2c->i2cmsr & ~(SR_BUS_BUSY | SR_IDLE | SR_BUSY | SR_DATA_ACK)) |
        SR_I2CMIF);
    if (i2c->session_read) {
        i2c->i2cmbuf_rx = i2c->rx_value;
    }
    ++i2c->cmd_count;
    i2c_charge(ctx, i2c, 1u);  // one data byte
}
#endif  // CMS8S_I2C_HOST_FALLBACK

// ABI address phase (open or repeated START) + the payload byte. Returns
// false only when the host fallback must take over.
bool i2c_abi_address_phase(Mcu51Context* ctx, Cms8sI2cState* i2c,
                           bool repeated, uint8_t cmd) {
    const uint8_t dir = ((i2c->i2cmsa & I2CMSA_RS_Msk) != 0u) ? 1u : 0u;
    const uint8_t addr7 = i2c_addr7(i2c->i2cmsa);
    pal_i2c_result_t res = {};
    wink_status_t st;
    if (repeated && i2c->session_id != SESSION_NONE) {
        st = js_pal_i2c_session_restart(i2c->session_id, addr7, dir, &res);
    } else {
        uint8_t sid = PAL_I2C_SESSION_INVALID;
        st = js_pal_i2c_session_open(I2C_PORT_LOGICAL, addr7, dir, &sid, &res);
        if (st >= 0) {
            i2c->session_id = sid;
        }
    }
    if (st == WINK_ERR_UNSUPPORTED) {
#if CMS8S_I2C_HOST_FALLBACK
        return false;
#else
        i2c_abi_fail(i2c);
        i2c_charge(ctx, i2c, 2u);
        return true;
#endif
    }
    if (st < 0) {
        i2c_abi_fail(i2c);
        i2c_charge(ctx, i2c, 2u);
        return true;
    }
    i2c->session_active = true;
    i2c->session_read = (dir != 0u);

    if (res.addr_nack != 0u) {
        ++i2c->addr_nack_count;
        i2c->addr_nacked = true;
        i2c->i2cmsr = static_cast<uint8_t>(
            (i2c->i2cmsr & ~(SR_BUS_BUSY | SR_IDLE | SR_BUSY | SR_ADD_ACK |
                             SR_ERROR | SR_DATA_ACK)) |
            SR_I2CMIF | SR_ADD_ACK | SR_ERROR);
        i2c_charge(ctx, i2c, 2u);  // START condition + address byte
        return true;
    }
    i2c->addr_nacked = false;
    i2c->i2cmsr = static_cast<uint8_t>(
        (i2c->i2cmsr & ~(SR_BUS_BUSY | SR_IDLE | SR_BUSY | SR_ADD_ACK |
                         SR_ERROR | SR_DATA_ACK)) |
        SR_I2CMIF);

    if (i2c->session_read) {
        uint8_t rx = 0u;
        pal_i2c_result_t rres = {};
        const uint8_t ack_mode =
            ((cmd & CMD_ACK) != 0u) ? PAL_I2C_ACK_ALL : PAL_I2C_NACK_ALL;
        const wink_status_t rst = js_pal_i2c_session_read(
            i2c->session_id, &rx, 1u, ack_mode, &rres);
        if (rst >= 0) {
            i2c->i2cmbuf_rx = rx;
        } else {
            i2c_abi_fail(i2c);
        }
    } else {
        const uint8_t tx = i2c->i2cmbuf_tx;
        pal_i2c_result_t wres = {};
        const wink_status_t wst = js_pal_i2c_session_write(
            i2c->session_id, &tx, 1u, &wres);
        if (wst >= 0) {
            if ((wres.nack_bits & 1u) != 0u) {
                i2c->i2cmsr |= SR_DATA_ACK;  // slave NACKed the byte
            }
        } else {
            i2c_abi_fail(i2c);
        }
    }
    i2c_charge(ctx, i2c, 3u);  // START + address + one data byte
    return true;
}

// ABI data phase (RUN). Returns false only when the host fallback must take
// over.
bool i2c_abi_data_phase(Mcu51Context* ctx, Cms8sI2cState* i2c, uint8_t cmd) {
    if (i2c->addr_nacked) {
        // ADDR_NACKED: data commands are skipped; I2CMIF stays latched and
        // DATA_ACK is never updated (ADR-0086 §2).
        ++i2c->illegal_cmd_count;
        return true;
    }
    if (!i2c->session_active) {
        // RUN without an open session: never fake success (contract honesty).
        ++i2c->illegal_cmd_count;
        return true;
    }
    if (i2c->session_id == SESSION_NONE) {
#if CMS8S_I2C_HOST_FALLBACK
        return false;  // mock session (host fallback) owns this command
#else
        ++i2c->illegal_cmd_count;
        return true;
#endif
    }

    bool nacked = false;
    if (i2c->session_read) {
        uint8_t rx = 0u;
        pal_i2c_result_t rres = {};
        const uint8_t ack_mode =
            ((cmd & CMD_ACK) != 0u) ? PAL_I2C_ACK_ALL : PAL_I2C_NACK_ALL;
        const wink_status_t st = js_pal_i2c_session_read(
            i2c->session_id, &rx, 1u, ack_mode, &rres);
        if (st < 0) {
            i2c_abi_fail(i2c);
            return true;
        }
        i2c->i2cmbuf_rx = rx;
    } else {
        const uint8_t tx = i2c->i2cmbuf_tx;
        pal_i2c_result_t wres = {};
        const wink_status_t st = js_pal_i2c_session_write(
            i2c->session_id, &tx, 1u, &wres);
        if (st < 0) {
            i2c_abi_fail(i2c);
            return true;
        }
        nacked = (wres.nack_bits & 1u) != 0u;
    }

    i2c->i2cmsr = static_cast<uint8_t>(
        (i2c->i2cmsr & ~(SR_BUS_BUSY | SR_IDLE | SR_BUSY | SR_DATA_ACK)) |
        SR_I2CMIF);
    if (nacked) {
        i2c->i2cmsr |= SR_DATA_ACK;
    }
    ++i2c->cmd_count;
    i2c_charge(ctx, i2c, 1u);  // one data byte
    return true;
}

void i2c_exec_stop(Mcu51Context* ctx, Cms8sI2cState* i2c) {
    ++i2c->stop_count;
    i2c_abi_close(i2c);  // STOP releases the ADR-0086 session (idempotent)
    i2c->session_active = false;
    i2c->session_read = false;
    i2c->addr_nacked = false;
    i2c->i2cmsr = static_cast<uint8_t>(
        (i2c->i2cmsr & ~(SR_BUS_BUSY | SR_BUSY | SR_ADD_ACK | SR_ERROR)) |
        SR_IDLE);
    i2c_charge(ctx, i2c, 1u);  // STOP condition
}

// 0xF5 write decode. Order follows the vendor register semantics.
void i2c_handle_command(Mcu51Context* ctx, uint8_t cmd) {
    Cms8sI2cState* i2c = i2c_state(ctx);
    if (i2c == nullptr) {
        return;
    }
    if (cmd == 0u) {
        // I2C_ClearMasterIntFlag / I2C_EnableMasterMode: clear I2CMIF only.
        i2c->i2cmsr &= static_cast<uint8_t>(~SR_I2CMIF);
        return;
    }
    if ((cmd & CMD_RSTS) != 0u) {
        // Minimal software reset (scope-out §5.3.6): release the session and
        // reset the status image; never crash.
        i2c_abi_close(i2c);
        i2c->session_active = false;
        i2c->session_read = false;
        i2c->addr_nacked = false;
        i2c->i2cmsr = I2CMSR_RESET_VALUE;
        return;
    }
    if ((cmd & CMD_RUN) == 0u) {
        if ((cmd & CMD_STOP) != 0u) {
            i2c_exec_stop(ctx, i2c);
            return;
        }
        ++i2c->illegal_cmd_count;  // no RUN/STOP/RSTS: not a command
        return;
    }
    if ((cmd & CMD_START) != 0u) {
        const bool repeated = i2c->session_active;
        if (repeated) {
            ++i2c->restart_count;  // repeated START routes here, never open
        } else {
            ++i2c->start_count;
        }
        ++i2c->cmd_count;
#if CMS8S_I2C_HOST_FALLBACK
        if (!i2c_abi_address_phase(ctx, i2c, repeated, cmd)) {
            i2c_exec_address_phase(ctx, i2c);
        }
#else
        (void)i2c_abi_address_phase(ctx, i2c, repeated, cmd);
#endif
        return;
    }
#if CMS8S_I2C_HOST_FALLBACK
    if (!i2c_abi_data_phase(ctx, i2c, cmd)) {
        i2c_exec_data_phase(ctx, i2c);
    }
#else
    (void)i2c_abi_data_phase(ctx, i2c, cmd);
#endif
}

// ── Hooks (M3: C language linkage for the C-ABI hook table) ────────────────

extern "C" void on_i2cmcr_write(Mcu51Context* ctx, uint8_t addr,
                                uint8_t old_val, uint8_t new_val) {
    (void)addr;
    (void)old_val;
    if (!ctx) ctx = mcs51_get_context();
    if (!cms8s_hook_armed(ctx)) return;  // S2-1: stale hook on another family
    i2c_handle_command(ctx, new_val);
}

extern "C" void on_i2cmsr_read(Mcu51Context* ctx, uint8_t addr) {
    (void)addr;
    if (!ctx) ctx = mcs51_get_context();
    if (!cms8s_hook_armed(ctx)) return;
    Cms8sI2cState* i2c = i2c_state(ctx);
    if (i2c != nullptr) {
        ctx->sfr_shadow[SFR_I2CMSR] = i2c->i2cmsr;
    }
}

extern "C" void on_i2cmsa_write(Mcu51Context* ctx, uint8_t addr,
                                uint8_t old_val, uint8_t new_val) {
    (void)addr;
    (void)old_val;
    if (!ctx) ctx = mcs51_get_context();
    if (!cms8s_hook_armed(ctx)) return;
    Cms8sI2cState* i2c = i2c_state(ctx);
    if (i2c != nullptr) {
        i2c->i2cmsa = new_val;
    }
}

extern "C" void on_i2cmsa_read(Mcu51Context* ctx, uint8_t addr) {
    (void)addr;
    if (!ctx) ctx = mcs51_get_context();
    if (!cms8s_hook_armed(ctx)) return;
    Cms8sI2cState* i2c = i2c_state(ctx);
    if (i2c != nullptr) {
        ctx->sfr_shadow[SFR_I2CMSA] = i2c->i2cmsa;
    }
}

extern "C" void on_i2cmbuf_write(Mcu51Context* ctx, uint8_t addr,
                                 uint8_t old_val, uint8_t new_val) {
    (void)addr;
    (void)old_val;
    if (!ctx) ctx = mcs51_get_context();
    if (!cms8s_hook_armed(ctx)) return;
    Cms8sI2cState* i2c = i2c_state(ctx);
    if (i2c != nullptr) {
        i2c->i2cmbuf_tx = new_val;
    }
}

extern "C" void on_i2cmbuf_read(Mcu51Context* ctx, uint8_t addr) {
    (void)addr;
    if (!ctx) ctx = mcs51_get_context();
    if (!cms8s_hook_armed(ctx)) return;
    Cms8sI2cState* i2c = i2c_state(ctx);
    if (i2c != nullptr) {
        ctx->sfr_shadow[SFR_I2CMBUF] = i2c->i2cmbuf_rx;
    }
}

extern "C" void on_i2cmtp_write(Mcu51Context* ctx, uint8_t addr,
                                uint8_t old_val, uint8_t new_val) {
    (void)addr;
    (void)old_val;
    if (!ctx) ctx = mcs51_get_context();
    if (!cms8s_hook_armed(ctx)) return;
    Cms8sI2cState* i2c = i2c_state(ctx);
    if (i2c != nullptr) {
        i2c->i2cmtp = new_val;
    }
}

extern "C" void on_i2cmtp_read(Mcu51Context* ctx, uint8_t addr) {
    (void)addr;
    if (!ctx) ctx = mcs51_get_context();
    if (!cms8s_hook_armed(ctx)) return;
    Cms8sI2cState* i2c = i2c_state(ctx);
    if (i2c != nullptr) {
        ctx->sfr_shadow[SFR_I2CMTP] = i2c->i2cmtp;
    }
}

// 0xF2 slave block: writes are tolerated (never asserted, plan §5.1 note 4);
// reads serve the slave status image (Phase 1: 0, slave mode is scope-out).
extern "C" void on_i2cscr_write(Mcu51Context* ctx, uint8_t addr,
                                uint8_t old_val, uint8_t new_val) {
    (void)addr;
    (void)old_val;
    if (!ctx) ctx = mcs51_get_context();
    if (!cms8s_hook_armed(ctx)) return;
    Cms8sI2cState* i2c = i2c_state(ctx);
    if (i2c != nullptr) {
        i2c->i2cscr = new_val;
    }
}

extern "C" void on_i2cssr_read(Mcu51Context* ctx, uint8_t addr) {
    (void)addr;
    if (!ctx) ctx = mcs51_get_context();
    if (!cms8s_hook_armed(ctx)) return;
    Cms8sI2cState* i2c = i2c_state(ctx);
    if (i2c != nullptr) {
        ctx->sfr_shadow[SFR_I2CSSR] = i2c->i2cssr;
    }
}

}  // namespace

extern "C" {

void cms8s_i2c_reset(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    if (!ctx) return;
    if (ctx->family != MCS51_FAMILY_CMS8S78XX) {
        return;  // belt-and-braces: the registry mask already filters
    }
    cms8s_soc_bind(ctx);  // defensive: standalone resets bind too
    Cms8sI2cState* i2c = i2c_state(ctx);
    if (i2c != nullptr) {
        i2c_abi_close(i2c);  // release any ADR-0086 session before the wipe
        Cms8sI2cState fresh = {};
        fresh.i2cmsr = I2CMSR_RESET_VALUE;
        fresh.session_id = SESSION_NONE;  // no active session after reset
        fresh.rx_value = 0x00u;       // Phase 1 mock receive byte
        fresh.addr_ack_inject = 0u;   // Phase 1 mock: all addresses ACK
        *i2c = fresh;
    }
    ctx->sfr_shadow[SFR_I2CMSA] = 0x00u;
    ctx->sfr_shadow[SFR_I2CMBUF] = 0x00u;
    ctx->sfr_shadow[SFR_I2CMTP] = 0x00u;
    ctx->sfr_shadow[SFR_I2CMCR] = I2CMSR_RESET_VALUE;  // 0xF5 read view
    ctx->sfr_shadow[SFR_I2CSCR] = 0x00u;               // 0xF2 read view
    // S4-H2: reset rebuilds the registration the context reset just wiped.
    mcs51_trap_register_sfr_write(SFR_I2CMCR, on_i2cmcr_write);
    mcs51_trap_register_sfr_read(SFR_I2CMSR, on_i2cmsr_read);
    mcs51_trap_register_sfr_write(SFR_I2CMSA, on_i2cmsa_write);
    mcs51_trap_register_sfr_read(SFR_I2CMSA, on_i2cmsa_read);
    mcs51_trap_register_sfr_write(SFR_I2CMBUF, on_i2cmbuf_write);
    mcs51_trap_register_sfr_read(SFR_I2CMBUF, on_i2cmbuf_read);
    mcs51_trap_register_sfr_write(SFR_I2CMTP, on_i2cmtp_write);
    mcs51_trap_register_sfr_read(SFR_I2CMTP, on_i2cmtp_read);
    mcs51_trap_register_sfr_write(SFR_I2CSCR, on_i2cscr_write);
    mcs51_trap_register_sfr_read(SFR_I2CSSR, on_i2cssr_read);
}

void cms8s_i2c_init(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    if (!ctx) return;
    if (ctx->family != MCS51_FAMILY_CMS8S78XX) {
        return;
    }
    cms8s_soc_bind(ctx);  // bind BEFORE any pool deref (ordering invariant)
    cms8s_i2c_reset(ctx);
}

uint32_t cms8s_i2c_cmd_count(void) {
    Cms8sI2cState* i2c = i2c_state(nullptr);
    return (i2c != nullptr) ? i2c->cmd_count : 0u;
}

uint32_t cms8s_i2c_start_count(void) {
    Cms8sI2cState* i2c = i2c_state(nullptr);
    return (i2c != nullptr) ? i2c->start_count : 0u;
}

uint32_t cms8s_i2c_restart_count(void) {
    Cms8sI2cState* i2c = i2c_state(nullptr);
    return (i2c != nullptr) ? i2c->restart_count : 0u;
}

uint32_t cms8s_i2c_stop_count(void) {
    Cms8sI2cState* i2c = i2c_state(nullptr);
    return (i2c != nullptr) ? i2c->stop_count : 0u;
}

uint32_t cms8s_i2c_addr_nack_count(void) {
    Cms8sI2cState* i2c = i2c_state(nullptr);
    return (i2c != nullptr) ? i2c->addr_nack_count : 0u;
}

uint32_t cms8s_i2c_illegal_cmd_count(void) {
    Cms8sI2cState* i2c = i2c_state(nullptr);
    return (i2c != nullptr) ? i2c->illegal_cmd_count : 0u;
}

uint32_t cms8s_i2c_last_charge_us(void) {
    Cms8sI2cState* i2c = i2c_state(nullptr);
    return (i2c != nullptr) ? i2c->last_charge_us : 0u;
}

uint32_t cms8s_i2c_abi_error_count(void) {
    Cms8sI2cState* i2c = i2c_state(nullptr);
    return (i2c != nullptr) ? i2c->abi_error_count : 0u;
}

void cms8s_i2c_set_mock_addr_ack(bool ack) {
    Cms8sI2cState* i2c = i2c_state(nullptr);
    if (i2c != nullptr) {
        i2c->addr_ack_inject = ack ? 0u : 1u;
    }
}

void cms8s_i2c_set_mock_rx_value(uint8_t value) {
    Cms8sI2cState* i2c = i2c_state(nullptr);
    if (i2c != nullptr) {
        i2c->rx_value = value;
    }
}

uint8_t cms8s_i2c_mock_rx_value(void) {
    Cms8sI2cState* i2c = i2c_state(nullptr);
    return (i2c != nullptr) ? i2c->rx_value : 0x00u;
}

}  // extern "C"
