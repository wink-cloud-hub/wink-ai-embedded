/* SPDX-License-Identifier: Apache-2.0
 * MCS-51 ADC0832 read — UNMODIFIED Keil C51 user source (M4, AD-15).
 *
 * Classic 3-wire (DI/DO shared on DIO) ADC0832 bit-bang driver: CS=P1.2,
 * CLK=P1.1, DIO=P1.0. The chip samples the 3 config bits (Start=1, SGL/DIF=1
 * single-ended, ODD/SIGN=channel) on the first three CLK RISING edges, then
 * presents the 8-bit result MSB-first on DIO at each subsequent CLK FALLING
 * edge (leading null bit occupies the 3rd-high/3rd-low half-cycle). The MCU
 * releases DIO high after the third config bit; in the output phase that
 * write is the quasi-bidirectional input-enable gesture.
 *
 * The sandbox Level-2 trap state machine (mcs51_adc0832.cpp) advances purely
 * on these pin edges with 0 us conversion; the converted value comes from the
 * channel-3 analog rail. The two channel reads land in XDATA for the driver
 * to assert. No ISR.
 */
#include <REGX52.H>
#include <absacc.h>

sbit ADC_CS  = P1^2;
sbit ADC_CLK = P1^1;
sbit ADC_DIO = P1^0;

static unsigned char adc0832_read(unsigned char channel) {
    unsigned char i;
    unsigned char dat = 0;

    ADC_CS  = 1;
    ADC_CLK = 0;
    ADC_CS  = 0;               /* CS fall: begin conversion */

    ADC_DIO = 1; ADC_CLK = 1; ADC_CLK = 0;   /* rise 1: Start bit = 1 */
    ADC_DIO = 1; ADC_CLK = 1; ADC_CLK = 0;   /* rise 2: SGL/DIF = 1 */
    ADC_DIO = channel & 1;                   /* ODD/SIGN: channel select */
    ADC_CLK = 1;                            /* rise 3: channel latched */
    ADC_DIO = 1;                            /* release DIO (input enable) */

    for (i = 0; i < 8; i++) {
        ADC_CLK = 0;                        /* falling: DO presents next bit */
        dat <<= 1;
        if (ADC_DIO) {                     /* read DIO (MSB first) */
            dat |= 1;
        }
        ADC_CLK = 1;
    }

    ADC_CS = 1;               /* CS rise: abort to idle */
    return dat;
}

void main(void) {
    XBYTE[0x0010u] = adc0832_read(0);   /* CH0 result */
    XBYTE[0x0011u] = adc0832_read(1);   /* CH1 result */
    while (1) {
        _nop_();
    }
}
