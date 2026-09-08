// SPDX-License-Identifier: Apache-2.0
// REG_CMS8S78XX.H — CMS8S78xx (Cmsemicon) SFR dialect shim for the host/wasm
// sandbox.
//
// Unmodified Keil C51 user sources written for the CMS8S78xx part
// `#include <REG_CMS8S78XX.H>` (or `#include <wink_mcu.h>`) and program the
// on-chip 12-bit ADC through the vendor register map (reference manual Ch.22,
// verified against the vendor device header cms8s78xx.h and StdDriver adc.c):
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
sfr TA     = 0x86;  // Time Access protection register
sfr CKCON  = 0x8E;  // Clock control (timer prescaler)
sfr FUNCCR = 0x91;  // Function control (UART0 clock source)
sfr T34MOD = 0x96;  // Timer 3/4 mode
sfr CLKDIV = 0x97;  // Clock divider
sfr SCON0  = 0x98;  // UART0 control
sfr SBUF0  = 0x99;  // UART0 data buffer
sfr P0TRIS = 0x9A;  // P0 direction (TRIS register, vendor GPIO driver)
sfr P1TRIS = 0xA1;
sfr P2TRIS = 0xA2;
sfr P3TRIS = 0xA3;
sfr EIE1   = 0xA9;  // extended interrupt enable 1
sfr EIE2   = 0xAA;  // extended interrupt enable 2 (bit4 = ADCIE)
sfr P0EXTIE = 0xAC; // P0 pin interrupt enable
sfr P1EXTIE = 0xAD; // P1 pin interrupt enable
sfr P2EXTIE = 0xAE; // P2 pin interrupt enable
sfr P3EXTIE = 0xAF; // P3 pin interrupt enable

sfr EIF2   = 0xB2;  // extended interrupt flag 2   (bit4 = ADCIF)
sfr P0EXTIF = 0xB4; // P0 pin interrupt flag
sfr P1EXTIF = 0xB5; // P1 pin interrupt flag
sfr P2EXTIF = 0xB6; // P2 pin interrupt flag
sfr P3EXTIF = 0xB7; // P3 pin interrupt flag
sfr EIP1   = 0xB9;  // extended interrupt priority
sfr EIP2   = 0xBA;
sfr EIP3   = 0xBB;

sfr T2CON  = 0xC8;  // Timer 2 control
sfr T2IF   = 0xC9;  // Timer 2 interrupt flags
sfr RLDL   = 0xCA;  // Timer 2 reload low
sfr RLDH   = 0xCB;  // Timer 2 reload high
sfr TL2    = 0xCC;  // Timer 2 counter low
sfr TH2    = 0xCD;  // Timer 2 counter high
sfr CCEN   = 0xCE;  // Timer 2 compare/capture enable
sfr T2IE   = 0xCF;  // Timer 2 interrupt enable

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

// ── 1.1) Port pin sbit aliases (CMS8S78xx vendor convention: P00 instead of P0_0) ──
sbit P00 = 0x80; sbit P01 = 0x81; sbit P02 = 0x82; sbit P03 = 0x83;
sbit P04 = 0x84; sbit P05 = 0x85; sbit P06 = 0x86; sbit P07 = 0x87;
sbit P10 = 0x90; sbit P11 = 0x91; sbit P12 = 0x92; sbit P13 = 0x93;
sbit P14 = 0x94; sbit P15 = 0x95; sbit P16 = 0x96; sbit P17 = 0x97;
sbit P20 = 0xA0; sbit P21 = 0xA1; sbit P22 = 0xA2; sbit P23 = 0xA3;
sbit P24 = 0xA4; sbit P25 = 0xA5; sbit P26 = 0xA6; sbit P27 = 0xA7;
sbit P30 = 0xB0; sbit P31 = 0xB1; sbit P32 = 0xB2; sbit P33 = 0xB3;
sbit P34 = 0xB4; sbit P35 = 0xB5; sbit P36 = 0xB6; sbit P37 = 0xB7;

// UART0 sbit aliases
sbit ES0   = 0xAC;  // IE^4 (UART0 interrupt enable)
sbit RI0   = 0x98;  // SCON0.0
sbit TI0   = 0x99;  // SCON0.1
sbit U0RB8 = 0x9A;  // SCON0.2
sbit U0TB8 = 0x9B;  // SCON0.3
sbit U0REN = 0x9C;  // SCON0.4
sbit U0SM2 = 0x9D;  // SCON0.5
sbit U0SM1 = 0x9E;  // SCON0.6
sbit U0SM0 = 0x9F;  // SCON0.7

// Timer 2 sbit aliases
sbit ET2   = 0xAD;  // IE^5 (Timer 2 interrupt enable)
sbit PT2   = 0xBD;  // IP^5 (Timer 2 interrupt priority)

// ── 2) XSFR proxies (MOVX space; wild-pointer-safe, see mcs51_xsfr.hpp) ────
#if defined(__cplusplus) && __cplusplus >= 201703L
  #define xsfr inline WinkXsfr
#else
  #define xsfr static WinkXsfr
#endif

