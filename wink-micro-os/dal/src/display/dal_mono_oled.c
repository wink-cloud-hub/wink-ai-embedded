// SPDX-License-Identifier: Apache-2.0
#define LOG_TAG "dal_mono_oled"

#include "dal_mono_oled.h"
#include "dal_mono_oled_font_internal.h"
#include "pal_hal.h"
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

wink_status_t dal_mono_oled_init(dal_mono_oled_t *dev, const dal_mono_oled_config_t *cfg) {
    if (dev == NULL || cfg == NULL) { return WINK_ERR_INVALID_ARG; }
    if (cfg->owner == NULL) { return WINK_ERR_INVALID_ARG; }
    if (cfg->i2c_port >= PAL_I2C_PORTS) { return WINK_ERR_INVALID_ARG; }

    if (cfg->width != 128) { return WINK_ERR_INVALID_ARG; }
    if (cfg->height != 32 && cfg->height != 64) { return WINK_ERR_INVALID_ARG; }
    if (cfg->i2c_addr < 0x08 || cfg->i2c_addr > 0x77) { return WINK_ERR_INVALID_ARG; }
    if (cfg->variant != DAL_MONO_OLED_VARIANT_SSD1306) {
        return WINK_ERR_UNSUPPORTED;
    }
    if (dev->initialized) { return WINK_ERR_ALREADY_INITIALIZED; }

    dev->initialized = false;

    uint32_t res_id = pal_resource_i2c_id(cfg->i2c_port, cfg->i2c_addr);
    wink_status_t rs = pal_resource_claim(PAL_RESOURCE_I2C_ADDR, res_id, cfg->owner);
    if (wink_status_is_error(rs)) { return rs; }

    const uint8_t *init_cmds;
    size_t init_cmds_len;
    if (cfg->height == 64) {
        init_cmds = s_init_cmds_64;
        init_cmds_len = sizeof(s_init_cmds_64);
    } else {
        init_cmds = s_init_cmds_32;
        init_cmds_len = sizeof(s_init_cmds_32);
    }

    uint8_t big_cmd_buf[1 + sizeof(s_init_cmds_64)];
    big_cmd_buf[0] = 0x00;
    memcpy(&big_cmd_buf[1], init_cmds, init_cmds_len);
    wink_status_t status = pal_i2c_transfer(cfg->i2c_port, cfg->i2c_addr,
                                             big_cmd_buf, 1 + init_cmds_len, NULL, 0);
    if (wink_status_is_error(status)) {
        WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_I2C_ADDR, res_id, cfg->owner));
        return status;
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

    uint8_t col_end = (uint8_t)(dev->config.width - 1);
    uint8_t page_end = (uint8_t)(dev->pages - 1);

    uint8_t addr_cmd[7] = {
        0x00,
        0x21, 0x00, col_end,
        0x22, 0x00, page_end
    };
    wink_status_t st = pal_i2c_transfer(dev->config.i2c_port, dev->config.i2c_addr,
                                         addr_cmd, sizeof(addr_cmd), NULL, 0);
    if (wink_status_is_error(st)) { return st; }

    for (uint8_t pg = 0; pg < dev->pages; pg++) {
        uint8_t page_buf[129];
        page_buf[0] = 0x40;
        uint16_t offset = (uint16_t)pg * dev->config.width;
        memcpy(&page_buf[1], &dev->framebuffer[offset], dev->config.width);
        st = pal_i2c_transfer(dev->config.i2c_port, dev->config.i2c_addr,
                              page_buf, (uint16_t)(1u + dev->config.width), NULL, 0);
        if (wink_status_is_error(st)) { return st; }
    }
    return WINK_OK;
}

wink_status_t dal_mono_oled_deinit(dal_mono_oled_t *dev) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_OK; }

    dev->initialized = false;

    uint8_t port = dev->config.i2c_port;
    uint16_t addr = dev->config.i2c_addr;
    const char *owner = dev->config.owner;

    wink_status_t first_err = WINK_OK;

    uint8_t cmd[2] = {0x00, 0xAE};
    LOGW_IF_RC(pal_i2c_transfer(port, addr, cmd, sizeof(cmd), NULL, 0), first_err);

    uint32_t res_id = pal_resource_i2c_id(port, addr);
    LOGW_IF_RC(pal_resource_release(PAL_RESOURCE_I2C_ADDR, res_id, owner), first_err);

    memset(dev, 0, sizeof(dal_mono_oled_t));

    return first_err;
}
