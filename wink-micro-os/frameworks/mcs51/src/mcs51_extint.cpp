// SPDX-License-Identifier: Apache-2.0
// MCS-51 external interrupt functional model (Stage 2 T3, ADR-0076 A-class).
// See wink_mcs51_extint.h for the model contract.
#include "wink_mcs51_extint.h"

#include "mcs51_proxy.hpp"
#include "wink_mcs51_clock.h"
#include "wink_mcs51_isr.h"

#include <cstdint>

namespace {

constexpr uint8_t SFR_TCON = 0x88;
constexpr uint8_t SFR_IE   = 0xA8;

// TCON bits: IT0/IE0 (bit 0/1), IT1/IE1 (bit 2/3). IE bits: EX0/EX1
// (bit 0/2), EA (bit 7). INT0 -> vector 0 (P3.2, linear pin 26);
// INT1 -> vector 2 (P3.3, linear pin 27).
constexpr uint8_t TCON_IT0 = 0u;
constexpr uint8_t TCON_IE0 = 1u;
constexpr uint8_t TCON_IT1 = 2u;
constexpr uint8_t TCON_IE1 = 3u;
constexpr uint8_t IE_EX0   = 0u;
constexpr uint8_t IE_EX1   = 2u;
constexpr uint8_t IE_EA    = 7u;

constexpr uint8_t VECTOR_INT0 = 0u;
constexpr uint8_t VECTOR_INT1 = 2u;

constexpr uint16_t PIN_INT0 = (3u << 3) | 2u;  // 26
constexpr uint16_t PIN_INT1 = (3u << 3) | 3u;  // 27

// js_pal_gpio_read_state state codes (mirror JS_GPIO_STATE_*).
constexpr uint8_t EXT_LOW = 0u;
constexpr uint8_t EXT_HIGH = 1u;

// Sample throttle: one evaluation per virtual slice. The external world only
// changes at quota-yield (slice) boundaries, so sampling faster observes
// nothing new and just burns JS bridge calls.
constexpr uint64_t SAMPLE_PERIOD_US = 10000ull;

struct ExtIntLine {
    uint16_t pin;
    uint8_t  vector;
    uint8_t  it_bit;   // TCON ITx bit
    uint8_t  ie_bit;   // TCON IEx bit
    uint8_t  ex_bit;   // IE EXx bit
    uint8_t  last_level;   // 0/1 once known; 0xFF = no definitive sample yet
    uint64_t last_sample_us;
    bool     have_sample;
};

ExtIntLine s_lines[2] = {
    {PIN_INT0, VECTOR_INT0, TCON_IT0, TCON_IE0, IE_EX0, 0xFFu, 0, false},
    {PIN_INT1, VECTOR_INT1, TCON_IT1, TCON_IE1, IE_EX1, 0xFFu, 0, false},
};

// Set by reset(): framework init also zeroes the virtual clock, so a plain
// timestamp reset would leave the first post-init poll throttled for a full
// slice (now=0, last_sample=0 -> 0 < PERIOD). Force one immediate sample.
bool s_sample_due = false;

// Re-entrancy guard: a vectored ISR runs on the same fiber and its SFR accesses
// funnel back through microstep -> poll. IEx is only hardware-cleared AFTER
// dispatch returns, so a nested poll would re-dispatch the same vector forever
// (recursive ISR). The throttle normally blocks this, but the post-reset force
// window bypasses the throttle — never re-enter poll from within poll.
bool s_in_poll = false;

void poll_line(ExtIntLine& ln) {
    uint8_t st = js_pal_gpio_read_state(ln.pin);
    // Resolve the pin level the same way the hardware does for an active-low
    // INT input: a driven 0/1 from the PinArbiter/button wins; HiZ or conflict
    // (no external driver) means the line idles HIGH via the 8051's internal
    // (weak) pull-up — a released button. Returning "no sample" here would
    // never establish a baseline for the common open/HiZ-at-rest wiring, so an
    // undriven INT line is modelled as deasserted (high), not ignored.
    uint8_t level = (st == EXT_LOW) ? EXT_LOW : EXT_HIGH;
    bool was_low = (ln.last_level == EXT_LOW);
    bool now_low = (level == EXT_LOW);
    bool falling = ln.have_sample && !was_low && now_low;
    ln.last_level = level;

    uint8_t tcon = wink_mcs51_sfr_shadow[SFR_TCON];
    uint8_t ie   = wink_mcs51_sfr_shadow[SFR_IE];
    bool gated = (ie & (1u << IE_EA)) && (ie & (1u << ln.ex_bit));
    bool edge_mode = (tcon & (1u << ln.it_bit)) != 0;

    if (edge_mode) {
        // ITx=1: a falling edge latches IEx; hardware clears it when the ISR
        // is vectored. A latched IEx from a slice where the interrupt was
        // disabled stays pending (firmware polls/vector later when enabled).
        if (falling) {
            wink_mcs51_sfr_shadow[SFR_TCON] =
                static_cast<uint8_t>(tcon | (1u << ln.ie_bit));
            tcon = wink_mcs51_sfr_shadow[SFR_TCON];
        }
        bool pending = (tcon & (1u << ln.ie_bit)) != 0;
        if (pending && gated) {
            (void)wink_mcs51_dispatch_vector(ln.vector);
            // Hardware auto-clears edge-mode IEx on vectoring.
            wink_mcs51_sfr_shadow[SFR_TCON] =
                static_cast<uint8_t>(wink_mcs51_sfr_shadow[SFR_TCON] &
                                     ~(1u << ln.ie_bit));
        }
    } else {
        // ITx=0: level mode — a low pin requests the interrupt; throttled to
        // one dispatch per slice by the caller's sample period. IEx is not
        // latched by model (level-triggered, re-requests while held low).
        if (now_low && gated) {
            (void)wink_mcs51_dispatch_vector(ln.vector);
        }
    }
}

}  // namespace

extern "C" {

void wink_mcs51_extint_poll(void) {
    if (s_in_poll) {
        return;  // nested poll from a vectored ISR's SFR access — see guard note
    }
    s_in_poll = true;
    bool force = s_sample_due;
    s_sample_due = false;  // consume before dispatching (ISR re-entry must not see it)
    uint64_t now = wink_mcs51_virtual_us();
    for (ExtIntLine& ln : s_lines) {
        if (!force && ln.have_sample &&
            (now - ln.last_sample_us) < SAMPLE_PERIOD_US) {
            continue;
        }
        ln.last_sample_us = now;
        ln.have_sample = true;
        poll_line(ln);
    }
    s_in_poll = false;
}

void wink_mcs51_extint_reset(void) {
    // The edge baseline (last_level / have_sample) is WORLD state — the
    // external pin level persists across framework inits exactly like the
    // host ext-pin array — so it is deliberately NOT cleared: a press that
    // falls between two runtime runs must still be seen as a high->low edge.
    // Only the per-slice throttle is reset so the first poll after init
    // samples immediately, plus any latched IE0/IE1 flags for a clean start.
    for (ExtIntLine& ln : s_lines) {
        ln.last_sample_us = 0;
    }
    s_sample_due = true;
    wink_mcs51_sfr_shadow[SFR_TCON] &=
        static_cast<uint8_t>(~((1u << TCON_IE0) | (1u << TCON_IE1)));
}

}  // extern "C"