// Pin Function MUX (0xF000..0xF03F)
xsfr P00CFG(0xF000); xsfr P01CFG(0xF001); xsfr P02CFG(0xF002); xsfr P03CFG(0xF003);
xsfr P04CFG(0xF004); xsfr P05CFG(0xF005); xsfr P06CFG(0xF006); xsfr P07CFG(0xF007);
xsfr P10CFG(0xF010); xsfr P11CFG(0xF011); xsfr P12CFG(0xF012); xsfr P13CFG(0xF013);
xsfr P14CFG(0xF014); xsfr P15CFG(0xF015); xsfr P16CFG(0xF016); xsfr P17CFG(0xF017);
xsfr P20CFG(0xF020); xsfr P21CFG(0xF021); xsfr P22CFG(0xF022); xsfr P23CFG(0xF023);
xsfr P24CFG(0xF024); xsfr P25CFG(0xF025);
xsfr P30CFG(0xF030); xsfr P31CFG(0xF031); xsfr P32CFG(0xF032); xsfr P33CFG(0xF033);

// Port External Interrupt Configuration (0xF080..0xF09B)
xsfr P00EICFG(0xF080); xsfr P01EICFG(0xF081); xsfr P02EICFG(0xF082); xsfr P03EICFG(0xF083);
xsfr P04EICFG(0xF084); xsfr P05EICFG(0xF085); xsfr P06EICFG(0xF086); xsfr P07EICFG(0xF087);
xsfr P10EICFG(0xF088); xsfr P11EICFG(0xF089); xsfr P12EICFG(0xF08A); xsfr P13EICFG(0xF08B);
xsfr P14EICFG(0xF08C); xsfr P15EICFG(0xF08D); xsfr P16EICFG(0xF08E); xsfr P17EICFG(0xF08F);
xsfr P20EICFG(0xF090); xsfr P21EICFG(0xF091); xsfr P22EICFG(0xF092); xsfr P23EICFG(0xF093);
xsfr P24EICFG(0xF094); xsfr P25EICFG(0xF095);
xsfr P30EICFG(0xF098); xsfr P31EICFG(0xF099); xsfr P32EICFG(0xF09A); xsfr P33EICFG(0xF09B);

// Port Drive Strength (0xF00C..0xF03C)
xsfr P0DR(0xF00C); xsfr P1DR(0xF01C); xsfr P2DR(0xF02C); xsfr P3DR(0xF03C);

// LED Segment Driver (0xF710..0xF716)
xsfr LEDSDRP0L(0xF710); xsfr LEDSDRP0H(0xF711);
xsfr LEDSDRP1L(0xF712); xsfr LEDSDRP1H(0xF713);
xsfr LEDSDRP2L(0xF714); xsfr LEDSDRP2H(0xF715);
xsfr LEDSDRP3L(0xF716);

// Open-drain / Pull-up / Pull-down
xsfr P0OD(0xF008); xsfr P1OD(0xF018); xsfr P2OD(0xF028); xsfr P3OD(0xF038);
xsfr P0UP(0xF009); xsfr P1UP(0xF019); xsfr P2UP(0xF029); xsfr P3UP(0xF039);
xsfr P0RD(0xF00A); xsfr P1RD(0xF01A); xsfr P2RD(0xF02A); xsfr P3RD(0xF03A);

// Pin Share function selectors (ref manual §7.2.3; MOVX/xdata space).
// PS_XX<6:0> = 0xPN selects port P + pin N for the peripheral function;
// silicon reset 0x7F = no pin connected. The extint model resolves these to
// the physical INT0/INT1 input pin (mcs51_extint.cpp).
xsfr PS_INT0(0xF0C0);  // INT0 input pin selector
xsfr PS_INT1(0xF0C1);  // INT1 input pin selector
xsfr PS_ADET(0xF0CC);  // ADET hardware trigger input pin selector
xsfr PS_RXD(0xF69F);   // RXD input pin selector

xsfr BRTCON(0xF5C0);   // Baud Rate Timer control
xsfr BRTDL(0xF5C1);    // Baud Rate Timer reload low
xsfr BRTDH(0xF5C2);    // Baud Rate Timer reload high

xsfr ADCLDO(0xF692);  // ADC LDO control (bit7 LDOEN, bits6:5 VSEL, bit4 OUTEN)

// ── 3) Vendor bit-position / mask macros (names verbatim from cms8s78xx.h,
//       so the vendor StdDriver adc.c can compile against this shim) ────────
// FUNCCR
#define SYS_FUNCCR_UART0_CKS_Pos (0)
#define SYS_FUNCCR_UART0_CKS_Msk (0x7 << SYS_FUNCCR_UART0_CKS_Pos)

// SCON0
#define UART_SCON0_U0SMn_Pos    (6)
#define UART_SCON0_U0SMn_Msk    (0x3 << UART_SCON0_U0SMn_Pos)
#define UART_SCON0_U0SM2_Pos    (5)
#define UART_SCON0_U0SM2_Msk    (0x1 << UART_SCON0_U0SM2_Pos)
#define UART_SCON0_U0REN_Pos    (4)
#define UART_SCON0_U0REN_Msk    (0x1 << UART_SCON0_U0REN_Pos)
#define UART_SCON0_U0TB8_Pos    (3)
#define UART_SCON0_U0TB8_Msk    (0x1 << UART_SCON0_U0TB8_Pos)
#define UART_SCON0_U0RB8_Pos    (2)
#define UART_SCON0_U0RB8_Msk    (0x1 << UART_SCON0_U0RB8_Pos)
#define UART_SCON0_TI0_Pos      (1)
#define UART_SCON0_TI0_Msk      (0x1 << UART_SCON0_TI0_Pos)
#define UART_SCON0_RI0_Pos      (0)
#define UART_SCON0_RI0_Msk      (0x1 << UART_SCON0_RI0_Pos)

