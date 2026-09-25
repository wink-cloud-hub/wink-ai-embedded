/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef _DPORT_ACCESS_H_
#define _DPORT_ACCESS_H_
#ifndef __WINK_HARVESTED_SOC_DPORT_ACCESS_H__
#define __WINK_HARVESTED_SOC_DPORT_ACCESS_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdint.h>

#include "sdkconfig.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef DPORT_CLEAR_PERI_REG_MASK
#define DPORT_CLEAR_PERI_REG_MASK(reg, mask) DPORT_WRITE_PERI_REG((reg), (DPORT_READ_PERI_REG(reg)&(~(mask))))
#endif
#ifndef DPORT_FIELD_TO_VALUE
#define DPORT_FIELD_TO_VALUE(_f, _v) (((_v)&(_f))<<_f##_S)
#endif
#ifndef DPORT_FIELD_TO_VALUE2
#define DPORT_FIELD_TO_VALUE2(_f, _v) (((_v)<<_f##_S) & (_f))
#endif
#ifndef DPORT_GET_PERI_REG_BITS
#define DPORT_GET_PERI_REG_BITS(reg, hipos,lowpos) ((DPORT_READ_PERI_REG(reg)>>(lowpos))&((1<<((hipos)-(lowpos)+1))-1))
#endif
#ifndef DPORT_GET_PERI_REG_BITS2
#define DPORT_GET_PERI_REG_BITS2(reg, mask,shift) ((DPORT_READ_PERI_REG(reg)>>(shift))&(mask))
#endif
#ifndef DPORT_GET_PERI_REG_MASK
#define DPORT_GET_PERI_REG_MASK(reg, mask) (DPORT_READ_PERI_REG(reg) & (mask))
#endif
#ifndef DPORT_INTERRUPT_DISABLE
#define DPORT_INTERRUPT_DISABLE() unsigned intLvl = __extension__({ unsigned __tmp;  __asm__ __volatile__("rsil %0, " XTSTR(SOC_DPORT_WORKAROUND_DIS_INTERRUPT_LVL) "\n"  : "=a" (__tmp) : : "memory" );  __tmp;})
#endif
#ifndef DPORT_INTERRUPT_RESTORE
#define DPORT_INTERRUPT_RESTORE() do{ unsigned __tmp = (intLvl);  __asm__ __volatile__("wsr.ps %0 ; rsync\n"  : : "a" (__tmp) : "memory" );  }while(0)
#endif
#ifndef DPORT_READ_PERI_REG
#define DPORT_READ_PERI_REG(reg) DPORT_REG_READ(reg)
#endif
#ifndef DPORT_REG_CLR_BIT
#define DPORT_REG_CLR_BIT(_r, _b) DPORT_REG_WRITE((_r), (DPORT_REG_READ(_r) & (~(_b))))
#endif
#ifndef DPORT_REG_GET_BIT
#define DPORT_REG_GET_BIT(_r, _b) (DPORT_REG_READ(_r) & (_b))
#endif
#ifndef DPORT_REG_GET_FIELD
#define DPORT_REG_GET_FIELD(_r, _f) ((DPORT_REG_READ(_r) >> (_f##_S)) & (_f##_V))
#endif
#ifndef DPORT_REG_READ
#define DPORT_REG_READ(reg) esp_dport_access_reg_read(reg)
#endif
#ifndef DPORT_REG_SET_BIT
#define DPORT_REG_SET_BIT(_r, _b) DPORT_REG_WRITE((_r), (DPORT_REG_READ(_r)|(_b)))
#endif
#ifndef DPORT_REG_SET_BITS
#define DPORT_REG_SET_BITS(_r, _b, _m) DPORT_REG_WRITE((_r), ((DPORT_REG_READ(_r) & (~(_m))) | ((_b) & (_m))))
#endif
#ifndef DPORT_REG_SET_FIELD
#define DPORT_REG_SET_FIELD(_r, _f, _v) DPORT_REG_WRITE((_r), ((DPORT_REG_READ(_r) & (~((_f##_V) << (_f##_S))))|(((_v) & (_f##_V))<<(_f##_S))))
#endif
#ifndef DPORT_REG_WRITE
#define DPORT_REG_WRITE(_r, _v) _DPORT_REG_WRITE((_r), (_v))
#endif
#ifndef DPORT_SEQUENCE_REG_READ
#define DPORT_SEQUENCE_REG_READ(reg) esp_dport_access_sequence_reg_read(reg)
#endif
#ifndef DPORT_SET_PERI_REG_BITS
#define DPORT_SET_PERI_REG_BITS(reg,bit_map,value,shift) DPORT_WRITE_PERI_REG((reg), ((DPORT_READ_PERI_REG(reg)&(~((bit_map)<<(shift))))|(((value) & bit_map)<<(shift))))
#endif
#ifndef DPORT_SET_PERI_REG_MASK
#define DPORT_SET_PERI_REG_MASK(reg, mask) DPORT_WRITE_PERI_REG((reg), (DPORT_READ_PERI_REG(reg)|(mask)))
#endif
#ifndef DPORT_VALUE_GET_FIELD
#define DPORT_VALUE_GET_FIELD(_r, _f) (((_r) >> (_f##_S)) & (_f))
#endif
#ifndef DPORT_VALUE_GET_FIELD2
#define DPORT_VALUE_GET_FIELD2(_r, _f) (((_r) & (_f))>> (_f##_S))
#endif
#ifndef DPORT_VALUE_SET_FIELD
#define DPORT_VALUE_SET_FIELD(_r, _f, _v) ((_r)=(((_r) & ~((_f) << (_f##_S)))|((_v)<<(_f##_S))))
#endif
#ifndef DPORT_VALUE_SET_FIELD2
#define DPORT_VALUE_SET_FIELD2(_r, _f, _v) ((_r)=(((_r) & ~(_f))|((_v)<<(_f##_S))))
#endif
#ifndef DPORT_WRITE_PERI_REG
#define DPORT_WRITE_PERI_REG(addr, val) _DPORT_WRITE_PERI_REG((addr), (val))
#endif
#ifndef XTSTR
#define XTSTR(x) _XTSTR(x)
#endif
#ifndef _DPORT_READ_PERI_REG
#define _DPORT_READ_PERI_REG(addr) (*((volatile uint32_t *)(addr)))
#endif
#ifndef _DPORT_REG_CLR_BIT
#define _DPORT_REG_CLR_BIT(_r, _b) _DPORT_REG_WRITE((_r), (_DPORT_REG_READ(_r) & (~(_b))))
#endif
#ifndef _DPORT_REG_READ
#define _DPORT_REG_READ(_r) (*(volatile uint32_t *)(_r))
#endif
#ifndef _DPORT_REG_SET_BIT
#define _DPORT_REG_SET_BIT(_r, _b) _DPORT_REG_WRITE((_r), (_DPORT_REG_READ(_r)|(_b)))
#endif
#ifndef _DPORT_REG_WRITE
#define _DPORT_REG_WRITE(_r, _v) (*(volatile uint32_t *)(_r)) = (_v)
#endif
#ifndef _DPORT_WRITE_PERI_REG
#define _DPORT_WRITE_PERI_REG(addr, val) (*((volatile uint32_t *)(addr))) = (uint32_t)(val)
#endif
#ifndef _XTSTR
#define _XTSTR(x) # x
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#if defined(__WINK_SIM__)
void esp_dport_access_read_buffer(uint32_t *buff_out, uint32_t address, uint32_t num_words) WINK_SLA_ERROR("Wink SLA Violation: esp_dport_access_read_buffer out of Core 8 scope.");
#else
void esp_dport_access_read_buffer(uint32_t *buff_out, uint32_t address, uint32_t num_words);
#endif

#if defined(__WINK_SIM__)
uint32_t esp_dport_access_reg_read(uint32_t reg) WINK_SLA_ERROR("Wink SLA Violation: esp_dport_access_reg_read out of Core 8 scope.");
#else
uint32_t esp_dport_access_reg_read(uint32_t reg);
#endif

#if defined(__WINK_SIM__)
uint32_t esp_dport_access_sequence_reg_read(uint32_t reg) WINK_SLA_ERROR("Wink SLA Violation: esp_dport_access_sequence_reg_read out of Core 8 scope.");
#else
uint32_t esp_dport_access_sequence_reg_read(uint32_t reg);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_SOC_DPORT_ACCESS_H__ */
#endif /* _DPORT_ACCESS_H_ */
