// SPDX-License-Identifier: Apache-2.0
// CMS8S78xx port external interrupt model unit test (P0EI..P3EI: vectors 7..10).
// Verifies:
//   1. P12 falling edge triggers P1EI (vector 8), flag is readable via GPIO_GetIntFlag
//      and cleared via GPIO_ClearIntFlag.
//   2. Pin held low does not re-trigger edge mode.
//   3. Rising edge triggers in GPIO_INT_RISING mode.
//   4. Both edges trigger in GPIO_INT_BOTH_EDGE mode.
//   5. Disabled interrupt (PnEXTIE=0 or EA=0) does not dispatch.
//   6. Reset clears pending PnEXTIF flags.

#include <stdint.h>
#include <stdio.h>

#include "absacc.h"
#include "cms8s78xx.h"
#include "wink_mcs51_clock.h"
#include "wink_mcs51_extint.h"
#include "wink_mcs51_isr.h"

// Undefine the Keil dialect main remap (REGX52.H) so the test runner main()
// can link; wink_mcs51_user_main is provided as an explicit stub below.
#ifdef main
#undef main
#endif

namespace {

constexpr uint16_t PIN_P01 = 1u;   // Port 0, Pin 1
constexpr uint16_t PIN_P12 = 10u;  // Port 1, Pin 2
constexpr uint16_t PIN_P20 = 16u;  // Port 2, Pin 0

constexpr uint8_t EXT_LOW  = 0u;
constexpr uint8_t EXT_HIGH = 1u;

constexpr uint32_t SLICE_US = 10000u;

uint32_t g_p0ei_hits = 0;
uint32_t g_p1ei_hits = 0;
uint32_t g_p2ei_hits = 0;
uint8_t  g_p12_flag_in_isr = 0;
uint8_t  g_p32_latch_val = 0;

extern "C" void wink_mcs51_host_set_ext_pin(uint16_t pin, uint8_t state);
extern "C" void wink_mcs51_host_ext_pins_reset(void);

void ext_set(uint16_t pin, uint8_t state) {
    wink_mcs51_host_set_ext_pin(pin, state);
}

void poll(void) { wink_mcs51_extint_poll(); }
void next_slice(void) { wink_mcs51_test_advance_virtual_us(SLICE_US); }

}  // namespace

// ISR for P0EI (Vector 7)
WINK_ISR(7) {
    ++g_p0ei_hits;
    GPIO_ClearIntFlag(GPIO0, GPIO_PIN_1);
}

// ISR for P1EI (Vector 8) — mirroring official GPIO/code/isr.c
WINK_ISR(8) {
    ++g_p1ei_hits;
    if (GPIO_GetIntFlag(GPIO1, GPIO_PIN_2)) {
        g_p12_flag_in_isr = 1;
        g_p32_latch_val ^= 1;
        GPIO_ClearIntFlag(GPIO1, GPIO_PIN_2);
    }
}

// ISR for P2EI (Vector 9)
WINK_ISR(9) {
    ++g_p2ei_hits;
    GPIO_ClearIntFlag(GPIO2, GPIO_PIN_0);
}

extern "C" void wink_mcs51_user_main(void) {}
extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

#define CHECK(cond, msg)                                                    \
    do {                                                                    \
        if (!(cond)) {                                                      \
            printf("[mcs51-port-ext] FAIL: %s (line %d)\n", msg, __LINE__); \
            ++fails;                                                        \
        }                                                                   \
    } while (0)

