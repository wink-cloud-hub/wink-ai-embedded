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

uint32_t g_hook_may_drive = 0u;
uint32_t g_hook_is_analog = 0u;
uint32_t g_hook_pullup = 0u;

bool count_may_drive(struct Mcu51Context*, uint16_t, uint8_t) {
    ++g_hook_may_drive;
    return true;
}

bool count_is_analog(struct Mcu51Context*, uint16_t) {
    ++g_hook_is_analog;
    return false;
}

uint8_t count_pullup(struct Mcu51Context*, uint16_t) {
    ++g_hook_pullup;
    return 0u;
}

void hook_counts_reset(void) {
    g_hook_may_drive = 0u;
    g_hook_is_analog = 0u;
    g_hook_pullup = 0u;
}

}  // namespace

extern "C" void wink_mcs51_user_main(void) {}
extern "C" void wink_mcs51_host_set_ext_pin(uint16_t pin, uint8_t state);
extern "C" void wink_mcs51_host_ext_pins_reset(void);
extern "C" uint32_t wink_mcs51_host_gpio_notify_count(void);
extern "C" void wink_mcs51_host_gpio_notify_reset(void);
extern "C" uint16_t wink_mcs51_host_gpio_notify_pin(uint32_t i);
extern "C" uint8_t wink_mcs51_host_gpio_notify_level(uint32_t i);
extern "C" uint8_t wink_mcs51_host_gpio_notify_strength(uint32_t i);
// P3: direction-change seam (same entry the SFR proxy uses) + release log.
extern "C" void wink_mcs51_on_sfr_write(uint8_t addr, uint8_t old_val,
                                         uint8_t new_val);
