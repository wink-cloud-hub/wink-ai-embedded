// SPDX-License-Identifier: Apache-2.0
#define LOG_TAG "dal_mono_oled"

#include "dal_mono_oled.h"
#include "dal_mono_oled_font_internal.h"
#include "pal_hal.h"
#include "pal_osal.h"
#include "pal_resource.h"
#include "pal_log.h"
#include <string.h>

#define LOGW_IF_RC(call, first_err) do {                                  \
    wink_status_t _rc = (call);                                            \
    if (wink_status_is_error(_rc) && !wink_status_is_error(first_err)) {  \
        first_err = _rc;                                                   \
        LOG_W("step failed rc=%d at line %d", (int)_rc, __LINE__);         \
    }                                                                      \
} while(0)

#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

/* SSD1306 / Monochrome OLED standard init command sequence (128x64) */
static const uint8_t s_init_cmds_64[] = {
    0xAE,       /* display off */
    0xD5, 0x80, /* clock divide */
    0xA8, 0x3F, /* multiplex ratio = 63 (64 rows) */
    0xD3, 0x00, /* display offset = 0 */
    0x40,       /* start line = 0 */
    0x8D, 0x14, /* charge pump enable */
    0x20, 0x00, /* horizontal addressing mode */
    0xA1,       /* segment remap */
    0xC8,       /* COM scan direction */
    0xDA, 0x12, /* COM pins hardware config (alt) */
    0x81, 0xCF, /* contrast */
    0xD9, 0xF1, /* pre-charge */
    0xDB, 0x40, /* VCOMH deselect */
    0xA4,       /* display RAM */
    0xA6,       /* normal display */
    0xAF,       /* display on */
};

/* SSD1306 / Monochrome OLED standard init command sequence (128x32) */
static const uint8_t s_init_cmds_32[] = {
    0xAE,       /* display off */
    0xD5, 0x80, /* clock divide */
    0xA8, 0x1F, /* multiplex ratio = 31 (32 rows) */
    0xD3, 0x00, /* display offset = 0 */
    0x40,       /* start line = 0 */
    0x8D, 0x14, /* charge pump enable */
    0x20, 0x00, /* horizontal addressing mode */
    0xA1,       /* segment remap */
    0xC8,       /* COM scan direction */
    0xDA, 0x02, /* COM pins hardware config (sequential) */
    0x81, 0x8F, /* contrast */
    0xD9, 0xF1, /* pre-charge */
    0xDB, 0x40, /* VCOMH deselect */
    0xA4,       /* display RAM */
    0xA6,       /* normal display */
    0xAF,       /* display on */
};

static wink_status_t spi_bitbang_write(const dal_mono_oled_config_t *cfg, bool is_cmd,
                                       const uint8_t *data, size_t len) {
    if (cfg->pin_dc < 0 || cfg->pin_clk < 0 || cfg->pin_mosi < 0) {
        return WINK_ERR_INVALID_ARG;
    }

    wink_status_t st = pal_gpio_write((wink_pin_t)cfg->pin_dc, !is_cmd);
    if (wink_status_is_error(st)) { return st; }

    if (cfg->pin_cs >= 0) {
        st = pal_gpio_write((wink_pin_t)cfg->pin_cs, false);
        if (wink_status_is_error(st)) { return st; }
    }

    for (size_t i = 0; i < len; i++) {
        uint8_t byte = data[i];
        for (int b = 7; b >= 0; b--) {
            bool bit = (byte >> b) & 0x01;
            (void)pal_gpio_write((wink_pin_t)cfg->pin_mosi, bit);
            (void)pal_gpio_write((wink_pin_t)cfg->pin_clk, true);
            (void)pal_gpio_write((wink_pin_t)cfg->pin_clk, false);
        }
    }

    if (cfg->pin_cs >= 0) {
        (void)pal_gpio_write((wink_pin_t)cfg->pin_cs, true);
    }
    return WINK_OK;
}

