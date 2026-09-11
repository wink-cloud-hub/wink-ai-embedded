// SPDX-License-Identifier: Apache-2.0
// A-03 UART TX per-byte synchronous charge (ADR-0081) — host tests.
//
// Release build: baud anchors per source (Timer1/BRT/TMR2/TMR4), frame-bit
// widths (mode 1 = 10, mode 3 = 11), virtual-time charge per byte, 22-byte
// telemetry scale (~23ms @9600), unready/uncomputable no-charge paths,
// classic-family Timer1 path.
//
// STRICT build (same TU, STRICT compat lib): uncomputable-source write aborts
// via child re-execution (mirrors test_mcs51_uart_tx_ready.cpp).
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mcs51_context.h"
#include "mcs51_test_harness.h"
#include "wink_mcs51_clock.h"
#include "wink_mcs51_isr.h"
#include "wink_mcs51_uart.h"

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

namespace {

constexpr uint8_t SFR_SCON   = 0x98;
constexpr uint8_t SFR_SBUF   = 0x99;
constexpr uint8_t SFR_TCON   = 0x88;
constexpr uint8_t SFR_TMOD   = 0x89;
constexpr uint8_t SFR_PCON   = 0x87;
constexpr uint8_t SFR_CKCON  = 0x8E;
constexpr uint8_t SFR_TH1    = 0x8D;
constexpr uint8_t SFR_FUNCCR = 0x91;
constexpr uint8_t SFR_T2CON  = 0xC8;
constexpr uint8_t SFR_T34MOD = 0xD2;
constexpr uint8_t SFR_RLDL   = 0xCA;
constexpr uint8_t SFR_RLDH   = 0xCB;
constexpr uint8_t SFR_TH4    = 0xE3;

constexpr uint16_t XSFR_BRT_CON = 0xF5C0u;
constexpr uint16_t XSFR_BRTDL   = 0xF5C1u;
constexpr uint16_t XSFR_BRTDH   = 0xF5C2u;

// Own BSS context for isolation (never stack/fiber).
Mcu51Context s_ctx;

void init_ctx(uint8_t family) {
    mcs51_set_active_context(&s_ctx);
    // Stage4: register + select BEFORE reset so the reset loop installs the
    // family's models (the old reset-first order predates registration).
    mcs51_test_register_family(family);
    mcs51_context_set_family(family);
    mcs51_context_reset(&s_ctx);
    wink_mcs51_uart_reset();
    wink_mcs51_clock_reset();
    wink_mcs51_isr_enable();
}

void tx_write(uint8_t b) {
    mcs51_get_context()->sfr_shadow[SFR_SBUF] = b;
    wink_mcs51_uart_on_write(SFR_SBUF);
}

// health_pot full-rate config: T1M + SMOD0 + TH1=217 + TR1 + SCON mode 1.
// Expect 24M*2/(128*1*39) = 9615bps, 10-bit frame -> 1040us/byte.
#ifndef WINK_MCS51_STRICT
void t1_9600_init(void) {
    Mcu51Context* ctx = mcs51_get_context();
    ctx->sfr_shadow[SFR_FUNCCR] = 0x00u;  // CKS=TMR1
    ctx->sfr_shadow[SFR_SCON] = 0x40u;    // mode 1, REN=0
    ctx->sfr_shadow[SFR_PCON] = 0x80u;    // SMOD0
    ctx->sfr_shadow[SFR_CKCON] = 0x17u;   // seed 0x07 + T1M
    ctx->sfr_shadow[SFR_TMOD] = 0x20u;    // Timer1 mode 2
    ctx->sfr_shadow[SFR_TH1] = 217u;
    ctx->sfr_shadow[SFR_TCON] = 0x40u;    // TR1
    ctx->sfr_shadow[0xA8u] = 0x00u;       // IE: polling
}
#endif

}  // namespace

extern "C" void wink_mcs51_user_main(void) {}

extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

#define CHECK(cond, msg)                                           \
    do {                                                           \
        if (!(cond)) {                                             \
            printf("[uart-charge] FAIL: %s (line %d)\n", msg, __LINE__); \
            ++fails;                                               \
        }                                                          \
    } while (0)

#ifdef WINK_MCS51_STRICT