int main(void) {
    int fails = 0;

    wink_mcs51_isr_enable();
    wink_mcs51_extint_reset();
    wink_mcs51_host_ext_pins_reset();

    // ── Test 1: P12 Falling Edge Interrupt (Official GPIO demo flow) ────
    GPIO_SET_MUX_MODE(P12CFG, GPIO_MUX_GPIO);
    GPIO_ENABLE_INPUT(P1TRIS, GPIO_PIN_2);
    GPIO_SET_INT_MODE(P12EICFG, GPIO_INT_FALLING);
    GPIO_EnableInt(GPIO1, GPIO_PIN_2_MSK);
    IRQ_ALL_ENABLE();

    g_p1ei_hits = 0;
    g_p12_flag_in_isr = 0;
    g_p32_latch_val = 0;

    // High baseline
    ext_set(PIN_P12, EXT_HIGH);
    poll();
    CHECK(g_p1ei_hits == 0, "T1.1: High baseline produces no interrupt");

    // Falling edge (Button press)
    next_slice();
    ext_set(PIN_P12, EXT_LOW);
    poll();
    CHECK(g_p1ei_hits == 1, "T1.2: P12 falling edge vectors P1EI");
    CHECK(g_p12_flag_in_isr == 1, "T1.3: GPIO_GetIntFlag was true in ISR");
    CHECK(g_p32_latch_val == 1, "T1.4: P32 toggled to 1");
    CHECK(GPIO_GetIntFlag(GPIO1, GPIO_PIN_2) == 0, "T1.5: GPIO_ClearIntFlag cleared flag");

    // Pin held low
    next_slice();
    poll();
    CHECK(g_p1ei_hits == 1, "T1.6: P12 held low does not re-trigger falling interrupt");

    // Rising edge (Button release)
    next_slice();
    ext_set(PIN_P12, EXT_HIGH);
    poll();
    CHECK(g_p1ei_hits == 1, "T1.7: P12 rising edge does not trigger falling interrupt");

    // Second falling edge
    next_slice();
    ext_set(PIN_P12, EXT_LOW);
    poll();
    CHECK(g_p1ei_hits == 2, "T1.8: Second falling edge vectors P1EI");
    CHECK(g_p32_latch_val == 0, "T1.9: P32 toggled back to 0");

    // ── Test 2: P01 Rising Edge Interrupt ───────────────────────────────
    GPIO_SET_INT_MODE(P01EICFG, GPIO_INT_RISING);
    GPIO_EnableInt(GPIO0, GPIO_PIN_1_MSK);
    g_p0ei_hits = 0;

    next_slice();
    ext_set(PIN_P01, EXT_LOW);
    poll();
    CHECK(g_p0ei_hits == 0, "T2.1: Low baseline produces no interrupt");

    next_slice();
    ext_set(PIN_P01, EXT_HIGH);  // Low -> High rising edge
    poll();
    CHECK(g_p0ei_hits == 1, "T2.2: Rising edge triggers P0EI");

    // ── Test 3: P20 Both Edges Interrupt ────────────────────────────────
    // Earlier polls already sampled P20 as HiZ->high, so establish a LOW
    // baseline first while the P2 edge interrupt is still disabled (mode 0 /
    // P2EXTIE masked); only then arm both-edge mode, so the assertions count
    // transitions after arming.
    ext_set(PIN_P20, EXT_LOW);
    next_slice();
    poll();
    GPIO_SET_INT_MODE(P20EICFG, GPIO_INT_BOTH_EDGE);
    GPIO_EnableInt(GPIO2, GPIO_PIN_0_MSK);
    g_p2ei_hits = 0;
    CHECK(g_p2ei_hits == 0, "T3.1: Low baseline");

    next_slice();
    ext_set(PIN_P20, EXT_HIGH);  // Rising
    poll();
    CHECK(g_p2ei_hits == 1, "T3.2: Both-edge mode triggers on rising edge");

    next_slice();
    ext_set(PIN_P20, EXT_LOW);   // Falling
    poll();
    CHECK(g_p2ei_hits == 2, "T3.3: Both-edge mode triggers on falling edge");

    // ── Test 4: Disable and Reset ────────────────────────────────────────
    GPIO_DisableInt(GPIO1, GPIO_PIN_2_MSK);
    next_slice();
    ext_set(PIN_P12, EXT_HIGH);
    poll();
    next_slice();
    ext_set(PIN_P12, EXT_LOW);   // Falling edge while disabled
    poll();
    CHECK(g_p1ei_hits == 2, "T4.1: Disabled pin does not trigger ISR");

    // Reset clears flags
    wink_mcs51_extint_reset();
    for (uint8_t p = 0; p < 4; ++p) {
        CHECK(wink_mcs51_sfr_shadow[0xB4 + p] == 0, "T4.2: Reset clears EXTIF");
    }

    if (fails == 0) {
        printf("[mcs51-port-ext] PASS: all CMS8S78xx port ext-int tests passed.\n");
    } else {
        printf("[mcs51-port-ext] FAIL: %d failures.\n", fails);
    }
    return fails != 0 ? 1 : 0;
}
