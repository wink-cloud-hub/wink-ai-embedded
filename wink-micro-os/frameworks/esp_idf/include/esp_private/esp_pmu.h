/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_PRIVATE_ESP_PMU_H
#define WINK_H_GUARD_ESP_PRIVATE_ESP_PMU_H
#ifndef __WINK_HARVESTED_ESP_PRIVATE_ESP_PMU_H__
#define __WINK_HARVESTED_ESP_PRIVATE_ESP_PMU_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "soc/clk_tree_defs.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef PMU_SLEEP_PD_CNNT
#define PMU_SLEEP_PD_CNNT BIT(15)
#endif
#ifndef PMU_SLEEP_PD_CPU
#define PMU_SLEEP_PD_CPU BIT(4)
#endif
#ifndef PMU_SLEEP_PD_HP_AON
#define PMU_SLEEP_PD_HP_AON BIT(5)
#endif
#ifndef PMU_SLEEP_PD_HP_PERIPH
#define PMU_SLEEP_PD_HP_PERIPH BIT(3)
#endif
#ifndef PMU_SLEEP_PD_LP_PERIPH
#define PMU_SLEEP_PD_LP_PERIPH BIT(14)
#endif
#ifndef PMU_SLEEP_PD_MEM
#define PMU_SLEEP_PD_MEM (PMU_SLEEP_PD_MEM_G0|PMU_SLEEP_PD_MEM_G1|PMU_SLEEP_PD_MEM_G2|PMU_SLEEP_PD_MEM_G3)
#endif
#ifndef PMU_SLEEP_PD_MEM_G0
#define PMU_SLEEP_PD_MEM_G0 BIT(6)
#endif
#ifndef PMU_SLEEP_PD_MEM_G1
#define PMU_SLEEP_PD_MEM_G1 BIT(7)
#endif
#ifndef PMU_SLEEP_PD_MEM_G2
#define PMU_SLEEP_PD_MEM_G2 BIT(8)
#endif
#ifndef PMU_SLEEP_PD_MEM_G3
#define PMU_SLEEP_PD_MEM_G3 BIT(9)
#endif
#ifndef PMU_SLEEP_PD_MODEM
#define PMU_SLEEP_PD_MODEM BIT(2)
#endif
#ifndef PMU_SLEEP_PD_RC32K
#define PMU_SLEEP_PD_RC32K BIT(13)
#endif
#ifndef PMU_SLEEP_PD_RC_FAST
#define PMU_SLEEP_PD_RC_FAST BIT(11)
#endif
#ifndef PMU_SLEEP_PD_TOP
#define PMU_SLEEP_PD_TOP BIT(0)
#endif
#ifndef PMU_SLEEP_PD_VDDSDIO
#define PMU_SLEEP_PD_VDDSDIO BIT(1)
#endif
#ifndef PMU_SLEEP_PD_XTAL
#define PMU_SLEEP_PD_XTAL BIT(10)
#endif
#ifndef PMU_SLEEP_PD_XTAL32K
#define PMU_SLEEP_PD_XTAL32K BIT(12)
#endif
#ifndef RTC_BROWNOUT_DET_TRIG_EN
#define RTC_BROWNOUT_DET_TRIG_EN 0
#endif
#ifndef RTC_BT_TRIG_EN
#define RTC_BT_TRIG_EN PMU_BLE_SOC_WAKEUP_EN
#endif
#ifndef RTC_EXT0_TRIG_EN
#define RTC_EXT0_TRIG_EN PMU_EXT0_WAKEUP_EN
#endif
#ifndef RTC_EXT1_TRIG_EN
#define RTC_EXT1_TRIG_EN PMU_EXT1_WAKEUP_EN
#endif
#ifndef RTC_GPIO_TRIG_EN
#define RTC_GPIO_TRIG_EN (PMU_GPIO_WAKEUP_EN | PMU_LP_GPIO_WAKEUP_EN)
#endif
#ifndef RTC_LP_CORE_TRAP_TRIG_EN
#define RTC_LP_CORE_TRAP_TRIG_EN PMU_LP_CORE_TRAP_WAKEUP_EN
#endif
#ifndef RTC_LP_CORE_TRIG_EN
#define RTC_LP_CORE_TRIG_EN PMU_LP_CORE_WAKEUP_HP_EN
#endif
#ifndef RTC_LP_VAD_TRIG_EN
#define RTC_LP_VAD_TRIG_EN PMU_LP_I2S_WAKEUP_EN
#endif
#ifndef RTC_SLEEP_DIG_USE_8M
#define RTC_SLEEP_DIG_USE_8M BIT(27)
#endif
#ifndef RTC_SLEEP_FLASH_DPD
#define RTC_SLEEP_FLASH_DPD BIT(24)
#endif
#ifndef RTC_SLEEP_LP_PERIPH_USE_RC_FAST
#define RTC_SLEEP_LP_PERIPH_USE_RC_FAST BIT(25)
#endif
#ifndef RTC_SLEEP_LP_PERIPH_USE_XTAL
#define RTC_SLEEP_LP_PERIPH_USE_XTAL BIT(31)
#endif
#ifndef RTC_SLEEP_NO_ULTRA_LOW
#define RTC_SLEEP_NO_ULTRA_LOW BIT(29)
#endif
#ifndef RTC_SLEEP_PD_CPU
#define RTC_SLEEP_PD_CPU PMU_SLEEP_PD_CPU
#endif
#ifndef RTC_SLEEP_PD_DIG
#define RTC_SLEEP_PD_DIG PMU_SLEEP_PD_TOP
#endif
#ifndef RTC_SLEEP_PD_DIG_PERIPH
#define RTC_SLEEP_PD_DIG_PERIPH PMU_SLEEP_PD_HP_PERIPH
#endif
#ifndef RTC_SLEEP_PD_INT_8M
#define RTC_SLEEP_PD_INT_8M PMU_SLEEP_PD_RC_FAST
#endif
#ifndef RTC_SLEEP_PD_MODEM
#define RTC_SLEEP_PD_MODEM PMU_SLEEP_PD_MODEM
#endif
#ifndef RTC_SLEEP_PD_RTC_PERIPH
#define RTC_SLEEP_PD_RTC_PERIPH PMU_SLEEP_PD_LP_PERIPH
#endif
#ifndef RTC_SLEEP_PD_VDDSDIO
#define RTC_SLEEP_PD_VDDSDIO PMU_SLEEP_PD_VDDSDIO
#endif
#ifndef RTC_SLEEP_PD_XTAL
#define RTC_SLEEP_PD_XTAL PMU_SLEEP_PD_XTAL
#endif
#ifndef RTC_SLEEP_POWER_BY_VBAT
#define RTC_SLEEP_POWER_BY_VBAT BIT(26)
#endif
#ifndef RTC_SLEEP_REJECT_MASK
#define RTC_SLEEP_REJECT_MASK (RTC_EXT0_TRIG_EN         |  RTC_EXT1_TRIG_EN         |  RTC_GPIO_TRIG_EN         |  RTC_TIMER_TRIG_EN        |  RTC_WIFI_TRIG_EN         |  RTC_UART0_TRIG_EN        |  RTC_UART1_TRIG_EN        |  RTC_UART2_TRIG_EN        |  RTC_UART3_TRIG_EN        |  RTC_UART4_TRIG_EN        |  RTC_BT_TRIG_EN           |  RTC_LP_CORE_TRIG_EN      |  RTC_TOUCH_TRIG_EN        |  RTC_XTAL32K_DEAD_TRIG_EN |  RTC_USB_TRIG_EN          |  RTC_LP_VAD_TRIG_EN       |  RTC_VBAT_UNDER_VOLT_TRIG_EN |  RTC_BROWNOUT_DET_TRIG_EN)
#endif
#ifndef RTC_SLEEP_USE_ADC_TESEN_MONITOR
#define RTC_SLEEP_USE_ADC_TESEN_MONITOR BIT(28)
#endif
#ifndef RTC_SLEEP_USE_RTC_WDT
#define RTC_SLEEP_USE_RTC_WDT BIT(23)
#endif
#ifndef RTC_SLEEP_XTAL_AS_RTC_FAST
#define RTC_SLEEP_XTAL_AS_RTC_FAST BIT(30)
#endif
#ifndef RTC_TIMER_TRIG_EN
#define RTC_TIMER_TRIG_EN PMU_RTC_TIMER_WAKEUP_EN
#endif
#ifndef RTC_TOUCH_TRIG_EN
#define RTC_TOUCH_TRIG_EN PMU_TOUCH_WAKEUP_EN
#endif
#ifndef RTC_UART0_TRIG_EN
#define RTC_UART0_TRIG_EN PMU_UART0_WAKEUP_EN
#endif
#ifndef RTC_UART1_TRIG_EN
#define RTC_UART1_TRIG_EN PMU_UART1_WAKEUP_EN
#endif
#ifndef RTC_UART2_TRIG_EN
#define RTC_UART2_TRIG_EN 0
#endif
#ifndef RTC_UART3_TRIG_EN
#define RTC_UART3_TRIG_EN 0
#endif
#ifndef RTC_UART4_TRIG_EN
#define RTC_UART4_TRIG_EN 0
#endif
#ifndef RTC_USB_TRIG_EN
#define RTC_USB_TRIG_EN PMU_USB_WAKEUP_EN
#endif
#ifndef RTC_VBAT_UNDER_VOLT_TRIG_EN
#define RTC_VBAT_UNDER_VOLT_TRIG_EN PMU_VBAT_UNDERVOLT_WAKEUP_EN
#endif
#ifndef RTC_WIFI_TRIG_EN
#define RTC_WIFI_TRIG_EN PMU_WIFI_SOC_WAKEUP_EN
#endif
#ifndef RTC_XTAL32K_DEAD_TRIG_EN
#define RTC_XTAL32K_DEAD_TRIG_EN 0
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
    pmu_hal_context_t * hal;
    void * mc;
    void * priv;
    bool flash_ldo_volt_1v8;
} pmu_context_t;