int child_main(int case_id) {
#ifdef _MSC_VER
    _set_abort_behavior(0, _WRITE_ABORT_MSG);
#endif
    init_ctx(MCS51_FAMILY_CMS8S78XX);
    Mcu51Context* ctx = mcs51_get_context();
    if (case_id == 1) {
        // TMR4 selected + TR4 running (A-01 ready) but T4 not in 8-bit
        // auto-reload mode -> baud uncomputable -> STRICT must abort.
        ctx->sfr_shadow[SFR_FUNCCR] = 0x01u;  // CKS=TMR4
        ctx->sfr_shadow[SFR_SCON] = 0x40u;
        ctx->sfr_shadow[SFR_T34MOD] = 0x80u;  // TR4 run, mode 0
        tx_write(0x41u);
    }
    return 0;
}

int run_child(const char* self, int case_id, bool must_die) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "\"%s\" --child %d", self, case_id);
    int rc = system(cmd);
    bool died = (rc != 0);
    if (died != must_die) {
        printf("[uart-charge] FAIL: child %d died=%d want=%d (rc=%d)\n",
               case_id, (int)died, (int)must_die, rc);
        return 1;
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc == 3 && strcmp(argv[1], "--child") == 0) {
        return child_main(atoi(argv[2]));
    }
    int fails = 0;
    // Uncomputable source must abort.
    fails += run_child(argv[0], 1, true);
    if (fails) {
        return 1;
    }
    printf("[uart-charge] PASS (STRICT): uncomputable-source abort\n");
    return 0;
}

#else

