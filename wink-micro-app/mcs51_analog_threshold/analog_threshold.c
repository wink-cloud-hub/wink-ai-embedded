/* SPDX-License-Identifier: Apache-2.0
 * MCS-51 CMS8S78xx on-chip ADC threshold drives LED — UNMODIFIED Keil C51 user
 * source (Stage 2 T4 headless proof).
 *
 * Production wasm-sim app proving the channel-3 LIVE ANALOG plane reaches
 * firmware end-to-end. The main loop polls the CMS8S78xx 12-bit ADC on AN0
 * through the vendor register map (ADCCHS/ADCON1/ADCON0/ADRESH/ADRESL, same
 * polled driver shape as the cms8s_adc_test vendor sample) and lights the LED
 * on P1.0 whenever the AN0 level is at/above mid-scale (raw >= 0x0800, i.e.
 * normalized >= ~0.5). There is no injection: the only analog source is the
 * headless INPUT_ANALOG step, which drives the PinArbiter analog rail
 * (AdcDomainHandler.writeNorm -> arbiter.setAnalogDriver(32)); the sandbox ADC
 * model pulls it back through js_pal_adc_read_norm(32 + AN0).
 *
 * The LED can ONLY change as a function of the live analog value, so headless
 * LED transitions across two different driven levels prove the analog value
 * crosses the bridge into firmware (and tracks changes, not a stuck rail):
 *   norm 0.2 -> raw ~0x333 <  mid -> LED off (P1.0 high)
 *   norm 0.8 -> raw ~0xCCC >= mid -> LED on  (P1.0 low)
 * Built as a real production app (cleanup -> .cpp -> links wink_mcs51_compat
 * -> wink_simulator.{js,wasm}); this original is never edited in place.
 * Linear pin (ADR-0074 D3): LED = P1.0 -> 8; analog rail pin = 32 + AN0 = 32.
 */
#include <REG_CMS8S.H>

sbit LED = P1^0;    /* threshold indicator on P1.0, low-drive-on (0 = lit) */

#define ADC_MID_SCALE  0x0800u   /* 12-bit mid-scale (norm ~0.5) */

/* Polled vendor-style single conversion on AN0, right-justified 12-bit. */
static unsigned int adc_read_an0(void) {
    ADCCHS = 0;                 /* select AN0 */
    ADCON1 |= 0x80;             /* ADEN = 1 (module enable) */
    ADCON0 = 0x40;              /* ADFM = 1: right-justify */
    ADCON0 |= 0x02;             /* ADGO = 1: start conversion */
    while (ADCON0 & 0x02) {     /* wait for ADGO self-clear (0-cycle passthrough) */
        ;
    }
    return 0x0FFFu & ((ADRESH << 8) | ADRESL);
}

void main(void) {
    LED = 1;                    /* initial: LED off */
    while (1) {
        unsigned int v = adc_read_an0();
        LED = (v >= ADC_MID_SCALE) ? 0 : 1;   /* light at/above mid-scale */
        _nop_();                /* cooperative microstep / external-sample point */
    }
}