// PCON
#define SYS_PCON_SMOD0_Pos      (7)
#define SYS_PCON_SMOD0_Msk      (0x1 << SYS_PCON_SMOD0_Pos)

// BRTCON
#define UART_BRTCON_BRTEN_Pos   (7)
#define UART_BRTCON_BRTEN_Msk   (0x1 << UART_BRTCON_BRTEN_Pos)
#define UART_BRTCON_BRTCKDIV_Pos (0)
#define UART_BRTCON_BRTCKDIV_Msk (0x7 << UART_BRTCON_BRTCKDIV_Pos)

// UART modes & baud timers (from StdDriver inc/uart.h)
#define UART_MOD_SYN             (0x00)
#define UART_MOD_ASY_8BIT        (0x01)
#define UART_MOD_ASY_9BIT        (0x03)
#define UART_MOD_ASY_9BIT_STATIC (0x02)

#define UART_BAUD_TMR1           (0x00)
#define UART_BAUD_TMR4           (0x01)
#define UART_BAUD_TMR2           (0x02)
#define UART_BAUD_BRT            (0x03)

#define BRT_CLK_DIV_1            (0x00)
#define BRT_CLK_DIV_2            (0x01)
#define BRT_CLK_DIV_4            (0x02)
#define BRT_CLK_DIV_8            (0x03)
#define BRT_CLK_DIV_16           (0x04)
#define BRT_CLK_DIV_32           (0x05)
#define BRT_CLK_DIV_64           (0x06)
#define BRT_CLK_DIV_128          (0x07)

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

// Result justification.
// NOTE: bodies use the vendor StdDriver adc.h EXACT token spacing (e.g.
// "(0x00<< ADC_..._Pos)"). GCC treats two macro definitions as identical only
// when their replacement lists match whitespace-for-whitespace — unlike Clang
// (/wd4005 / -Wno-macro-redefined), GCC has no flag to silence a non-identical
// redefinition and escalates it under -Werror. The vendor header re-#defines
// these same enums, so matching its spacing makes that redefinition silently
// legal on GCC too (tier-b harvest, ADR-0073 D6).
#define ADC_RESULT_LEFT	(0x00<< ADC_ADCON0_ADFM_Pos)
#define ADC_RESULT_RIGHT	(0x01<< ADC_ADCON0_ADFM_Pos)

// Hardware trigger sources/modes (ADCON2; accepted and ignored by the v1
// instant model — conversion runs synchronously on ADGO).
#define ADC_TG_PWM0      (0x00)
#define ADC_TG_PWM2      (0x01)
#define ADC_TG_ADET      (0x03)
#define ADC_TG_FALLING   (0x00)
#define ADC_TG_RISING    (0x01)
#define ADC_TG_PWM_PM    (0x02)
#define ADC_TG_PWM_PE    (0x03)

// Compare polarity (vendor-exact spacing — see result-justify note above).
#define ADC_ADRES_LESS_THAN_ADCMP		(0x01<< ADC_ADCMPC_ADCMPPS_Pos)
#define ADC_ADRES_MORE_THAN_ADCMP		(0x00<< ADC_ADCMPC_ADCMPPS_Pos)

// Reference voltage (ADCLDO VSEL; v1 model: fixed full-scale, VSEL ignored).
// VSEL enums use "(0xNN<<ADC_..._Pos)" with NO space around the shift in the
// vendor header — matched here byte-for-byte for GCC identical-redefinition.
#define ADC_VREF_1P2V  (0x00)
#define ADC_VREF_2V				(0x01<<ADC_ADCLDO_VSEL_Pos)
#define ADC_VREF_2P4V			(0x02<<ADC_ADCLDO_VSEL_Pos)
#define ADC_VREF_3V				(0x03<<ADC_ADCLDO_VSEL_Pos)

// Busy test / conversion start (verbatim vendor idioms). ADGO self-clears
// inside the write hook, so ADC_GO()'s poll (if any) exits immediately.
// ADC_GO() body tokens match the vendor multi-line macro ("do{" ... "}while(0)").
#define ADC_IS_BUSY  (ADCON0 & ADC_ADCON0_ADGO_Msk)
#define ADC_GO()     do{ ADCON0 |= ADC_ADCON0_ADGO_Msk; }while(0)

