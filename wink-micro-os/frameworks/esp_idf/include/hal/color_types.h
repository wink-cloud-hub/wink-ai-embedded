/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_HAL_COLOR_TYPES_H
#define WINK_H_GUARD_HAL_COLOR_TYPES_H
#ifndef __WINK_HARVESTED_HAL_COLOR_TYPES_H__
#define __WINK_HARVESTED_HAL_COLOR_TYPES_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef ESP_COLOR_FOURCC
#define ESP_COLOR_FOURCC(a, b, c, d) ((uint32_t)(a) | ((uint32_t)(b) << 8) | ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))
#endif
#ifndef ESP_COLOR_FOURCC_ALPHA4
#define ESP_COLOR_FOURCC_ALPHA4 ESP_COLOR_FOURCC('A', 'L', 'P', '4')
#endif
#ifndef ESP_COLOR_FOURCC_ALPHA8
#define ESP_COLOR_FOURCC_ALPHA8 ESP_COLOR_FOURCC('A', 'L', 'P', '8')
#endif
#ifndef ESP_COLOR_FOURCC_BGR24
#define ESP_COLOR_FOURCC_BGR24 ESP_COLOR_FOURCC('B', 'G', 'R', '3')
#endif
#ifndef ESP_COLOR_FOURCC_BGRA32
#define ESP_COLOR_FOURCC_BGRA32 ESP_COLOR_FOURCC('B', 'A', '2', '4')
#endif
#ifndef ESP_COLOR_FOURCC_GREY
#define ESP_COLOR_FOURCC_GREY ESP_COLOR_FOURCC('G', 'R', 'E', 'Y')
#endif
#ifndef ESP_COLOR_FOURCC_OUYY_EVYY
#define ESP_COLOR_FOURCC_OUYY_EVYY ESP_COLOR_FOURCC('O', 'U', 'E', 'V')
#endif
#ifndef ESP_COLOR_FOURCC_RAW10
#define ESP_COLOR_FOURCC_RAW10 ESP_COLOR_FOURCC('R', 'A', 'W', 'A')
#endif
#ifndef ESP_COLOR_FOURCC_RAW12
#define ESP_COLOR_FOURCC_RAW12 ESP_COLOR_FOURCC('R', 'A', 'W', 'C')
#endif
#ifndef ESP_COLOR_FOURCC_RAW16
#define ESP_COLOR_FOURCC_RAW16 ESP_COLOR_FOURCC('R', 'A', 'W', 'G')
#endif
#ifndef ESP_COLOR_FOURCC_RAW8
#define ESP_COLOR_FOURCC_RAW8 ESP_COLOR_FOURCC('R', 'A', 'W', '8')
#endif
#ifndef ESP_COLOR_FOURCC_RGB16
#define ESP_COLOR_FOURCC_RGB16 ESP_COLOR_FOURCC('R', 'G', 'B', 'L')
#endif
#ifndef ESP_COLOR_FOURCC_RGB16_BE
#define ESP_COLOR_FOURCC_RGB16_BE ESP_COLOR_FOURCC('R', 'G', 'B', 'E')
#endif
#ifndef ESP_COLOR_FOURCC_RGB24
#define ESP_COLOR_FOURCC_RGB24 ESP_COLOR_FOURCC('R', 'G', 'B', '3')
#endif
#ifndef ESP_COLOR_FOURCC_UYVY
#define ESP_COLOR_FOURCC_UYVY ESP_COLOR_FOURCC('U', 'Y', 'V', 'Y')
#endif
#ifndef ESP_COLOR_FOURCC_VYUY
#define ESP_COLOR_FOURCC_VYUY ESP_COLOR_FOURCC('V', 'Y', 'U', 'Y')
#endif
#ifndef ESP_COLOR_FOURCC_YUV
#define ESP_COLOR_FOURCC_YUV ESP_COLOR_FOURCC('V', '3', '0', '8')
#endif
#ifndef ESP_COLOR_FOURCC_YUYV
#define ESP_COLOR_FOURCC_YUYV ESP_COLOR_FOURCC('Y', 'U', 'Y', 'V')
#endif
#ifndef ESP_COLOR_FOURCC_YVYU
#define ESP_COLOR_FOURCC_YVYU ESP_COLOR_FOURCC('Y', 'V', 'Y', 'U')
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    COLOR_RANGE_LIMIT = 0,
    COLOR_RANGE_FULL = 1,
} color_range_t;
typedef enum {
    COLOR_CONV_STD_RGB_YUV_BT601 = 0,
    COLOR_CONV_STD_RGB_YUV_BT709 = 1,
} color_conv_std_rgb_yuv_t;
typedef enum {
    COLOR_RAW_ELEMENT_ORDER_BGGR = 0,
    COLOR_RAW_ELEMENT_ORDER_GBRG = 1,
    COLOR_RAW_ELEMENT_ORDER_GRBG = 2,
    COLOR_RAW_ELEMENT_ORDER_RGGB = 3,
} color_raw_element_order_t;
typedef enum {
    COLOR_RGB_ELEMENT_ORDER_RGB = 0,
    COLOR_RGB_ELEMENT_ORDER_BGR = 1,
} color_rgb_element_order_t;
typedef enum {
    COLOR_COMPONENT_R = 0,
    COLOR_COMPONENT_G = 1,
    COLOR_COMPONENT_B = 2,
    COLOR_COMPONENT_INVALID = 3,
} color_component_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef uint32_t esp_color_fourcc_t;
typedef union {
struct {
        uint32_t b: 8;      
        uint32_t g: 8;      
        uint32_t r: 8;      
        uint32_t a: 8;      
    };
    uint32_t val;
} color_pixel_argb8888_data_t;
typedef union {
struct {
        uint8_t b;          
        uint8_t g;          
        uint8_t r;          
    };
    uint32_t val;
} color_pixel_rgb888_data_t;
typedef union {
struct {
        uint16_t b: 5;      
        uint16_t g: 6;      
        uint16_t r: 5;      
    };
    uint16_t val;
} color_pixel_rgb565_data_t;
typedef union {
struct {
        uint8_t gray;      
    };
    uint8_t val;
} color_pixel_gray8_data_t;
typedef struct {
    uint8_t y;
    uint8_t u;
    uint8_t v;
} color_macroblock_yuv_data_t;



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_HAL_COLOR_TYPES_H__ */
#endif /* WINK_H_GUARD_HAL_COLOR_TYPES_H */