#if defined(__WINK_SIM__)
pmu_context_t * PMU_instance(void) WINK_SLA_ERROR("Wink SLA Violation: PMU_instance out of Core 8 scope.");
#else
pmu_context_t * PMU_instance(void);
#endif

#if defined(__WINK_SIM__)
void pmu_init(void) WINK_SLA_ERROR("Wink SLA Violation: pmu_init out of Core 8 scope.");
#else
void pmu_init(void);
#endif

#if defined(__WINK_SIM__)
uint32_t pmu_sleep_calculate_hp_hw_wait_time(uint32_t sleep_flags, uint32_t slowclk_period, uint32_t fastclk_period) WINK_SLA_ERROR("Wink SLA Violation: pmu_sleep_calculate_hp_hw_wait_time out of Core 8 scope.");
#else
uint32_t pmu_sleep_calculate_hp_hw_wait_time(uint32_t sleep_flags, uint32_t slowclk_period, uint32_t fastclk_period);
#endif

#if defined(__WINK_SIM__)
uint32_t pmu_sleep_calculate_hw_wait_time(uint32_t sleep_flags, soc_rtc_slow_clk_src_t slowclk_src, uint32_t slowclk_period, uint32_t fastclk_period) WINK_SLA_ERROR("Wink SLA Violation: pmu_sleep_calculate_hw_wait_time out of Core 8 scope.");
#else
uint32_t pmu_sleep_calculate_hw_wait_time(uint32_t sleep_flags, soc_rtc_slow_clk_src_t slowclk_src, uint32_t slowclk_period, uint32_t fastclk_period);
#endif

