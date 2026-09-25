/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_SOC_SOC_H
#define WINK_H_GUARD_SOC_SOC_H
#ifndef __WINK_HARVESTED_SOC_SOC_H__
#define __WINK_HARVESTED_SOC_SOC_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdint.h>

#include "esp_assert.h"
#include "esp_bit_defs.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef APB_CLK_FREQ
#define APB_CLK_FREQ ( 80*1000000 )
#endif
#ifndef APP_CPU_NUM
#define APP_CPU_NUM (1)
#endif
#ifndef ASSERT_IF_DPORT_REG
#define ASSERT_IF_DPORT_REG(_r, OP) TRY_STATIC_ASSERT(!IS_DPORT_REG(_r), (Cannot use OP for DPORT registers use DPORT_##OP));
#endif
#ifndef CLEAR_PERI_REG_MASK
#define CLEAR_PERI_REG_MASK(reg, mask) do {                                                                            ASSERT_IF_DPORT_REG((reg), CLEAR_PERI_REG_MASK);                                                            WRITE_PERI_REG((reg), (READ_PERI_REG(reg)&(~(mask))));                                                      } while(0)
#endif
#ifndef ETS_BT_HOST_INUM
#define ETS_BT_HOST_INUM 1
#endif
#ifndef ETS_CACHED_ADDR
#define ETS_CACHED_ADDR(addr) (addr)
#endif
#ifndef ETS_CACHEERR_INUM
#define ETS_CACHEERR_INUM ETS_MEMACCESS_ERR_INUM
#endif
#ifndef ETS_INT_WDT_INUM
#define ETS_INT_WDT_INUM (ETS_T1_WDT_INUM)
#endif
#ifndef ETS_INVALID_INUM
#define ETS_INVALID_INUM 6
#endif
#ifndef ETS_IPC_ISR_INUM
#define ETS_IPC_ISR_INUM 31
#endif
#ifndef ETS_MEMACCESS_ERR_INUM
#define ETS_MEMACCESS_ERR_INUM ETS_T1_WDT_CACHEERR_INUM
#endif
#ifndef ETS_SLC_INUM
#define ETS_SLC_INUM 1
#endif
#ifndef ETS_T1_WDT_CACHEERR_INUM
#define ETS_T1_WDT_CACHEERR_INUM 26
#endif
#ifndef ETS_T1_WDT_INUM
#define ETS_T1_WDT_INUM ETS_T1_WDT_CACHEERR_INUM
#endif
#ifndef ETS_UART0_INUM
#define ETS_UART0_INUM 5
#endif
#ifndef ETS_UART1_INUM
#define ETS_UART1_INUM 5
#endif
#ifndef ETS_UNCACHED_ADDR
#define ETS_UNCACHED_ADDR(addr) (addr)
#endif
#ifndef ETS_WBB_INUM
#define ETS_WBB_INUM 4
#endif
#ifndef ETS_WMAC_INUM
#define ETS_WMAC_INUM 0
#endif
#ifndef FIELD_TO_VALUE
#define FIELD_TO_VALUE(_f, _v) (((_v)&(_f))<<_f##_S)
#endif
#ifndef FIELD_TO_VALUE2
#define FIELD_TO_VALUE2(_f, _v) (((_v)<<_f##_S) & (_f))
#endif
#ifndef GET_PERI_REG_BITS
#define GET_PERI_REG_BITS(reg, hipos,lowpos) ({                                                                         ASSERT_IF_DPORT_REG((reg), GET_PERI_REG_BITS);                                                              ((READ_PERI_REG(reg)>>(lowpos))&((1<<((hipos)-(lowpos)+1))-1));                                             })
#endif
#ifndef GET_PERI_REG_BITS2
#define GET_PERI_REG_BITS2(reg, mask,shift) ({                                                                          ASSERT_IF_DPORT_REG((reg), GET_PERI_REG_BITS2);                                                             ((READ_PERI_REG(reg)>>(shift))&(mask));                                                                     })
#endif
#ifndef GET_PERI_REG_MASK
#define GET_PERI_REG_MASK(reg, mask) ({                                                                                 ASSERT_IF_DPORT_REG((reg), GET_PERI_REG_MASK);                                                              (READ_PERI_REG(reg) & (mask));                                                                              })
#endif
#ifndef IS_DPORT_REG
#define IS_DPORT_REG(_r) (((_r) >= DR_REG_DPORT_BASE) && (_r) <= DR_REG_DPORT_END)
#endif
#ifndef MODEM_REQUIRED_MIN_APB_CLK_FREQ
#define MODEM_REQUIRED_MIN_APB_CLK_FREQ ( 80*1000000 )
#endif
#ifndef PRO_CPU_NUM
#define PRO_CPU_NUM (0)
#endif
#ifndef READ_PERI_REG
#define READ_PERI_REG(addr) ({                                                                                          ASSERT_IF_DPORT_REG((addr), READ_PERI_REG);                                                                 (*((volatile uint32_t *)ETS_UNCACHED_ADDR(addr)));                                                          })
#endif
#ifndef REF_CLK_FREQ
#define REF_CLK_FREQ ( 1000000 )
#endif
#ifndef REG_CLR_BIT
#define REG_CLR_BIT(_r, _b) do {                                                                                       ASSERT_IF_DPORT_REG((_r), REG_CLR_BIT);                                                                     *(volatile uint32_t*)(_r) = (*(volatile uint32_t*)(_r)) & (~(_b));                                          } while(0)
#endif
#ifndef REG_GET_BIT
#define REG_GET_BIT(_r, _b) ({                                                                                         ASSERT_IF_DPORT_REG((_r), REG_GET_BIT);                                                                     (*(volatile uint32_t*)(_r) & (_b));                                                                         })
#endif
#ifndef REG_GET_FIELD
#define REG_GET_FIELD(_r, _f) ({                                                                                        ASSERT_IF_DPORT_REG((_r), REG_GET_FIELD);                                                                   ((REG_READ(_r) >> (_f##_S)) & (_f##_V));                                                                    })
#endif
#ifndef REG_READ
#define REG_READ(_r) ({                                                                                                 ASSERT_IF_DPORT_REG((_r), REG_READ);                                                                        (*(volatile uint32_t *)(_r));                                                                               })
#endif
#ifndef REG_SET_BIT
#define REG_SET_BIT(_r, _b) do {                                                                                       ASSERT_IF_DPORT_REG((_r), REG_SET_BIT);                                                                     *(volatile uint32_t*)(_r) = (*(volatile uint32_t*)(_r)) | (_b);                                             } while(0)
#endif
#ifndef REG_SET_BITS
#define REG_SET_BITS(_r, _b, _m) do {                                                                                   ASSERT_IF_DPORT_REG((_r), REG_SET_BITS);                                                                    *(volatile uint32_t*)(_r) = (*(volatile uint32_t*)(_r) & ~(_m)) | ((_b) & (_m));                            } while(0)
#endif
#ifndef REG_SET_FIELD
#define REG_SET_FIELD(_r, _f, _v) do {                                                                                  ASSERT_IF_DPORT_REG((_r), REG_SET_FIELD);                                                                   REG_WRITE((_r),((REG_READ(_r) & ~((_f##_V) << (_f##_S)))|(((_v) & (_f##_V))<<(_f##_S))));                   } while(0)
#endif
#ifndef REG_WRITE
#define REG_WRITE(_r, _v) do {                                                                                         ASSERT_IF_DPORT_REG((_r), REG_WRITE);                                                                       (*(volatile uint32_t *)(_r)) = (_v);                                                                        } while(0)
#endif
#ifndef SET_PERI_REG_BITS
#define SET_PERI_REG_BITS(reg,bit_map,value,shift) do {                                                                 ASSERT_IF_DPORT_REG((reg), SET_PERI_REG_BITS);                                                              WRITE_PERI_REG((reg),(READ_PERI_REG(reg)&(~((bit_map)<<(shift))))|(((value) & (bit_map))<<(shift)) );       } while(0)
#endif
#ifndef SET_PERI_REG_MASK
#define SET_PERI_REG_MASK(reg, mask) do {                                                                               ASSERT_IF_DPORT_REG((reg), SET_PERI_REG_MASK);                                                              WRITE_PERI_REG((reg), (READ_PERI_REG(reg)|(mask)));                                                         } while(0)
#endif
#ifndef SOC_BYTE_ACCESSIBLE_HIGH
#define SOC_BYTE_ACCESSIBLE_HIGH 0x40000000
#endif
#ifndef SOC_BYTE_ACCESSIBLE_LOW
#define SOC_BYTE_ACCESSIBLE_LOW 0x3FF90000
#endif
#ifndef SOC_CACHE_APP_HIGH
#define SOC_CACHE_APP_HIGH 0x40080000
#endif
#ifndef SOC_CACHE_APP_LOW
#define SOC_CACHE_APP_LOW 0x40078000
#endif
#ifndef SOC_CACHE_PRO_HIGH
#define SOC_CACHE_PRO_HIGH 0x40078000
#endif
#ifndef SOC_CACHE_PRO_LOW
#define SOC_CACHE_PRO_LOW 0x40070000
#endif
#ifndef SOC_DIRAM_DRAM_HIGH
#define SOC_DIRAM_DRAM_HIGH 0x40000000
#endif
#ifndef SOC_DIRAM_DRAM_LOW
#define SOC_DIRAM_DRAM_LOW 0x3FFE0000
#endif
#ifndef SOC_DIRAM_INVERTED
#define SOC_DIRAM_INVERTED 1
#endif
#ifndef SOC_DIRAM_IRAM_HIGH
#define SOC_DIRAM_IRAM_HIGH 0x400C0000
#endif
#ifndef SOC_DIRAM_IRAM_LOW
#define SOC_DIRAM_IRAM_LOW 0x400A0000
#endif
#ifndef SOC_DMA_HIGH
#define SOC_DMA_HIGH 0x40000000
#endif
#ifndef SOC_DMA_LOW
#define SOC_DMA_LOW 0x3FFAE000
#endif
#ifndef SOC_DRAM_HIGH
#define SOC_DRAM_HIGH 0x40000000
#endif
#ifndef SOC_DRAM_LOW
#define SOC_DRAM_LOW 0x3FFAE000
#endif
#ifndef SOC_DROM_HIGH
#define SOC_DROM_HIGH 0x3F800000
#endif
#ifndef SOC_DROM_LOW
#define SOC_DROM_LOW 0x3F400000
#endif
#ifndef SOC_EXTRAM_DATA_HIGH
#define SOC_EXTRAM_DATA_HIGH 0x3FC00000
#endif
#ifndef SOC_EXTRAM_DATA_LOW
#define SOC_EXTRAM_DATA_LOW 0x3F800000
#endif
#ifndef SOC_EXTRAM_DATA_SIZE
#define SOC_EXTRAM_DATA_SIZE (SOC_EXTRAM_DATA_HIGH - SOC_EXTRAM_DATA_LOW)
#endif
#ifndef SOC_IRAM_HIGH
#define SOC_IRAM_HIGH 0x400AA000
#endif
#ifndef SOC_IRAM_LOW
#define SOC_IRAM_LOW 0x40080000
#endif
#ifndef SOC_IROM_HIGH
#define SOC_IROM_HIGH 0x40400000
#endif
#ifndef SOC_IROM_LOW
#define SOC_IROM_LOW 0x400D0000
#endif
#ifndef SOC_IROM_MASK_HIGH
#define SOC_IROM_MASK_HIGH 0x40070000
#endif
#ifndef SOC_IROM_MASK_LOW
#define SOC_IROM_MASK_LOW 0x40000000
#endif
#ifndef SOC_MAX_CONTIGUOUS_RAM_SIZE
#define SOC_MAX_CONTIGUOUS_RAM_SIZE 0x400000
#endif
#ifndef SOC_MEM_INTERNAL_HIGH
#define SOC_MEM_INTERNAL_HIGH 0x400C2000
#endif
#ifndef SOC_MEM_INTERNAL_LOW
#define SOC_MEM_INTERNAL_LOW 0x3FF90000
#endif
#ifndef SOC_ROM_STACK_START
#define SOC_ROM_STACK_START 0x3ffe3f20
#endif
#ifndef SOC_RTC_DATA_HIGH
#define SOC_RTC_DATA_HIGH 0x50002000
#endif
#ifndef SOC_RTC_DATA_LOW
#define SOC_RTC_DATA_LOW 0x50000000
#endif
#ifndef SOC_RTC_DRAM_HIGH
#define SOC_RTC_DRAM_HIGH 0x3FF82000
#endif
#ifndef SOC_RTC_DRAM_LOW
#define SOC_RTC_DRAM_LOW 0x3FF80000
#endif
#ifndef SOC_RTC_IRAM_HIGH
#define SOC_RTC_IRAM_HIGH 0x400C2000
#endif
#ifndef SOC_RTC_IRAM_LOW
#define SOC_RTC_IRAM_LOW 0x400C0000
#endif
#ifndef VALUE_GET_FIELD
#define VALUE_GET_FIELD(_r, _f) (((_r) >> (_f##_S)) & (_f))
#endif
#ifndef VALUE_GET_FIELD2
#define VALUE_GET_FIELD2(_r, _f) (((_r) & (_f))>> (_f##_S))
#endif
#ifndef VALUE_SET_FIELD
#define VALUE_SET_FIELD(_r, _f, _v) ((_r)=(((_r) & ~((_f) << (_f##_S)))|((_v)<<(_f##_S))))
#endif
#ifndef VALUE_SET_FIELD2
#define VALUE_SET_FIELD2(_r, _f, _v) ((_r)=(((_r) & ~(_f))|((_v)<<(_f##_S))))
#endif
#ifndef WRITE_PERI_REG
#define WRITE_PERI_REG(addr, val) do {                                                                                  ASSERT_IF_DPORT_REG((addr), WRITE_PERI_REG);                                                                (*((volatile uint32_t *)ETS_UNCACHED_ADDR(addr))) = (uint32_t)(val);                                        } while(0)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_SOC_SOC_H__ */
#endif /* WINK_H_GUARD_SOC_SOC_H */
