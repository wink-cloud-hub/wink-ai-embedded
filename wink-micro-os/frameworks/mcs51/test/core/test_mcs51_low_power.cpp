// SPDX-License-Identifier: GPL-3.0-only
// Task R6: MCS-51 PCON low power (IDLE / Power-Down) dual-mode scheduling test.
#include <stdint.h>
#include <stdio.h>
#include <cstring>

#include "absacc.h"
#include "mcs51_context.h"
#include "mcs51_test_harness.h"
#include "mcs51_pcon.h"
#include "mcs51_proxy.hpp"
#include "wink_event.h"
#include "wink_mcs51_clock.h"
#include "wink_mcs51_isr.h"
#include "wink_mcs51_timer.h"

namespace {

int g_fails = 0;
uint32_t g_t0_hits = 0;
uint32_t g_int0_hits = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        printf("[mcs51-lowpower] FAIL: %s\n", what);
        ++g_fails;
    }
}

} // namespace

WINK_ISR(1) {
    ++g_t0_hits;
}

WINK_ISR(0) {
    ++g_int0_hits;
}

extern "C" void wink_mcs51_user_main(void) {}
extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}
extern "C" void wink_mcs51_host_set_ext_pin(uint16_t pin, uint8_t state);
extern "C" void wink_mcs51_host_ext_pins_reset(void);
extern "C" void wink_mcs51_extint_poll(void);
extern "C" uint64_t mcs51_extint_next_event_us(struct Mcu51Context* ctx);
// Chip port-interrupt poll (stage4 split: Test 3 drives port silicon).
extern "C" void cms8s_port_extint_poll(struct Mcu51Context* ctx);