#if defined(__WINK_SIM__)
uint32_t pmu_sleep_calculate_lp_hw_wait_time(uint32_t sleep_flags, uint32_t slowclk_period, uint32_t fastclk_period) WINK_SLA_ERROR("Wink SLA Violation: pmu_sleep_calculate_lp_hw_wait_time out of Core 8 scope.");
#else
uint32_t pmu_sleep_calculate_lp_hw_wait_time(uint32_t sleep_flags, uint32_t slowclk_period, uint32_t fastclk_period);
#endif

#if defined(__WINK_SIM__)
const pmu_sleep_config_t* pmu_sleep_config_default(pmu_sleep_config_t *config, uint32_t sleep_flags, pmu_sleep_clk_icg_flags_t clk_flags, uint32_t adjustment, soc_rtc_slow_clk_src_t slowclk_src, uint32_t slowclk_period, uint32_t fastclk_period, bool dslp) WINK_SLA_ERROR("Wink SLA Violation: pmu_sleep_config_default out of Core 8 scope.");
#else
const pmu_sleep_config_t* pmu_sleep_config_default(pmu_sleep_config_t *config, uint32_t sleep_flags, pmu_sleep_clk_icg_flags_t clk_flags, uint32_t adjustment, soc_rtc_slow_clk_src_t slowclk_src, uint32_t slowclk_period, uint32_t fastclk_period, bool dslp);
#endif

