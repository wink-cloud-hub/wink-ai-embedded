/* SPDX-License-Identifier: Apache-2.0
 * MCS-51 iron_ntc — UNMODIFIED-style Keil C51 user source (M6, closed-loop).
 *
 * A soldering-iron / hot-plate NTC thermostat built on the same 3-wire
 * ADC0832 bit-bang as adc0832_read.c, but wired per the board codegen SSOT
 * (wink-app.json -> mcs51_board_config.h):
 *   ADC0832  CS=P2.0, CLK=P2.1, DIO=P2.2 (DI/DO shared, 3-wire)
 *   HEATER   drive = P1.0 (relay / heater enable, active high)
 *
 * The firmware carries its OWN 8-bit code -> temperature lookup (the mcs51
 * zero-intrusion sandbox links no DAL; the NTC thermistor math stays in the
 * Keil program exactly as a real 8051 appliance would implement it). The NTC
 * is pulled up to VREF, so the ADC code is HIGH when cold and LOW when hot.
 *
 * Safety (v1, recomputed every loop iteration — a fault cuts the heater
 * immediately): code at/near full scale (>= NTC_OPEN_CODE) means the sensor
 * is open; code at/near zero (<= NTC_SHORT_CODE) means it is shorted. Either
 * fault forces HEATER=0 and latches a fault code for the host to assert.
 * Power-on fault latching across thermal time constants belongs to the
 * thermal model (track B); here the safe state is re-derived each pass.
 *
 * The sandbox Level-2 trap FSM (mcs51_adc0832.cpp) advances on the pin edges
 * with 0 us conversion; the injected NTC code comes from the channel-3 rail.
 * No ISR. The super-loop yields via _nop_() so the cooperative fiber does not
 * freeze (a bare while(1){} would).
 */
#include <REGX52.H>
#include <absacc.h>

sbit ADC_CS  = P2^0;
sbit ADC_CLK = P2^1;
sbit ADC_DIO = P2^2;
sbit HEATER  = P1^0;

/* Fault/safety thresholds on the raw 8-bit ADC code. */
#define NTC_OPEN_CODE   250u   /* code >= this: sensor open (full-scale)  */
#define NTC_SHORT_CODE  8u     /* code <= this: sensor shorted (zero)     */
#define SETPOINT_C      180u   /* thermostat target in °C                 */

/* XDATA telemetry slots the host e2e asserts on. */
#define TLM_CODE    0x0010u    /* last raw ADC code                       */
#define TLM_HEATER  0x0011u    /* heater drive level (0/1)                */
#define TLM_FAULT   0x0012u    /* 0=ok, 1=open, 2=short                   */

/* code -> °C breakpoints, HIGH code (cold) first. NTC pull-up: code falls
 * as temperature rises. ntc_code_to_temp() returns the first bracket whose
 * code threshold the reading meets. Temps are clamped to <=250 °C (unsigned
 * char); the two hottest brackets only need to read above the 180 °C setpoint
 * to switch the heater off. */
static unsigned char code ntc_lut_code[6] = {230, 180, 120, 80, 40, 10};
static unsigned char code ntc_lut_temp[6] = { 25,  80, 140, 180, 240, 250};

static unsigned char ntc_code_to_temp(unsigned char adc_code) {
    unsigned char i;
    for (i = 0; i < 6u; i++) {
        if (adc_code >= ntc_lut_code[i]) {
            return ntc_lut_temp[i];
        }
    }
    return 250u;   /* below the lowest bracket: hotter than the table */
}

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
    unsigned char code_val;
    unsigned char temp_c;

    HEATER = 0;

    while (1) {
        code_val = adc0832_read(0);
        XBYTE[TLM_CODE] = code_val;

        if (code_val >= NTC_OPEN_CODE) {
            /* Sensor open: force heater off, safe state. */
            HEATER = 0;
            XBYTE[TLM_FAULT] = 1u;
        } else if (code_val <= NTC_SHORT_CODE) {
            /* Sensor shorted: force heater off, safe state. */
            HEATER = 0;
            XBYTE[TLM_FAULT] = 2u;
        } else {
            XBYTE[TLM_FAULT] = 0u;
            temp_c = ntc_code_to_temp(code_val);
            /* Bang-bang thermostat: heat while below setpoint. */
            HEATER = (temp_c < SETPOINT_C) ? 1 : 0;
        }

        XBYTE[TLM_HEATER] = HEATER;
        _nop_();            /* cooperative yield: never spin bare */
    }
}
