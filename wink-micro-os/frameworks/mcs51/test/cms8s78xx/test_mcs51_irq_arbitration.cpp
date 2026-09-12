// SPDX-License-Identifier: Apache-2.0
// Task R3 / ADR-0078 Characterization test:
//   1. Two-phase IRQ: peripheral raise -> rendezvous dispatch
//   2. In-service priority masking (same prio does NOT preempt)
//   3. High-priority nested preemption (P_req > P_curr DOES preempt)
//   4. ADC ISR writes SBUF without recursion (call stack isolation)
//   5. EA critical section masking & pending conservation
//   6. RETI single-instruction execution suppression
//   7. MCS51_IRQ_SW_CLEAR vs HW_AUTO_CLEAR flag contract
#include <stdint.h>
#include <stdio.h>

#include "absacc.h"
#include "cms8s_adc.h"
#include "mcs51_context.h"
#include "mcs51_proxy.hpp"
#include "mcs51_test_harness.h"
#include "mcs51_trap.h"
#include "wink_mcs51_clock.h"
#include "wink_mcs51_isr.h"
#include "wink_mcs51_uart.h"

namespace {

constexpr uint8_t SFR_TCON = 0x88u;
constexpr uint8_t SFR_SCON = 0x98u;
constexpr uint8_t SFR_SBUF = 0x99u;
constexpr uint8_t SFR_IE   = 0xA8u;
constexpr uint8_t SFR_EIE2 = 0xAAu;
constexpr uint8_t SFR_EIF2 = 0xB2u;
constexpr uint8_t SFR_IP   = 0xB8u;
constexpr uint8_t SFR_EIP2 = 0xB9u;

constexpr uint8_t IE_EA    = 7u;
constexpr uint8_t IE_ES    = 4u;
constexpr uint8_t IE_ET1   = 3u;
constexpr uint8_t IE_ET0   = 1u;
constexpr uint8_t EIE2_ADCIE = 4u;

uint32_t g_adc_hits = 0;
uint32_t g_uart_hits = 0;
uint32_t g_t0_hits = 0;
uint32_t g_t1_hits = 0;

uint8_t  g_depth_during_adc = 0;
uint8_t  g_depth_during_t0 = 0;
uint8_t  g_depth_during_t1 = 0;
uint32_t g_uart_hits_during_adc = 0;

bool g_clear_uart_in_isr = true;
bool g_trigger_t1_in_t0 = false;
bool g_write_sbuf_in_adc = false;

int g_fails = 0;

void check(bool cond, const char* msg) {
    if (!cond) {
        printf("[mcs51-irq] FAIL: %s\n", msg);
        ++g_fails;
    }
}

}  // namespace

// ADC ISR (Vector 19)
WINK_ISR(19) {
    ++g_adc_hits;
    g_depth_during_adc = wink_mcs51_get_in_service_depth();

    // Clear ADCIF
    mcs51_get_context()->sfr_shadow[SFR_EIF2] &= static_cast<uint8_t>(~(1u << 4));

    if (g_write_sbuf_in_adc) {
        // Write to SBUF inside ADC ISR:
        // Prior to ADR-0078, this synchronously recursed into UART ISR on the same stack.
        // Under ADR-0078 two-phase dispatch, UART IRQ is raised to pending, but because
        // both have priority 0, it does NOT preempt this ISR!
        mcs51_get_context()->sfr_shadow[SFR_SBUF] = static_cast<uint8_t>('Z');
        wink_mcs51_uart_on_write(SFR_SBUF);

        // Attempt scan_and_dispatch inside the ISR: must NOT dispatch same-prio UART0!
        mcs51_irq_scan_and_dispatch();

        g_uart_hits_during_adc = g_uart_hits;
    }
}

// UART ISR (Vector 4)
WINK_ISR(4) {
    ++g_uart_hits;
    if (g_clear_uart_in_isr) {
        mcs51_get_context()->sfr_shadow[SFR_SCON] &= static_cast<uint8_t>(~0x03u);  // clear RI and TI
    }
}

// Timer0 ISR (Vector 1)
WINK_ISR(1) {
    ++g_t0_hits;
    g_depth_during_t0 = wink_mcs51_get_in_service_depth();

    if (g_trigger_t1_in_t0) {
        // Raise Timer1 IRQ while inside Timer0 ISR
        mcs51_raise_irq(IRQ_SOURCE_TIMER1);
        // Rendezvous dispatch point
        mcs51_irq_scan_and_dispatch();
    }
}

// Timer1 ISR (Vector 3)
WINK_ISR(3) {
    ++g_t1_hits;
    g_depth_during_t1 = wink_mcs51_get_in_service_depth();
}

