// SPDX-License-Identifier: Apache-2.0
// Stage 2 T3 (ADR-0076 A-class): external interrupt model — INT0 (P3.2,
// linear pin 26, vector 0) and INT1 (P3.3, pin 27, vector 2). External levels
// are driven through the channel-1 read seam (js_pal_gpio_read_state; on host
// the compat fallback in mcs51_uni_bridge.cpp is scripted via
// wink_mcs51_host_set_ext_pin). Hardware rules verified:
//   * ITx=1 (edge mode): a high->low falling edge latches IEx (TCON.1/TCON.3);
//     the ISR is vectored when EA+EXx are set, and hardware auto-clears IEx on
//     vectoring. An edge latched while the interrupt is disabled stays pending
//     and vectors after EXx is enabled;
//   * ITx=0 (level mode): a low INT pin requests the interrupt once per virtual
//     slice for as long as it is held; nothing is latched;
//   * sampling self-throttles to one evaluation per 10 ms virtual slice
//     (sub-slice polls observe a frozen world and must not redispatch);
//   * HiZ / conflict external state is ignored (no external driver);
//   * reset clears latched IE0/IE1 and the slice throttle, but keeps the edge
//     baseline (the external pin level is world state, persisting across inits).
#include <stdint.h>
#include <stdio.h>

#include "mcs51_proxy.hpp"
#include "wink_mcs51_clock.h"
#include "wink_mcs51_extint.h"
#include "wink_mcs51_isr.h"

namespace {

constexpr uint8_t SFR_TCON = 0x88;
constexpr uint8_t SFR_IE   = 0xA8;
constexpr uint8_t TCON_IT0 = 0u;
constexpr uint8_t TCON_IE0 = 1u;
constexpr uint8_t TCON_IT1 = 2u;
constexpr uint8_t TCON_IE1 = 3u;
constexpr uint8_t IE_EX0   = 0u;
constexpr uint8_t IE_EX1   = 2u;
constexpr uint8_t IE_EA    = 7u;

constexpr uint16_t PIN_INT0 = 26u;  // P3.2
constexpr uint16_t PIN_INT1 = 27u;  // P3.3

constexpr uint8_t EXT_LOW  = 0u;
constexpr uint8_t EXT_HIGH = 1u;
constexpr uint8_t EXT_HIZ  = 2u;

constexpr uint32_t SLICE_US = 10000u;  // model SAMPLE_PERIOD_US

uint32_t g_isr0_hits = 0;
uint32_t g_isr1_hits = 0;

// Host compat fallback seam (mcs51_uni_bridge.cpp); wasm builds don't compile
// this model-direct test (host-only, like test_uart_rx_model.cpp).
extern "C" void wink_mcs51_host_set_ext_pin(uint16_t pin, uint8_t state);
extern "C" void wink_mcs51_host_ext_pins_reset(void);

void ext_set(uint16_t pin, uint8_t state) {
    wink_mcs51_host_set_ext_pin(pin, state);
}

void ie0_config(bool it0, bool ex0, bool ea) {
    uint8_t tcon = wink_mcs51_sfr_shadow[SFR_TCON];
    tcon &= static_cast<uint8_t>(~(1u << TCON_IT0));
    if (it0) tcon |= (1u << TCON_IT0);
    wink_mcs51_sfr_shadow[SFR_TCON] = tcon;
    uint8_t ie = 0;
    if (ex0) ie |= (1u << IE_EX0);
    if (ea)  ie |= (1u << IE_EA);
    // Keep EX1/IT1 untouched for the INT1 case.
    ie |= static_cast<uint8_t>(wink_mcs51_sfr_shadow[SFR_IE] & (1u << IE_EX1));
    wink_mcs51_sfr_shadow[SFR_IE] = ie;
}

void ie1_config(bool it1, bool ex1, bool ea) {
    uint8_t tcon = wink_mcs51_sfr_shadow[SFR_TCON];
    tcon &= static_cast<uint8_t>(~(1u << TCON_IT1));
    if (it1) tcon |= (1u << TCON_IT1);
    wink_mcs51_sfr_shadow[SFR_TCON] = tcon;
    uint8_t ie = wink_mcs51_sfr_shadow[SFR_IE];
    ie &= static_cast<uint8_t>(~((1u << IE_EX1) | (1u << IE_EA)));
    if (ex1) ie |= (1u << IE_EX1);
    if (ea)  ie |= (1u << IE_EA);
    wink_mcs51_sfr_shadow[SFR_IE] = ie;
}

uint8_t ie0_bit(void) {
    return static_cast<uint8_t>(
        (wink_mcs51_sfr_shadow[SFR_TCON] >> TCON_IE0) & 1u);
}

// One model evaluation, plus the virtual-slice advance the next evaluation
// needs to pass the sample throttle.
void poll(void) { wink_mcs51_extint_poll(); }
void next_slice(void) { wink_mcs51_test_advance_virtual_us(SLICE_US); }

}  // namespace

