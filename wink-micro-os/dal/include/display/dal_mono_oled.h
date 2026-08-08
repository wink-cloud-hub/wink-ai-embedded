// SPDX-License-Identifier: Apache-2.0
#ifndef DAL_MONO_OLED_H
#define DAL_MONO_OLED_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Monochrome OLED maximum framebuffer size (128x64 / 8 = 1024 bytes) */
#define MONO_OLED_FB_SIZE 1024

/** @brief Compact font dimensions */
#define MONO_OLED_FONT_WIDTH  6
#define MONO_OLED_FONT_GLYPH_W 5

/**
 * @brief Monochrome OLED physical interface variant (affects pinout)
 */
typedef uint8_t dal_mono_oled_variant_t;
enum {
    DAL_MONO_OLED_VARIANT_SSD1306_I2C = 0,  /**< I2C mode: [VCC, GND, SCL, SDA] 4-Pin */
    DAL_MONO_OLED_VARIANT_SSD1306_SPI = 1,  /**< SPI mode: [VCC, GND, CLK, MOSI, CS, DC, RES] 7-Pin */
};
#define DAL_MONO_OLED_VARIANT_COUNT 2

#ifdef __cplusplus
static_assert(sizeof(dal_mono_oled_variant_t) == 1,
              "variant must stay 1 byte to keep dal_mono_oled_config_t layout stable");
static_assert(DAL_MONO_OLED_VARIANT_COUNT == 2,
              "Variant count mismatch with SSOT §2 and codegen YAML");
static_assert(DAL_MONO_OLED_VARIANT_SSD1306_SPI + 1 == DAL_MONO_OLED_VARIANT_COUNT,
              "Sequential variant ordering error");
#else
_Static_assert(sizeof(dal_mono_oled_variant_t) == 1,
               "variant must stay 1 byte to keep dal_mono_oled_config_t layout stable");
_Static_assert(DAL_MONO_OLED_VARIANT_COUNT == 2,
               "Variant count mismatch with SSOT §2 and codegen YAML");
_Static_assert(DAL_MONO_OLED_VARIANT_SSD1306_SPI + 1 == DAL_MONO_OLED_VARIANT_COUNT,
               "Sequential variant ordering error");
#endif

/**
 * @brief Monochrome OLED display controller IC variant (algorithm only, affects_pins: false)
 */
typedef uint8_t dal_mono_oled_ic_t;
enum {
    DAL_MONO_OLED_IC_SSD1306 = 0,  /**< Standard SSD1306 (horizontal/page addressing) */
    DAL_MONO_OLED_IC_SH1106  = 1,  /**< SH1106 (page addressing with 0x02 column offset) */
};
#define DAL_MONO_OLED_IC_COUNT 2

#ifdef __cplusplus
static_assert(sizeof(dal_mono_oled_ic_t) == 1,
              "panel_ic must stay 1 byte to keep dal_mono_oled_config_t layout stable");
#else
_Static_assert(sizeof(dal_mono_oled_ic_t) == 1,
               "panel_ic must stay 1 byte to keep dal_mono_oled_config_t layout stable");
#endif

/**
 * @brief Monochrome OLED configuration struct (Flat layout with sentinel trimming)
 */
typedef struct {
    const char             *owner;      /**< Instance owner static string */
    dal_mono_oled_variant_t variant;    /**< Physical variant (determines pinout) */
    dal_mono_oled_ic_t      panel_ic;   /**< Controller IC (determines flush algorithm) */
    uint8_t                 i2c_port;   /**< Logical I2C bus index */
    uint8_t                 padding0;   /**< Explicit byte alignment padding */
    uint16_t                i2c_addr;   /**< 7-bit I2C address (typically 0x3C or 0x3D) */
    uint16_t                width;      /**< Display width in pixels (typically 128) */
    uint16_t                height;     /**< Display height in pixels (typically 64) */

    /* SPI fields (Active when variant == DAL_MONO_OLED_VARIANT_SSD1306_SPI, trimmed to -1 on I2C) */
    int16_t                 pin_clk;    /**< SPI Clock pin */
    int16_t                 pin_mosi;   /**< SPI MOSI pin */
    int16_t                 pin_cs;     /**< SPI Chip Select pin (-1 if bus dedicated) */
    int16_t                 pin_dc;     /**< SPI Data/Command pin */
    int16_t                 pin_res;    /**< SPI Reset pin (-1 if hardwired) */
    int16_t                 padding1;   /**< Explicit 2-byte alignment padding */
} dal_mono_oled_config_t;