extern "C" uint32_t wink_mcs51_host_gpio_release_count(void);
extern "C" uint16_t wink_mcs51_host_gpio_release_pin(uint32_t i);
extern "C" void wink_mcs51_host_gpio_release_reset(void);
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
        wink_mcs51_host_gpio_release_reset();
        // TRIS reset is 0x00 (all input); latch write must not drive.
        // (Reset latch is 0xFF, so drive a 1->0 edge to observe gating.)
        const uint32_t n0 = g_host_notifies_before();
        mcs51_gpio_bit_write(2, 0, 0u);
        CHECK(mcs51_gpio_bit_read_latch(2, 0) == 0u, "T2: latch must hold");
        CHECK(wink_mcs51_host_gpio_notify_count() == n0,
              "T2: input-direction write must not notify");
        CHECK(wink_mcs51_gpio_output_suppressed_count() == 1u,
              "T2: suppression must count");
        // Configure output through the SFR proxy seam (P3): the direction
        // change itself re-drives the latched 0 onto the pad.
        mcs51_get_context()->sfr_shadow[0xA2u] = 0x01u;  // P2TRIS.0 = output
        wink_mcs51_on_sfr_write(0xA2u, 0x00u, 0x01u);
        CHECK(wink_mcs51_host_gpio_notify_count() == n0 + 1u &&
              wink_mcs51_host_gpio_notify_pin(n0) == 16u &&
              wink_mcs51_host_gpio_notify_level(n0) == 0u &&
              wink_mcs51_host_gpio_notify_strength(n0) == 3u,
              "T2: 0->1 TRIS must re-drive the latched 0");
        mcs51_gpio_bit_write(2, 0, 1u);
        CHECK(wink_mcs51_host_gpio_notify_count() == n0 + 2u,
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

    // ── 6) L1: standard parts pay zero hook indirection (caps short-circuit)
    {
        mcs51_test_register_family(MCS51_FAMILY_CLASSIC);
        mcs51_context_set_family(MCS51_FAMILY_CLASSIC);
        mcs51_context_reset(mcs51_get_context());
        Mcu51Context* ctx = mcs51_get_context();
        ctx->gpio_hooks.may_drive = count_may_drive;
        ctx->gpio_hooks.is_analog = count_is_analog;
        ctx->gpio_hooks.pullup = count_pullup;
        hook_counts_reset();
        mcs51_gpio_sfr_write(1, 0xFEu);
        mcs51_gpio_bit_write(1, 0, 1u);
        (void)mcs51_gpio_read_pin(1);
        (void)mcs51_gpio_bit_read_pin(1, 0);
        CHECK(g_hook_may_drive == 0u, "T6: classic must not call may_drive");
        CHECK(g_hook_is_analog == 0u, "T6: classic must not call is_analog");
        CHECK(g_hook_pullup == 0u, "T6: classic must not call pullup");
    }

    // ── 7) Enhanced parts route through the hooks ──────────────────────
    {
        mcs51_test_register_family(MCS51_FAMILY_CMS8S78XX);
        mcs51_context_set_family(MCS51_FAMILY_CMS8S78XX);
        mcs51_context_reset(mcs51_get_context());
        Mcu51Context* ctx = mcs51_get_context();
        ctx->gpio_hooks.may_drive = count_may_drive;
        ctx->gpio_hooks.is_analog = count_is_analog;
        ctx->gpio_hooks.pullup = count_pullup;
        hook_counts_reset();
        mcs51_gpio_sfr_write(1, 0xFEu);
        mcs51_gpio_bit_write(1, 0, 1u);
        (void)mcs51_gpio_read_pin(1);
        (void)mcs51_gpio_bit_read_pin(1, 0);
        CHECK(g_hook_may_drive > 0u, "T7: enhanced must call may_drive");
        CHECK(g_hook_is_analog > 0u, "T7: enhanced must call is_analog");
        CHECK(g_hook_pullup > 0u, "T7: enhanced must call pullup");
    }

    // ── 8) P3: TRIS input->output re-drives the latch ─────────────────────
    {
        mcs51_test_register_family(MCS51_FAMILY_CMS8S78XX);
        mcs51_context_set_family(MCS51_FAMILY_CMS8S78XX);
        mcs51_context_reset(mcs51_get_context());
        wink_mcs51_gpio_diag_reset();
        wink_mcs51_host_gpio_notify_reset();
        wink_mcs51_host_gpio_release_reset();
        mcs51_gpio_bit_write(2, 3, 0u);  // latch 0 while input: suppressed
        CHECK(wink_mcs51_host_gpio_notify_count() == 0u,
              "T8: input latch write stays suppressed");
        mcs51_get_context()->sfr_shadow[0xA2u] = 0x08u;  // P2TRIS.3 = output
        wink_mcs51_on_sfr_write(0xA2u, 0x00u, 0x08u);
        CHECK(wink_mcs51_host_gpio_notify_count() == 1u &&
              wink_mcs51_host_gpio_notify_pin(0) == 19u &&
              wink_mcs51_host_gpio_notify_level(0) == 0u &&
              wink_mcs51_host_gpio_notify_strength(0) == 3u,
              "T8: 0->1 TRIS must re-drive latch 0 as SUPPLY");
        CHECK(wink_mcs51_host_gpio_release_count() == 0u,
              "T8: re-drive is a write, not a release");
    }

    // ── 9) P3: TRIS output->input releases the MCU driver ─────────────────
    {
        mcs51_test_register_family(MCS51_FAMILY_CMS8S78XX);
        mcs51_context_set_family(MCS51_FAMILY_CMS8S78XX);
        mcs51_context_reset(mcs51_get_context());
        wink_mcs51_host_gpio_notify_reset();
        wink_mcs51_host_gpio_release_reset();
        // Reset latch is 1: enabling output re-drives WEAK-HIGH; switching
        // back to input must release the driver (no stale self-drive).
        mcs51_get_context()->sfr_shadow[0xA2u] = 0x08u;
        wink_mcs51_on_sfr_write(0xA2u, 0x00u, 0x08u);
        CHECK(wink_mcs51_host_gpio_notify_count() == 1u &&
              wink_mcs51_host_gpio_notify_pin(0) == 19u &&
              wink_mcs51_host_gpio_notify_level(0) == 1u,
              "T9: 0->1 TRIS drives latch 1 weak");
        mcs51_get_context()->sfr_shadow[0xA2u] = 0x00u;
        wink_mcs51_on_sfr_write(0xA2u, 0x08u, 0x00u);
        CHECK(wink_mcs51_host_gpio_release_count() == 1u &&
              wink_mcs51_host_gpio_release_pin(0) == 19u,
              "T9: 1->0 TRIS must release the MCU driver");
    }

    // ── 10) P3: open-drain release and analog pins never drive ────────────
    {
        mcs51_test_register_family(MCS51_FAMILY_CMS8S78XX);
        mcs51_context_set_family(MCS51_FAMILY_CMS8S78XX);
        mcs51_context_reset(mcs51_get_context());
        wink_mcs51_host_gpio_notify_reset();
        wink_mcs51_host_gpio_release_reset();
        Mcu51Context* ctx = mcs51_get_context();
        // P2.3 open-drain + latch 1 (reset): enabling output is a HiZ release.
        ctx->xdata_shadow[0xF029u] = 0x08u;  // P2OD.3
        ctx->sfr_shadow[0xA2u] = 0x08u;
        wink_mcs51_on_sfr_write(0xA2u, 0x00u, 0x08u);
        CHECK(wink_mcs51_host_gpio_notify_count() == 0u &&
              wink_mcs51_host_gpio_release_count() == 1u &&
              wink_mcs51_host_gpio_release_pin(0) == 19u,
              "T10: OD latch=1 enable releases instead of driving");
        // P2.4 analog mux: enabling output must not drive at all.
        ctx->xdata_shadow[0xF029u] = 0x00u;  // OD off
        ctx->xdata_shadow[0xF024u] = 0x01u;  // P2CFG.4 = AN
        ctx->sfr_shadow[0xA2u] = 0x18u;      // P2TRIS.4 = output
        wink_mcs51_on_sfr_write(0xA2u, 0x08u, 0x18u);
        CHECK(wink_mcs51_host_gpio_notify_count() == 0u &&
              wink_mcs51_host_gpio_release_count() == 1u,
              "T10: AN-configured pin gets no digital drive");
    }

    // ── 11) P3: classic family has no TRIS seam (zero regression) ─────────
    {
        mcs51_test_register_family(MCS51_FAMILY_CLASSIC);
        mcs51_context_set_family(MCS51_FAMILY_CLASSIC);
        mcs51_context_reset(mcs51_get_context());
        wink_mcs51_host_gpio_notify_reset();
        wink_mcs51_host_gpio_release_reset();
        mcs51_get_context()->sfr_shadow[0xA2u] = 0x01u;
        wink_mcs51_on_sfr_write(0xA2u, 0x00u, 0x01u);
        CHECK(wink_mcs51_host_gpio_notify_count() == 0u &&
              wink_mcs51_host_gpio_release_count() == 0u,
              "T11: classic must not re-drive/release on TRIS writes");
    }

    if (g_fails) {
        printf("[gpio-dir] %d FAILURES!\n", g_fails);
        return 1;
    }
    printf("[gpio-dir] ALL TESTS PASSED\n");
    return 0;
}