WINK_ISR(0) { ++g_isr0_hits; }
WINK_ISR(2) { ++g_isr1_hits; }

// The bridge TU references the user entry; this test drives the model
// directly, so an empty definition closes the link.
extern "C" void wink_mcs51_user_main(void) {}

extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

#define CHECK(cond, msg)                                            \
    do {                                                            \
        if (!(cond)) {                                              \
            printf("[mcs51-ext] FAIL: %s (line %d)\n", msg, __LINE__); \
            ++fails;                                                \
        }                                                           \
    } while (0)

int main(void) {
    int fails = 0;

    wink_mcs51_isr_enable();
    wink_mcs51_extint_reset();
    wink_mcs51_host_ext_pins_reset();
    g_isr0_hits = 0;
    g_isr1_hits = 0;

    // ── A: edge mode, enabled — falling edge latches IE0, vectors, clears ──
    ie0_config(/*it0=*/true, /*ex0=*/true, /*ea=*/true);
    ext_set(PIN_INT0, EXT_HIGH);
    poll();  // establish baseline: high, no edge
    CHECK(g_isr0_hits == 0 && ie0_bit() == 0,
          "A: high baseline produces no interrupt");
    next_slice();
    ext_set(PIN_INT0, EXT_LOW);  // button press: falling edge
    poll();
    CHECK(g_isr0_hits == 1, "A: falling edge vectors INT0 ISR");
    CHECK(ie0_bit() == 0, "A: IE0 hardware-cleared after vectoring");
    next_slice();
    poll();  // still held low — edge mode does not re-request
    CHECK(g_isr0_hits == 1, "A: held-low does not re-trigger edge mode");

    // ── B: edge latched while disabled stays pending; vectors on enable ────
    g_isr0_hits = 0;
    ie0_config(/*it0=*/true, /*ex0=*/false, /*ea=*/true);  // EX0=0
    next_slice();
    ext_set(PIN_INT0, EXT_HIGH);
    poll();  // release: baseline high again
    next_slice();
    ext_set(PIN_INT0, EXT_LOW);
    poll();  // falling edge with EX0=0
    CHECK(g_isr0_hits == 0, "B: no vector while EX0=0");
    CHECK(ie0_bit() == 1, "B: IE0 latched pending despite EX0=0");
    poll();  // immediate re-poll: throttled, still nothing
    CHECK(g_isr0_hits == 0, "B: sub-slice re-poll does not dispatch");
    ie0_config(/*it0=*/true, /*ex0=*/true, /*ea=*/true);  // enable EX0
    next_slice();
    poll();  // pending IE0 vectors now
    CHECK(g_isr0_hits == 1, "B: pending edge vectors after EX0 enabled");
    CHECK(ie0_bit() == 0, "B: IE0 cleared on vector");

    // ── C: level mode — re-requests once per slice while held low ──────────
    g_isr0_hits = 0;
    ie0_config(/*it0=*/false, /*ex0=*/true, /*ea=*/true);
    next_slice();
    ext_set(PIN_INT0, EXT_HIGH);
    poll();  // high: not asserted
    CHECK(g_isr0_hits == 0, "C: high pin does not request level-mode INT0");
    next_slice();
    ext_set(PIN_INT0, EXT_LOW);
    poll();  // low: request
    CHECK(g_isr0_hits == 1, "C: low pin requests level-mode INT0");
    poll();  // same slice: throttled
    CHECK(g_isr0_hits == 1, "C: no re-request within one slice");
    next_slice();
    poll();  // still low, next slice: request again
    CHECK(g_isr0_hits == 2, "C: held low re-requests on the next slice");
    next_slice();
    ext_set(PIN_INT0, EXT_HIGH);  // release
    poll();
    CHECK(g_isr0_hits == 2, "C: release stops level-mode requests");

    // ── D: level mode with EX0=0 — no dispatch, nothing latched ───────────
    g_isr0_hits = 0;
    ie0_config(/*it0=*/false, /*ex0=*/false, /*ea=*/true);
    next_slice();
    ext_set(PIN_INT0, EXT_LOW);
    poll();
    CHECK(g_isr0_hits == 0, "D: level mode does not dispatch with EX0=0");
    CHECK(ie0_bit() == 0, "D: level mode never latches IE0");
    ie0_config(/*it0=*/false, /*ex0=*/true, /*ea=*/true);
    next_slice();
    poll();  // still low -> requests now that it is enabled
    CHECK(g_isr0_hits == 1, "D: enabling EX0 lets the held-low request through");

    // ── E: INT1 line independently (pin 27, edge, vector 2) ───────────────
    // Quiesce INT0 first: case D left it level-enabled with the pin held low,
    // and poll() evaluates both lines, so release it and mask EX0 before
    // asserting INT1 in isolation.
    uint32_t isr0_before_e = g_isr0_hits;
    g_isr1_hits = 0;
    ext_set(PIN_INT0, EXT_HIGH);
    ie0_config(/*it0=*/true, /*ex0=*/false, /*ea=*/false);
    ie1_config(/*it1=*/true, /*ex1=*/true, /*ea=*/true);
    next_slice();
    ext_set(PIN_INT1, EXT_HIGH);
    poll();  // INT0 baseline re-sampled high (EX0=0, no dispatch either way)
    next_slice();
    ext_set(PIN_INT1, EXT_LOW);
    poll();
    CHECK(g_isr1_hits == 1, "E: falling edge on P3.3 vectors INT1 ISR");
    CHECK(g_isr0_hits == isr0_before_e,
          "E: INT1 edge does not touch INT0 state");

    // ── F: HiZ external state is ignored (no external driver) ─────────────
    g_isr0_hits = 0;
    ie0_config(/*it0=*/true, /*ex0=*/true, /*ea=*/true);
    next_slice();
    ext_set(PIN_INT0, EXT_HIGH);
    poll();  // baseline high
    next_slice();
    ext_set(PIN_INT0, EXT_HIZ);  // driver releases mid-run
    poll();                      // ignored: baseline stays high
    CHECK(g_isr0_hits == 0 && ie0_bit() == 0,
          "F: HiZ sample neither edges nor latches");
    next_slice();
    ext_set(PIN_INT0, EXT_LOW);  // real press after HiZ
    poll();
    CHECK(g_isr0_hits == 1, "F: edge still detected after a HiZ sample");

    // ── G: reset clears latched IEx + throttle, keeps world baseline ───────
    g_isr0_hits = 0;
    ie0_config(/*it0=*/true, /*ex0=*/false, /*ea=*/true);
    next_slice();
    ext_set(PIN_INT0, EXT_HIGH);
    poll();
    next_slice();
    ext_set(PIN_INT0, EXT_LOW);
    poll();  // edge latched, EX0=0 -> pending
    CHECK(ie0_bit() == 1, "G: IE0 latched pending before reset");
    wink_mcs51_extint_reset();  // framework init clears flags + throttle
    CHECK(ie0_bit() == 0, "G: reset clears latched IE0");
    ie0_config(/*it0=*/true, /*ex0=*/true, /*ea=*/true);
    poll();  // immediate (throttle reset); pin still low, baseline already low
    CHECK(g_isr0_hits == 0,
          "G: no spurious vector — baseline is world state kept across reset");

    if (fails) {
        return 1;
    }
    printf("[mcs51-ext] PASS: INT0/INT1 model — edge latch+auto-clear, pending "
           "through disable, level-mode per-slice re-request, 10 ms throttle, "
           "HiZ ignore, reset flag/baseline semantics\n");
    return 0;
}
