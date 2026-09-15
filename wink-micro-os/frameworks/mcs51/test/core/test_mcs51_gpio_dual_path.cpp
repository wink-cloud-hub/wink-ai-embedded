// SPDX-License-Identifier: GPL-3.0-only
// Task R0: Characterization test for GPIO dual read path (Read-Pin vs Read-Latch)
// and pin arbitration services (ADR-0077).
#include <cstdio>
#include <cstdint>

#include "wink_mcs51_gpio.h"
#include "mcs51_proxy.hpp"
#include "mcs51_trap.h"

extern "C" {
void wink_mcs51_host_set_ext_pin(uint16_t pin, uint8_t state);
void wink_mcs51_host_ext_pins_reset(void);
void wink_mcs51_user_main(void) {}
void setUp(void) {}
void tearDown(void) {}
}

namespace {

int g_fails = 0;
#define CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            printf("[mcs51-gpio] FAIL: %s (line %d)\n", msg, __LINE__); \
            ++g_fails; \
        } \
    } while (0)

uint8_t s_trap_val = 0;
uint8_t mock_pin_trap_read(void* ctx) {
    (void)ctx;
    return s_trap_val;
}

uint8_t s_trap_written_val = 0xFF;
void mock_pin_trap_write(void* ctx, uint8_t level) {
    (void)ctx;
    s_trap_written_val = level;
}

}  // namespace

int main(void) {
    printf("[mcs51-gpio] Starting Task R0 GPIO dual-path characterization tests...\n");

    // Initialize shadow and traps
    mcs51_trap_reset();
    wink_mcs51_host_ext_pins_reset();

    // ── Test 1: Read-Latch vs Read-Pin distinction ──────────────────────────
    // P1 latch is set to 0xFF (all 1s, weak pull-up inputs).
    // External pin P1.2 is pulled LOW by external driver.
    // Read-Latch must return 1 for bit 2 (latch unchanged).
    // Read-Pin must return 0 for bit 2 (observes external low level).
    {
        mcs51_gpio_sfr_write(1, 0xFFu);
        CHECK(mcs51_gpio_read_latch(1) == 0xFFu, "T1: Latch write failed");

        // External pin for P1.2 is global_pin = (1 << 3) | 2 = 10
        wink_mcs51_host_set_ext_pin(10, 0u);  // Driven LOW

        CHECK(mcs51_gpio_bit_read_latch(1, 2) == 1u, "T1: Read-Latch bit 2 must be 1");
        CHECK(mcs51_gpio_read_latch(1) == 0xFFu, "T1: Read-Latch whole port must be 0xFF");

        CHECK(mcs51_gpio_bit_read_pin(1, 2) == 0u, "T1: Read-Pin bit 2 must be 0 (externally low)");
        CHECK(mcs51_gpio_read_pin(1) == 0xFBu, "T1: Read-Pin whole port must be 0xFB (bit 2 low)");
    }

    // ── Test 2: Priority 1 (internal on_read trap override) ─────────────────
    // Even if external pin is driven LOW, an active on_read trap (e.g. SPI/ADC DO)
    // takes highest priority (Priority 1 > Priority 2).
    {
        s_trap_val = 1;
        mcs51_trap_register_read(1, 2, mock_pin_trap_read, nullptr);

        CHECK(mcs51_gpio_bit_read_pin(1, 2) == 1u, "T2: Priority 1 trap must override external pin");
        CHECK(mcs51_gpio_read_pin(1) == 0xFFu, "T2: Whole port Read-Pin must see trap level");

        s_trap_val = 0;
        CHECK(mcs51_gpio_bit_read_pin(1, 2) == 0u, "T2: Trap returning 0 reflects 0");

        // Unregister trap
        mcs51_trap_register_read(1, 2, nullptr, nullptr);
    }

    // ── Test 3: Priority 3 (HiZ fallback to latch) ──────────────────────────
    // When external pin is HiZ (2), Read-Pin falls back to latch.
    {
        wink_mcs51_host_set_ext_pin(10, 2u);  // HiZ
        mcs51_gpio_bit_write(1, 2, 0u);
        CHECK(mcs51_gpio_bit_read_latch(1, 2) == 0u, "T3: Latch bit is 0");
        CHECK(mcs51_gpio_bit_read_pin(1, 2) == 0u, "T3: HiZ pin falls back to latch 0");

        mcs51_gpio_bit_write(1, 2, 1u);
        CHECK(mcs51_gpio_bit_read_latch(1, 2) == 1u, "T3: Latch bit is 1");
        CHECK(mcs51_gpio_bit_read_pin(1, 2) == 1u, "T3: HiZ pin falls back to latch 1");
    }

    // ── Test 4: Write path and write trap dispatch ──────────────────────────
    {
        s_trap_written_val = 0xFF;
        mcs51_trap_register_write(1, 3, mock_pin_trap_write, nullptr);

        mcs51_gpio_bit_write(1, 3, 0u);
        CHECK(s_trap_written_val == 0u, "T4: Write trap fired with 0");
        CHECK(mcs51_gpio_bit_read_latch(1, 3) == 0u, "T4: Latch holds written 0");

        mcs51_gpio_bit_write(1, 3, 1u);
        CHECK(s_trap_written_val == 1u, "T4: Write trap fired with 1");
        CHECK(mcs51_gpio_bit_read_latch(1, 3) == 1u, "T4: Latch holds written 1");

        // Whole SFR write
        s_trap_written_val = 0xFF;
        mcs51_gpio_sfr_write(1, 0xF7u);  // bit 3 cleared
        CHECK(s_trap_written_val == 0u, "T4: SFR write dispatched bit 3 edge to trap");
        CHECK(mcs51_gpio_read_latch(1) == 0xF7u, "T4: Whole latch holds 0xF7");

        mcs51_trap_register_write(1, 3, nullptr, nullptr);
    }

    // ── Test 5: WinkSfr and WinkSbit RMW integration ────────────────────────
    // Verifies that WinkSfr/WinkSbit RMW operators correctly use Read-Latch
    // so an externally-held-low input is NOT latched low on RMW!
    {
        WinkSfr P1(0x90);
        P1 = 0xFFu;

        // Pin P1.0 externally pulled low
        wink_mcs51_host_set_ext_pin(8, 0u);  // global_pin = (1<<3)|0 = 8

        // Verify Read-Pin sees 0 for P1.0
        CHECK(static_cast<uint8_t>(P1) == 0xFEu, "T5: Read-Pin sees external low on P1.0");

        // Perform RMW operation: P1 |= 0x02 (set bit 1)
        // If RMW used Read-Pin, bit 0 would be written back as 0!
        // Because RMW uses Read-Latch, bit 0 latch remains 1!
        P1 |= 0x02u;

        CHECK(mcs51_gpio_read_latch(1) == 0xFFu, "T5: Latch bit 0 preserved across whole-SFR RMW!");

        // Release external pin back to HiZ
        wink_mcs51_host_set_ext_pin(8, 2u);
        CHECK(static_cast<uint8_t>(P1) == 0xFFu, "T5: Released pin returns to high via latch pull-up");
    }

    if (g_fails) {
        printf("[mcs51-gpio] %d FAILURES!\n", g_fails);
        return 1;
    }

    printf("[mcs51-gpio] ALL TESTS PASSED: Read-Pin vs Read-Latch, 3-tier arbitration, and RMW latch isolation verified.\n");
    return 0;
}
