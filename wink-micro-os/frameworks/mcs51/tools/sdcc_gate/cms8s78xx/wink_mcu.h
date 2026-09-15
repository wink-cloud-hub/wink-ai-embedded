/* SPDX-License-Identifier: GPL-3.0-only
 * SDCC compile-gate <wink_mcu.h> for CMS8S78xx-family apps (GAP-03).
 *
 * Uses the transpiled vendor device header + the UNMODIFIED vendor
 * StdDriver inline API headers (gpio/adc/uart/buzzer/system/extint),
 * mirroring the C++ sandbox shim's API surface. The StdDriver include
 * directory is passed with -I from the gate runner; the transpiled
 * cms8s78xx.h is generated next to this file by mcs51_sdcc_devhdr.py. */
#ifndef WINK_MCU_H
#define WINK_MCU_H

#include "cms8s78xx.h"
#include "gpio.h"
#include "adc.h"
#include "uart.h"
#include "buzzer.h"
#include "system.h"
#include "extint.h"
#include "classic_alias.h"

#endif
