// SPDX-License-Identifier: GPL-3.0-only
// T1.3: CMS8S78xx on-chip I2C master model unit tests
// (PLAN-20260921-CMS8S78XX-I2C-SPI-DEADLOCK §5.3).
//
// Locks the Phase 1 contract:
//   * 0xF5 write/read split: writes decode I2CMCR commands, reads serve
//     I2CMSR; writing 0x00 clears I2CMIF (vendor I2C_ClearMasterIntFlag /
//     I2C_EnableMasterMode) and is idempotent;
//   * vendor demo command sequence (START|RUN / RUN / STOP) completes with
//     I2CMIF auto-set, ADD_ACK/DATA_ACK backfilled and the poll exiting on
//     the first read (no simulation deadlock);
//   * repeated START routes to restart (never a second open);
//   * address NACK enters ADDR_NACKED: I2CMIF/ERROR/ADD_ACK latch, data
//     commands are skipped, STOP/restart recover;
//   * STOP does not set I2CMIF (documented behavioral-level choice);
//   * mock receive byte injection + 0xF2 slave-write tolerance;
//   * SCL charging: SCL = 30*Tsys (I2CMTP=0) or 20*(1+I2CMTP)*Tsys, per
//     command unit count (start/address/data/stop = 9 SCL each), rounded up;
//   * RSTS minimal release and the host channel-2 I2C fallback link contract.
#include <stdint.h>
#include <stdio.h>

#include "cms8s78xx.h"
#include "cms8s_i2c.h"
#include "mcs51_context.h"
#include "mcs51_test_harness.h"
#include "wink_mcs51_clock.h"

#undef main
#undef printf
extern "C" void wink_mcs51_user_main(void) {}
extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

// Host-side channel-2 I2C fallback (Phase 2 prerequisite): must link and
// fail closed on host (no JS bus data plane).
extern "C" bool js_pal_i2c_transfer(uint8_t port, uint16_t dev_addr,
                                    const uint8_t* write_buf,
                                    uint32_t write_len, uint8_t* read_buf,
                                    uint32_t read_len);

namespace {

int g_fails = 0;

void check(bool cond, const char* msg) {
    if (!cond) {
        printf("[cms8s_i2c] FAIL: %s\n", msg);
        ++g_fails;
    }
}

constexpr uint8_t I2CMIF   = 0x80u;
constexpr uint8_t BUS_BUSY = 0x40u;
constexpr uint8_t IDLE     = 0x20u;
constexpr uint8_t DATA_ACK = 0x08u;
constexpr uint8_t ADD_ACK  = 0x04u;
constexpr uint8_t ERROR    = 0x02u;
constexpr uint8_t BUSY     = 0x01u;

// Vendor demo_i2c.c command macros.
constexpr uint8_t CMD_START_SEND  = 0x02u | 0x01u;         // START|RUN
constexpr uint8_t CMD_SEND        = 0x01u;                  // RUN
constexpr uint8_t CMD_STOP        = 0x04u;                  // STOP
constexpr uint8_t CMD_START_RX_ACK = 0x02u | 0x01u | 0x08u; // START|RUN|ACK
constexpr uint8_t CMD_RECEIVE_ACK = 0x01u | 0x08u;          // RUN|ACK
constexpr uint8_t CMD_RECEIVE_NACK = 0x01u;                 // RUN
constexpr uint8_t CMD_RSTS        = 0x80u;

// Vendor polling idiom: I2C_GetMasterIntFlag() then I2C_ClearMasterIntFlag().
void wait_i2cmif(void) {
    while (!(I2CMSR & I2C_I2CMSR_I2CMIF_Msk)) {
    }
}

void clear_i2cmif(void) {
    I2CMSR = 0x00u;
}

}  // namespace

