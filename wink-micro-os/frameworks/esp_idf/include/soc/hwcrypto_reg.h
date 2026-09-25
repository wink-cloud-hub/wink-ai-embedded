/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef __HWCRYPTO_REG_H__
#define __HWCRYPTO_REG_H__
#ifndef __WINK_HARVESTED_SOC_HWCRYPTO_REG_H__
#define __WINK_HARVESTED_SOC_HWCRYPTO_REG_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"


#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef AES_ENDIAN
#define AES_ENDIAN ((DR_REG_AES_BASE) + 0x40)
#endif
#ifndef AES_IDLE_REG
#define AES_IDLE_REG ((DR_REG_AES_BASE) + 0x04)
#endif
#ifndef AES_KEY_BASE
#define AES_KEY_BASE ((DR_REG_AES_BASE) + 0x10)
#endif
#ifndef AES_MODE_REG
#define AES_MODE_REG ((DR_REG_AES_BASE) + 0x08)
#endif
#ifndef AES_START_REG
#define AES_START_REG ((DR_REG_AES_BASE) + 0x00)
#endif
#ifndef AES_TEXT_BASE
#define AES_TEXT_BASE ((DR_REG_AES_BASE) + 0x30)
#endif
#ifndef RSA_CLEAN_REG
#define RSA_CLEAN_REG (RSA_QUERY_CLEAN_REG)
#endif
#ifndef RSA_CLEAR_INTERRUPT_REG
#define RSA_CLEAR_INTERRUPT_REG (DR_REG_RSA_BASE + 0x814)
#endif
#ifndef RSA_INTERRUPT_REG
#define RSA_INTERRUPT_REG (RSA_CLEAR_INTERRUPT_REG)
#endif
#ifndef RSA_MEM_M_BLOCK_BASE
#define RSA_MEM_M_BLOCK_BASE ((DR_REG_RSA_BASE)+0x000)
#endif
#ifndef RSA_MEM_RB_BLOCK_BASE
#define RSA_MEM_RB_BLOCK_BASE ((DR_REG_RSA_BASE)+0x200)
#endif
#ifndef RSA_MEM_X_BLOCK_BASE
#define RSA_MEM_X_BLOCK_BASE ((DR_REG_RSA_BASE)+0x600)
#endif
#ifndef RSA_MEM_Y_BLOCK_BASE
#define RSA_MEM_Y_BLOCK_BASE ((DR_REG_RSA_BASE)+0x400)
#endif
#ifndef RSA_MEM_Z_BLOCK_BASE
#define RSA_MEM_Z_BLOCK_BASE ((DR_REG_RSA_BASE)+0x200)
#endif
#ifndef RSA_MODEXP_MODE_REG
#define RSA_MODEXP_MODE_REG (DR_REG_RSA_BASE + 0x804)
#endif
#ifndef RSA_MODEXP_START_REG
#define RSA_MODEXP_START_REG (DR_REG_RSA_BASE + 0x808)
#endif
#ifndef RSA_MULT_MODE_REG
#define RSA_MULT_MODE_REG (DR_REG_RSA_BASE + 0x80c)
#endif
#ifndef RSA_MULT_START_REG
#define RSA_MULT_START_REG (DR_REG_RSA_BASE + 0x810)
#endif
#ifndef RSA_M_DASH_REG
#define RSA_M_DASH_REG (DR_REG_RSA_BASE + 0x800)
#endif
#ifndef RSA_QUERY_CLEAN_REG
#define RSA_QUERY_CLEAN_REG (DR_REG_RSA_BASE + 0x818)
#endif
#ifndef RSA_QUERY_INTERRUPT_REG
#define RSA_QUERY_INTERRUPT_REG (DR_REG_RSA_BASE + 0x814)
#endif
#ifndef RSA_START_MODEXP_REG
#define RSA_START_MODEXP_REG (RSA_MODEXP_START_REG)
#endif
#ifndef SHA_1_BUSY_REG
#define SHA_1_BUSY_REG ((DR_REG_SHA_BASE) + 0x8c)
#endif
#ifndef SHA_1_CONTINUE_REG
#define SHA_1_CONTINUE_REG ((DR_REG_SHA_BASE) + 0x84)
#endif
#ifndef SHA_1_LOAD_REG
#define SHA_1_LOAD_REG ((DR_REG_SHA_BASE) + 0x88)
#endif
#ifndef SHA_1_START_REG
#define SHA_1_START_REG ((DR_REG_SHA_BASE) + 0x80)
#endif
#ifndef SHA_256_BUSY_REG
#define SHA_256_BUSY_REG ((DR_REG_SHA_BASE) + 0x9c)
#endif
#ifndef SHA_256_CONTINUE_REG
#define SHA_256_CONTINUE_REG ((DR_REG_SHA_BASE) + 0x94)
#endif
#ifndef SHA_256_LOAD_REG
#define SHA_256_LOAD_REG ((DR_REG_SHA_BASE) + 0x98)
#endif
#ifndef SHA_256_START_REG
#define SHA_256_START_REG ((DR_REG_SHA_BASE) + 0x90)
#endif
#ifndef SHA_384_BUSY_REG
#define SHA_384_BUSY_REG ((DR_REG_SHA_BASE) + 0xac)
#endif
#ifndef SHA_384_CONTINUE_REG
#define SHA_384_CONTINUE_REG ((DR_REG_SHA_BASE) + 0xa4)
#endif
#ifndef SHA_384_LOAD_REG
#define SHA_384_LOAD_REG ((DR_REG_SHA_BASE) + 0xa8)
#endif
#ifndef SHA_384_START_REG
#define SHA_384_START_REG ((DR_REG_SHA_BASE) + 0xa0)
#endif
#ifndef SHA_512_BUSY_REG
#define SHA_512_BUSY_REG ((DR_REG_SHA_BASE) + 0xbc)
#endif
#ifndef SHA_512_CONTINUE_REG
#define SHA_512_CONTINUE_REG ((DR_REG_SHA_BASE) + 0xb4)
#endif
#ifndef SHA_512_LOAD_REG
#define SHA_512_LOAD_REG ((DR_REG_SHA_BASE) + 0xb8)
#endif
#ifndef SHA_512_START_REG
#define SHA_512_START_REG ((DR_REG_SHA_BASE) + 0xb0)
#endif
#ifndef SHA_TEXT_BASE
#define SHA_TEXT_BASE ((DR_REG_SHA_BASE) + 0x00)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_SOC_HWCRYPTO_REG_H__ */
#endif /* __HWCRYPTO_REG_H__ */
