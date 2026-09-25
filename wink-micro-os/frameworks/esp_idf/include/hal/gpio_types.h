/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_HAL_GPIO_TYPES_H
#define WINK_H_GUARD_HAL_GPIO_TYPES_H
#ifndef __WINK_HARVESTED_HAL_GPIO_TYPES_H__
#define __WINK_HARVESTED_HAL_GPIO_TYPES_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>
#include <stdint.h>

#include "esp_bit_defs.h"
#include "soc/gpio_num.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef GPIO_IS_VALID_DIGITAL_IO_PAD
#define GPIO_IS_VALID_DIGITAL_IO_PAD(gpio_num) ((gpio_num >= 0) && (gpio_num < SOC_GPIO_PIN_COUNT) &&  (((1ULL << (gpio_num)) & SOC_GPIO_VALID_DIGITAL_IO_PAD_MASK) != 0))
#endif
#ifndef GPIO_IS_VALID_GPIO
#define GPIO_IS_VALID_GPIO(gpio_num) ((gpio_num >= 0) && (gpio_num < SOC_GPIO_PIN_COUNT) &&  (((1ULL << (gpio_num)) & SOC_GPIO_VALID_GPIO_MASK) != 0))
#endif
#ifndef GPIO_IS_VALID_OUTPUT_GPIO
#define GPIO_IS_VALID_OUTPUT_GPIO(gpio_num) ((gpio_num >= 0) && (gpio_num < SOC_GPIO_PIN_COUNT) &&  (((1ULL << (gpio_num)) & SOC_GPIO_VALID_OUTPUT_GPIO_MASK) != 0))
#endif
#ifndef GPIO_MODE_DEF_DISABLE
#define GPIO_MODE_DEF_DISABLE (0)
#endif
#ifndef GPIO_MODE_DEF_INPUT
#define GPIO_MODE_DEF_INPUT (BIT0)
#endif
#ifndef GPIO_MODE_DEF_OD
#define GPIO_MODE_DEF_OD (BIT2)
#endif
#ifndef GPIO_MODE_DEF_OUTPUT
#define GPIO_MODE_DEF_OUTPUT (BIT1)
#endif
#ifndef GPIO_PIN_COUNT
#define GPIO_PIN_COUNT (SOC_GPIO_PIN_COUNT)
#endif
#ifndef GPIO_PIN_REG_0
#define GPIO_PIN_REG_0 IO_MUX_GPIO0_REG
#endif
#ifndef GPIO_PIN_REG_1
#define GPIO_PIN_REG_1 IO_MUX_GPIO1_REG
#endif
#ifndef GPIO_PIN_REG_10
#define GPIO_PIN_REG_10 IO_MUX_GPIO10_REG
#endif
#ifndef GPIO_PIN_REG_11
#define GPIO_PIN_REG_11 IO_MUX_GPIO11_REG
#endif
#ifndef GPIO_PIN_REG_12
#define GPIO_PIN_REG_12 IO_MUX_GPIO12_REG
#endif
#ifndef GPIO_PIN_REG_13
#define GPIO_PIN_REG_13 IO_MUX_GPIO13_REG
#endif
#ifndef GPIO_PIN_REG_14
#define GPIO_PIN_REG_14 IO_MUX_GPIO14_REG
#endif
#ifndef GPIO_PIN_REG_15
#define GPIO_PIN_REG_15 IO_MUX_GPIO15_REG
#endif
#ifndef GPIO_PIN_REG_16
#define GPIO_PIN_REG_16 IO_MUX_GPIO16_REG
#endif
#ifndef GPIO_PIN_REG_17
#define GPIO_PIN_REG_17 IO_MUX_GPIO17_REG
#endif
#ifndef GPIO_PIN_REG_18
#define GPIO_PIN_REG_18 IO_MUX_GPIO18_REG
#endif
#ifndef GPIO_PIN_REG_19
#define GPIO_PIN_REG_19 IO_MUX_GPIO19_REG
#endif
#ifndef GPIO_PIN_REG_2
#define GPIO_PIN_REG_2 IO_MUX_GPIO2_REG
#endif
#ifndef GPIO_PIN_REG_20
#define GPIO_PIN_REG_20 IO_MUX_GPIO20_REG
#endif
#ifndef GPIO_PIN_REG_21
#define GPIO_PIN_REG_21 IO_MUX_GPIO21_REG
#endif
#ifndef GPIO_PIN_REG_22
#define GPIO_PIN_REG_22 IO_MUX_GPIO22_REG
#endif
#ifndef GPIO_PIN_REG_23
#define GPIO_PIN_REG_23 IO_MUX_GPIO23_REG
#endif
#ifndef GPIO_PIN_REG_24
#define GPIO_PIN_REG_24 IO_MUX_GPIO24_REG
#endif
#ifndef GPIO_PIN_REG_25
#define GPIO_PIN_REG_25 IO_MUX_GPIO25_REG
#endif
#ifndef GPIO_PIN_REG_26
#define GPIO_PIN_REG_26 IO_MUX_GPIO26_REG
#endif
#ifndef GPIO_PIN_REG_27
#define GPIO_PIN_REG_27 IO_MUX_GPIO27_REG
#endif
#ifndef GPIO_PIN_REG_28
#define GPIO_PIN_REG_28 IO_MUX_GPIO28_REG
#endif
#ifndef GPIO_PIN_REG_29
#define GPIO_PIN_REG_29 IO_MUX_GPIO29_REG
#endif
#ifndef GPIO_PIN_REG_3
#define GPIO_PIN_REG_3 IO_MUX_GPIO3_REG
#endif
#ifndef GPIO_PIN_REG_30
#define GPIO_PIN_REG_30 IO_MUX_GPIO30_REG
#endif
#ifndef GPIO_PIN_REG_31
#define GPIO_PIN_REG_31 IO_MUX_GPIO31_REG
#endif
#ifndef GPIO_PIN_REG_32
#define GPIO_PIN_REG_32 IO_MUX_GPIO32_REG
#endif
#ifndef GPIO_PIN_REG_33
#define GPIO_PIN_REG_33 IO_MUX_GPIO33_REG
#endif
#ifndef GPIO_PIN_REG_34
#define GPIO_PIN_REG_34 IO_MUX_GPIO34_REG
#endif
#ifndef GPIO_PIN_REG_35
#define GPIO_PIN_REG_35 IO_MUX_GPIO35_REG
#endif
#ifndef GPIO_PIN_REG_36
#define GPIO_PIN_REG_36 IO_MUX_GPIO36_REG
#endif
#ifndef GPIO_PIN_REG_37
#define GPIO_PIN_REG_37 IO_MUX_GPIO37_REG
#endif
#ifndef GPIO_PIN_REG_38
#define GPIO_PIN_REG_38 IO_MUX_GPIO38_REG
#endif
#ifndef GPIO_PIN_REG_39
#define GPIO_PIN_REG_39 IO_MUX_GPIO39_REG
#endif
#ifndef GPIO_PIN_REG_4
#define GPIO_PIN_REG_4 IO_MUX_GPIO4_REG
#endif
#ifndef GPIO_PIN_REG_40
#define GPIO_PIN_REG_40 IO_MUX_GPIO40_REG
#endif
#ifndef GPIO_PIN_REG_41
#define GPIO_PIN_REG_41 IO_MUX_GPIO41_REG
#endif
#ifndef GPIO_PIN_REG_42
#define GPIO_PIN_REG_42 IO_MUX_GPIO42_REG
#endif
#ifndef GPIO_PIN_REG_43
#define GPIO_PIN_REG_43 IO_MUX_GPIO43_REG
#endif
#ifndef GPIO_PIN_REG_44
#define GPIO_PIN_REG_44 IO_MUX_GPIO44_REG
#endif
#ifndef GPIO_PIN_REG_45
#define GPIO_PIN_REG_45 IO_MUX_GPIO45_REG
#endif
#ifndef GPIO_PIN_REG_46
#define GPIO_PIN_REG_46 IO_MUX_GPIO46_REG
#endif
#ifndef GPIO_PIN_REG_47
#define GPIO_PIN_REG_47 IO_MUX_GPIO47_REG
#endif
#ifndef GPIO_PIN_REG_48
#define GPIO_PIN_REG_48 IO_MUX_GPIO48_REG
#endif
#ifndef GPIO_PIN_REG_49
#define GPIO_PIN_REG_49 IO_MUX_GPIO49_REG
#endif
#ifndef GPIO_PIN_REG_5
#define GPIO_PIN_REG_5 IO_MUX_GPIO5_REG
#endif
#ifndef GPIO_PIN_REG_50
#define GPIO_PIN_REG_50 IO_MUX_GPIO50_REG
#endif
#ifndef GPIO_PIN_REG_51
#define GPIO_PIN_REG_51 IO_MUX_GPIO51_REG
#endif
#ifndef GPIO_PIN_REG_52
#define GPIO_PIN_REG_52 IO_MUX_GPIO52_REG
#endif
#ifndef GPIO_PIN_REG_53
#define GPIO_PIN_REG_53 IO_MUX_GPIO53_REG
#endif
#ifndef GPIO_PIN_REG_54
#define GPIO_PIN_REG_54 IO_MUX_GPIO54_REG
#endif
#ifndef GPIO_PIN_REG_6
#define GPIO_PIN_REG_6 IO_MUX_GPIO6_REG
#endif
#ifndef GPIO_PIN_REG_7
#define GPIO_PIN_REG_7 IO_MUX_GPIO7_REG
#endif
#ifndef GPIO_PIN_REG_8
#define GPIO_PIN_REG_8 IO_MUX_GPIO8_REG
#endif
#ifndef GPIO_PIN_REG_9
#define GPIO_PIN_REG_9 IO_MUX_GPIO9_REG
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    GPIO_PORT_0 = 0,
    GPIO_PORT_MAX = 1,
} gpio_port_t;
typedef enum {
    GPIO_INTR_DISABLE = 0,
    GPIO_INTR_POSEDGE = 1,
    GPIO_INTR_NEGEDGE = 2,
    GPIO_INTR_ANYEDGE = 3,
    GPIO_INTR_LOW_LEVEL = 4,
    GPIO_INTR_HIGH_LEVEL = 5,
    GPIO_INTR_MAX = 6,
} gpio_int_type_t;
typedef enum {
    GPIO_MODE_DISABLE = 0,
    GPIO_MODE_INPUT = 1,
    GPIO_MODE_OUTPUT = 2,
    GPIO_MODE_OUTPUT_OD = 6,
    GPIO_MODE_INPUT_OUTPUT_OD = 7,
    GPIO_MODE_INPUT_OUTPUT = 3,
} gpio_mode_t;
typedef enum {
    GPIO_PULLUP_DISABLE = 0,
    GPIO_PULLUP_ENABLE = 1,
} gpio_pullup_t;
typedef enum {
    GPIO_PULLDOWN_DISABLE = 0,
    GPIO_PULLDOWN_ENABLE = 1,
} gpio_pulldown_t;
typedef enum {
    GPIO_PULLUP_ONLY = 0,
    GPIO_PULLDOWN_ONLY = 1,
    GPIO_PULLUP_PULLDOWN = 2,
    GPIO_FLOATING = 3,
} gpio_pull_mode_t;
typedef enum {
    GPIO_DRIVE_CAP_0 = 0,
    GPIO_DRIVE_CAP_1 = 1,
    GPIO_DRIVE_CAP_2 = 2,
    GPIO_DRIVE_CAP_DEFAULT = 2,
    GPIO_DRIVE_CAP_3 = 3,
    GPIO_DRIVE_CAP_MAX = 4,
} gpio_drive_cap_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
    gpio_drive_cap_t drv;
    uint32_t fun_sel : 8;
    uint32_t sig_out : 16;
    uint32_t pu : 1;
    uint32_t pd : 1;
    uint32_t ie : 1;
    uint32_t oe : 1;
    uint32_t oe_ctrl_by_periph : 1;
    uint32_t oe_inv : 1;
    uint32_t od : 1;
    uint32_t slp_sel : 1;
} gpio_io_config_t;



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_HAL_GPIO_TYPES_H__ */
#endif /* WINK_H_GUARD_HAL_GPIO_TYPES_H */