wink_status_t dal_mono_oled_init(dal_mono_oled_t *dev, const dal_mono_oled_config_t *cfg) {
    if (dev == NULL || cfg == NULL) { return WINK_ERR_INVALID_ARG; }
    if (cfg->owner == NULL) { return WINK_ERR_INVALID_ARG; }

    if (cfg->width != 128) { return WINK_ERR_INVALID_ARG; }
    if (cfg->height != 32 && cfg->height != 64) { return WINK_ERR_INVALID_ARG; }
    if (cfg->variant > DAL_MONO_OLED_VARIANT_SSD1306_SPI) { return WINK_ERR_INVALID_ARG; }
    if (cfg->panel_ic > DAL_MONO_OLED_IC_SH1106) { return WINK_ERR_INVALID_ARG; }
    if (dev->initialized) { return WINK_ERR_ALREADY_INITIALIZED; }

    dev->initialized = false;

    const uint8_t *init_cmds;
    size_t init_cmds_len;
    if (cfg->height == 64) {
        init_cmds = s_init_cmds_64;
        init_cmds_len = sizeof(s_init_cmds_64);
    } else {
        init_cmds = s_init_cmds_32;
        init_cmds_len = sizeof(s_init_cmds_32);
    }

    if (cfg->variant == DAL_MONO_OLED_VARIANT_SSD1306_I2C) {
        if (cfg->i2c_port >= PAL_I2C_PORTS) { return WINK_ERR_INVALID_ARG; }
        if (cfg->i2c_addr < 0x08 || cfg->i2c_addr > 0x77) { return WINK_ERR_INVALID_ARG; }

        uint32_t res_id = pal_resource_i2c_id(cfg->i2c_port, cfg->i2c_addr);
        wink_status_t rs = pal_resource_claim(PAL_RESOURCE_I2C_ADDR, res_id, cfg->owner);
        if (wink_status_is_error(rs)) { return rs; }

        uint8_t big_cmd_buf[1 + sizeof(s_init_cmds_64)];
        big_cmd_buf[0] = 0x00;
        memcpy(&big_cmd_buf[1], init_cmds, init_cmds_len);
        wink_status_t status = pal_i2c_transfer(cfg->i2c_port, cfg->i2c_addr,
                                                 big_cmd_buf, (uint32_t)(1 + init_cmds_len), NULL, 0);
        if (wink_status_is_error(status)) {
            WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_I2C_ADDR, res_id, cfg->owner));
            return status;
        }
    } else { /* DAL_MONO_OLED_VARIANT_SSD1306_SPI */
        if (cfg->pin_clk < 0 || cfg->pin_mosi < 0 || cfg->pin_dc < 0) {
            return WINK_ERR_INVALID_ARG;
        }

        wink_status_t rs = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_clk, cfg->owner);
        if (wink_status_is_error(rs)) { return rs; }

        rs = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_mosi, cfg->owner);
        if (wink_status_is_error(rs)) {
            (void)pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_clk, cfg->owner);
            return rs;
        }

        rs = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_dc, cfg->owner);
        if (wink_status_is_error(rs)) {
            (void)pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_clk, cfg->owner);
            (void)pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_mosi, cfg->owner);
            return rs;
        }

        if (cfg->pin_cs >= 0) {
            rs = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_cs, cfg->owner);
            if (wink_status_is_error(rs)) {
                (void)pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_clk, cfg->owner);
                (void)pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_mosi, cfg->owner);
                (void)pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_dc, cfg->owner);
                return rs;
            }
        }

        if (cfg->pin_res >= 0) {
            rs = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_res, cfg->owner);
            if (wink_status_is_error(rs)) {
                (void)pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_clk, cfg->owner);
                (void)pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_mosi, cfg->owner);
                (void)pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_dc, cfg->owner);
                if (cfg->pin_cs >= 0) {
                    (void)pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_cs, cfg->owner);
                }
                return rs;
            }
        }

        (void)pal_gpio_init((wink_pin_t)cfg->pin_clk, PAL_GPIO_OUTPUT_PUSH_PULL);
        (void)pal_gpio_init((wink_pin_t)cfg->pin_mosi, PAL_GPIO_OUTPUT_PUSH_PULL);
        (void)pal_gpio_init((wink_pin_t)cfg->pin_dc, PAL_GPIO_OUTPUT_PUSH_PULL);

        if (cfg->pin_cs >= 0) {
            (void)pal_gpio_init((wink_pin_t)cfg->pin_cs, PAL_GPIO_OUTPUT_PUSH_PULL);
            (void)pal_gpio_write((wink_pin_t)cfg->pin_cs, true);
        }

        if (cfg->pin_res >= 0) {
            (void)pal_gpio_init((wink_pin_t)cfg->pin_res, PAL_GPIO_OUTPUT_PUSH_PULL);
            (void)pal_gpio_write((wink_pin_t)cfg->pin_res, false);
            pal_os_busy_wait_us(10000);
            (void)pal_gpio_write((wink_pin_t)cfg->pin_res, true);
            pal_os_busy_wait_us(10000);
        }

        wink_status_t status = spi_bitbang_write(cfg, true, init_cmds, init_cmds_len);
        if (wink_status_is_error(status)) {
            dal_mono_oled_deinit(dev);
            return status;
        }
    }

    memset(dev->framebuffer, 0, MONO_OLED_FB_SIZE);
    memcpy(&dev->config, cfg, sizeof(dal_mono_oled_config_t));
    dev->pages       = (uint8_t)(cfg->height >> 3);
    dev->initialized = true;
    return WINK_OK;
}