// ── 5) Vendor GPIO driver macros (verbatim from StdDriver inc/gpio.h) ──────
#define GPIO_SET_MUX_MODE(PortCFG, PinMode)  (PortCFG = PinMode)
#define GPIO_MUX_GPIO                        (0x00)
// Analog channel pin-mux selectors (verbatim values from StdDriver inc/gpio.h).
// P00 configured as AN0 = mux code 0x01. The XSFR PxxCFG write lands on the
// XSFR shadow window; mux routing to the ADC rail is behavioral (no-op).
#define GPIO_P00_MUX_AN0                     (0x01)
#define GPIO_ENABLE_OUTPUT(PortTRIS, PinNum) ((PortTRIS) |= (1 << (PinNum)))
#define GPIO_ENABLE_INPUT(PortTRIS, PinNum)  ((PortTRIS) &= ~(1 << (PinNum)))
#define GPIO_ENABLE_OD(PortOD, PinNum)       ((PortOD) |= (1 << (PinNum)))
#define GPIO_DISABLE_OD(PortOD, PinNum)      ((PortOD) &= ~(1 << (PinNum)))
#define GPIO_ENABLE_UP(PortUP, PinNum)       ((PortUP) |= (1 << (PinNum)))
#define GPIO_DISABLE_UP(PortUP, PinNum)      ((PortUP) &= ~(1 << (PinNum)))
#define GPIO_ENABLE_RD(PortRD, PinNum)       ((PortRD) |= (1 << (PinNum)))
#define GPIO_DISABLE_RD(PortRD, PinNum)      ((PortRD) &= ~(1 << (PinNum)))

// Pin-share mux: PS_XX selector <- 0xPN pin code (verbatim StdDriver inc/gpio.h)
#define GPIO_SET_PS_MODE(Ps_mode, gpio)      (Ps_mode = gpio)
#define GPIO_P30_MUX_INT0                    (0x30)  // PS_INT0 <- P3.0
#define GPIO_P31_MUX_INT1                    (0x31)  // PS_INT1 <- P3.1
#define GPIO_P05_MUX_ADET                    (0x05)  // PS_ADET <- P0.5
#define GPIO_P13_MUX_ADET                    (0x13)  // PS_ADET <- P1.3
#define GPIO_P14_MUX_TXD                     (0x03)  // P1.4 as UART TXD
#define GPIO_P13_MUX_RXD                     (0x02)  // P1.3 as UART RXD
#define GPIO_P13                             (0x13)  // PS_RXD <- P1.3

#define GPIO_PIN_0_MSK                       (0x01)
#define GPIO_PIN_1_MSK                       (0x02)
#define GPIO_PIN_2_MSK                       (0x04)
#define GPIO_PIN_3_MSK                       (0x08)
#define GPIO_PIN_4_MSK                       (0x10)
#define GPIO_PIN_5_MSK                       (0x20)
#define GPIO_PIN_6_MSK                       (0x40)
#define GPIO_PIN_7_MSK                       (0x80)

#define GPIO_PIN_0                           (0x00)
#define GPIO_PIN_1                           (0x01)
#define GPIO_PIN_2                           (0x02)
#define GPIO_PIN_3                           (0x03)
#define GPIO_PIN_4                           (0x04)
#define GPIO_PIN_5                           (0x05)
#define GPIO_PIN_6                           (0x06)
#define GPIO_PIN_7                           (0x07)

#define GPIO0                                (0x00)
#define GPIO1                                (0x01)
#define GPIO2                                (0x02)
#define GPIO3                                (0x03)

#define GPIO_SET_INT_MODE(PortEICFG, PinIntMode) ((PortEICFG) = (PinIntMode))
#define GPIO_INT_RISING                      (0x01)
#define GPIO_INT_FALLING                     (0x02)
#define GPIO_INT_BOTH_EDGE                   (0x03)

static inline void GPIO_EnableInt(uint8_t Port, uint8_t PinMSK) {
    switch (Port) {
        case GPIO0: P0EXTIE |= PinMSK; break;
        case GPIO1: P1EXTIE |= PinMSK; break;
        case GPIO2: P2EXTIE |= PinMSK; break;
        case GPIO3: P3EXTIE |= PinMSK; break;
        default: break;
    }
}

static inline void GPIO_DisableInt(uint8_t Port, uint8_t PinMSK) {
    switch (Port) {
        case GPIO0: P0EXTIE &= ~PinMSK; break;
        case GPIO1: P1EXTIE &= ~PinMSK; break;
        case GPIO2: P2EXTIE &= ~PinMSK; break;
        case GPIO3: P3EXTIE &= ~PinMSK; break;
        default: break;
    }
}

static inline uint8_t GPIO_GetIntFlag(uint8_t Port, uint8_t PinNum) {
    uint8_t PinIntFlag = 0;
    switch (Port) {
        case GPIO0: PinIntFlag = (uint8_t)(P0EXTIF & (1u << PinNum)); break;
        case GPIO1: PinIntFlag = (uint8_t)(P1EXTIF & (1u << PinNum)); break;
        case GPIO2: PinIntFlag = (uint8_t)(P2EXTIF & (1u << PinNum)); break;
        case GPIO3: PinIntFlag = (uint8_t)(P3EXTIF & (1u << PinNum)); break;
        default: break;
    }
    return PinIntFlag;
}

