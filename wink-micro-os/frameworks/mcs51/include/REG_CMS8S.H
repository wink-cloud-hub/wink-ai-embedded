// SPDX-License-Identifier: Apache-2.0
// REG_CMS8S.H — CMS8S78xx (Cmsemicon) SFR dialect shim for the host/wasm
// sandbox.
//
// Unmodified Keil C51 user sources written for the CMS8S78xx part
// `#include <REG_CMS8S.H>` and program the on-chip 12-bit ADC through the
// vendor register map (reference manual Ch.22, verified against the vendor
// device header cms8s78xx.h and StdDriver adc.c):
//
//   ADCON0 (0xDF)  bit1 ADGO  — write 1 to start; hardware self-clears to 0
//                               when the conversion completes, so the bit IS
//                               the busy flag (`while(ADCON0 & 0x02);`).
//                  bit6 ADFM  — 0 = left-justify result, 1 = right-justify.
//   ADCON1 (0xDE)  bit7 ADEN  — ADC module enable.
//   ADCCHS (0xD9)  bits5:0    — channel select (0..25 = AN0..AN25, 0x3F=AN63).
//   ADRESH (0xDD) / ADRESL (0xDC) — conversion result, read-only.
//   EIE2   (0xAA)  bit4 ADCIE — end-of-conversion interrupt enable.
//   EIF2   (0xB2)  bit4 ADCIF— end-of-conversion flag (latched; SW-cleared).
//   Keil interrupt vector 19 (vector address 0x9B) — ADC ISR.
//
// Result packing (vendor ADC_GetADCResult, verbatim formulas):
//   right-justify: 0xFFF & ((ADRESH << 8) | ADRESL)
//   left-justify : 0xFFF & ((ADRESH << 4) | (ADRESL >> 4))
//
// XSFR registers (ADCLDO @ 0xF692, pin mux PxxCFG @ 0xF000..) live in the
// MOVX/xdata space on real silicon and are proxied by WinkXsfr over the
// bounds-checked XSFR window — see mcs51_xsfr.hpp.
//
// The conversion itself is modeled as a 0-cycle passthrough: the ADCON0 write
// hook (cms8s_adc.cpp) completes the conversion synchronously inside the
// write, so the ADGO poll exits on its first iteration (M5, ADR-0073).
#pragma once

// REGX52.H first: dialect erasure (sfr/sbit/xdata/interrupt), WinkSfr,
// WINK_ISR, main remap, intrins. Then the XSFR lvalue proxy.
#include "REGX52.H"
#include "mcs51_xsfr.hpp"

#ifndef __C51__  // never active under a real Keil toolchain

// ── 1) CMS8S78xx extended SFR bytes (addresses per vendor cms8s78xx.h) ──────
// All are non-GPIO addresses (port index 0xFF) → SFR hook path only.
sfr P0TRIS = 0x9A;  // P0 direction (TRIS register, vendor GPIO driver)
sfr P1TRIS = 0xA1;
sfr P2TRIS = 0xA2;
sfr P3TRIS = 0xA3;
sfr EIE2   = 0xAA;  // extended interrupt enable 2 (bit4 = ADCIE)
sfr EIF2   = 0xB2;  // extended interrupt flag 2   (bit4 = ADCIF)
sfr EIP1   = 0xB9;  // extended interrupt priority
sfr EIP2   = 0xBA;
sfr EIP3   = 0xBB;

sfr ADCMPC = 0xD1;  // ADC compare control
sfr ADDLYL = 0xD3;  // hardware-trigger delay low
sfr ADCMPL = 0xD4;  // compare value low
sfr ADCMPH = 0xD5;  // compare value high
sfr ADCCHS = 0xD9;  // channel select
sfr ADRESL = 0xDC;  // result low  (read-only; loaded by the model)
sfr ADRESH = 0xDD;  // result high (read-only; loaded by the model)
sfr ADCON1 = 0xDE;  // bit7 ADEN, bits6:4 ADCKS clock divider
sfr ADCON0 = 0xDF;  // bit6 ADFM, bit1 ADGO
sfr ADCON2 = 0xE9;  // trigger config

