// SPDX-License-Identifier: Apache-2.0
// MCS-51 external interrupt functional model (Stage 2 T3, ADR-0076 A-class).
// See wink_mcs51_extint.h for the model contract.
#include "wink_mcs51_extint.h"

#include "absacc.h"
#include "mcs51_proxy.hpp"
#include "wink_mcs51_clock.h"
#include "wink_mcs51_isr.h"

#include <cstdint>

namespace {

constexpr uint8_t SFR_TCON = 0x88;
constexpr uint8_t SFR_IE   = 0xA8;

// TCON bits: IT0/IE0 (bit 0/1), IT1/IE1 (bit 2/3). IE bits: EX0/EX1
// (bit 0/2), EA (bit 7). INT0 -> vector 0; INT1 -> vector 2.
//
// On a classic 8051 the INT inputs are bond-fixed to P3.2 (linear pin 26) and
// P3.3 (pin 27). The CMS8S78xx routes them through a pin-share mux instead:
// PS_INT0 (XSFR 0xF0C0) / PS_INT1 (XSFR 0xF0C1), value 0xPN = port P, pin N
// (ref manual §7.2.3; reset 0x7F = no pin connected). The vendor EXTINT demo
// muxes INT0->P3.0 (pin 24) and INT1->P3.1 (pin 25). An unprogrammed/reserved
// selector falls back to the classic P3.2/P3.3 pins, so generic-8051 firmware
// that never touches PS keeps the textbook mapping.
constexpr uint8_t TCON_IT0 = 0u;
constexpr uint8_t TCON_IE0 = 1u;
constexpr uint8_t TCON_IT1 = 2u;
constexpr uint8_t TCON_IE1 = 3u;
constexpr uint8_t IE_EX0   = 0u;
constexpr uint8_t IE_EX1   = 2u;
constexpr uint8_t IE_EA    = 7u;

constexpr uint8_t VECTOR_INT0 = 0u;
constexpr uint8_t VECTOR_INT1 = 2u;

constexpr uint16_t PIN_INT0 = (3u << 3) | 2u;  // 26, classic default
constexpr uint16_t PIN_INT1 = (3u << 3) | 3u;  // 27, classic default

// js_pal_gpio_read_state state codes (mirror JS_GPIO_STATE_*).
constexpr uint8_t EXT_LOW = 0u;
constexpr uint8_t EXT_HIGH = 1u;

// Sample throttle: one evaluation per virtual slice. The external world only
// changes at quota-yield (slice) boundaries, so sampling faster observes
// nothing new and just burns JS bridge calls.
constexpr uint64_t SAMPLE_PERIOD_US = 10000ull;

// CMS8S78xx port external interrupt model (P0EI..P3EI: vectors 7..10)
constexpr uint8_t SFR_P0EXTIE = 0xACu;
constexpr uint8_t SFR_P0EXTIF = 0xB4u;
constexpr uint8_t PORT_PINS[4] = {8u, 8u, 6u, 4u};
constexpr uint16_t PORT_EICFG_BASE[4] = {0xF080u, 0xF088u, 0xF090u, 0xF098u};
constexpr uint8_t PORT_VECTORS[4] = {7u, 8u, 9u, 10u};

// Pin-share selector XSFR addresses (ref manual §7.2.3; reset value 0x7F).
constexpr uint16_t XSFR_PS_INT0 = 0xF0C0u;
constexpr uint16_t XSFR_PS_INT1 = 0xF0C1u;
constexpr uint8_t  PS_RESET     = 0x7Fu;  // "no pin connected"

// Decode a PS_XX<6:0> selector: 0xPN encodes port P (bits 6:4) + pin N
// (bits 3:0), e.g. 0x30 = P3.0. The reset value (0x7F), 0xFF, and any
// out-of-range encoding mean "not connected here" -> classic default pin.
uint16_t resolve_int_pin(uint16_t ps_addr, uint16_t fallback_pin) {
    uint8_t sel  = wink_mcs51_xdata_shadow[ps_addr];
    uint8_t port = (sel >> 4) & 0x07u;
    uint8_t bit  = sel & 0x0Fu;
    if (port < 4u && bit < PORT_PINS[port]) {
        return static_cast<uint16_t>((port << 3) | bit);
    }
    return fallback_pin;
}

struct ExtIntLine {
    uint16_t pin;            // currently-resolved input pin (linear 0..27)
    uint16_t fallback_pin;   // classic-8051 pin when PS is unprogrammed
    uint16_t ps_addr;        // XSFR pin-share selector address
    uint8_t  vector;
    uint8_t  it_bit;   // TCON ITx bit
    uint8_t  ie_bit;   // TCON IEx bit
    uint8_t  ex_bit;   // IE EXx bit
    uint8_t  last_level;   // 0/1 once known; 0xFF = no definitive sample yet
    uint64_t last_sample_us;
    bool     have_sample;
};

ExtIntLine s_lines[2] = {
    {PIN_INT0, PIN_INT0, XSFR_PS_INT0, VECTOR_INT0, TCON_IT0, TCON_IE0, IE_EX0, 0xFFu, 0, false},
    {PIN_INT1, PIN_INT1, XSFR_PS_INT1, VECTOR_INT1, TCON_IT1, TCON_IE1, IE_EX1, 0xFFu, 0, false},
};

struct PortPinState {
    uint8_t last_level;
    bool    have_sample;
};

PortPinState s_port_pins[4][8] = {
    {{0xFFu, false}, {0xFFu, false}, {0xFFu, false}, {0xFFu, false}, {0xFFu, false}, {0xFFu, false}, {0xFFu, false}, {0xFFu, false}},
    {{0xFFu, false}, {0xFFu, false}, {0xFFu, false}, {0xFFu, false}, {0xFFu, false}, {0xFFu, false}, {0xFFu, false}, {0xFFu, false}},
    {{0xFFu, false}, {0xFFu, false}, {0xFFu, false}, {0xFFu, false}, {0xFFu, false}, {0xFFu, false}, {0xFFu, false}, {0xFFu, false}},
    {{0xFFu, false}, {0xFFu, false}, {0xFFu, false}, {0xFFu, false}, {0xFFu, false}, {0xFFu, false}, {0xFFu, false}, {0xFFu, false}},
};

uint64_t s_port_last_sample_us = 0;

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
    // Resolve the physical INT pin through the PS_INTx pin-share mux
    // (0xF0C0/0xF0C1). A mux change is a configuration event, not world
    // state: drop the edge baseline so the old pin's last level and the new
    // pin's first sample cannot synthesize a spurious edge.
    uint16_t pin = resolve_int_pin(ln.ps_addr, ln.fallback_pin);
    bool mux_changed = (pin != ln.pin);
    if (mux_changed) {
        ln.pin = pin;
        ln.last_level = 0xFFu;
    }
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
    bool falling = !mux_changed && ln.have_sample && !was_low && now_low;
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

void poll_port_ints(bool force, uint64_t now) {
    if (!force && (now - s_port_last_sample_us) < SAMPLE_PERIOD_US) {
        return;
    }
    s_port_last_sample_us = now;

    uint8_t ie = wink_mcs51_sfr_shadow[SFR_IE];
    bool ea = (ie & (1u << IE_EA)) != 0;

    for (uint8_t p = 0; p < 4u; ++p) {
        uint8_t extie = wink_mcs51_sfr_shadow[SFR_P0EXTIE + p];
        uint8_t extif = wink_mcs51_sfr_shadow[SFR_P0EXTIF + p];
        uint8_t npins = PORT_PINS[p];

        for (uint8_t b = 0; b < npins; ++b) {
            uint16_t pin = static_cast<uint16_t>((p << 3) | b);
            uint8_t st = js_pal_gpio_read_state(pin);
            uint8_t level = (st == EXT_LOW) ? EXT_LOW : EXT_HIGH;
            PortPinState& ps = s_port_pins[p][b];
            bool was_low = (ps.last_level == EXT_LOW);
            bool now_low = (level == EXT_LOW);
            bool have = ps.have_sample;
            ps.last_level = level;
            ps.have_sample = true;

            if ((extie & (1u << b)) != 0 && have) {
                uint16_t eicfg_addr = static_cast<uint16_t>(PORT_EICFG_BASE[p] + b);
                uint8_t mode = wink_mcs51_xdata_shadow[eicfg_addr] & 0x03u;
                bool match = false;
                if (mode == 1u) {
                    match = was_low && !now_low;        // Rising
                } else if (mode == 2u) {
                    match = !was_low && now_low;        // Falling
                } else if (mode == 3u) {
                    match = (was_low != now_low);       // Both edges
                }
                if (match) {
                    extif |= static_cast<uint8_t>(1u << b);
                    wink_mcs51_sfr_shadow[SFR_P0EXTIF + p] = extif;
                }
            }
        }

        // Interrupt pending: software clears the flag via GPIO_ClearIntFlag in the ISR
        if (ea && (extif & extie) != 0) {
            (void)wink_mcs51_dispatch_vector(PORT_VECTORS[p]);
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
    poll_port_ints(force, now);
    s_in_poll = false;
}

void wink_mcs51_extint_reset(void) {
    // The edge baseline (last_level / have_sample) is WORLD state — the
    // external pin level persists across framework inits exactly like the
    // host ext-pin array — so it is deliberately NOT cleared: a press that
    // falls between two runtime runs must still be seen as a high->low edge.
    // Only the per-slice throttle is reset so the first poll after init
    // samples immediately, plus any latched flags for a clean start.
    for (ExtIntLine& ln : s_lines) {
        ln.last_sample_us = 0;
    }
    s_port_last_sample_us = 0;
    s_sample_due = true;
    wink_mcs51_sfr_shadow[SFR_TCON] &=
        static_cast<uint8_t>(~((1u << TCON_IE0) | (1u << TCON_IE1)));
    for (uint8_t p = 0; p < 4u; ++p) {
        wink_mcs51_sfr_shadow[SFR_P0EXTIF + p] = 0;
    }
    // PS_INT0/PS_INT1 pin-share selectors reset to 0x7F ("no pin connected",
    // ref manual §7.2.3); resolve_int_pin() maps that to the classic P3.2/
    // P3.3 INT pins. Bridge init calls this AFTER wink_mcs51_xdata_reset(),
    // so the xdata zeroing does not wipe the seed.
    wink_mcs51_xdata_shadow[XSFR_PS_INT0] = PS_RESET;
    wink_mcs51_xdata_shadow[XSFR_PS_INT1] = PS_RESET;
}

}  // extern "C"