/**
 * @brief Monochrome OLED instance struct (POD)
 */
typedef struct {
    dal_mono_oled_config_t config;    /**< Config copy */
    uint8_t                framebuffer[MONO_OLED_FB_SIZE]; /**< Framebuffer (page-based: 8 rows x 128 cols per page) */
    uint8_t                pages;     /**< Derived: config.height / 8 */
    bool                   initialized; /**< Set to true after successful init */
} dal_mono_oled_t;

_Static_assert(offsetof(dal_mono_oled_t, config) == 0, "config must be the first member");

#if INTPTR_MAX == INT32_MAX   /* ILP32: ESP32 xtensa, WASM32 */
_Static_assert(sizeof(dal_mono_oled_config_t) == 28, "ABI break: config size changed on 32-bit target");
_Static_assert(offsetof(dal_mono_oled_t, initialized) == 1053, "ABI break: initialized offset changed on 32-bit");
_Static_assert(sizeof(dal_mono_oled_t) == 1056, "ABI break: handle size changed on 32-bit target");
#else                         /* LP64 / LLP64: 64-bit Host Simulation */
_Static_assert(sizeof(dal_mono_oled_config_t) == 32, "ABI break: config size changed on 64-bit host");
_Static_assert(offsetof(dal_mono_oled_t, initialized) == 1057, "ABI break: initialized offset changed on 64-bit host");
_Static_assert(sizeof(dal_mono_oled_t) == 1064, "ABI break: handle size changed on 64-bit host");
#endif

/**
 * @brief Initialize monochrome OLED driver instance
 *
 * @param[in,out] dev Display instance handle.
 * @param[in] cfg Configuration struct.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_mono_oled_init(dal_mono_oled_t *dev, const dal_mono_oled_config_t *cfg);

/**
 * @brief Clear framebuffer (memory operation)
 *
 * @param[in,out] dev Display instance handle.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_mono_oled_clear(dal_mono_oled_t *dev);

/**
 * @brief Render ASCII text string into framebuffer
 *
 * @param[in,out] dev Display instance handle.
 * @param[in] col Starting column (0 .. width-1).
 * @param[in] page Starting page (0 .. pages-1).
 * @param[in] str Null-terminated ASCII string.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_mono_oled_draw_text(dal_mono_oled_t *dev, uint16_t col, uint8_t page,
                                       const char *str);

/**
 * @brief Flush framebuffer to OLED display hardware
 *
 * @param[in,out] dev Display instance handle.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_mono_oled_flush(dal_mono_oled_t *dev);

/**
 * @brief Deinitialize monochrome OLED driver
 *
 * @param[in,out] dev Display instance handle.
 * @return WINK_OK on success, error status code otherwise.
 */
wink_status_t dal_mono_oled_deinit(dal_mono_oled_t *dev);

#ifdef __cplusplus
}
#endif

/* Compile-time pruning stubs */
#if !defined(WINK_USE_MONO_OLED) || !WINK_USE_MONO_OLED
#define WINK_MONO_OLED_DISABLED_MSG \
    "mono_oled display driver not enabled; add a \"mono_oled\" device to " \
    "wink-app.json (or set -DWINK_USE_MONO_OLED=ON)."
WINK_UNAVAILABLE_MSG(WINK_MONO_OLED_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_mono_oled_init(dal_mono_oled_t *dev, const dal_mono_oled_config_t *cfg);
WINK_UNAVAILABLE_MSG(WINK_MONO_OLED_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_mono_oled_clear(dal_mono_oled_t *dev);
WINK_UNAVAILABLE_MSG(WINK_MONO_OLED_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_mono_oled_draw_text(dal_mono_oled_t *dev, uint16_t col, uint8_t page,
                                       const char *str);
WINK_UNAVAILABLE_MSG(WINK_MONO_OLED_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_mono_oled_flush(dal_mono_oled_t *dev);
WINK_UNAVAILABLE_MSG(WINK_MONO_OLED_DISABLED_MSG)
wink_status_t dal_mono_oled_deinit(dal_mono_oled_t *dev);
#endif /* !WINK_USE_MONO_OLED */

#endif /* DAL_MONO_OLED_H */

