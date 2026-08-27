// SPDX-License-Identifier: Apache-2.0
// REGX52.H — Keil C51 <REGx52.H> dialect shim for the host/wasm sandbox.
//
// Unmodified Keil user sources `#include <REGX52.H>` and use the 8051 dialect
// (sfr/sbit/code/xdata/interrupt). This header erases that dialect so the
// cleaned .cpp compiles as C++17. The user `main` is remapped to
// wink_mcs51_user_main (boundary ①); ISRs use WINK_ISR (boundary ②, see
// wink_mcs51_isr.h). Timers/UART functional models arrive in M2/M3 — the SFR
// bytes declared here are plain shadow storage until then.
#pragma once

// ── 1) Pre-include system headers BEFORE the `main` macro pollutes anything ─
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "mcs51_proxy.hpp"
#include "wink_mcs51_isr.h"
#include "intrins.h"

#ifndef __C51__  // never active under a real Keil toolchain

#ifdef __cplusplus
extern "C" {
#endif
void wink_mcs51_user_main(void);
#ifdef __cplusplus
}
#endif

// ── 2) Symbol remap (boundary ①) ────────────────────────────────────────────
#define main wink_mcs51_user_main

// ── 3) Keil dialect erasure ─────────────────────────────────────────────────
#define reentrant
#define _at_(addr)
#define data
#define idata
#define xdata
#define pdata
#define code   const
#define bit    uint8_t

// sfr/sbit -> C++ proxy instances. `inline` (C++17) gives a single definition
// across TUs (ODR-safe); the C++14 fallback uses static (Spike-S2 §5.2).
#if defined(__cplusplus) && __cplusplus >= 201703L
  #define sfr  inline WinkSfr
  #define sbit inline WinkSbit
#else
  #define sfr  static WinkSfr
  #define sbit static WinkSbit
#endif

// ── 4) SFR byte registers (Keil absolute-address form) ──────────────────────
sfr P0   = 0x80;
sfr SP   = 0x81;
sfr DPL  = 0x82;
sfr DPH  = 0x83;
sfr PCON = 0x87;
sfr TCON = 0x88;
sfr TMOD = 0x89;
sfr TL0  = 0x8A;
sfr TL1  = 0x8B;
sfr TH0  = 0x8C;
sfr TH1  = 0x8D;
sfr P1   = 0x90;
sfr SCON = 0x98;
sfr SBUF = 0x99;
sfr P2   = 0xA0;
sfr IE   = 0xA8;
sfr P3   = 0xB0;
sfr IP   = 0xB8;
sfr PSW  = 0xD0;
sfr ACC  = 0xE0;
sfr B    = 0xF0;

// ── 5) Predefined sbits (absolute bit-address form) ─────────────────────────
// Port bits (bit addr = SFR base + bit; e.g. P1.0 = 0x90).
sbit P0_0 = 0x80; sbit P0_1 = 0x81; sbit P0_2 = 0x82; sbit P0_3 = 0x83;
sbit P0_4 = 0x84; sbit P0_5 = 0x85; sbit P0_6 = 0x86; sbit P0_7 = 0x87;
sbit P1_0 = 0x90; sbit P1_1 = 0x91; sbit P1_2 = 0x92; sbit P1_3 = 0x93;
sbit P1_4 = 0x94; sbit P1_5 = 0x95; sbit P1_6 = 0x96; sbit P1_7 = 0x97;
sbit P2_0 = 0xA0; sbit P2_1 = 0xA1; sbit P2_2 = 0xA2; sbit P2_3 = 0xA3;
sbit P2_4 = 0xA4; sbit P2_5 = 0xA5; sbit P2_6 = 0xA6; sbit P2_7 = 0xA7;
sbit P3_0 = 0xB0; sbit P3_1 = 0xB1; sbit P3_2 = 0xB2; sbit P3_3 = 0xB3;
sbit P3_4 = 0xB4; sbit P3_5 = 0xB5; sbit P3_6 = 0xB6; sbit P3_7 = 0xB7;

// TCON (0x88) bits.
sbit IT0 = 0x88; sbit IE0 = 0x89; sbit IT1 = 0x8A; sbit IE1 = 0x8B;
sbit TR0 = 0x8C; sbit TF0 = 0x8D; sbit TR1 = 0x8E; sbit TF1 = 0x8F;

// SCON (0x98) bits.
sbit RI  = 0x98; sbit TI  = 0x99; sbit RB8 = 0x9A; sbit TB8 = 0x9B;
sbit REN = 0x9C; sbit SM2 = 0x9D; sbit SM1 = 0x9E; sbit SM0 = 0x9F;

// IE (0xA8) bits.
sbit EX0 = 0xA8; sbit ET0 = 0xA9; sbit EX1 = 0xAA; sbit ET1 = 0xAB;
sbit ES  = 0xAC; sbit EA  = 0xAF;

// IP (0xB8) bits.
sbit PX0 = 0xB8; sbit PT0 = 0xB9; sbit PX1 = 0xBA; sbit PT1 = 0xBB; sbit PS = 0xBC;

// PSW (0xD0) bits.
sbit P  = 0xD0; sbit OV  = 0xD2; sbit RS0 = 0xD3; sbit RS1 = 0xD4;
sbit F0 = 0xD5; sbit AC  = 0xD6; sbit CY  = 0xD7;

#endif  // __C51__
