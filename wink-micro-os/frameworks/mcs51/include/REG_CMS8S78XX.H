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
sfr T34MOD = 0x96;  // Timer 3/4 mode
sfr CLKDIV = 0x97;  // Clock divider
sfr P0TRIS = 0x9A;  // P0 direction (TRIS register, vendor GPIO driver)
sfr P1TRIS = 0xA1;
sfr P2TRIS = 0xA2;
sfr P3TRIS = 0xA3;
sfr EIE1   = 0xA9;  // extended interrupt enable 1
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

// ── 1.1) Port pin sbit aliases (CMS8S78xx vendor convention: P00 instead of P0_0) ──
sbit P00 = 0x80; sbit P01 = 0x81; sbit P02 = 0x82; sbit P03 = 0x83;
sbit P04 = 0x84; sbit P05 = 0x85; sbit P06 = 0x86; sbit P07 = 0x87;
sbit P10 = 0x90; sbit P11 = 0x91; sbit P12 = 0x92; sbit P13 = 0x93;
sbit P14 = 0x94; sbit P15 = 0x95; sbit P16 = 0x96; sbit P17 = 0x97;
sbit P20 = 0xA0; sbit P21 = 0xA1; sbit P22 = 0xA2; sbit P23 = 0xA3;
sbit P24 = 0xA4; sbit P25 = 0xA5; sbit P26 = 0xA6; sbit P27 = 0xA7;
sbit P30 = 0xB0; sbit P31 = 0xB1; sbit P32 = 0xB2; sbit P33 = 0xB3;
sbit P34 = 0xB4; sbit P35 = 0xB5; sbit P36 = 0xB6; sbit P37 = 0xB7;

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

xsfr ADCLDO(0xF692);  // ADC LDO control (bit7 LDOEN, bits6:5 VSEL, bit4 OUTEN)

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
#define GPIO_ENABLE_OUTPUT(PortTRIS, PinNum) ((PortTRIS) |= (1 << (PinNum)))
#define GPIO_ENABLE_INPUT(PortTRIS, PinNum)  ((PortTRIS) &= ~(1 << (PinNum)))
#define GPIO_ENABLE_OD(PortOD, PinNum)       ((PortOD) |= (1 << (PinNum)))
#define GPIO_DISABLE_OD(PortOD, PinNum)      ((PortOD) &= ~(1 << (PinNum)))
#define GPIO_ENABLE_UP(PortUP, PinNum)       ((PortUP) |= (1 << (PinNum)))
#define GPIO_DISABLE_UP(PortUP, PinNum)      ((PortUP) &= ~(1 << (PinNum)))
#define GPIO_ENABLE_RD(PortRD, PinNum)       ((PortRD) |= (1 << (PinNum)))
#define GPIO_DISABLE_RD(PortRD, PinNum)      ((PortRD) &= ~(1 << (PinNum)))

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

#endif  // __C51__