static inline void GPIO_ClearIntFlag(uint8_t Port, uint8_t PinNum) {
    switch (Port) {
        case GPIO0: P0EXTIF = (uint8_t)(0xff & (~(1u << PinNum))); break;
        case GPIO1: P1EXTIF = (uint8_t)(0xff & (~(1u << PinNum))); break;
        case GPIO2: P2EXTIF = (uint8_t)(0xff & (~(1u << PinNum))); break;
        case GPIO3: P3EXTIF = (uint8_t)(0xff & (~(1u << PinNum))); break;
        default: break;
    }
}

// ── 5b) Vendor External-Interrupt driver (verbatim semantics from
//        StdDriver inc/extint.h + src/extint.c): classic 8051 INT0/INT1 with
//        TCON.ITx trigger select (0 = low level, 1 = falling edge) and
//        IE.EXx enable; the physical input pin is routed via the PS_INT0/
//        PS_INT1 pin-share mux (modeled in mcs51_extint.cpp). ────────────────
#define EXTINT0                              (0x00)
#define EXTINT1                              (0x01)
#define EXTINT_TRIG_LOW                      (0x00)
#define EXTINT_TRIG_FALLING                  (0x01)

static inline void EXTINT_ConfigInt(uint8_t Extintn, uint8_t IntMode) {
    if (EXTINT0 == Extintn) {
        if (EXTINT_TRIG_LOW == IntMode) {
            IT0 = 0;
        }
        if (EXTINT_TRIG_FALLING == IntMode) {
            IT0 = 1;
        }
    }
    if (EXTINT1 == Extintn) {
        if (EXTINT_TRIG_LOW == IntMode) {
            IT1 = 0;
        }
        if (EXTINT_TRIG_FALLING == IntMode) {
            IT1 = 1;
        }
    }
}

static inline void EXTINT_EnableInt(uint8_t Extintn) {
    if (EXTINT0 == Extintn) { EX0 = 1; }
    if (EXTINT1 == Extintn) { EX1 = 1; }
}

static inline void EXTINT_DisableInt(uint8_t Extintn) {
    if (EXTINT0 == Extintn) { EX0 = 0; }
    if (EXTINT1 == Extintn) { EX1 = 0; }
}

static inline uint8_t EXTINT_GetIntFlag(uint8_t Extintn) {
    if (EXTINT0 == Extintn) { return (uint8_t)IE0; }
    if (EXTINT1 == Extintn) { return (uint8_t)IE1; }
    return 0u;
}

static inline void EXTINT_ClearIntFlag(uint8_t Extintn) {
    if (EXTINT0 == Extintn) { IE0 = 0; }
    if (EXTINT1 == Extintn) { IE1 = 0; }
}

// ── 6) Vendor System clock macros (verbatim from StdDriver inc/system.h) ───
#define SYS_SET_SYSTEM_CLK(SystemClkDiv)     do{ CLKDIV = (SystemClkDiv); }while(0)
#define SYS_CLK_DIV_1                        (0x00)
#define SYS_CLK_DIV_2                        (0x01)
#define SYS_CLK_DIV_4                        (0x02)
#define SYS_CLK_DIV_6                        (0x03)
#define SYS_CLK_DIV_8                        (0x04)
#define SYS_CLK_DIV_10                       (0x05)
#define SYS_CLK_DIV_12                       (0x06)
#define SYS_CLK_DIV_14                       (0x07)
#define SYS_CLK_DIV_16                       (0x08)
#define SYS_CLK_DIV_18                       (0x09)
#define SYS_CLK_DIV_20                       (0x0a)

// ── 7) Vendor IRQ macros, priorities, and interrupt vectors ────────────────
#define IRQ_ALL_ENABLE()                     do{ EA = 1; }while(0)
#define IRQ_ALL_DISABLE()                    do{ EA = 0; }while(0)
#define IRQ_SET_PRIORITY(Module, Priority)   do{ (void)(Module); (void)(Priority); }while(0)
#define IRQ_PRIORITY_LOW                     (0x00)
#define IRQ_PRIORITY_HIGH                    (0x01)

typedef enum {
    IRQ_INT0    = 0,
    IRQ_TMR0    = 1,
    IRQ_INT1    = 2,
    IRQ_TMR1    = 3,
    IRQ_UART0   = 4,
    IRQ_TMR2    = 5,
    IRQ_P0      = 8,
    IRQ_P1      = 9,
    IRQ_P2      = 10,
    IRQ_P3      = 11,
    IRQ_ACMP    = 15,
    IRQ_TMR3    = 16,
    IRQ_TMR4    = 17,
    IRQ_EPWM    = 18,
    IRQ_ADC     = 19,
    IRQ_WDT     = 20,
    IRQ_I2C     = 21,
    IRQ_SPI     = 22,
    IRQ_LSE_SCM = 25,
    IRQ_LVD     = 26
} en_Priority_Module_t;

#define INT0_VECTOR                          0
#define TMR0_VECTOR                          1
#define INT1_VECTOR                          2
#define TMR1_VECTOR                          3
#define UART0_VECTOR                         4
#define TMR2_VECTOR                          5
#define P0EI_VECTOR                          7
#define P1EI_VECTOR                          8
#define P2EI_VECTOR                          9
#define P3EI_VECTOR                          10
#define ACMP_VECTOR                          14
#define TMR3_VECTOR                          15
#define TMR4_VECTOR                          16
#define EPWM_VECTOR                          18
#define ADC_VECTOR                           19
#define WDT_VECTOR                           20
#define I2C_VECTOR                           21
#define SPI_VECTOR                           22
#define LSE_SCM_VECTOR                       25
#define LVD_VECTOR                           26

