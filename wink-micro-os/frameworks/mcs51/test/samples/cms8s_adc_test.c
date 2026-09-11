/* SPDX-License-Identifier: Apache-2.0
 * MCS-51 CMS8S78xx on-chip ADC read — UNMODIFIED Keil C51 user source (M5).
 *
 * Vendor-style polled driver for the CMS8S78xx 12-bit ADC (real register map:
 * ADCON0/ADCON1/ADCCHS/ADRESH/ADRESL, reference manual Ch.22):
 *   - enable the module   (ADCON1.ADEN = 1)
 *   - select the channel  (ADCCHS = ANn)
 *   - set justification    (ADCON0.ADFM)
 *   - start                (ADCON0.ADGO = 1) and poll ADGO — hardware clears
 *                           it when the conversion completes
 *   - combine ADRESH/ADRESL with the vendor ADC_GetADCResult formulas:
 *       right-justify: 0xFFF & ((ADRESH << 8) | ADRESL)
 *       left-justify : 0xFFF & ((ADRESH << 4) | (ADRESL >> 4))
 *
 * The sandbox model (cms8s_adc.cpp) completes the conversion synchronously
 * inside the ADCON0 write (0-cycle passthrough), so the ADGO poll exits on
 * its first iteration. Three conversions land in XDATA for the driver to
 * assert: AN0 right-justify, AN1 left-justify, AN25 right-justify. No ISR.
 */
#include <REG_CMS8S78XX.H>
#include <absacc.h>

static unsigned int cms8s_adc_read(unsigned char channel,
                                   unsigned char right_justify) {
    ADCCHS = channel;
    ADCON1 |= 0x80;                 /* ADEN = 1 (module enable) */
    if (right_justify) {
        ADCON0 = 0x40;              /* ADFM = 1: right-justify */
    } else {
        ADCON0 = 0x00;              /* ADFM = 0: left-justify */
    }
    ADCON0 |= 0x02;                 /* ADGO = 1: start conversion */
    while (ADCON0 & 0x02) {         /* wait for ADGO self-clear (0 cycles) */
        ;
    }
    if (right_justify) {
        return 0x0FFF & ((ADRESH << 8) | ADRESL);
    }
    return 0x0FFF & ((ADRESH << 4) | (ADRESL >> 4));
}

void main(void) {
    unsigned int v;

    v = cms8s_adc_read(0, 1);       /* AN0, right-justify */
    XBYTE[0x0010u] = (v >> 8) & 0x0F;
    XBYTE[0x0011u] = v & 0xFF;

    v = cms8s_adc_read(1, 0);       /* AN1, left-justify */
    XBYTE[0x0012u] = (v >> 8) & 0x0F;
    XBYTE[0x0013u] = v & 0xFF;

    v = cms8s_adc_read(25, 1);      /* AN25 (P3.1), right-justify */
    XBYTE[0x0014u] = (v >> 8) & 0x0F;
    XBYTE[0x0015u] = v & 0xFF;

    while (1) {
        _nop_();
    }
}