int main(void) {
    printf("[cms8s_i2c] Starting CMS8S78xx I2C unit tests...\n");

    mcs51_test_register_family(MCS51_FAMILY_CMS8S78XX);
    mcs51_context_set_family(MCS51_FAMILY_CMS8S78XX);
    Mcu51Context* ctx = mcs51_get_context();
    mcs51_context_reset(ctx);
    ctx->clock_hz = 24000000u;  // demo comment "400K" assumes 24 MHz + TP=2
    ctx->virtual_us = 0;
    cms8s_i2c_init(ctx);

    // ── 1) Reset state ──────────────────────────────────────────────────────
    check((static_cast<uint8_t>(I2CMSR) & I2CMIF) == 0u,
          "I2CMIF must be clear after reset");
    check((static_cast<uint8_t>(I2CMSR) & IDLE) != 0u,
          "I2CMSR reset value must report IDLE");
    check(cms8s_i2c_cmd_count() == 0u && cms8s_i2c_start_count() == 0u &&
              cms8s_i2c_restart_count() == 0u && cms8s_i2c_stop_count() == 0u,
          "command counters must start at 0");
    check(cms8s_i2c_mock_rx_value() == 0x00u, "mock RX must default to 0x00");

    // ── 2) Vendor write-byte idiom: START|RUN completes synchronously ───────
    I2CMCR = 0x00u;   // I2C_EnableMasterMode (idempotent flag clear)
    I2CMCR = 0x00u;   // second call must stay a no-op
    I2CSCR = 0x00u;   // slave control write is tolerated
    I2CMTP = 2u;      // I2C_ConfigCLK(2)
    I2CMSA = 0xA0u;   // AT24C256 write address (8-bit vendor form)
    I2CMBUF = 0x00u;
    I2CMCR = CMD_START_SEND;
    wait_i2cmif();
    check(cms8s_i2c_last_charge_us() == 68u,
          "START|RUN charge != 68us (3 units @ 24MHz/TP2)");
    check((static_cast<uint8_t>(I2CMSR) & ADD_ACK) == 0u,
          "address phase must mock-ACK (ADD_ACK=0)");
    check((static_cast<uint8_t>(I2CMSR) & DATA_ACK) == 0u,
          "data phase must mock-ACK (DATA_ACK=0)");
    check((static_cast<uint8_t>(I2CMSR) & ERROR) == 0u,
          "ACKed address must clear ERROR");
    check((static_cast<uint8_t>(I2CMSR) & IDLE) == 0u,
          "IDLE must be deasserted while a session is active");
    check((static_cast<uint8_t>(I2CMSR) & (BUSY | BUS_BUSY)) == 0u,
          "BUSY/BUS_BUSY must be clear after command completion");
    check(cms8s_i2c_cmd_count() == 1u && cms8s_i2c_start_count() == 1u,
          "START|RUN must count one command and one open");
    clear_i2cmif();
    check((static_cast<uint8_t>(I2CMSR) & I2CMIF) == 0u,
          "I2CMSR=0 must clear I2CMIF");
    check(cms8s_i2c_cmd_count() == 1u, "flag clear must not count as command");

    // ── 3) SEND (RUN): one more byte, DATA_ACK backfilled ───────────────────
    I2CMBUF = 0x10u;
    I2CMCR = CMD_SEND;
    wait_i2cmif();
    check(cms8s_i2c_last_charge_us() == 23u,
          "RUN send charge != 23us (1 unit @ 24MHz/TP2)");
    check((static_cast<uint8_t>(I2CMSR) & DATA_ACK) == 0u,
          "SEND must mock-ACK the byte");
    clear_i2cmif();

    // ── 4) STOP: release bus, no I2CMIF (behavioral deviation contract) ─────
    I2CMCR = CMD_STOP;
    check(cms8s_i2c_stop_count() == 1u, "STOP must count one release");
    check((static_cast<uint8_t>(I2CMSR) & IDLE) != 0u,
          "STOP must set IDLE");
    check((static_cast<uint8_t>(I2CMSR) & (BUSY | BUS_BUSY)) == 0u,
          "STOP must clear BUSY/BUS_BUSY");
    check((static_cast<uint8_t>(I2CMSR) & I2CMIF) == 0u,
          "STOP must not set I2CMIF (documented deviation)");
    check(cms8s_i2c_last_charge_us() == 23u, "STOP charge != 23us");

    // ── 5) Data command after STOP is illegal (no session) ──────────────────
    I2CMCR = CMD_SEND;
    check(cms8s_i2c_illegal_cmd_count() == 1u,
          "RUN without a session must be counted illegal");
    check((static_cast<uint8_t>(I2CMSR) & I2CMIF) == 0u,
          "illegal command must not fake completion");

    // ── 6) Repeated START routes to restart, never a second open ────────────
    I2CMSA = 0xA0u;
    I2CMBUF = 0x00u;
    I2CMCR = CMD_START_SEND;
    wait_i2cmif();
    clear_i2cmif();
    I2CMCR = CMD_START_SEND;  // firmware repeated START
    wait_i2cmif();
    check(cms8s_i2c_restart_count() == 1u,
          "repeated START must route to restart");
    check(cms8s_i2c_start_count() == 2u,
          "restart must not count a new open (2nd open was section 6)");
    clear_i2cmif();
    I2CMCR = CMD_STOP;

    // ── 7) ADDR_NACKED reservation and recovery ─────────────────────────────
    cms8s_i2c_set_mock_addr_ack(false);
    I2CMSA = 0xA0u;
    I2CMCR = CMD_START_SEND;
    check(cms8s_i2c_addr_nack_count() == 1u, "address NACK not counted");
    check((static_cast<uint8_t>(I2CMSR) & ADD_ACK) != 0u,
          "NACKed address must set ADD_ACK");
    check((static_cast<uint8_t>(I2CMSR) & ERROR) != 0u,
          "NACKed address must set ERROR (vendor getter semantics)");
    check((static_cast<uint8_t>(I2CMSR) & DATA_ACK) == 0u,
          "NACKed address must not touch DATA_ACK");
    check((static_cast<uint8_t>(I2CMSR) & I2CMIF) != 0u,
          "NACKed address must keep I2CMIF latched");
    const uint32_t cmds_before = cms8s_i2c_cmd_count();
    I2CMCR = CMD_SEND;  // skipped in ADDR_NACKED
    check(cms8s_i2c_cmd_count() == cmds_before,
          "ADDR_NACKED data command must be skipped");
    check(cms8s_i2c_illegal_cmd_count() == 2u,
          "ADDR_NACKED data command must be counted");
    check((static_cast<uint8_t>(I2CMSR) & I2CMIF) != 0u,
          "ADDR_NACKED must keep I2CMIF set");
    // Recovery: repeated START with the slave now ACKing.
    cms8s_i2c_set_mock_addr_ack(true);
    clear_i2cmif();
    I2CMCR = CMD_START_SEND;
    wait_i2cmif();
    check((static_cast<uint8_t>(I2CMSR) & (ADD_ACK | ERROR)) == 0u,
          "restart from ADDR_NACKED must clear ADD_ACK/ERROR");
    check(cms8s_i2c_restart_count() == 2u, "recovery restart not counted");
    clear_i2cmif();
    I2CMCR = CMD_STOP;

    // ── 8) Read direction: mock RX byte injection ───────────────────────────
    cms8s_i2c_set_mock_rx_value(0x5Au);
    I2CMSA = 0xA1u;  // read address
    I2CMCR = CMD_START_RX_ACK;
    wait_i2cmif();
    check(static_cast<uint8_t>(I2CMBUF) == 0x5Au,
          "START_RECEIVE_ACK must serve the injected byte");
    check(cms8s_i2c_last_charge_us() == 68u,
          "read command charge != 68us (START+addr+rx)");
    clear_i2cmif();
    I2CMCR = CMD_RECEIVE_ACK;
    wait_i2cmif();
    check(static_cast<uint8_t>(I2CMBUF) == 0x5Au,
          "RECEIVE_ACK must serve the injected byte");
    clear_i2cmif();
    I2CMCR = CMD_RECEIVE_NACK;
    wait_i2cmif();
    check(static_cast<uint8_t>(I2CMBUF) == 0x5Au,
          "RECEIVE_NACK must serve the injected byte");
    clear_i2cmif();
    I2CMCR = CMD_STOP;

    // ── 9) I2CMTP=0 clock variant: SCL = 30*Tsys ────────────────────────────
    I2CMTP = 0u;
    I2CMSA = 0xA0u;
    I2CMCR = CMD_START_SEND;
    check(cms8s_i2c_last_charge_us() == 34u,
          "START|RUN charge != 34us (I2CMTP=0 @ 24MHz)");
    clear_i2cmif();
    I2CMCR = CMD_STOP;

    // ── 10) Slave registers tolerate writes; read view is status ────────────
    I2CSCR = 0x00u;   // vendor I2C_EnableMasterMode writes 0xF2 too
    I2CSADR = 0x11u;
    I2CSBUF = 0x22u;
    check(static_cast<uint8_t>(I2CSSR) == 0x00u,
          "0xF2 read view must serve slave status (tolerated writes)");
    check(static_cast<uint8_t>(I2CSADR) == 0x11u, "0xF1 write/read lost");
    check(static_cast<uint8_t>(I2CSBUF) == 0x22u, "0xF3 write/read lost");

    // ── 11) RSTS: minimal release, never crash ──────────────────────────────
    I2CMTP = 2u;
    I2CMSA = 0xA0u;
    I2CMCR = CMD_START_SEND;
    wait_i2cmif();
    clear_i2cmif();
    I2CMCR = CMD_RSTS;
    check((static_cast<uint8_t>(I2CMSR) & IDLE) != 0u,
          "RSTS must return the master to IDLE");
    I2CMCR = CMD_SEND;  // session was released
    check(cms8s_i2c_illegal_cmd_count() == 3u,
          "RSTS must release the session (data command now illegal)");

    // ── 12) Host I2C bus fallback (Phase 2 prerequisite) ────────────────────
    {
        const uint8_t tx = 0x00u;
        uint8_t rx_buf = 0x00u;
        check(!js_pal_i2c_transfer(0u, 0x50u, &tx, 1u, &rx_buf, 1u),
              "host js_pal_i2c_transfer fallback must fail closed");
    }

    // ── 13) Reset restores the contract and re-installs hooks (S4-H2) ───────
    cms8s_i2c_reset(ctx);
    check((static_cast<uint8_t>(I2CMSR) & I2CMIF) == 0u,
          "reset: I2CMIF not clear");
    check((static_cast<uint8_t>(I2CMSR) & IDLE) != 0u, "reset: not IDLE");
    check(cms8s_i2c_cmd_count() == 0u && cms8s_i2c_stop_count() == 0u,
          "reset: counters not zeroed");
    check(cms8s_i2c_mock_rx_value() == 0x00u, "reset: mock RX != 0x00");
    I2CMTP = 2u;
    I2CMSA = 0xA0u;
    I2CMBUF = 0x00u;
    I2CMCR = CMD_START_SEND;
    wait_i2cmif();
    check(cms8s_i2c_cmd_count() == 1u && cms8s_i2c_last_charge_us() == 68u,
          "reset did not re-install the 0xF5 hooks (S4-H2)");

    if (g_fails) {
        printf("[cms8s_i2c] %d failure(s)\n", g_fails);
        return 1;
    }
    printf("[cms8s_i2c] PASS: 0xF5 command FSM, repeated START, ADDR_NACKED, "
           "STOP deviation, mock RX, SCL charge, 0xF2 tolerance, RSTS, "
           "reset semantics\n");
    return 0;
}