wink_status_t dal_mono_oled_clear(dal_mono_oled_t *dev) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }
    memset(dev->framebuffer, 0, MONO_OLED_FB_SIZE);
    return WINK_OK;
}

wink_status_t dal_mono_oled_draw_text(dal_mono_oled_t *dev, uint16_t col, uint8_t page,
                                       const char *str) {
    if (dev == NULL || str == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }
    if (page >= dev->pages) { return WINK_ERR_INVALID_ARG; }

    uint16_t x = col;
    for (const char *p = str; *p != '\0'; p++) {
        if (x + MONO_OLED_FONT_GLYPH_W > dev->config.width) { break; }
        const uint8_t *glyph = dal_mono_oled_font_glyph(*p);
        uint16_t base = (uint16_t)page * dev->config.width + x;
        for (int c = 0; c < MONO_OLED_FONT_GLYPH_W; c++) {
            dev->framebuffer[base + c] = glyph[c];
        }
        x += MONO_OLED_FONT_WIDTH;
    }
    return WINK_OK;
}

wink_status_t dal_mono_oled_flush(dal_mono_oled_t *dev) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }

    const dal_mono_oled_config_t *cfg = &dev->config;

    if (cfg->variant == DAL_MONO_OLED_VARIANT_SSD1306_I2C) {
        if (cfg->panel_ic == DAL_MONO_OLED_IC_SSD1306) {
            uint8_t col_end = (uint8_t)(cfg->width - 1);
            uint8_t page_end = (uint8_t)(dev->pages - 1);
            uint8_t addr_cmd[7] = {
                0x00,
                0x21, 0x00, col_end,
                0x22, 0x00, page_end
            };
            wink_status_t st = pal_i2c_transfer(cfg->i2c_port, cfg->i2c_addr,
                                                 addr_cmd, sizeof(addr_cmd), NULL, 0);
            if (wink_status_is_error(st)) { return st; }

            for (uint8_t pg = 0; pg < dev->pages; pg++) {
                uint8_t page_buf[129];
                page_buf[0] = 0x40;
                uint16_t offset = (uint16_t)pg * cfg->width;
                memcpy(&page_buf[1], &dev->framebuffer[offset], cfg->width);
                st = pal_i2c_transfer(cfg->i2c_port, cfg->i2c_addr,
                                      page_buf, (uint32_t)(1u + cfg->width), NULL, 0);
                if (wink_status_is_error(st)) { return st; }
            }
        } else { /* DAL_MONO_OLED_IC_SH1106 */
            for (uint8_t pg = 0; pg < dev->pages; pg++) {
                uint8_t page_cmd[4] = {
                    0x00,
                    (uint8_t)(0xB0 + pg),
                    0x02, /* Lower column address (0x00 + 0x02 column offset) */
                    0x10  /* Upper column address 0x10 */
                };
                wink_status_t st = pal_i2c_transfer(cfg->i2c_port, cfg->i2c_addr,
                                                     page_cmd, sizeof(page_cmd), NULL, 0);
                if (wink_status_is_error(st)) { return st; }

                uint8_t page_buf[129];
                page_buf[0] = 0x40;
                uint16_t offset = (uint16_t)pg * cfg->width;
                memcpy(&page_buf[1], &dev->framebuffer[offset], cfg->width);
                st = pal_i2c_transfer(cfg->i2c_port, cfg->i2c_addr,
                                      page_buf, (uint32_t)(1u + cfg->width), NULL, 0);
                if (wink_status_is_error(st)) { return st; }
            }
        }
    } else { /* DAL_MONO_OLED_VARIANT_SSD1306_SPI */
        if (cfg->panel_ic == DAL_MONO_OLED_IC_SSD1306) {
            uint8_t col_end = (uint8_t)(cfg->width - 1);
            uint8_t page_end = (uint8_t)(dev->pages - 1);
            uint8_t addr_cmd[6] = {
                0x21, 0x00, col_end,
                0x22, 0x00, page_end
            };
            wink_status_t st = spi_bitbang_write(cfg, true, addr_cmd, sizeof(addr_cmd));
            if (wink_status_is_error(st)) { return st; }

            st = spi_bitbang_write(cfg, false, dev->framebuffer, MONO_OLED_FB_SIZE);
            if (wink_status_is_error(st)) { return st; }
        } else { /* DAL_MONO_OLED_IC_SH1106 */
            for (uint8_t pg = 0; pg < dev->pages; pg++) {
                uint8_t page_cmd[3] = {
                    (uint8_t)(0xB0 + pg),
                    0x02,
                    0x10
                };
                wink_status_t st = spi_bitbang_write(cfg, true, page_cmd, sizeof(page_cmd));
                if (wink_status_is_error(st)) { return st; }

                uint16_t offset = (uint16_t)pg * cfg->width;
                st = spi_bitbang_write(cfg, false, &dev->framebuffer[offset], cfg->width);
                if (wink_status_is_error(st)) { return st; }
            }
        }
    }
    return WINK_OK;
}