int main(void) {
    Mcu51Context* ctx = mcs51_get_context();
    mcs51_context_reset(ctx);
    wink_mcs51_isr_enable();

    // ── Test 1: IDLE Mode with Active Scheduled Event (Timer 0) ──────────────
    // Configure Timer0 in 16-bit mode (Mode 1) to overflow after 100 microseconds
    // 65536 - 100 = 65436 (0xFF9C)
    ctx->sfr_shadow[0x89] = 0x01u; // TMOD = Mode 1
    wink_mcs51_timer_on_write(0x89);
    ctx->sfr_shadow[0x8A] = 0x9Cu; // TL0
    wink_mcs51_timer_on_write(0x8A);
    ctx->sfr_shadow[0x8C] = 0xFFu; // TH0
    wink_mcs51_timer_on_write(0x8C);
    ctx->sfr_shadow[0x88] = 0x10u; // TR0 = 1 (TCON bit 4)
    wink_mcs51_timer_on_write(0x88);
    ctx->sfr_shadow[0xA8] = 0x82u; // EA = 1, ET0 = 1

    uint64_t start_us = ctx->virtual_us;
    g_t0_hits = 0;

    // Enter IDLE mode: PCON |= 0x01
    // mcs51_on_pcon_write should step-pump until Timer 0 triggers and exits IDLE
    mcs51_on_pcon_write(ctx, 0x87, 0x00, 0x01);

    check(g_t0_hits >= 1, "Timer0 ISR must be dispatched in IDLE mode");
    check((ctx->sfr_shadow[0x87] & 0x01u) == 0, "IDLE bit must be cleared upon exit");
    check(ctx->virtual_us >= start_us + 100u, "Virtual clock must advance to match scheduled event");

    // ── Test 2: PD Mode Wake via External Interrupt INT0 ──────────────────────
    wink_status_t st = wink_event_queue_init(16);
    (void)st;
    // Set EA=1, EX0=1
    ctx->sfr_shadow[0xA8] = 0x81u;
    g_int0_hits = 0;

    // Simulate an external wake event waking from PD mode
    wink_event_t wake_ev = { .type = 1 };
    st = wink_event_post(&wake_ev);
    (void)st;

    // Enter PD mode: PCON |= 0x02
    mcs51_on_pcon_write(ctx, 0x87, 0x00, 0x02);
    check((ctx->sfr_shadow[0x87] & 0x02u) == 0, "PD bit must be cleared upon wake event");

    // Test that raising INT0 dispatches the ISR
    mcs51_raise_irq(IRQ_SOURCE_INT0);
    mcs51_irq_scan_and_dispatch();
    check(g_int0_hits == 1, "INT0 ISR must be dispatched when raised");

    wink_event_queue_deinit();

    // ── Test 3: PD wake via GPIO port interrupt (GAP-17') ───────────────────
    // Port interrupts are a CMS8S78xx STOP wake source (ref manual §5.4.1);
    // the model posts one wake event per matched edge (EA + PD gated).
    mcs51_test_register_family(MCS51_FAMILY_CMS8S78XX);
    mcs51_context_set_family(MCS51_FAMILY_CMS8S78XX);
    mcs51_context_reset(ctx);
    wink_mcs51_isr_enable();
    st = wink_event_queue_init(16);
    (void)st;
    wink_mcs51_host_ext_pins_reset();

    ctx->sfr_shadow[0xA8] = 0x80u;    // EA = 1
    ctx->sfr_shadow[0xAD] = 0x04u;    // P1EXTIE.2 enable
    ctx->xdata_shadow[0xF08Au] = 0x02u;  // P1EICFG2 = falling edge
    wink_mcs51_host_set_ext_pin(10u, 1u);  // P1.2 idle high
    wink_mcs51_extint_poll();              // prime sample (have_sample)
    cms8s_port_extint_poll(nullptr);
    ctx->sfr_shadow[0x87] = 0x02u;    // enter PD (direct shadow: no block)
    wink_mcs51_test_advance_virtual_us(10000u);  // pass 10 ms sample throttle
    wink_mcs51_host_set_ext_pin(10u, 0u);  // falling edge
    wink_mcs51_extint_poll();
    cms8s_port_extint_poll(nullptr);

    check((ctx->sfr_shadow[0xB5] & 0x04u) != 0u, "P1EXTIF.2 flag must be set");
    {
        wink_event_t evt;
        check(wink_event_pend(&evt, 0u) == 0, "port edge in PD must post wake");
        check(wink_event_pend(&evt, 0u) != 0, "held level must not re-post");
    }

    // EA = 0 gate: flag still records, CPU must not wake.
    ctx->sfr_shadow[0xB5] = 0x00u;    // clear flag (test seam)
    ctx->sfr_shadow[0xA8] = 0x00u;    // EA = 0
    wink_mcs51_host_set_ext_pin(10u, 1u);
    wink_mcs51_test_advance_virtual_us(10000u);
    wink_mcs51_extint_poll();              // re-prime high
    cms8s_port_extint_poll(nullptr);
    wink_mcs51_host_set_ext_pin(10u, 0u);
    wink_mcs51_test_advance_virtual_us(10000u);
    wink_mcs51_extint_poll();
    cms8s_port_extint_poll(nullptr);
    check((ctx->sfr_shadow[0xB5] & 0x04u) != 0u, "flag must set even with EA=0");
    {
        wink_event_t evt;
        check(wink_event_pend(&evt, 0u) != 0, "EA=0 must not post PD wake");
    }

    wink_event_queue_deinit();

    // ── Test 4: classic IDLE keeps sampling INT0/INT1 (schedule advertised) ──
    // The extint model must advertise its next line-sampling slot so IDLE
    // Mode A keeps step-pumping and the microstep polls keep watching the
    // lines; UINT64_MAX would drop a classic part into Mode B (event-queue
    // wait) with no scheduled wake source when no timer is active.
    mcs51_test_register_family(MCS51_FAMILY_CLASSIC);
    mcs51_context_set_family(MCS51_FAMILY_CLASSIC);
    mcs51_context_reset(ctx);
    wink_mcs51_isr_enable();
    wink_mcs51_extint_poll();  // prime the line-sample baseline at now
    const uint64_t extint_next = mcs51_extint_next_event_us(ctx);
    check(extint_next != UINT64_MAX,
          "classic extint must advertise its next line sample");
    check(extint_next > ctx->virtual_us,
          "advertised sample must be in the future (IDLE Mode A)");

    if (g_fails != 0) {
        printf("[mcs51-lowpower] FAILED with %d errors\n", g_fails);
        return 1;
    }
    printf("[mcs51-lowpower] PASS: PCON IDL/PD dual-mode scheduling verified\n");
    return 0;
}
