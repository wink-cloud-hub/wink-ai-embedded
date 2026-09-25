/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_ERR_H
#define WINK_H_GUARD_ESP_ERR_H
#ifndef __WINK_HARVESTED_ESP_ERR_H__
#define __WINK_HARVESTED_ESP_ERR_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "esp_compiler.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef ESP_ERROR_CHECK
#define ESP_ERROR_CHECK(x) do {                                          esp_err_t err_rc_ = (x);                                         if (unlikely(err_rc_ != ESP_OK)) {                               _esp_error_check_failed(err_rc_, __FILE__, __LINE__,         __ASSERT_FUNC, #x);                  }                                                                } while(0)
#endif
#ifndef ESP_ERROR_CHECK_WITHOUT_ABORT
#define ESP_ERROR_CHECK_WITHOUT_ABORT(x) ({                                          esp_err_t err_rc_ = (x);                                                     if (unlikely(err_rc_ != ESP_OK)) {                                           _esp_error_check_failed_without_abort(err_rc_, __FILE__, __LINE__,       __ASSERT_FUNC, #x);                }                                                                            err_rc_;                                                                     })
#endif
#ifndef ESP_ERR_FLASH_BASE
#define ESP_ERR_FLASH_BASE 0x6000
#endif
#ifndef ESP_ERR_HW_CRYPTO_BASE
#define ESP_ERR_HW_CRYPTO_BASE 0xc000
#endif
#ifndef ESP_ERR_INVALID_ARG
#define ESP_ERR_INVALID_ARG 0x102
#endif
#ifndef ESP_ERR_INVALID_CRC
#define ESP_ERR_INVALID_CRC 0x109
#endif
#ifndef ESP_ERR_INVALID_MAC
#define ESP_ERR_INVALID_MAC 0x10B
#endif
#ifndef ESP_ERR_INVALID_RESPONSE
#define ESP_ERR_INVALID_RESPONSE 0x108
#endif
#ifndef ESP_ERR_INVALID_SIZE
#define ESP_ERR_INVALID_SIZE 0x104
#endif
#ifndef ESP_ERR_INVALID_STATE
#define ESP_ERR_INVALID_STATE 0x103
#endif
#ifndef ESP_ERR_INVALID_VERSION
#define ESP_ERR_INVALID_VERSION 0x10A
#endif
#ifndef ESP_ERR_MEMPROT_BASE
#define ESP_ERR_MEMPROT_BASE 0xd000
#endif
#ifndef ESP_ERR_MESH_BASE
#define ESP_ERR_MESH_BASE 0x4000
#endif
#ifndef ESP_ERR_NOT_ALLOWED
#define ESP_ERR_NOT_ALLOWED 0x10D
#endif
#ifndef ESP_ERR_NOT_FINISHED
#define ESP_ERR_NOT_FINISHED 0x10C
#endif
#ifndef ESP_ERR_NOT_FOUND
#define ESP_ERR_NOT_FOUND 0x105
#endif
#ifndef ESP_ERR_NOT_SUPPORTED
#define ESP_ERR_NOT_SUPPORTED 0x106
#endif
#ifndef ESP_ERR_NO_MEM
#define ESP_ERR_NO_MEM 0x101
#endif
#ifndef ESP_ERR_TIMEOUT
#define ESP_ERR_TIMEOUT 0x107
#endif
#ifndef ESP_ERR_WIFI_BASE
#define ESP_ERR_WIFI_BASE 0x3000
#endif
#ifndef ESP_FAIL
#define ESP_FAIL -1
#endif
#ifndef ESP_OK
#define ESP_OK 0
#endif
#ifndef __ASSERT_FUNC
#define __ASSERT_FUNC "??"
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef int esp_err_t;

const char * esp_err_to_name(esp_err_t code);
const char * esp_err_to_name_r(esp_err_t code, char *buf, size_t buflen);


#if defined(__WINK_SIM__)
void _esp_error_check_failed(esp_err_t rc, const char *file, int line, const char *function, const char *expression) WINK_SLA_ERROR("Wink SLA Violation: _esp_error_check_failed out of Core 8 scope.");
#else
void _esp_error_check_failed(esp_err_t rc, const char *file, int line, const char *function, const char *expression);
#endif

#if defined(__WINK_SIM__)
void _esp_error_check_failed_without_abort(esp_err_t rc, const char *file, int line, const char *function, const char *expression) WINK_SLA_ERROR("Wink SLA Violation: _esp_error_check_failed_without_abort out of Core 8 scope.");
#else
void _esp_error_check_failed_without_abort(esp_err_t rc, const char *file, int line, const char *function, const char *expression);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_ERR_H__ */
#endif /* WINK_H_GUARD_ESP_ERR_H */