// ── 8) Vendor Timer driver macros & shims (verbatim from StdDriver inc/timer.h) ───
#define TMR0                                 (0x00)
#define TMR1                                 (0x01)
#define TMR2                                 (0x02)
#define TMR3                                 (0x03)
#define TMR4                                 (0x04)

#define TMR_MODE_TIMING                      (0x00)
#define TMR_MODE_COUNT                       (0x01)
#define TMR_CLK_DIV_12                       (0x00)
#define TMR_CLK_DIV_4                        (0x01)

#define TMR_TIM_13BIT                        (0x00)
#define TMR_TIM_16BIT                        (0x01)
#define TMR_TIM_AUTO_8BIT                    (0x02)
#define TMR_TIM_TWO_INDEPENDENT_8BIT         (0x03)

#define TMR0_SET_PERIOD(TimerPeriodH, TimerPeriodL) do{ \
    TH0 = (uint8_t)(TimerPeriodH); \
    TL0 = (uint8_t)(TimerPeriodL); \
}while(0)

#define TMR1_SET_PERIOD(TimerPeriodH, TimerPeriodL) do{ \
    TH1 = (uint8_t)(TimerPeriodH); \
    TL1 = (uint8_t)(TimerPeriodL); \
}while(0)

static inline void TMR_ConfigRunMode(uint8_t Timern, uint8_t TimerMode, uint8_t TimerModeBranch) {
    if (Timern == 0) {
        TMOD = (TMOD & 0xF0) | ((TimerMode & 0x01) << 2) | (TimerModeBranch & 0x03);
    } else if (Timern == 1) {
        TMOD = (TMOD & 0x0F) | ((((TimerMode & 0x01) << 2) | (TimerModeBranch & 0x03)) << 4);
    }
}

static inline void TMR_ConfigTimerClk(uint8_t Timern, uint8_t TimerClkDiv) {
    (void)Timern;
    (void)TimerClkDiv;
}

static inline void TMR_ConfigTimerPeriod(uint8_t Timern, uint16_t TimerPeriodH, uint16_t TimerPeriodL) {
    if (Timern == 0) {
        TH0 = (uint8_t)TimerPeriodH;
        TL0 = (uint8_t)TimerPeriodL;
    } else if (Timern == 1) {
        TH1 = (uint8_t)TimerPeriodH;
        TL1 = (uint8_t)TimerPeriodL;
    }
}

static inline void TMR_EnableOverflowInt(uint8_t Timern) {
    if (Timern == 0) {
        ET0 = 1;
    } else if (Timern == 1) {
        ET1 = 1;
    }
}

static inline void TMR_DisableOverflowInt(uint8_t Timern) {
    if (Timern == 0) {
        ET0 = 0;
    } else if (Timern == 1) {
        ET1 = 0;
    }
}

static inline void TMR_Start(uint8_t Timern) {
    if (Timern == 0) {
        TR0 = 1;
    } else if (Timern == 1) {
        TR1 = 1;
    }
}

static inline void TMR_Stop(uint8_t Timern) {
    if (Timern == 0) {
        TR0 = 0;
    } else if (Timern == 1) {
        TR1 = 0;
    }
}

// Timer 2 control & interrupt bit definitions
#define TMR_T2CON_T2PS_Pos   (7)
#define TMR_T2CON_T2PS_Msk   (0x1 << TMR_T2CON_T2PS_Pos)
#define TMR_T2CON_T2Rn_Pos   (3)
#define TMR_T2CON_T2Rn_Msk   (0x3 << TMR_T2CON_T2Rn_Pos)
#define TMR_T2CON_T2In_Pos   (0)
#define TMR_T2CON_T2In_Msk   (0x3 << TMR_T2CON_T2In_Pos)
#define TMR_TMOD_T0Mn_Msk    (0x3 << 0)

#define TMR2_MODE_TIMING     (0x01)
#define TMR2_MODE_COUNT      (0x02)
#define TMR2_MODE_COMPARE    (0x02)

#define TMR2_CLK_DIV_12      (0x00)
#define TMR2_CLK_DIV_24      (0x01 << TMR_T2CON_T2PS_Pos)

#define TMR2_AUTO_LOAD       (0x02 << TMR_T2CON_T2Rn_Pos)
#define TMR2_T2EX_LOAD       (0x03 << TMR_T2CON_T2Rn_Pos)
#define TMR2_LOAD_DISBALE    (0x00 << TMR_T2CON_T2Rn_Pos)

#define IRQ_T2IE_T2OVIE_Pos  (7)
#define IRQ_T2IE_T2OVIE_Msk  (0x1 << IRQ_T2IE_T2OVIE_Pos)
#define IRQ_T2IF_T2F_Pos     (7)
#define IRQ_T2IF_T2F_Msk     (0x1 << IRQ_T2IF_T2F_Pos)

