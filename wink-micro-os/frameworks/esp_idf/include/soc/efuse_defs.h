/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_SOC_EFUSE_DEFS_H
#define WINK_H_GUARD_SOC_EFUSE_DEFS_H
#ifndef __WINK_HARVESTED_SOC_EFUSE_DEFS_H__
#define __WINK_HARVESTED_SOC_EFUSE_DEFS_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"


#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef EFUSE_CODING_SCHEME_VAL_34
#define EFUSE_CODING_SCHEME_VAL_34 0x1
#endif
#ifndef EFUSE_CODING_SCHEME_VAL_NONE
#define EFUSE_CODING_SCHEME_VAL_NONE 0x0
#endif
#ifndef EFUSE_CODING_SCHEME_VAL_REPEAT
#define EFUSE_CODING_SCHEME_VAL_REPEAT 0x2
#endif
#ifndef EFUSE_RD_CHIP_VER_PKG_ESP32D0WDQ5
#define EFUSE_RD_CHIP_VER_PKG_ESP32D0WDQ5 1
#endif
#ifndef EFUSE_RD_CHIP_VER_PKG_ESP32D0WDQ6
#define EFUSE_RD_CHIP_VER_PKG_ESP32D0WDQ6 0
#endif
#ifndef EFUSE_RD_CHIP_VER_PKG_ESP32D0WDR2V3
#define EFUSE_RD_CHIP_VER_PKG_ESP32D0WDR2V3 7
#endif
#ifndef EFUSE_RD_CHIP_VER_PKG_ESP32D2WDQ5
#define EFUSE_RD_CHIP_VER_PKG_ESP32D2WDQ5 2
#endif
#ifndef EFUSE_RD_CHIP_VER_PKG_ESP32PICOD2
#define EFUSE_RD_CHIP_VER_PKG_ESP32PICOD2 4
#endif
#ifndef EFUSE_RD_CHIP_VER_PKG_ESP32PICOD4
#define EFUSE_RD_CHIP_VER_PKG_ESP32PICOD4 5
#endif
#ifndef EFUSE_RD_CHIP_VER_PKG_ESP32PICOV302
#define EFUSE_RD_CHIP_VER_PKG_ESP32PICOV302 6
#endif
#ifndef EFUSE_RD_CHIP_VER_PKG_ESP32U4WDH
#define EFUSE_RD_CHIP_VER_PKG_ESP32U4WDH 4
#endif
#ifndef EFUSE_RD_DIS_BLK1
#define EFUSE_RD_DIS_BLK1 (1<<16)
#endif
#ifndef EFUSE_RD_DIS_BLK2
#define EFUSE_RD_DIS_BLK2 (1<<17)
#endif
#ifndef EFUSE_RD_DIS_BLK3
#define EFUSE_RD_DIS_BLK3 (1<<18)
#endif
#ifndef EFUSE_READ_OP_CODE
#define EFUSE_READ_OP_CODE 0x5aa5
#endif
#ifndef EFUSE_WRITE_OP_CODE
#define EFUSE_WRITE_OP_CODE 0x5a5a
#endif
#ifndef EFUSE_WR_DIS_ABS_DONE_0
#define EFUSE_WR_DIS_ABS_DONE_0 (1<<12)
#endif
#ifndef EFUSE_WR_DIS_ABS_DONE_1
#define EFUSE_WR_DIS_ABS_DONE_1 (1<<13)
#endif
#ifndef EFUSE_WR_DIS_BLK1
#define EFUSE_WR_DIS_BLK1 (1<<7)
#endif
#ifndef EFUSE_WR_DIS_BLK2
#define EFUSE_WR_DIS_BLK2 (1<<8)
#endif
#ifndef EFUSE_WR_DIS_BLK3
#define EFUSE_WR_DIS_BLK3 (1<<9)
#endif
#ifndef EFUSE_WR_DIS_CONSOLE_DL_DISABLE
#define EFUSE_WR_DIS_CONSOLE_DL_DISABLE (1<<15)
#endif
#ifndef EFUSE_WR_DIS_FLASH_CRYPT_CNT
#define EFUSE_WR_DIS_FLASH_CRYPT_CNT (1<<2)
#endif
#ifndef EFUSE_WR_DIS_FLASH_CRYPT_CODING_SCHEME
#define EFUSE_WR_DIS_FLASH_CRYPT_CODING_SCHEME (1<<10)
#endif
#ifndef EFUSE_WR_DIS_JTAG_DISABLE
#define EFUSE_WR_DIS_JTAG_DISABLE (1<<14)
#endif
#ifndef EFUSE_WR_DIS_MAC_SPI_CONFIG_HD
#define EFUSE_WR_DIS_MAC_SPI_CONFIG_HD (1<<3)
#endif
#ifndef EFUSE_WR_DIS_RD_DIS
#define EFUSE_WR_DIS_RD_DIS (1<<0)
#endif
#ifndef EFUSE_WR_DIS_SPI_PAD_CONFIG
#define EFUSE_WR_DIS_SPI_PAD_CONFIG (1<<6)
#endif
#ifndef EFUSE_WR_DIS_WR_DIS
#define EFUSE_WR_DIS_WR_DIS (1<<1)
#endif
#ifndef EFUSE_WR_DIS_XPD_SDIO
#define EFUSE_WR_DIS_XPD_SDIO (1<<5)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_SOC_EFUSE_DEFS_H__ */
#endif /* WINK_H_GUARD_SOC_EFUSE_DEFS_H */
