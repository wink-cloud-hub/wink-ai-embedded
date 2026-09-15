// SPDX-License-Identifier: GPL-3.0-only
// GAP-02 UART TX-link readiness gate (A-01) — host tests.
//
// Release build (no WINK_MCS51_STRICT): the readiness predicate mask and the
// per-reason trigger counters, including a health_pot-equivalent positive
// sequence that hard-asserts zero regression (all counters stay 0).
//
// STRICT build (same TU, STRICT-built compat lib): the abort policy is
// process-fatal, so misconfigured SBUF writes are verified via child
// re-execution (the parent asserts non-zero child exit; a good-config child
// must exit 0, proving the harness discriminates).
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mcs51_context.h"
#include "mcs51_test_harness.h"
#include "wink_mcs51_isr.h"
#include "wink_mcs51_uart.h"

#ifdef _MSC_VER
// No abort dialog in CI: fail fast with an exit code instead.
#include <crtdbg.h>
#endif

namespace {

constexpr uint8_t SFR_SCON   = 0x98;
constexpr uint8_t SFR_SBUF   = 0x99;
constexpr uint8_t SFR_TCON   = 0x88;
constexpr uint8_t SFR_TMOD   = 0x89;
constexpr uint8_t SFR_FUNCCR = 0x91;
constexpr uint8_t SFR_T2CON  = 0xC8;
constexpr uint8_t SFR_T34MOD = 0xD2;

constexpr uint16_t XSFR_P13CFG = 0xF013u;
constexpr uint16_t XSFR_P22CFG = 0xF022u;
constexpr uint16_t XSFR_BRT_CON = 0xF5C0u;
constexpr uint16_t XSFR_PS_RXD = 0xF69Fu;

// Own BSS context for isolation (never stack/fiber); the framework default
// context is left untouched.
Mcu51Context s_ctx;

void init_ctx(uint8_t family) {
    mcs51_set_active_context(&s_ctx);
    // Stage4: register + select BEFORE reset so the reset loop installs the
    // family's models (the old reset-first order predates registration).
    mcs51_test_register_family(family);
    mcs51_context_set_family(family);
    mcs51_context_reset(&s_ctx);
    wink_mcs51_uart_reset();
    wink_mcs51_isr_enable();
}

void tx_write(uint8_t b) {
    mcs51_get_context()->sfr_shadow[SFR_SBUF] = b;
    wink_mcs51_uart_on_write(SFR_SBUF);
}

// health_pot.c uart_init() equivalent (CMS8S78xx, Timer1 baud source):
// P22CFG=TXD alt, FUNCCR CKS=TMR1, SCON mode 1 TX-only, T1M, TMOD mode 2,
// TH1=217, TR1=1. REN=0 so the RXD path is out of scope here.
void health_pot_uart_init(void) {
    Mcu51Context* ctx = mcs51_get_context();
    ctx->xdata_shadow[XSFR_P22CFG] = 0x03u;
    ctx->sfr_shadow[SFR_FUNCCR] = 0x00u;
    ctx->sfr_shadow[SFR_SCON] = 0x40u;
    ctx->sfr_shadow[SFR_TMOD] = 0x20u;
    ctx->sfr_shadow[0x8Du] = 217u;  // TH1
    ctx->sfr_shadow[SFR_TCON] = 0x40u;  // TR1
    ctx->sfr_shadow[0xA8u] = 0x00u;  // IE: polling, no ISR
}

}  // namespace

extern "C" void wink_mcs51_user_main(void) {}

extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

#define CHECK(cond, msg)                                            \
    do {                                                            \
        if (!(cond)) {                                              \
            printf("[mcs51-txready] FAIL: %s (line %d)\n", msg, __LINE__); \
            ++fails;                                                \
        }                                                           \
    } while (0)

#ifdef WINK_MCS51_STRICT

// ── STRICT: death-test policy via child re-execution ────────────────────────
// Child case ids (argv: --child <id>). Each bad case must abort (non-zero
// exit); the good case must exit 0.
int child_main(int case_id) {
#ifdef _MSC_VER
    _set_abort_behavior(0, _WRITE_ABORT_MSG);
#endif
    init_ctx(MCS51_FAMILY_CMS8S78XX);
    Mcu51Context* ctx = mcs51_get_context();
    switch (case_id) {
        case 1:  // TR1 stopped: no baud source
            health_pot_uart_init();
            ctx->sfr_shadow[SFR_TCON] = 0x00u;
            break;
        case 2:  // SCON mode 0: no async framing
            health_pot_uart_init();
            ctx->sfr_shadow[SFR_SCON] = 0x00u;
            break;
        case 3:  // REN + selector points at unmuxed pin
            health_pot_uart_init();
            ctx->sfr_shadow[SFR_SCON] = 0x50u;
            ctx->xdata_shadow[XSFR_PS_RXD] = 0x13u;
            ctx->xdata_shadow[XSFR_P13CFG] = 0x00u;
            break;
        case 4:  // GAP-25: second write with TI still set (frame overwrite)
            health_pot_uart_init();
            tx_write('U');
            break;
        case 9:  // good config: must survive
            health_pot_uart_init();
            break;
        default:
            printf("[mcs51-txready] FAIL: unknown child case %d\n", case_id);
            return 2;
    }
    tx_write('U');
    return 0;
}

