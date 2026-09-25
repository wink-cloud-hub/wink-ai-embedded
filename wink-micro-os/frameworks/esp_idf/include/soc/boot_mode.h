/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef _SOC_BOOT_MODE_H_
#define _SOC_BOOT_MODE_H_
#ifndef __WINK_HARVESTED_SOC_BOOT_MODE_H__
#define __WINK_HARVESTED_SOC_BOOT_MODE_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"


#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef BOOT_MODE_GET
#define BOOT_MODE_GET() (GPIO_REG_READ(GPIO_STRAP))
#endif
#ifndef ETS_IS_ATE_BOOT
#define ETS_IS_ATE_BOOT() IS_01110(BOOT_MODE_GET())
#endif
#ifndef ETS_IS_FAST_FLASH_BOOT
#define ETS_IS_FAST_FLASH_BOOT() (IS_1XXXX(BOOT_MODE_GET()) || IS_010XX(BOOT_MODE_GET()))
#endif
#ifndef ETS_IS_FLASH_BOOT
#define ETS_IS_FLASH_BOOT() (IS_1XXXX(BOOT_MODE_GET()) || IS_010XX(BOOT_MODE_GET()) || IS_01100(BOOT_MODE_GET()))
#endif
#ifndef ETS_IS_HSPI_FLASH_BOOT
#define ETS_IS_HSPI_FLASH_BOOT() IS_010XX(BOOT_MODE_GET())
#endif
#ifndef ETS_IS_PRINT_BOOT
#define ETS_IS_PRINT_BOOT() (BOOT_MODE_GET() & 0x2)
#endif
#ifndef ETS_IS_SDIO_BOOT
#define ETS_IS_SDIO_BOOT() IS_01101(BOOT_MODE_GET())
#endif
#ifndef ETS_IS_SDIO_FEI_FEO_V2_BOOT
#define ETS_IS_SDIO_FEI_FEO_V2_BOOT() IS_00X00(BOOT_MODE_GET())
#endif
#ifndef ETS_IS_SDIO_FEI_REO_V2_BOOT
#define ETS_IS_SDIO_FEI_REO_V2_BOOT() IS_00X01(BOOT_MODE_GET())
#endif
#ifndef ETS_IS_SDIO_REI_FEO_V1_BOOT
#define ETS_IS_SDIO_REI_FEO_V1_BOOT() IS_01101(BOOT_MODE_GET())
#endif
#ifndef ETS_IS_SDIO_REI_FEO_V2_BOOT
#define ETS_IS_SDIO_REI_FEO_V2_BOOT() IS_00X10(BOOT_MODE_GET())
#endif
#ifndef ETS_IS_SDIO_REI_REO_V2_BOOT
#define ETS_IS_SDIO_REI_REO_V2_BOOT() IS_00X11(BOOT_MODE_GET())
#endif
#ifndef ETS_IS_SDIO_UART_BOOT
#define ETS_IS_SDIO_UART_BOOT() IS_00XXX(BOOT_MODE_GET())
#endif
#ifndef ETS_IS_SPI_FLASH_BOOT
#define ETS_IS_SPI_FLASH_BOOT() (IS_1XXXX(BOOT_MODE_GET()) || IS_01100(BOOT_MODE_GET()))
#endif
#ifndef ETS_IS_UART_BOOT
#define ETS_IS_UART_BOOT() IS_01111(BOOT_MODE_GET())
#endif
#ifndef IS_00X00
#define IS_00X00(v) (((v)&0x1b)==0x00)
#endif
#ifndef IS_00X01
#define IS_00X01(v) (((v)&0x1b)==0x01)
#endif
#ifndef IS_00X10
#define IS_00X10(v) (((v)&0x1b)==0x02)
#endif
#ifndef IS_00X11
#define IS_00X11(v) (((v)&0x1b)==0x03)
#endif
#ifndef IS_00XXX
#define IS_00XXX(v) (((v)&0x18)==0x00)
#endif
#ifndef IS_010XX
#define IS_010XX(v) (((v)&0x1c)==0x08)
#endif
#ifndef IS_01100
#define IS_01100(v) (((v)&0x1f)==0x0c)
#endif
#ifndef IS_01101
#define IS_01101(v) (((v)&0x1f)==0x0d)
#endif
#ifndef IS_01110
#define IS_01110(v) (((v)&0x1f)==0x0e)
#endif
#ifndef IS_01111
#define IS_01111(v) (((v)&0x1f)==0x0f)
#endif
#ifndef IS_1XXXX
#define IS_1XXXX(v) (((v)&0x10)==0x10)
#endif
#ifndef SEL_NO_BOOT
#define SEL_NO_BOOT 0
#endif
#ifndef SEL_SDIO_BOOT
#define SEL_SDIO_BOOT BIT0
#endif
#ifndef SEL_UART_BOOT
#define SEL_UART_BOOT BIT1
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_SOC_BOOT_MODE_H__ */
#endif /* _SOC_BOOT_MODE_H_ */
