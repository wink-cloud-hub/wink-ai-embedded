// SPDX-License-Identifier: Apache-2.0
// Spike-S2 MINIMAL REGX52.H (throwaway). Eats Keil C51 dialect so unchanged
// user .c compiles as C++17 on GCC/MSVC/emcc.
#pragma once

// 1) pre-include system headers BEFORE the main macro pollutes prototypes
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "mcs51_proxy.hpp"
#include "wink_mcs51_isr.h"

#ifndef __C51__
  #ifdef __cplusplus
  extern "C" {
  #endif
  void wink_mcs51_user_main(void);
  #ifdef __cplusplus
  }
  #endif

  // symbol remap (boundary 1)
  #define main wink_mcs51_user_main

  // dialect erasure
  #define reentrant
  #define _at_(addr)
  #define data
  #define idata
  #define xdata
  #define pdata
  #define code            const
  #define bit             uint8_t
  #if defined(__cplusplus) && __cplusplus >= 201703L
    #define sfr           inline WinkSfr
    #define sbit          inline auto
  #else
    #define sfr           static WinkSfr
    #define sbit          static auto
  #endif

  // SFR ports (Keil absolute-address form -> single-arg WinkSfr)
  sfr P0 = 0x80;
  sfr P1 = 0x90;
  sfr P2 = 0xA0;
  sfr P3 = 0xB0;
#endif