// Timer 2 StdDriver shims (verbatim from StdDriver/src/timer.c)
static inline void TMR2_ConfigRunMode(uint8_t Timer2Mode, uint8_t Timer2LoadMode) {
    uint8_t temp = T2CON;
    temp &= ~(TMR_T2CON_T2Rn_Msk | TMR_T2CON_T2In_Msk);
    temp |= (Timer2Mode | Timer2LoadMode);
    T2CON = temp;
}

static inline void TMR2_ConfigTimerClk(uint8_t TimerClkDiv) {
    uint8_t temp = T2CON;
    temp &= ~(TMR_T2CON_T2PS_Msk);
    temp |= TimerClkDiv;
    T2CON = temp;
}

static inline void TMR2_ConfigTimerPeriod(uint16_t TimerPeriod) {
    TL2 = (uint8_t)TimerPeriod;
    TH2 = (uint8_t)(TimerPeriod >> 8);
    if ((TMR2_AUTO_LOAD == (T2CON & TMR_T2CON_T2Rn_Msk)) || (TMR2_T2EX_LOAD == (T2CON & TMR_T2CON_T2Rn_Msk))) {
        RLDL = (uint8_t)TimerPeriod;
        RLDH = (uint8_t)(TimerPeriod >> 8);
    }
}

static inline void TMR2_EnableOverflowInt(void) {
    T2IE |= IRQ_T2IE_T2OVIE_Msk;
}

static inline void TMR2_DisableOverflowInt(void) {
    T2IE &= ~(IRQ_T2IE_T2OVIE_Msk);
}

static inline uint8_t TMR2_GetOverflowIntFlag(void) {
    return (uint8_t)((T2IF & IRQ_T2IF_T2F_Msk) ? 1 : 0);
}

static inline void TMR2_ClearOverflowIntFlag(void) {
    T2IF = (uint8_t)(0xFF & (~IRQ_T2IF_T2F_Msk));
}

static inline void TMR2_AllIntEnable(void) {
    ET2 = 1;
}

static inline void TMR2_AllIntDisable(void) {
    ET2 = 0;
}

static inline void TMR2_Start(void) {
    if (0x0 == (T2CON & TMR_T2CON_T2In_Msk)) {
        T2CON |= (0x1 << TMR_T2CON_T2In_Pos);
    }
}

static inline void TMR2_Stop(void) {
    T2CON &= ~(TMR_T2CON_T2In_Msk);
}


// ── 9) Vendor ADC driver shims (verbatim bodies from StdDriver/src/adc.c) ──
// Demo apps compile WITHOUT the vendor Keil-side adc.c (see extint/timer
// precedent), so the StdDriver API they call is provided here as file-local
// inline bodies mirroring adc.c line-for-line against the SFR proxies. The
// tier-b host test compiles the UNMODIFIED vendor adc.c instead and defines
// WINK_CMS8S_VENDOR_ADC to suppress these and avoid a linkage/redefinition
// clash (see test/CMakeLists.txt test_mcs51_cms8s_vendor).
#ifndef WINK_CMS8S_VENDOR_ADC
static inline void ADC_Start(void) {
    ADCON1 |= ADC_ADCON1_ADEN_Msk;
}
static inline void ADC_Stop(void) {
    ADCON1 &= ~(ADC_ADCON1_ADEN_Msk);
}
static inline void ADC_ConfigRunMode(uint8_t ADCClkDiv, uint8_t ADCResultType) {
    uint8_t temp;
    temp = ADCON0;
    temp &= ~(ADC_ADCON0_ADFM_Msk);
    temp |= ADCResultType;
    ADCON0 = temp;

    temp = ADCON1;
    temp &= ~(ADC_ADCON1_ADCKS_Msk);
    temp |= (uint8_t)(ADCClkDiv << ADC_ADCON1_ADCKS_Pos);
    ADCON1 = temp;
}
static inline void ADC_EnableChannel(uint8_t ADCChannel) {
    ADCCHS = ADCChannel;
}
static inline uint16_t ADC_GetADCResult(void) {
    if (!(ADCON0 & ADC_ADCON0_ADFM_Msk)) { /* left-justify (ADFM=0) */
        return (uint16_t)(0xfff & (((ADRESH << 4) | (ADRESL >> 4))));
    }
    return (uint16_t)(0xfff & ((ADRESH << 8) | ADRESL));
}
static inline void ADC_EnableInt(void) {
    EIE2 |= IRQ_EIE2_ADCIE_Msk;
}
static inline void ADC_DisableInt(void) {
    EIE2 &= ~(IRQ_EIE2_ADCIE_Msk);
}
static inline uint8_t ADC_GetIntFlag(void) {
    return (uint8_t)((EIF2 & IRQ_EIF2_ADCIF_Msk) ? 1 : 0);
}
static inline void ADC_ClearIntFlag(void) {
    EIF2 = (uint8_t)(0xFF & (~(IRQ_EIF2_ADCIF_Msk)));
}
static inline void ADC_EnableLDO(void) {
    ADCLDO |= ADC_ADCLDO_LDOEN_Msk;
}
static inline void ADC_DisableLDO(void) {
    ADCLDO &= ~(ADC_ADCLDO_LDOEN_Msk);
}
static inline void ADC_ConfigADCVref(uint8_t ADCVref) {
    uint8_t temp;
    temp = ADCLDO;
    temp &= ~(ADC_ADCLDO_VSEL_Msk);
    temp |= ADCVref;
    ADCLDO = temp;
}
static inline void ADC_EnableLDOOutput(void) {
    ADCLDO |= ADC_ADCLDO_OUTEN_Msk;
}
static inline void ADC_EnableHardwareTrig(void) {
    ADCON2 |= (ADC_ADCON2_ADCEX_Msk);
}
static inline void ADC_DisableHardwareTrig(void) {
    ADCON2 &= ~(ADC_ADCON2_ADCEX_Msk);
}
static inline void ADC_ConfigHardwareTrig(uint8_t ADCTGSource, uint8_t TGMode) {
    uint8_t temp;
    temp = ADCON2;
    temp &= ~(ADC_ADCON2_ADTGS_Msk | ADC_ADCON2_ADEGS_Msk);
    temp |= (uint8_t)((ADCTGSource << ADC_ADCON2_ADTGS_Pos) | (TGMode << ADC_ADCON2_ADEGS_Pos));
    ADCON2 = temp;
}
#endif  // WINK_CMS8S_VENDOR_ADC