int run_child(const char* exe, int case_id, bool expect_abort) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "\"%s\" --child %d", exe, case_id);
    const int rc = system(cmd);
    if (expect_abort && rc == 0) {
        printf("[mcs51-txready] FAIL: child %d should have aborted (rc=0)\n",
               case_id);
        return 1;
    }
    if (!expect_abort && rc != 0) {
        printf("[mcs51-txready] FAIL: good-config child exited rc=%d\n", rc);
        return 1;
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc == 3 && strcmp(argv[1], "--child") == 0) {
        return child_main(atoi(argv[2]));
    }
    int fails = 0;
    fails += run_child(argv[0], 1, true);
    fails += run_child(argv[0], 2, true);
    fails += run_child(argv[0], 3, true);
    fails += run_child(argv[0], 4, true);
    fails += run_child(argv[0], 9, false);
    if (fails) {
        return 1;
    }
    printf("[mcs51-txready] PASS (STRICT): 3 misconfigs + overwrite abort, good config survives\n");
    return 0;
}

#else

// ── Release: predicate mask + per-reason counters ───────────────────────────
int main(void) {
    int fails = 0;

    // ── A: health_pot-equivalent positive (zero-regression hard assertion) ──
    init_ctx(MCS51_FAMILY_CMS8S78XX);
    health_pot_uart_init();
    CHECK(wink_mcs51_uart_notready_mask() == 0u, "A: health_pot config ready");
    tx_write('U');
    CHECK(wink_mcs51_uart_notready_count(WINK_MCS51_UART_NOTREADY_BAUD) == 0u,
          "A: no BAUD trigger on good config");
    CHECK(wink_mcs51_uart_notready_count(WINK_MCS51_UART_NOTREADY_MODE) == 0u,
          "A: no MODE trigger on good config");
    CHECK(wink_mcs51_uart_notready_count(WINK_MCS51_UART_NOTREADY_TXD) == 0u,
          "A: no TXD trigger on good config");
    CHECK(wink_mcs51_uart_notready_count(WINK_MCS51_UART_NOTREADY_RXD) == 0u,
          "A: no RXD trigger on good config");
    CHECK(wink_mcs51_uart_byte_count() == 1u, "A: byte still captured");

    // ── B: TR1 stopped → BAUD only ──────────────────────────────────────────
    init_ctx(MCS51_FAMILY_CMS8S78XX);
    health_pot_uart_init();
    mcs51_get_context()->sfr_shadow[SFR_TCON] = 0x00u;
    CHECK(wink_mcs51_uart_notready_mask() == WINK_MCS51_UART_NOTREADY_BAUD,
          "B: mask is BAUD-only");
    tx_write('U');
    tx_write('U');  // second write: trigger counts every occurrence
    CHECK(wink_mcs51_uart_notready_count(WINK_MCS51_UART_NOTREADY_BAUD) == 2u,
          "B: BAUD counted per write");
    CHECK(wink_mcs51_uart_notready_count(WINK_MCS51_UART_NOTREADY_MODE) == 0u,
          "B: no MODE trigger");
    CHECK(wink_mcs51_uart_byte_count() == 2u,
          "B: release still sends (scenarios stay green)");
    CHECK(wink_mcs51_uart_overwrite_total() == 1u,
          "B: second write without TI clear counts overwrite");

    // ── C: SCON mode 0 → MODE (REN=0 keeps RXD out of scope) ────────────────
    init_ctx(MCS51_FAMILY_CMS8S78XX);
    health_pot_uart_init();
    mcs51_get_context()->sfr_shadow[SFR_SCON] = 0x00u;
    CHECK(wink_mcs51_uart_notready_mask() == WINK_MCS51_UART_NOTREADY_MODE,
          "C: mask is MODE-only");
    tx_write('U');
    CHECK(wink_mcs51_uart_notready_count(WINK_MCS51_UART_NOTREADY_MODE) == 1u,
          "C: MODE counted");

    // ── D: BRT selected but BRTEN=0 → BAUD; BRTEN=1 → ready ─────────────────
    init_ctx(MCS51_FAMILY_CMS8S78XX);
    health_pot_uart_init();
    mcs51_get_context()->sfr_shadow[SFR_FUNCCR] = 0x03u;  // CKS=BRT
    CHECK((wink_mcs51_uart_notready_mask() & WINK_MCS51_UART_NOTREADY_BAUD) != 0u,
          "D: BRT without BRTEN is BAUD");
    mcs51_get_context()->xdata_shadow[XSFR_BRT_CON] = 0x80u;  // BRTEN
    CHECK(wink_mcs51_uart_notready_mask() == 0u, "D: BRTEN ready");

    // ── E: reserved CKS + TMR4/TMR2 run bits ────────────────────────────────
    init_ctx(MCS51_FAMILY_CMS8S78XX);
    health_pot_uart_init();
    mcs51_get_context()->sfr_shadow[SFR_FUNCCR] = 0x05u;  // reserved
    CHECK((wink_mcs51_uart_notready_mask() & WINK_MCS51_UART_NOTREADY_BAUD) != 0u,
          "E: reserved CKS never pretends ready");
    mcs51_get_context()->sfr_shadow[SFR_FUNCCR] = 0x01u;  // CKS=TMR4
    CHECK((wink_mcs51_uart_notready_mask() & WINK_MCS51_UART_NOTREADY_BAUD) != 0u,
          "E: TMR4 stopped is BAUD");
    mcs51_get_context()->sfr_shadow[SFR_T34MOD] = 0x80u;  // TR4
    CHECK(wink_mcs51_uart_notready_mask() == 0u, "E: TR4 ready");
    mcs51_get_context()->sfr_shadow[SFR_FUNCCR] = 0x02u;  // CKS=TMR2
    mcs51_get_context()->sfr_shadow[SFR_T34MOD] = 0x00u;
    CHECK((wink_mcs51_uart_notready_mask() & WINK_MCS51_UART_NOTREADY_BAUD) != 0u,
          "E: TMR2 stopped is BAUD");
    mcs51_get_context()->sfr_shadow[SFR_T2CON] = 0x01u;  // T2I != 0
    CHECK(wink_mcs51_uart_notready_mask() == 0u, "E: T2 running ready");

    // ── F: REN + selector mismatch → RXD; defaults stay silent ──────────────
    init_ctx(MCS51_FAMILY_CMS8S78XX);
    health_pot_uart_init();
    mcs51_get_context()->sfr_shadow[SFR_SCON] = 0x50u;  // mode 1 + REN
    CHECK(wink_mcs51_uart_notready_mask() == 0u,
          "F: REN with default P3.0 path ready");
    mcs51_get_context()->xdata_shadow[XSFR_PS_RXD] = 0x13u;  // select P1.3...
    mcs51_get_context()->xdata_shadow[XSFR_P13CFG] = 0x00u;  // ...but not muxed
    CHECK(wink_mcs51_uart_notready_mask() == WINK_MCS51_UART_NOTREADY_RXD,
          "F: selector/mux mismatch is RXD");
    tx_write('U');
    CHECK(wink_mcs51_uart_notready_count(WINK_MCS51_UART_NOTREADY_RXD) == 1u,
          "F: RXD counted");

    // ── G: classic family ignores FUNCCR; T1-only ───────────────────────────
    init_ctx(MCS51_FAMILY_CLASSIC);
    mcs51_get_context()->sfr_shadow[SFR_SCON] = 0x40u;
    mcs51_get_context()->sfr_shadow[SFR_TMOD] = 0x20u;
    mcs51_get_context()->sfr_shadow[SFR_TCON] = 0x40u;
    mcs51_get_context()->sfr_shadow[SFR_FUNCCR] = 0x03u;  // garbage: unread
    CHECK(wink_mcs51_uart_notready_mask() == 0u,
          "G: classic ignores FUNCCR, T1 ready");
    mcs51_get_context()->sfr_shadow[SFR_TCON] = 0x00u;
    CHECK(wink_mcs51_uart_notready_mask() == WINK_MCS51_UART_NOTREADY_BAUD,
          "G: classic TR1=0 is BAUD");

    // ── H: TXD silicon rule — default P3.1 hardwired, never unready ─────────
    init_ctx(MCS51_FAMILY_CMS8S78XX);
    health_pot_uart_init();
    mcs51_get_context()->xdata_shadow[XSFR_P22CFG] = 0x00u;  // alt unmuxed
    CHECK((wink_mcs51_uart_notready_mask() & WINK_MCS51_UART_NOTREADY_TXD) == 0u,
          "H: default P3.1 keeps TXD ready (documented silicon rule)");
    CHECK(wink_mcs51_uart_notready_count(0x10u) == 0u,
          "H: unknown reason bit reads 0");

    // ── I: GAP-25 SBUF overwrite — TI still set from the previous byte ─────
    init_ctx(MCS51_FAMILY_CMS8S78XX);
    health_pot_uart_init();
    tx_write('U');
    CHECK(wink_mcs51_uart_overwrite_total() == 0u, "I: first write clean");
    tx_write('U');  // TI never cleared: previous frame unconsumed
    CHECK(wink_mcs51_uart_overwrite_total() == 1u, "I: rewrite counted");
    CHECK(wink_mcs51_uart_byte_count() == 2u,
          "I: release still sends (scenarios stay green)");
    mcs51_get_context()->sfr_shadow[SFR_SCON] &=
        static_cast<uint8_t>(~(1u << 1));  // Keil idiom: TI = 0
    tx_write('U');
    CHECK(wink_mcs51_uart_overwrite_total() == 1u,
          "I: TI-cleared write stays clean");

    if (fails) {
        return 1;
    }
    printf("[mcs51-txready] PASS: TX-link readiness mask + counters (A-I)\n");
    return 0;
}

#endif