wink_status_t dal_mono_oled_deinit(dal_mono_oled_t *dev) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_OK; }

    dev->initialized = false;

    const dal_mono_oled_config_t *cfg = &dev->config;
    wink_status_t first_err = WINK_OK;

    if (cfg->variant == DAL_MONO_OLED_VARIANT_SSD1306_I2C) {
        uint8_t cmd[2] = {0x00, 0xAE};
        LOGW_IF_RC(pal_i2c_transfer(cfg->i2c_port, cfg->i2c_addr, cmd, sizeof(cmd), NULL, 0), first_err);

        uint32_t res_id = pal_resource_i2c_id(cfg->i2c_port, cfg->i2c_addr);
        LOGW_IF_RC(pal_resource_release(PAL_RESOURCE_I2C_ADDR, res_id, cfg->owner), first_err);
    } else { /* DAL_MONO_OLED_VARIANT_SSD1306_SPI */
        uint8_t cmd[1] = {0xAE};
        LOGW_IF_RC(spi_bitbang_write(cfg, true, cmd, sizeof(cmd)), first_err);

        pal_gpio_reset_pin((wink_pin_t)cfg->pin_clk);
        LOGW_IF_RC(pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_clk, cfg->owner), first_err);

        pal_gpio_reset_pin((wink_pin_t)cfg->pin_mosi);
        LOGW_IF_RC(pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_mosi, cfg->owner), first_err);

        pal_gpio_reset_pin((wink_pin_t)cfg->pin_dc);
        LOGW_IF_RC(pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_dc, cfg->owner), first_err);

        if (cfg->pin_cs >= 0) {
            pal_gpio_reset_pin((wink_pin_t)cfg->pin_cs);
            LOGW_IF_RC(pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_cs, cfg->owner), first_err);
        }

        if (cfg->pin_res >= 0) {
            pal_gpio_reset_pin((wink_pin_t)cfg->pin_res);
            LOGW_IF_RC(pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin_res, cfg->owner), first_err);
        }
    }

    memset(dev, 0, sizeof(dal_mono_oled_t));
    return first_err;
}