// ── 2) XSFR proxies (MOVX space; wild-pointer-safe, see mcs51_xsfr.hpp) ────
#if defined(__cplusplus) && __cplusplus >= 201703L
  #define xsfr inline WinkXsfr
#else
  #define xsfr static WinkXsfr
#endif

xsfr ADCLDO(0xF692);  // ADC LDO control (bit7 LDOEN, bits6:5 VSEL, bit4 OUTEN)
xsfr P00CFG(0xF000);  // P0.0 pin function mux
xsfr P32CFG(0xF032);  // P3.2 pin function mux

// ── 3) Vendor bit-position / mask macros (names verbatim from cms8s78xx.h,
//       so the vendor StdDriver adc.c can compile against this shim) ────────
// ADCON0
#define ADC_ADCON0_ADFM_Pos    (6)
#define ADC_ADCON0_ADFM_Msk    (0x1 << ADC_ADCON0_ADFM_Pos)   // 0x40
#define ADC_ADCON0_ANACH_Pos   (2)
#define ADC_ADCON0_ANACH_Msk   (0xF << ADC_ADCON0_ANACH_Pos)  // AN63 sub-channel
#define ADC_ADCON0_ADGO_Pos    (1)
#define ADC_ADCON0_ADGO_Msk    (0x1 << ADC_ADCON0_ADGO_Pos)   // 0x02
// ADCON1
#define ADC_ADCON1_ADEN_Pos    (7)
#define ADC_ADCON1_ADEN_Msk    (0x1 << ADC_ADCON1_ADEN_Pos)   // 0x80
#define ADC_ADCON1_ADCKS_Pos   (4)
#define ADC_ADCON1_ADCKS_Msk   (0x7 << ADC_ADCON1_ADCKS_Pos)
// ADCON2
#define ADC_ADCON2_ADCEX_Pos   (7)
#define ADC_ADCON2_ADCEX_Msk   (0x1 << ADC_ADCON2_ADCEX_Pos)
#define ADC_ADCON2_ADTGS_Pos   (4)
#define ADC_ADCON2_ADTGS_Msk   (0x3 << ADC_ADCON2_ADTGS_Pos)
#define ADC_ADCON2_ADEGS_Pos   (2)
#define ADC_ADCON2_ADEGS_Msk   (0x3 << ADC_ADCON2_ADEGS_Pos)
// ADCMPC
#define ADC_ADCMPC_ADFBEN_Pos  (7)
#define ADC_ADCMPC_ADFBEN_Msk  (0x1 << ADC_ADCMPC_ADFBEN_Pos)
#define ADC_ADCMPC_ADCMPPS_Pos (6)
#define ADC_ADCMPC_ADCMPPS_Msk (0x1 << ADC_ADCMPC_ADCMPPS_Pos)
#define ADC_ADCMPC_ADCMPO_Pos  (4)
#define ADC_ADCMPC_ADCMPO_Msk  (0x1 << ADC_ADCMPC_ADCMPO_Pos)
#define ADC_ADCMPC_ADDLY_Pos   (0)
#define ADC_ADCMPC_ADDLY_Msk   (0x3 << ADC_ADCMPC_ADDLY_Pos)
// ADCLDO (XSFR)
#define ADC_ADCLDO_LDOEN_Pos   (7)
#define ADC_ADCLDO_LDOEN_Msk   (0x1 << ADC_ADCLDO_LDOEN_Pos)  // 0x80
#define ADC_ADCLDO_VSEL_Pos    (5)
#define ADC_ADCLDO_VSEL_Msk    (0x3 << ADC_ADCLDO_VSEL_Pos)
#define ADC_ADCLDO_OUTEN_Pos   (4)
#define ADC_ADCLDO_OUTEN_Msk   (0x1 << ADC_ADCLDO_OUTEN_Pos)  // 0x10
// EIE2 / EIF2 (ADC bits only)
#define IRQ_EIE2_ADCIE_Pos     (4)
#define IRQ_EIE2_ADCIE_Msk     (0x1 << IRQ_EIE2_ADCIE_Pos)    // 0x10
#define IRQ_EIF2_ADCIF_Pos     (4)
#define IRQ_EIF2_ADCIF_Msk     (0x1 << IRQ_EIF2_ADCIF_Pos)    // 0x10

