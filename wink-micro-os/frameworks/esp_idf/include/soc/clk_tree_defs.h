/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_SOC_CLK_TREE_DEFS_H
#define WINK_H_GUARD_SOC_CLK_TREE_DEFS_H
#ifndef __WINK_HARVESTED_SOC_CLK_TREE_DEFS_H__
#define __WINK_HARVESTED_SOC_CLK_TREE_DEFS_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"


#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef SOC_ADC_DIGI_CLKS
#define SOC_ADC_DIGI_CLKS {SOC_MOD_CLK_APLL, SOC_MOD_CLK_PLL_F160M}
#endif
#ifndef SOC_ADC_RTC_CLKS
#define SOC_ADC_RTC_CLKS {SOC_MOD_CLK_RC_FAST}
#endif
#ifndef SOC_CLK_RC_FAST_D256_FREQ_APPROX
#define SOC_CLK_RC_FAST_D256_FREQ_APPROX (SOC_CLK_RC_FAST_FREQ_APPROX / 256)
#endif
#ifndef SOC_CLK_RC_FAST_FREQ_APPROX
#define SOC_CLK_RC_FAST_FREQ_APPROX 8500000
#endif
#ifndef SOC_CLK_RC_SLOW_FREQ_APPROX
#define SOC_CLK_RC_SLOW_FREQ_APPROX 150000
#endif
#ifndef SOC_CLK_XTAL32K_FREQ_APPROX
#define SOC_CLK_XTAL32K_FREQ_APPROX 32768
#endif
#ifndef SOC_DAC_COSINE_CLKS
#define SOC_DAC_COSINE_CLKS {SOC_MOD_CLK_RTC_FAST}
#endif
#ifndef SOC_DAC_DIGI_CLKS
#define SOC_DAC_DIGI_CLKS {SOC_MOD_CLK_PLL_D2, SOC_MOD_CLK_APLL}
#endif
#ifndef SOC_GPTIMER_CLKS
#define SOC_GPTIMER_CLKS {SOC_MOD_CLK_APB}
#endif
#ifndef SOC_I2C_CLKS
#define SOC_I2C_CLKS {SOC_MOD_CLK_APB}
#endif
#ifndef SOC_I2S_CLKS
#define SOC_I2S_CLKS {SOC_MOD_CLK_PLL_F160M, SOC_MOD_CLK_APLL}
#endif
#ifndef SOC_LCD_CLKS
#define SOC_LCD_CLKS {SOC_MOD_CLK_PLL_F160M}
#endif
#ifndef SOC_LEDC_CLKS
#define SOC_LEDC_CLKS {SOC_MOD_CLK_APB, SOC_MOD_CLK_RC_FAST, SOC_MOD_CLK_REF_TICK}
#endif
#ifndef SOC_LEDC_CLK_STRS
#define SOC_LEDC_CLK_STRS {"LEDC_USE_APB_CLK", "LEDC_USE_RC_FAST_CLK", "LEDC_USE_REF_TICK"}
#endif
#ifndef SOC_MCPWM_CAPTURE_CLKS
#define SOC_MCPWM_CAPTURE_CLKS {SOC_MOD_CLK_APB}
#endif
#ifndef SOC_MCPWM_CARRIER_CLKS
#define SOC_MCPWM_CARRIER_CLKS {SOC_MOD_CLK_PLL_F160M}
#endif
#ifndef SOC_MCPWM_TIMER_CLKS
#define SOC_MCPWM_TIMER_CLKS {SOC_MOD_CLK_PLL_F160M}
#endif
#ifndef SOC_MWDT_CLKS
#define SOC_MWDT_CLKS {SOC_MOD_CLK_APB}
#endif
#ifndef SOC_PCNT_CLKS
#define SOC_PCNT_CLKS {SOC_MOD_CLK_APB}
#endif
#ifndef SOC_RMT_CLKS
#define SOC_RMT_CLKS {SOC_MOD_CLK_APB, SOC_MOD_CLK_REF_TICK}
#endif
#ifndef SOC_SDMMC_CLKS
#define SOC_SDMMC_CLKS {SOC_MOD_CLK_PLL_F160M}
#endif
#ifndef SOC_SDM_CLKS
#define SOC_SDM_CLKS {SOC_MOD_CLK_APB}
#endif
#ifndef SOC_SPI_CLKS
#define SOC_SPI_CLKS {SOC_MOD_CLK_APB}
#endif
#ifndef SOC_TWAI_CLKS
#define SOC_TWAI_CLKS {(soc_periph_twai_clk_src_t)SOC_MOD_CLK_APB}
#endif
#ifndef SOC_UART_CLKS
#define SOC_UART_CLKS {SOC_MOD_CLK_APB, SOC_MOD_CLK_REF_TICK}
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    SOC_ROOT_CLK_INT_RC_FAST = 0,
    SOC_ROOT_CLK_INT_RC_SLOW = 1,
    SOC_ROOT_CLK_EXT_XTAL = 2,
    SOC_ROOT_CLK_EXT_XTAL32K = 3,
} soc_root_clk_t;
typedef enum {
    SOC_ROOT_CIRCUIT_CLK_BBPLL = 0,
    SOC_ROOT_CIRCUIT_CLK_APLL = 1,
} soc_root_clk_circuit_t;
typedef enum {
    SOC_CPU_CLK_SRC_XTAL = 0,
    SOC_CPU_CLK_SRC_PLL = 1,
    SOC_CPU_CLK_SRC_RC_FAST = 2,
    SOC_CPU_CLK_SRC_APLL = 3,
    SOC_CPU_CLK_SRC_INVALID = 4,
} soc_cpu_clk_src_t;
typedef enum {
    SOC_RTC_SLOW_CLK_SRC_RC_SLOW = 0,
    SOC_RTC_SLOW_CLK_SRC_XTAL32K = 1,
    SOC_RTC_SLOW_CLK_SRC_RC_FAST_D256 = 2,
    SOC_RTC_SLOW_CLK_SRC_INVALID = 3,
    SOC_RTC_SLOW_CLK_SRC_DEFAULT = 0,
} soc_rtc_slow_clk_src_t;
typedef enum {
    SOC_RTC_FAST_CLK_SRC_XTAL_D4 = 0,
    SOC_RTC_FAST_CLK_SRC_RC_FAST = 1,
    SOC_RTC_FAST_CLK_SRC_INVALID = 2,
    SOC_RTC_FAST_CLK_SRC_DEFAULT = 0,
    SOC_RTC_FAST_CLK_SRC_XTAL_DIV = 0,
} soc_rtc_fast_clk_src_t;
typedef enum {
    SOC_XTAL_FREQ_AUTO = 0,
    SOC_XTAL_FREQ_24M = 24,
    SOC_XTAL_FREQ_26M = 26,
    SOC_XTAL_FREQ_40M = 40,
} soc_xtal_freq_t;
typedef enum {
    SOC_MOD_CLK_CPU = 1,
    SOC_MOD_CLK_RTC_FAST = 2,
    SOC_MOD_CLK_RTC_SLOW = 3,
    SOC_MOD_CLK_APB = 4,
    SOC_MOD_CLK_PLL_D2 = 5,
    SOC_MOD_CLK_PLL_F160M = 6,
    SOC_MOD_CLK_XTAL32K = 7,
    SOC_MOD_CLK_RC_FAST = 8,
    SOC_MOD_CLK_RC_FAST_D256 = 9,
    SOC_MOD_CLK_XTAL = 10,
    SOC_MOD_CLK_REF_TICK = 11,
    SOC_MOD_CLK_APLL = 12,
    SOC_MOD_CLK_INVALID = 13,
} soc_module_clk_t;
typedef enum {
    SYSTIMER_CLK_SRC_XTAL = 10,
    SYSTIMER_CLK_SRC_DEFAULT = 10,
} soc_periph_systimer_clk_src_t;
typedef enum {
    GPTIMER_CLK_SRC_APB = 4,
    GPTIMER_CLK_SRC_DEFAULT = 4,
} soc_periph_gptimer_clk_src_t;
typedef enum {
    LCD_CLK_SRC_PLL160M = 6,
    LCD_CLK_SRC_DEFAULT = 6,
} soc_periph_lcd_clk_src_t;
typedef enum {
    RMT_CLK_SRC_APB = 4,
    RMT_CLK_SRC_REF_TICK = 11,
    RMT_CLK_SRC_DEFAULT = 4,
} soc_periph_rmt_clk_src_t;
typedef enum {
    PCNT_CLK_SRC_APB = 4,
    PCNT_CLK_SRC_DEFAULT = 4,
} soc_periph_pcnt_clk_src_t;
typedef enum {
    UART_SCLK_APB = 4,
    UART_SCLK_REF_TICK = 11,
    UART_SCLK_DEFAULT = 4,
} soc_periph_uart_clk_src_legacy_t;
typedef enum {
    MCPWM_TIMER_CLK_SRC_PLL160M = 6,
    MCPWM_TIMER_CLK_SRC_DEFAULT = 6,
} soc_periph_mcpwm_timer_clk_src_t;
typedef enum {
    MCPWM_CAPTURE_CLK_SRC_APB = 4,
    MCPWM_CAPTURE_CLK_SRC_DEFAULT = 4,
} soc_periph_mcpwm_capture_clk_src_t;
typedef enum {
    MCPWM_CARRIER_CLK_SRC_PLL160M = 6,
    MCPWM_CARRIER_CLK_SRC_DEFAULT = 6,
} soc_periph_mcpwm_carrier_clk_src_t;
typedef enum {
    I2S_CLK_SRC_DEFAULT = 6,
    I2S_CLK_SRC_PLL_160M = 6,
    I2S_CLK_SRC_APLL = 12,
} soc_periph_i2s_clk_src_t;
typedef enum {
    I2C_CLK_SRC_APB = 4,
    I2C_CLK_SRC_DEFAULT = 4,
} soc_periph_i2c_clk_src_t;
typedef enum {
    SPI_CLK_SRC_DEFAULT = 4,
    SPI_CLK_SRC_APB = 4,
} soc_periph_spi_clk_src_t;
typedef enum {
    SDM_CLK_SRC_APB = 4,
    SDM_CLK_SRC_DEFAULT = 4,
} soc_periph_sdm_clk_src_t;
typedef enum {
    DAC_DIGI_CLK_SRC_PLLD2 = 5,
    DAC_DIGI_CLK_SRC_APLL = 12,
    DAC_DIGI_CLK_SRC_DEFAULT = 5,
} soc_periph_dac_digi_clk_src_t;
typedef enum {
    DAC_COSINE_CLK_SRC_RTC_FAST = 2,
    DAC_COSINE_CLK_SRC_DEFAULT = 2,
} soc_periph_dac_cosine_clk_src_t;
typedef enum {
    TWAI_CLK_SRC_APB = 4,
    TWAI_CLK_SRC_DEFAULT = 4,
} soc_periph_twai_clk_src_t;
typedef enum {
    ADC_DIGI_CLK_SRC_PLL_F160M = 6,
    ADC_DIGI_CLK_SRC_APLL = 12,
    ADC_DIGI_CLK_SRC_DEFAULT = 6,
} soc_periph_adc_digi_clk_src_t;
typedef enum {
    ADC_RTC_CLK_SRC_RC_FAST = 8,
    ADC_RTC_CLK_SRC_DEFAULT = 8,
} soc_periph_adc_rtc_clk_src_t;
typedef enum {
    MWDT_CLK_SRC_APB = 4,
    MWDT_CLK_SRC_DEFAULT = 4,
} soc_periph_mwdt_clk_src_t;
typedef enum {
    LEDC_AUTO_CLK = 0,
    LEDC_USE_APB_CLK = 4,
    LEDC_USE_RC_FAST_CLK = 8,
    LEDC_USE_REF_TICK = 11,
} soc_periph_ledc_clk_src_legacy_t;
typedef enum {
    SDMMC_CLK_SRC_DEFAULT = 6,
    SDMMC_CLK_SRC_PLL160M = 6,
} soc_periph_sdmmc_clk_src_t;
typedef enum {
    CLKOUT_SIG_I2S0 = 0,
    CLKOUT_SIG_PLL = 1,
    CLKOUT_SIG_RC_SLOW = 4,
    CLKOUT_SIG_XTAL = 5,
    CLKOUT_SIG_APLL = 6,
    CLKOUT_SIG_REF_TICK = 12,
    CLKOUT_SIG_PLL_F80M = 13,
    CLKOUT_SIG_RC_FAST = 14,
    CLKOUT_SIG_I2S1 = 15,
    CLKOUT_SIG_INVALID = 255,
} soc_clkout_sig_id_t;
typedef enum {
    CLK_CAL_RTC_SLOW = 0,
    CLK_CAL_RC_FAST_D256 = 1,
    CLK_CAL_32K_XTAL = 2,
} soc_clk_freq_calculation_src_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_SOC_CLK_TREE_DEFS_H__ */
#endif /* WINK_H_GUARD_SOC_CLK_TREE_DEFS_H */