#if defined(__WINK_SIM__)
void pmu_sleep_disable_regdma_backup(void) WINK_SLA_ERROR("Wink SLA Violation: pmu_sleep_disable_regdma_backup out of Core 8 scope.");
#else
void pmu_sleep_disable_regdma_backup(void);
#endif

#if defined(__WINK_SIM__)
void pmu_sleep_enable_regdma_backup(void) WINK_SLA_ERROR("Wink SLA Violation: pmu_sleep_enable_regdma_backup out of Core 8 scope.");
#else
void pmu_sleep_enable_regdma_backup(void);
#endif

#if defined(__WINK_SIM__)
bool pmu_sleep_finish(bool dslp) WINK_SLA_ERROR("Wink SLA Violation: pmu_sleep_finish out of Core 8 scope.");
#else
bool pmu_sleep_finish(bool dslp);
#endif

#if defined(__WINK_SIM__)
uint32_t pmu_sleep_get_wakup_retention_cost(void) WINK_SLA_ERROR("Wink SLA Violation: pmu_sleep_get_wakup_retention_cost out of Core 8 scope.");
#else
uint32_t pmu_sleep_get_wakup_retention_cost(void);
#endif

#if defined(__WINK_SIM__)
void pmu_sleep_increase_ldo_volt(void) WINK_SLA_ERROR("Wink SLA Violation: pmu_sleep_increase_ldo_volt out of Core 8 scope.");
#else
void pmu_sleep_increase_ldo_volt(void);
#endif

#if defined(__WINK_SIM__)
void pmu_sleep_init(const pmu_sleep_config_t *config, bool dslp) WINK_SLA_ERROR("Wink SLA Violation: pmu_sleep_init out of Core 8 scope.");
#else
void pmu_sleep_init(const pmu_sleep_config_t *config, bool dslp);
#endif

#if defined(__WINK_SIM__)
bool pmu_sleep_pll_already_enabled(void) WINK_SLA_ERROR("Wink SLA Violation: pmu_sleep_pll_already_enabled out of Core 8 scope.");
#else
bool pmu_sleep_pll_already_enabled(void);
#endif

#if defined(__WINK_SIM__)
void pmu_sleep_shutdown_dcdc(void) WINK_SLA_ERROR("Wink SLA Violation: pmu_sleep_shutdown_dcdc out of Core 8 scope.");
#else
void pmu_sleep_shutdown_dcdc(void);
#endif

#if defined(__WINK_SIM__)
uint32_t pmu_sleep_start(uint32_t wakeup_opt, uint32_t reject_opt, uint32_t lslp_mem_inf_fpu, bool dslp) WINK_SLA_ERROR("Wink SLA Violation: pmu_sleep_start out of Core 8 scope.");
#else
uint32_t pmu_sleep_start(uint32_t wakeup_opt, uint32_t reject_opt, uint32_t lslp_mem_inf_fpu, bool dslp);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PRIVATE_ESP_PMU_H__ */
#endif /* WINK_H_GUARD_ESP_PRIVATE_ESP_PMU_H */