// ── 4) Vendor enumeration macros (verbatim from StdDriver inc/adc.h) ───────
// Channel select
#define ADC_CH_0    (0x00)   // AN0
#define ADC_CH_1    (0x01)
#define ADC_CH_2    (0x02)
#define ADC_CH_3    (0x03)
#define ADC_CH_4    (0x04)
#define ADC_CH_5    (0x05)
#define ADC_CH_6    (0x06)
#define ADC_CH_7    (0x07)
#define ADC_CH_8    (0x08)
#define ADC_CH_9    (0x09)
#define ADC_CH_10   (0x0a)
#define ADC_CH_11   (0x0b)
#define ADC_CH_12   (0x0c)
#define ADC_CH_13   (0x0d)
#define ADC_CH_14   (0x0e)
#define ADC_CH_15   (0x0f)
#define ADC_CH_16   (0x10)
#define ADC_CH_17   (0x11)
#define ADC_CH_18   (0x12)
#define ADC_CH_19   (0x13)
#define ADC_CH_20   (0x14)
#define ADC_CH_21   (0x15)
#define ADC_CH_22   (0x16)   // AN22..AN25 = P3.0..P3.3
#define ADC_CH_23   (0x17)
#define ADC_CH_24   (0x18)
#define ADC_CH_25   (0x19)
#define ADC_CH_63   (0x3f)   // internal channel mux

#define ADC_CH_63_BGR       (0x0)  // AN63: 1.2 V bandgap reference
#define ADC_CH_63_ACMP_VREF (0x1)  // AN63: ACMP reference
#define ADC_CH_63_TS        (0x4)  // AN63: temperature sensor
#define ADC_CH_63_VSS       (0x5)  // AN63: VSS reference
#define ADC_CH_63_VDD       (0x7)  // AN63: VDD reference
// v1 model note: AN63 internal conversions return code 0 (no BGR/TS model).

// ADC clock divider (ADCON1 bits6:4)
#define ADC_CLK_DIV_2    (0x00)
#define ADC_CLK_DIV_4    (0x01)
#define ADC_CLK_DIV_8    (0x02)
#define ADC_CLK_DIV_16   (0x03)
#define ADC_CLK_DIV_32   (0x04)
#define ADC_CLK_DIV_64   (0x05)
#define ADC_CLK_DIV_128  (0x06)
#define ADC_CLK_DIV_256  (0x07)

// Result justification
#define ADC_RESULT_LEFT   (0x00 << ADC_ADCON0_ADFM_Pos)
#define ADC_RESULT_RIGHT  (0x01 << ADC_ADCON0_ADFM_Pos)

// Hardware trigger sources/modes (ADCON2; accepted and ignored by the v1
// instant model — conversion runs synchronously on ADGO).
#define ADC_TG_PWM0      (0x00)
#define ADC_TG_PWM2      (0x01)
#define ADC_TG_ADET      (0x03)
#define ADC_TG_FALLING   (0x00)
#define ADC_TG_RISING    (0x01)
#define ADC_TG_PWM_PM    (0x02)
#define ADC_TG_PWM_PE    (0x03)

// Compare polarity
#define ADC_ADRES_LESS_THAN_ADCMP  (0x01 << ADC_ADCMPC_ADCMPPS_Pos)
#define ADC_ADRES_MORE_THAN_ADCMP  (0x00 << ADC_ADCMPC_ADCMPPS_Pos)

// Reference voltage (ADCLDO VSEL; v1 model: fixed full-scale, VSEL ignored).
#define ADC_VREF_1P2V  (0x00)
#define ADC_VREF_2V    (0x01 << ADC_ADCLDO_VSEL_Pos)
#define ADC_VREF_2P4V  (0x02 << ADC_ADCLDO_VSEL_Pos)
#define ADC_VREF_3V    (0x03 << ADC_ADCLDO_VSEL_Pos)

// Busy test / conversion start (verbatim vendor idioms). ADGO self-clears
// inside the write hook, so ADC_GO()'s poll (if any) exits immediately.
#define ADC_IS_BUSY  (ADCON0 & ADC_ADCON0_ADGO_Msk)
#define ADC_GO()     do { ADCON0 |= ADC_ADCON0_ADGO_Msk; } while (0)

#endif  // __C51__
