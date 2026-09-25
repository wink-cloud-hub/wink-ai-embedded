/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_PRIVATE_PERIPH_CTRL_H
#define WINK_H_GUARD_ESP_PRIVATE_PERIPH_CTRL_H
#ifndef __WINK_HARVESTED_ESP_PRIVATE_PERIPH_CTRL_H__
#define __WINK_HARVESTED_ESP_PRIVATE_PERIPH_CTRL_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdint.h>

#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef PERIPH_RCC_ACQUIRE_ATOMIC
#define PERIPH_RCC_ACQUIRE_ATOMIC(rc_periph, rc_name) for (uint8_t rc_name, _rc_cnt = 1, __DECLARE_RCC_RC_ATOMIC_ENV                        ;  _rc_cnt ? (rc_name = periph_rcc_acquire_enter(rc_periph), 1) : 0;                   periph_rcc_acquire_exit(rc_periph, rc_name), _rc_cnt--)
#endif
#ifndef PERIPH_RCC_ATOMIC
#define PERIPH_RCC_ATOMIC() for (int _rc_cnt = 1, __DECLARE_RCC_ATOMIC_ENV                        ;  _rc_cnt ? (periph_rcc_enter(), 1) : 0;                              periph_rcc_exit(), _rc_cnt--)
#endif
#ifndef PERIPH_RCC_RELEASE_ATOMIC
#define PERIPH_RCC_RELEASE_ATOMIC(rc_periph, rc_name) for (uint8_t rc_name, _rc_cnt = 1, __DECLARE_RCC_RC_ATOMIC_ENV                        ;  _rc_cnt ? (rc_name = periph_rcc_release_enter(rc_periph), 1) : 0;                   periph_rcc_release_exit(rc_periph, rc_name), _rc_cnt--)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#if defined(__WINK_SIM__)
void coex_module_disable(void) WINK_SLA_ERROR("Wink SLA Violation: coex_module_disable out of Core 8 scope.");
#else
void coex_module_disable(void);
#endif

#if defined(__WINK_SIM__)
void coex_module_enable(void) WINK_SLA_ERROR("Wink SLA Violation: coex_module_enable out of Core 8 scope.");
#else
void coex_module_enable(void);
#endif

#if defined(__WINK_SIM__)
__PERIPH_CTRL_DEPRECATE_ATTR void periph_module_disable(shared_periph_module_t periph) WINK_SLA_ERROR("Wink SLA Violation: periph_module_disable out of Core 8 scope.");
#else
__PERIPH_CTRL_DEPRECATE_ATTR void periph_module_disable(shared_periph_module_t periph);
#endif

#if defined(__WINK_SIM__)
__PERIPH_CTRL_DEPRECATE_ATTR void periph_module_enable(shared_periph_module_t periph) WINK_SLA_ERROR("Wink SLA Violation: periph_module_enable out of Core 8 scope.");
#else
__PERIPH_CTRL_DEPRECATE_ATTR void periph_module_enable(shared_periph_module_t periph);
#endif

#if defined(__WINK_SIM__)
__PERIPH_CTRL_DEPRECATE_ATTR void periph_module_reset(shared_periph_module_t periph) WINK_SLA_ERROR("Wink SLA Violation: periph_module_reset out of Core 8 scope.");
#else
__PERIPH_CTRL_DEPRECATE_ATTR void periph_module_reset(shared_periph_module_t periph);
#endif

#if defined(__WINK_SIM__)
uint8_t periph_rcc_acquire_enter(shared_periph_module_t periph) WINK_SLA_ERROR("Wink SLA Violation: periph_rcc_acquire_enter out of Core 8 scope.");
#else
uint8_t periph_rcc_acquire_enter(shared_periph_module_t periph);
#endif

#if defined(__WINK_SIM__)
void periph_rcc_acquire_exit(shared_periph_module_t periph, uint8_t ref_count) WINK_SLA_ERROR("Wink SLA Violation: periph_rcc_acquire_exit out of Core 8 scope.");
#else
void periph_rcc_acquire_exit(shared_periph_module_t periph, uint8_t ref_count);
#endif

#if defined(__WINK_SIM__)
void periph_rcc_enter(void) WINK_SLA_ERROR("Wink SLA Violation: periph_rcc_enter out of Core 8 scope.");
#else
void periph_rcc_enter(void);
#endif

#if defined(__WINK_SIM__)
void periph_rcc_exit(void) WINK_SLA_ERROR("Wink SLA Violation: periph_rcc_exit out of Core 8 scope.");
#else
void periph_rcc_exit(void);
#endif

#if defined(__WINK_SIM__)
uint8_t periph_rcc_release_enter(shared_periph_module_t periph) WINK_SLA_ERROR("Wink SLA Violation: periph_rcc_release_enter out of Core 8 scope.");
#else
uint8_t periph_rcc_release_enter(shared_periph_module_t periph);
#endif

#if defined(__WINK_SIM__)
void periph_rcc_release_exit(shared_periph_module_t periph, uint8_t ref_count) WINK_SLA_ERROR("Wink SLA Violation: periph_rcc_release_exit out of Core 8 scope.");
#else
void periph_rcc_release_exit(shared_periph_module_t periph, uint8_t ref_count);
#endif

#if defined(__WINK_SIM__)
void phy_module_disable(void) WINK_SLA_ERROR("Wink SLA Violation: phy_module_disable out of Core 8 scope.");
#else
void phy_module_disable(void);
#endif

#if defined(__WINK_SIM__)
void phy_module_enable(void) WINK_SLA_ERROR("Wink SLA Violation: phy_module_enable out of Core 8 scope.");
#else
void phy_module_enable(void);
#endif

#if defined(__WINK_SIM__)
bool phy_module_has_clock_bits(uint32_t mask) WINK_SLA_ERROR("Wink SLA Violation: phy_module_has_clock_bits out of Core 8 scope.");
#else
bool phy_module_has_clock_bits(uint32_t mask);
#endif

#if defined(__WINK_SIM__)
void wifi_bt_common_module_disable(void) WINK_SLA_ERROR("Wink SLA Violation: wifi_bt_common_module_disable out of Core 8 scope.");
#else
void wifi_bt_common_module_disable(void);
#endif

#if defined(__WINK_SIM__)
void wifi_bt_common_module_enable(void) WINK_SLA_ERROR("Wink SLA Violation: wifi_bt_common_module_enable out of Core 8 scope.");
#else
void wifi_bt_common_module_enable(void);
#endif

#if defined(__WINK_SIM__)
void wifi_module_disable(void) WINK_SLA_ERROR("Wink SLA Violation: wifi_module_disable out of Core 8 scope.");
#else
void wifi_module_disable(void);
#endif

#if defined(__WINK_SIM__)
void wifi_module_enable(void) WINK_SLA_ERROR("Wink SLA Violation: wifi_module_enable out of Core 8 scope.");
#else
void wifi_module_enable(void);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PRIVATE_PERIPH_CTRL_H__ */
#endif /* WINK_H_GUARD_ESP_PRIVATE_PERIPH_CTRL_H */