int main(void) {
    int fails = 0;

    // ── 1) Timer1 anchor: 9615bps, 1040us/byte ────────────────────────────
    {
        init_ctx(MCS51_FAMILY_CMS8S78XX);
        t1_9600_init();
        const uint64_t t0 = wink_mcs51_virtual_us();
        tx_write(0x41u);
        const uint64_t dt = wink_mcs51_virtual_us() - t0;
        CHECK(wink_mcs51_uart_last_baud_hz() == 9615u, "T1 baud must be 9615");
        CHECK(dt == 1040u, "T1 byte charge must be 1040us");
        CHECK(wink_mcs51_uart_notready_total() == 0u, "T1 config must be clean");
        CHECK((mcs51_get_context()->sfr_shadow[SFR_SCON] & 0x02u) != 0u,
              "TI must still be set synchronously");
    }

    // ── 2) 22-byte telemetry occupies ~23ms virtual ────────────────────────
    {
        init_ctx(MCS51_FAMILY_CMS8S78XX);
        t1_9600_init();
        const uint64_t t0 = wink_mcs51_virtual_us();
        for (uint8_t i = 0; i < 22; ++i) {
            tx_write(i);
            // Keil idiom: consume TI so the GAP-25 overwrite tripwire
            // (correctly) stays silent while measuring the A-03 charge.
            mcs51_get_context()->sfr_shadow[SFR_SCON] &=
                static_cast<uint8_t>(~(1u << 1));
        }
        const uint64_t dt = wink_mcs51_virtual_us() - t0;
        CHECK(dt == 22u * 1040u, "22B frame must occupy 22880us");
        CHECK(wink_mcs51_uart_overwrite_total() == 0u,
              "TI-cleared loop must not trip overwrite");
    }

    // ── 3) Mode 3 charges 11-bit frames ────────────────────────────────────
    {
        init_ctx(MCS51_FAMILY_CMS8S78XX);
        t1_9600_init();
        mcs51_get_context()->sfr_shadow[SFR_SCON] = 0xC0u;  // mode 3
        const uint64_t t0 = wink_mcs51_virtual_us();
        tx_write(0x41u);
        CHECK(wink_mcs51_virtual_us() - t0 == 1144u, "mode 3 must charge 1144us");
    }

    // ── 4) BRT anchor: div=1, n=78 -> 9615bps ──────────────────────────────
    {
        init_ctx(MCS51_FAMILY_CMS8S78XX);
        Mcu51Context* ctx = mcs51_get_context();
        ctx->sfr_shadow[SFR_FUNCCR] = 0x03u;  // CKS=BRT
        ctx->sfr_shadow[SFR_SCON] = 0x40u;
        ctx->xdata_shadow[XSFR_BRT_CON] = 0x80u;  // BRTEN, CKDIV=0
        ctx->xdata_shadow[XSFR_BRTDL] = 0xB2u;    // reload 0xFFB2, n=78
        ctx->xdata_shadow[XSFR_BRTDH] = 0xFFu;
        const uint64_t t0 = wink_mcs51_virtual_us();
        tx_write(0x41u);
        CHECK(wink_mcs51_uart_last_baud_hz() == 9615u, "BRT baud must be 9615");
        CHECK(wink_mcs51_virtual_us() - t0 == 1040u, "BRT charge must be 1040us");
    }

    // ── 5) TMR2 anchor: T2PS=0, n=6 -> 10416bps ────────────────────────────
    {
        init_ctx(MCS51_FAMILY_CMS8S78XX);
        Mcu51Context* ctx = mcs51_get_context();
        ctx->sfr_shadow[SFR_FUNCCR] = 0x02u;  // CKS=TMR2
        ctx->sfr_shadow[SFR_SCON] = 0x40u;
        ctx->sfr_shadow[SFR_T2CON] = 0x01u;   // T2I running, T2PS=0
        ctx->sfr_shadow[SFR_RLDL] = 0xFAu;    // reload 0xFFFA, n=6
        ctx->sfr_shadow[SFR_RLDH] = 0xFFu;
        tx_write(0x41u);
        CHECK(wink_mcs51_uart_last_baud_hz() == 10416u, "TMR2 baud must be 10416");
    }

    // ── 6) TMR4 anchor: mode 2, T4M=1, TH4=236 -> 9375bps ──────────────────
    {
        init_ctx(MCS51_FAMILY_CMS8S78XX);
        Mcu51Context* ctx = mcs51_get_context();
        ctx->sfr_shadow[SFR_FUNCCR] = 0x01u;  // CKS=TMR4
        ctx->sfr_shadow[SFR_SCON] = 0x40u;
        ctx->sfr_shadow[SFR_T34MOD] = 0xE0u;  // TR4 + T4M + mode 2
        ctx->sfr_shadow[SFR_TH4] = 236u;      // n=20
        tx_write(0x41u);
        CHECK(wink_mcs51_uart_last_baud_hz() == 9375u, "TMR4 baud must be 9375");
    }

    // ── 7) Unready link: no charge, BAUD counted ───────────────────────────
    {
        init_ctx(MCS51_FAMILY_CMS8S78XX);
        t1_9600_init();
        mcs51_get_context()->sfr_shadow[SFR_TCON] = 0x00u;  // TR1 stopped
        const uint64_t t0 = wink_mcs51_virtual_us();
        tx_write(0x41u);
        CHECK(wink_mcs51_virtual_us() == t0, "unready write must not charge");
        CHECK(wink_mcs51_uart_notready_total() == 1u, "unready must count");
    }

    // ── 8) Uncomputable source: no charge, BAUD counted ────────────────────
    {
        init_ctx(MCS51_FAMILY_CMS8S78XX);
        Mcu51Context* ctx = mcs51_get_context();
        ctx->sfr_shadow[SFR_FUNCCR] = 0x01u;  // CKS=TMR4
        ctx->sfr_shadow[SFR_SCON] = 0x40u;
        ctx->sfr_shadow[SFR_T34MOD] = 0x80u;  // TR4 run, mode 0
        const uint64_t t0 = wink_mcs51_virtual_us();
        tx_write(0x41u);
        CHECK(wink_mcs51_virtual_us() == t0, "uncomputable must not charge");
        CHECK(wink_mcs51_uart_notready_total() == 1u, "uncomputable must count BAUD");
    }

    // ── 9) Classic family: Timer1 12T, 12MHz crystal ───────────────────────
    {
        init_ctx(MCS51_FAMILY_CLASSIC);
        // Classic seeds leave clock_hz untouched (board crystal); the shared
        // BSS context may still hold the previous family's rate, so seed it
        // explicitly (production builds have a fixed compile-time family).
        wink_mcs51_set_hardware_clock_hz(12000000u);
        Mcu51Context* ctx = mcs51_get_context();
        ctx->sfr_shadow[SFR_SCON] = 0x40u;
        ctx->sfr_shadow[SFR_PCON] = 0x00u;
        ctx->sfr_shadow[SFR_TMOD] = 0x20u;
        ctx->sfr_shadow[SFR_TH1] = 0xFDu;    // 11.0592MHz textbook; model Fsys
        ctx->sfr_shadow[SFR_TCON] = 0x40u;
        tx_write(0x41u);
        // Model Fsys=12MHz: 12M/(128*3*3) = 10416bps.
        CHECK(wink_mcs51_uart_last_baud_hz() == 10416u, "classic baud must be 10416");
        CHECK(wink_mcs51_uart_notready_total() == 0u, "classic config must be clean");
    }

    if (fails) {
        return 1;
    }
    printf("[uart-charge] PASS: baud anchors, frame widths, 22B scale, no-charge paths\n");
    return 0;
}

#endif