extern "C" void wink_mcs51_user_main(void) {}
extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

int main(void) {
    printf("[mcs51-irq] Starting Task R3 characterization tests...\n");

    // Stage5 S5-1: the extended profile (ADC vector 19) is chip-owned and
    // loaded per context — select the family so the chip reset installs it.
    // This test lives in the chip suite; the classic path is covered by the
    // family-insulation test instead.
    mcs51_test_use_family(MCS51_FAMILY_CMS8S78XX);

    // ── Test 1: In-service masking & call stack isolation (ADC ISR writes SBUF) ──
    {
        wink_mcs51_reset_irq_state();
        wink_mcs51_isr_enable();

        g_adc_hits = 0;
        g_uart_hits = 0;
        g_uart_hits_during_adc = 0;
        g_write_sbuf_in_adc = true;

        // Enable EA, ES, ADCIE (all default priority 0)
        mcs51_get_context()->sfr_shadow[SFR_IE]   = (1u << IE_EA) | (1u << IE_ES);
        mcs51_get_context()->sfr_shadow[SFR_EIE2] = (1u << EIE2_ADCIE);
        mcs51_get_context()->sfr_shadow[SFR_EIF2] = (1u << 4);  // ADCIF set on conversion complete
        mcs51_get_context()->sfr_shadow[SFR_IP]   = 0u;
        mcs51_get_context()->sfr_shadow[SFR_EIP2] = 0u;

        // Raise ADC IRQ
        mcs51_raise_irq(IRQ_SOURCE_ADC);
        check((wink_mcs51_get_pending_interrupts() & (1u << IRQ_SOURCE_ADC)) != 0,
              "T1: ADC IRQ not pending after raise");

        // Microstep / rendezvous dispatch
        mcs51_irq_scan_and_dispatch();

        check(g_adc_hits == 1, "T1: ADC ISR did not run");
        check(g_depth_during_adc == 1, "T1: In-service depth during ADC ISR != 1");
        check(g_uart_hits_during_adc == 0, "T1: UART ISR preempted ADC ISR (stack recursion!)");
        check((wink_mcs51_get_pending_interrupts() & (1u << IRQ_SOURCE_UART0)) != 0,
              "T1: UART IRQ not pending after ADC ISR completed");

        // RETI single-instruction suppression: first scan after RETI is suppressed
        uint8_t ran = mcs51_irq_scan_and_dispatch();
        check(ran == 0, "T1: RETI suppression did not skip one cycle");

        // Next scan dispatches pending UART ISR
        ran = mcs51_irq_scan_and_dispatch();
        check(ran == 1 && g_uart_hits == 1, "T1: UART ISR did not run after suppression cycle");
        check(wink_mcs51_get_in_service_depth() == 0, "T1: Final in-service depth != 0");
    }

    // ── Test 2: Same-priority does NOT preempt (Timer0 vs Timer1) ───────────
    {
        wink_mcs51_reset_irq_state();
        wink_mcs51_isr_enable();

        g_t0_hits = 0;
        g_t1_hits = 0;
        g_trigger_t1_in_t0 = true;

        // Enable EA, ET0, ET1. Both priority 0.
        mcs51_get_context()->sfr_shadow[SFR_IE] = (1u << IE_EA) | (1u << IE_ET0) | (1u << IE_ET1);
        mcs51_get_context()->sfr_shadow[SFR_IP] = 0u;

        mcs51_raise_irq(IRQ_SOURCE_TIMER0);
        mcs51_irq_scan_and_dispatch();

        check(g_t0_hits == 1, "T2: Timer0 ISR did not run");
        check(g_t1_hits == 0, "T2: Timer1 preempted same-priority Timer0!");
        check((wink_mcs51_get_pending_interrupts() & (1u << IRQ_SOURCE_TIMER1)) != 0,
              "T2: Timer1 not pending after Timer0 returned");

        // Skip suppression, then dispatch Timer1
        mcs51_irq_scan_and_dispatch();  // suppression skip
        mcs51_irq_scan_and_dispatch();  // Timer1 dispatch
        check(g_t1_hits == 1, "T2: Timer1 ISR did not run after Timer0 finished");
    }

    // ── Test 3: High-priority DOES preempt low-priority (Timer1 prio 1) ──────
    {
        wink_mcs51_reset_irq_state();
        wink_mcs51_isr_enable();

        g_t0_hits = 0;
        g_t1_hits = 0;
        g_trigger_t1_in_t0 = true;

        // Enable EA, ET0, ET1. Timer1 has priority 1 (IP.PT1 = 1).
        mcs51_get_context()->sfr_shadow[SFR_IE] = (1u << IE_EA) | (1u << IE_ET0) | (1u << IE_ET1);
        mcs51_get_context()->sfr_shadow[SFR_IP] = (1u << 3);  // PT1 = 1

        mcs51_raise_irq(IRQ_SOURCE_TIMER0);
        mcs51_irq_scan_and_dispatch();

        check(g_t0_hits == 1, "T3: Timer0 ISR did not run");
        check(g_t1_hits == 1, "T3: High-priority Timer1 failed to preempt low-priority Timer0!");
        check(g_depth_during_t1 == 2, "T3: In-service depth during nested Timer1 ISR != 2");
        check(wink_mcs51_get_in_service_depth() == 0, "T3: Final depth != 0");
    }

    // ── Test 4: EA critical section masking & pending conservation ───────────
    {
        wink_mcs51_reset_irq_state();
        wink_mcs51_isr_enable();

        g_t0_hits = 0;
        g_trigger_t1_in_t0 = false;

        // EA = 0 (critical section), ET0 = 1
        mcs51_get_context()->sfr_shadow[SFR_IE] = (1u << IE_ET0);

        mcs51_raise_irq(IRQ_SOURCE_TIMER0);
        uint8_t dispatched = mcs51_irq_scan_and_dispatch();

        check(dispatched == 0 && g_t0_hits == 0, "T4: ISR dispatched while EA=0!");
        check((wink_mcs51_get_pending_interrupts() & (1u << IRQ_SOURCE_TIMER0)) != 0,
              "T4: Pending bit lost while EA=0");

        // Re-enable EA
        mcs51_get_context()->sfr_shadow[SFR_IE] |= (1u << IE_EA);
        dispatched = mcs51_irq_scan_and_dispatch();

        check(dispatched == 1 && g_t0_hits == 1, "T4: ISR did not dispatch after EA restored");
    }

    // ── Test 5: SW_CLEAR leaves hardware flag set after dispatch ─────────────
    {
        wink_mcs51_reset_irq_state();
        wink_mcs51_isr_enable();

        g_uart_hits = 0;
        g_clear_uart_in_isr = false;  // Firmware stub leaves RI/TI set

        mcs51_get_context()->sfr_shadow[SFR_IE]   = (1u << IE_EA) | (1u << IE_ES);
        mcs51_get_context()->sfr_shadow[SFR_SCON] = 0x02u;  // TI = 1

        mcs51_raise_irq(IRQ_SOURCE_UART0);
        mcs51_irq_scan_and_dispatch();

        check(g_uart_hits == 1, "T5: First UART ISR did not run");
        // Because clear_mode is SW_CLEAR, hardware must NOT auto-clear SCON.TI!
        check((mcs51_get_context()->sfr_shadow[SFR_SCON] & 0x02u) != 0,
              "T5: SW_CLEAR flag was erroneously cleared by hardware");

        // When firmware clears the flag, SCON is cleared
        mcs51_get_context()->sfr_shadow[SFR_SCON] = 0u;
        check((mcs51_get_context()->sfr_shadow[SFR_SCON] & 0x02u) == 0,
              "T5: Software flag clear failed");
    }

    // ── Test 6: HW_AUTO_CLEAR clears hardware flag automatically ─────────────
    {
        wink_mcs51_reset_irq_state();
        wink_mcs51_isr_enable();

        g_t0_hits = 0;
        g_trigger_t1_in_t0 = false;

        mcs51_get_context()->sfr_shadow[SFR_IE]   = (1u << IE_EA) | (1u << IE_ET0);
        mcs51_get_context()->sfr_shadow[SFR_TCON] = (1u << 5);  // TF0 = 1

        mcs51_raise_irq(IRQ_SOURCE_TIMER0);
        mcs51_irq_scan_and_dispatch();

        check(g_t0_hits == 1, "T6: Timer0 ISR did not run");
        // TF0 should be automatically cleared by hardware
        check((mcs51_get_context()->sfr_shadow[SFR_TCON] & (1u << 5)) == 0,
              "T6: TF0 was not automatically cleared by HW_AUTO_CLEAR");
    }

    if (g_fails) {
        printf("[mcs51-irq] %d FAILURES!\n", g_fails);
        return 1;
    }

    printf("[mcs51-irq] ALL TESTS PASSED: Two-phase IRQ, in-service priority masking, "
           "non-recursive SBUF write, EA masking, RETI suppression, and SW_CLEAR contract verified.\n");
    return 0;
}
