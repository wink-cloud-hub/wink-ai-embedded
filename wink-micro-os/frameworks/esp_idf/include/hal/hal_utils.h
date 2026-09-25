/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_HAL_HAL_UTILS_H
#define WINK_H_GUARD_HAL_HAL_UTILS_H
#ifndef __WINK_HARVESTED_HAL_HAL_UTILS_H__
#define __WINK_HARVESTED_HAL_HAL_UTILS_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    HAL_DIV_ROUND_DOWN = 0,
    HAL_DIV_ROUND_UP = 1,
    HAL_DIV_ROUND = 2,
} hal_utils_div_round_opt_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
uint32_t src_freq_hz;   
    uint32_t exp_freq_hz;   
    uint32_t max_integ;     
    uint32_t min_integ;     
    union {
        uint32_t max_fract;     

        hal_utils_div_round_opt_t round_opt;     
    };
} hal_utils_clk_info_t;
typedef struct {
    uint32_t integer;
    uint32_t denominator;
    uint32_t numerator;
} hal_utils_clk_div_t;
typedef struct {
    uint32_t int_bit;
    uint32_t frac_bit;
    bool saturation;
} hal_utils_fixed_point_t;



#if defined(__WINK_SIM__)
uint8_t hal_utils_bitwise_reverse8(uint8_t n) WINK_SLA_ERROR("Wink SLA Violation: hal_utils_bitwise_reverse8 out of Core 8 scope.");
#else
uint8_t hal_utils_bitwise_reverse8(uint8_t n);
#endif

#if defined(__WINK_SIM__)
uint32_t hal_utils_calc_clk_div_frac_accurate(const hal_utils_clk_info_t *clk_info, hal_utils_clk_div_t *clk_div) WINK_SLA_ERROR("Wink SLA Violation: hal_utils_calc_clk_div_frac_accurate out of Core 8 scope.");
#else
uint32_t hal_utils_calc_clk_div_frac_accurate(const hal_utils_clk_info_t *clk_info, hal_utils_clk_div_t *clk_div);
#endif

#if defined(__WINK_SIM__)
uint32_t hal_utils_calc_clk_div_frac_fast(const hal_utils_clk_info_t *clk_info, hal_utils_clk_div_t *clk_div) WINK_SLA_ERROR("Wink SLA Violation: hal_utils_calc_clk_div_frac_fast out of Core 8 scope.");
#else
uint32_t hal_utils_calc_clk_div_frac_fast(const hal_utils_clk_info_t *clk_info, hal_utils_clk_div_t *clk_div);
#endif

#if defined(__WINK_SIM__)
uint32_t hal_utils_calc_clk_div_integer(const hal_utils_clk_info_t *clk_info, uint32_t *int_div) WINK_SLA_ERROR("Wink SLA Violation: hal_utils_calc_clk_div_integer out of Core 8 scope.");
#else
uint32_t hal_utils_calc_clk_div_integer(const hal_utils_clk_info_t *clk_info, uint32_t *int_div);
#endif

#if defined(__WINK_SIM__)
uint32_t hal_utils_calc_lcm(uint32_t a, uint32_t b) WINK_SLA_ERROR("Wink SLA Violation: hal_utils_calc_lcm out of Core 8 scope.");
#else
uint32_t hal_utils_calc_lcm(uint32_t a, uint32_t b);
#endif

#if defined(__WINK_SIM__)
int hal_utils_float_to_fixed_point_32b(float flt, const hal_utils_fixed_point_t *fp_cfg, uint32_t *fp_out) WINK_SLA_ERROR("Wink SLA Violation: hal_utils_float_to_fixed_point_32b out of Core 8 scope.");
#else
int hal_utils_float_to_fixed_point_32b(float flt, const hal_utils_fixed_point_t *fp_cfg, uint32_t *fp_out);
#endif

#if defined(__WINK_SIM__)
uint32_t hal_utils_gcd(uint32_t num_1, uint32_t num_2) WINK_SLA_ERROR("Wink SLA Violation: hal_utils_gcd out of Core 8 scope.");
#else
uint32_t hal_utils_gcd(uint32_t num_1, uint32_t num_2);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_HAL_HAL_UTILS_H__ */
#endif /* WINK_H_GUARD_HAL_HAL_UTILS_H */
