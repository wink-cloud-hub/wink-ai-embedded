// SPDX-License-Identifier: Apache-2.0
// A-05 GPIO direction modeling unit test (GAP-08 + GAP-25 analog sub-item):
// TRIS-gated output suppression, pull-up HiZ default, analog digital-read
// mask. Classic family keeps legacy unconditional drive.
#include <stdint.h>
#include <stdio.h>

#include "mcs51_context.h"
#include "mcs51_test_harness.h"
#include "mcs51_trap.h"
#include "wink_mcs51_gpio.h"

namespace {

int g_fails = 0;
#define CHECK(cond, msg)                          \
    do {                                          \
        if (!(cond)) {                            \
            printf("[gpio-dir] FAIL: %s\n", msg); \
            ++g_fails;                            \
        }                                         \
    } while (0)

uint32_t g_host_notifies_before(void);

}  // namespace

extern "C" void wink_mcs51_user_main(void) {}
extern "C" void wink_mcs51_host_set_ext_pin(uint16_t pin, uint8_t state);
extern "C" void wink_mcs51_host_ext_pins_reset(void);
extern "C" uint32_t wink_mcs51_host_gpio_notify_count(void);
extern "C" void wink_mcs51_host_gpio_notify_reset(void);
extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

namespace {
uint32_t g_host_notifies_before(void) {
    return wink_mcs51_host_gpio_notify_count();
}
}  // namespace

int main(void) {
    printf("[gpio-dir] Starting A-05 direction modeling tests...\n");
    mcs51_trap_reset();
    wink_mcs51_host_ext_pins_reset();
    wink_mcs51_host_gpio_notify_reset();

    // ── 1) Classic family: unconditional drive (legacy) ────────────────────
    {
        mcs51_test_register_family(MCS51_FAMILY_CLASSIC);
        mcs51_context_set_family(MCS51_FAMILY_CLASSIC);
        mcs51_context_reset(mcs51_get_context());
        wink_mcs51_gpio_diag_reset();
        wink_mcs51_host_gpio_notify_reset();
        const uint32_t n0 = g_host_notifies_before();
        mcs51_gpio_sfr_write(1, 0xFFu);
        mcs51_gpio_bit_write(1, 0, 0u);
        CHECK(wink_mcs51_host_gpio_notify_count() == n0 + 1u,
              "T1: classic must notify unconditionally");
        CHECK(wink_mcs51_gpio_output_suppressed_count() == 0u,
              "T1: classic must not suppress");
    }

    // ── 2) CMS8S TRIS=input suppresses external notify, latch holds ───────
    {
        mcs51_test_register_family(MCS51_FAMILY_CMS8S78XX);
        mcs51_context_set_family(MCS51_FAMILY_CMS8S78XX);
        mcs51_context_reset(mcs51_get_context());
        wink_mcs51_gpio_diag_reset();
        wink_mcs51_host_gpio_notify_reset();
        // TRIS reset is 0x00 (all input); latch write must not drive.
        // (Reset latch is 0xFF, so drive a 1->0 edge to observe gating.)
        const uint32_t n0 = g_host_notifies_before();
        mcs51_gpio_bit_write(2, 0, 0u);
        CHECK(mcs51_gpio_bit_read_latch(2, 0) == 0u, "T2: latch must hold");
        CHECK(wink_mcs51_host_gpio_notify_count() == n0,
              "T2: input-direction write must not notify");
        CHECK(wink_mcs51_gpio_output_suppressed_count() == 1u,
              "T2: suppression must count");
        // Configure output (TRIS=1) -> notify resumes.
        mcs51_get_context()->sfr_shadow[0xA2u] |= 0x01u;  // P2TRIS.0 = output
        mcs51_gpio_bit_write(2, 0, 1u);
        CHECK(wink_mcs51_host_gpio_notify_count() == n0 + 1u,
              "T2: output-direction write must notify");
    }

    // ── 3) HiZ + pull-up defaults to 1 ─────────────────────────────────────
    {
        mcs51_context_reset(mcs51_get_context());
        wink_mcs51_gpio_diag_reset();
        // P0.4 input (TRIS=0) + P0UP.4=1, external HiZ (reset default).
        mcs51_get_context()->sfr_shadow[0x9Au] &= ~0x10u;   // P0TRIS.4 = input
        mcs51_get_context()->xdata_shadow[0xF00Au] |= 0x10u;  // P0UP.4 = on
        wink_mcs51_host_set_ext_pin(4, 2u);                   // HiZ
        CHECK(mcs51_gpio_bit_read_pin(0, 4) == 1u,
              "T3: HiZ+pullup must read 1");
        // Pull-up off -> falls back to latch (0 after reset? latch=0xFF so 1;
        // drive latch low first to distinguish).
        mcs51_get_context()->xdata_shadow[0xF00Au] &= ~0x10u;
        mcs51_gpio_bit_write(0, 4, 0u);
        CHECK(mcs51_gpio_bit_read_pin(0, 4) == 0u,
              "T3: HiZ without pullup must fall back to latch");
    }

    // ── 4) Analog-configured pin digital read returns latch + counts ──────
    {
        mcs51_context_reset(mcs51_get_context());
        wink_mcs51_gpio_diag_reset();
        mcs51_get_context()->xdata_shadow[0xF000u] = 0x01u;  // P00CFG=AN0
        wink_mcs51_host_set_ext_pin(0, 0u);                   // external low
        mcs51_gpio_bit_write(0, 0, 1u);  // latch 1
        CHECK(mcs51_gpio_bit_read_pin(0, 0) == 1u,
              "T4: analog digital-read must return latch");
        CHECK(wink_mcs51_gpio_analog_read_count() == 1u,
              "T4: analog read must count");
        mcs51_get_context()->xdata_shadow[0xF000u] = 0x00u;  // restore GPIO
    }

    // ── 5) health_pot-equivalent config: outputs drive, buttons read ───────
    {
        mcs51_context_reset(mcs51_get_context());
        wink_mcs51_gpio_diag_reset();
        wink_mcs51_host_gpio_notify_reset();
        // health_pot main excerpt: P1 all output, P0.4/5 input + pullup.
        mcs51_get_context()->sfr_shadow[0xA1u] = 0xFFu;  // P1TRIS
        mcs51_get_context()->sfr_shadow[0x9Au] =
            static_cast<uint8_t>((0x00u | 0x46u) & ~0x30u);
        mcs51_get_context()->xdata_shadow[0xF00Au] |= 0x30u;  // P0UP.4/5
        const uint32_t n0 = g_host_notifies_before();
        mcs51_gpio_bit_write(1, 0, 0u);  // segment drive
        CHECK(wink_mcs51_host_gpio_notify_count() == n0 + 1u,
              "T5: configured output must notify");
        wink_mcs51_host_set_ext_pin(4, 1u);  // button released (high)
        CHECK(mcs51_gpio_bit_read_pin(0, 4) == 1u, "T5: button high must read 1");
        wink_mcs51_host_set_ext_pin(4, 0u);  // pressed (low)
        CHECK(mcs51_gpio_bit_read_pin(0, 4) == 0u, "T5: button low must read 0");
        CHECK(wink_mcs51_gpio_analog_read_count() == 0u,
              "T5: health_pot config must not trip analog mask");
    }

    if (g_fails) {
        printf("[gpio-dir] %d FAILURES!\n", g_fails);
        return 1;
    }
    printf("[gpio-dir] ALL TESTS PASSED\n");
    return 0;
}