// ── 10) Vendor UART driver shims (verbatim bodies from StdDriver/src/uart.c) ──
#ifndef WINK_CMS8S_VENDOR_UART
static inline void UART_ConfigRunMode(uint8_t UARTMode, uint8_t UARTBaudTimer) {
    uint8_t temp = SCON0;
    temp &= ~(UART_SCON0_U0SMn_Msk);
    temp |= (uint8_t)(UARTMode << UART_SCON0_U0SMn_Pos);
    SCON0 = temp;
    temp = FUNCCR;
    temp &= ~(SYS_FUNCCR_UART0_CKS_Msk);
    temp |= UARTBaudTimer;
    FUNCCR = temp;
}

static inline void UART_EnableDoubleFrequency(void) {
    PCON |= SYS_PCON_SMOD0_Msk;
}

static inline void UART_DisableDoubleFrequency(void) {
    PCON &= ~(SYS_PCON_SMOD0_Msk);
}

static inline void UART_EnableReceive(void) {
    U0REN = 1;
}

static inline void UART_DisableReceive(void) {
    U0REN = 0;
}

static inline void UART_EnableMult(void) {
    SCON0 |= UART_SCON0_U0SM2_Msk;
}

static inline void UART_DisableMult(void) {
    SCON0 &= ~(UART_SCON0_U0SM2_Msk);
}

static inline void UART_EnableInt(void) {
    ES0 = 1;
}

static inline void UART_DisableInt(void) {
    ES0 = 0;
}

static inline uint8_t UART_GetReceiveIntFlag(void) {
    return ((SCON0 & UART_SCON0_RI0_Msk) ? 1 : 0);
}

static inline void UART_ClearReceiveIntFlag(void) {
    RI0 = 0;
}

static inline uint8_t UART_GetSendIntFlag(void) {
    return ((SCON0 & UART_SCON0_TI0_Msk) ? 1 : 0);
}

static inline void UART_ClearSendIntFlag(void) {
    TI0 = 0;
}

static inline uint8_t UART_GetBuff(void) {
    return SBUF0;
}

static inline void UART_SendBuff(uint8_t UARTSendValue) {
    SBUF0 = UARTSendValue;
}

static inline void UART_SendNinthBit(uint8_t UARTSendValue) {
    if (0 == UARTSendValue) {
        U0TB8 = 0;
    } else {
        U0TB8 = 1;
    }
}

static inline uint8_t UART_GetNinthBit(void) {
    return ((SCON0 & UART_SCON0_U0RB8_Msk) ? 1 : 0);
}

static inline void UART_EnableBRT(void) {
    BRTCON |= UART_BRTCON_BRTEN_Msk;
}

static inline void UART_DisableBRT(void) {
    BRTCON &= ~(UART_BRTCON_BRTEN_Msk);
}

static inline void UART_ConfigBRTClk(uint8_t BRTClkDiv) {
    uint8_t temp = BRTCON;
    temp &= ~(UART_BRTCON_BRTCKDIV_Msk);
    temp |= BRTClkDiv;
    BRTCON = temp;
}

static inline void UART_ConfigBRTPeriod(uint16_t BRTPeriod) {
    BRTDL = (uint8_t)BRTPeriod;
    BRTDH = (uint8_t)(BRTPeriod >> 8);
}

static inline uint16_t UART_ConfigBaudRate(uint32_t BaudRateValue) {
    (void)BaudRateValue;
    return 0;
}
#endif  // WINK_CMS8S_VENDOR_UART

// ── 11) Keil C51 printf & putchar bridge ───────────────────────────────────────
// In Keil C51, printf formats strings and emits each character via user-defined
// putchar(char ch). In the Wasm/host C++ sandbox, we redirect printf calls to
// format into a local buffer and invoke putchar(char ch), ensuring unmodified
// vendor firmware printf statements route through SBUF0 to UARTBus.
#include <stdarg.h>

char putchar(char ch);

static inline int wink_mcs51_printf(const char* fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    for (int i = 0; i < n; ++i) {
        putchar(buf[i]);
    }
    return n;
}
#define printf wink_mcs51_printf

#endif  // __C51__
