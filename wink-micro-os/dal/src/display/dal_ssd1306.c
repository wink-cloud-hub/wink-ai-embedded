#include "dal_ssd1306.h"
#include "dal_ssd1306_font_internal.h"
#include "pal_hal.h"
#include "pal_resource.h"
#include <string.h>

/* ADR-0017 层 1 例外：本 TU 合法调用 WINK_BLOCKING API (pal_i2c_transfer)。
 * SSD1306 显示驱动本质要走 I2C 传输，是"消费端"角色；WINK_BLOCKING 的告警
 * 作用于新代码抑制误用，对 DAL 内部有意为之的调用退化为 no-op。
 * 严格模式（-DWINK_STRICT_NONBLOCKING=1）下 pal_i2c_transfer 声明消失，本文件将链接失败——
 * 这正是设计意图（严格 nonblocking 构建路径需要非阻塞 I2C 后端）。 */
#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

/* ---- SSD1306 标准 init 命令序列（128×64，水平寻址，页式） ---- */
static const uint8_t s_init_cmds_64[] = {
    0xAE,       /* display off */
    0xD5, 0x80, /* clock divide */
    0xA8, 0x3F, /* multiplex ratio = 63 (64 行) */
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

/* ---- SSD1306 标准 init 命令序列（128×32，水平寻址，页式） ---- */
static const uint8_t s_init_cmds_32[] = {
    0xAE,       /* display off */
    0xD5, 0x80, /* clock divide */
    0xA8, 0x1F, /* multiplex ratio = 31 (32 行) */
    0xD3, 0x00, /* display offset = 0 */
    0x40,       /* start line = 0 */
    0x8D, 0x14, /* charge pump enable */
    0x20, 0x00, /* horizontal addressing mode */
    0xA1,       /* segment remap */
    0xC8,       /* COM scan direction */
    0xDA, 0x02, /* COM pins hardware config (sequential) */
    0x81, 0x8F, /* contrast (32 高屏常用值) */
    0xD9, 0xF1, /* pre-charge */
    0xDB, 0x40, /* VCOMH deselect */
    0xA4,       /* display RAM */
    0xA6,       /* normal display */
    0xAF,       /* display on */
};

wink_status_t dal_ssd1306_init(dal_ssd1306_t *dev, const dal_ssd1306_config_t *cfg) {
    if (dev == NULL || cfg == NULL) { return WINK_ERR_INVALID_ARG; }
    if (cfg->owner == NULL) { return WINK_ERR_INVALID_ARG; }
    if (cfg->i2c_port >= PAL_I2C_PORTS) { return WINK_ERR_INVALID_ARG; }

    /* 宽度/高度合法性校验（P0 E3 栈溢出防护）：
     *   - SSD1306 驱动当前只实现 128 像素宽
     *   - 高度仅支持 32 / 64（两种常见商用模组）
     * 任何其他尺寸都会导致帧缓冲溢出、init 序列 mux/COM 配置错误或 flush 栈缓冲越界。
     * 未来若加 96×16/64×32 等尺寸，需要同步：
     *   ① 扩大/调整 SSD1306_FB_SIZE；
     *   ② 增加对应 s_init_cmds_xx 表；
     *   ③ 调整 flush 栈缓冲上限。 */
    if (cfg->width != 128) { return WINK_ERR_INVALID_ARG; }
    if (cfg->height != 32 && cfg->height != 64) { return WINK_ERR_INVALID_ARG; }
    /* i2c 7-bit 地址合法性 (0x00~0x07 和 0x78~0x7F 保留，仅 0x08~0x77 可用于设备) */
    if (cfg->i2c_addr < 0x08 || cfg->i2c_addr > 0x77) { return WINK_ERR_INVALID_ARG; }

    /* Phase 2：(port,addr) 粒度地址冲突治理 */
    uint32_t res_id = pal_resource_i2c_id(cfg->i2c_port, cfg->i2c_addr);
    wink_status_t rs = pal_resource_claim(PAL_RESOURCE_I2C_ADDR, res_id, cfg->owner);
    if (wink_status_is_error(rs)) { return rs; }

    /* 根据 height 选择 init 命令序列 */
    const uint8_t *init_cmds;
    size_t init_cmds_len;
    if (cfg->height == 64) {
        init_cmds = s_init_cmds_64;
        init_cmds_len = sizeof(s_init_cmds_64);
    } else {
        init_cmds = s_init_cmds_32;
        init_cmds_len = sizeof(s_init_cmds_32);
    }

    /* 下发 init 命令序列（控制字节 0x00 = Co=0, D/C#=0）。
     * 一次性下发完整 init 序列：栈缓冲 1+19=20B，足够两种尺寸（18B、18B）。*/
    uint8_t big_cmd_buf[1 + sizeof(s_init_cmds_64)];
    big_cmd_buf[0] = 0x00;
    memcpy(&big_cmd_buf[1], init_cmds, init_cmds_len);
    wink_status_t status = pal_i2c_transfer(cfg->i2c_port, cfg->i2c_addr,
                                             big_cmd_buf, 1 + init_cmds_len, NULL, 0);
    if (wink_status_is_error(status)) {
        /* Track A（M1）：I2C init 命令序列失败须回滚 claim，与其他 DAL 保持行为一致。 */
        WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_I2C_ADDR, res_id, cfg->owner));
        return status;
    }

    memset(dev->framebuffer, 0, SSD1306_FB_SIZE);
    /* 深拷贝配置到实例（支持 ADR-0008 Flash 动态覆写，与 led/button/ultrasonic 一致）*/
    memcpy(&dev->config, cfg, sizeof(dal_ssd1306_config_t));
    dev->pages       = (uint8_t)(cfg->height >> 3); /* /8：派生量，缓存到顶层字段 */
    dev->initialized = true;
    return WINK_OK;
}

wink_status_t dal_ssd1306_clear(dal_ssd1306_t *dev) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }
    memset(dev->framebuffer, 0, SSD1306_FB_SIZE);
    return WINK_OK;
}

wink_status_t dal_ssd1306_draw_text(dal_ssd1306_t *dev, uint16_t col, uint8_t page,
                                    const char *str) {
    if (dev == NULL || str == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }
    if (page >= dev->pages) { return WINK_ERR_INVALID_ARG; }

    uint16_t x = col;
    for (const char *p = str; *p != '\0'; p++) {
        if (x + SSD1306_FONT_GLYPH_W > dev->config.width) { break; }
        const uint8_t *glyph = dal_ssd1306_font_glyph(*p);
        uint16_t base = (uint16_t)page * dev->config.width + x;
        for (int c = 0; c < SSD1306_FONT_GLYPH_W; c++) {
            dev->framebuffer[base + c] = glyph[c];
        }
        x += SSD1306_FONT_WIDTH; /* 5 像素字宽 + 1 像素间距 */
    }
    return WINK_OK;
}

wink_status_t dal_ssd1306_flush(dal_ssd1306_t *dev) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }

    /* 栈缓冲约束：init 已强制 dev->config.width==128，page_buf[129] 固定 1+128B 不溢出。
     * 若未来放宽 width 上限，需同步扩大 page_buf 或使用动态缓冲。 */
    uint8_t col_end = (uint8_t)(dev->config.width - 1);
    uint8_t page_end = (uint8_t)(dev->pages - 1);

    /* 先设置列地址与页地址范围，使后续数据连续写入 */
    uint8_t addr_cmd[7] = {
        0x00,                    /* 控制字节：命令 */
        0x21, 0x00, col_end,     /* 列地址 0..width-1 */
        0x22, 0x00, page_end     /* 页地址 0..pages-1 */
    };
    wink_status_t st = pal_i2c_transfer(dev->config.i2c_port, dev->config.i2c_addr,
                                         addr_cmd, sizeof(addr_cmd), NULL, 0);
    if (wink_status_is_error(st)) { return st; }

    /* 按页发送帧缓冲（1 控制字节 + width 数据字节 = 1+width B 栈缓冲）。
     * 仅支持 width<=128 的 SSD1306 模组（init 已校验），故 129B 足够。 */
    for (uint8_t pg = 0; pg < dev->pages; pg++) {
        uint8_t page_buf[129];
        page_buf[0] = 0x40; /* 控制字节：数据 */
        uint16_t offset = (uint16_t)pg * dev->config.width;
        /* init 已校验 width=128，此处 memcpy 长度固定 128，不会栈溢出。*/
        memcpy(&page_buf[1], &dev->framebuffer[offset], dev->config.width);
        st = pal_i2c_transfer(dev->config.i2c_port, dev->config.i2c_addr,
                              page_buf, (uint16_t)(1u + dev->config.width), NULL, 0);
        if (wink_status_is_error(st)) { return st; }
    }
    return WINK_OK;
}

wink_status_t dal_ssd1306_deinit(dal_ssd1306_t *dev) {
    /* ADR-0024 §4 deinit — checked: 1(best-effort display-off command 0xAE)/
     *   2(N/A: I2C client, SDA/SCL owned by bus-owner; no GPIO to reset)/
     *   3(N/A: no GPIO ISR)/4(N/A: no DMA, single I2C transfer is force-stopped
     *   by bus-owner if needed)/5(N/A: bus-owner deinit does SCL 9-pulse)/
     *   6(client-level deinit: does NOT call pal_i2c_bus_deinit, only releases
     *   its own I2C_ADDR claim; other clients on same bus remain usable)/
     *   7(memset clears framebuffer+config)/8(NULL+uninit idempotent)/
     *   9(single best-effort transfer ≤5ms)/10(signature unified) */
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_OK; }  /* idempotent no-op on un-init dev */

    /* Read fields before mutation/memset. */
    uint8_t port = dev->config.i2c_port;
    uint16_t addr = dev->config.i2c_addr;
    const char *owner = dev->config.owner;

    /* 1. Best-effort turn screen off (command 0xAE); ignore error (bus may
     *    already be wedged, we are tearing down anyway). */
    uint8_t cmd[2] = {0x00, 0xAE};
    WINK_IGNORE_UNUSED(pal_i2c_transfer(port, addr, cmd, sizeof(cmd), NULL, 0));

    /* 6. Release only this client's I2C address claim — do NOT touch the bus
     *    (other clients like EEPROM may still be active on the same port;
     *    bus lifecycle is the bus-owner's responsibility per ADR-0024 §4 #6). */
    uint32_t res_id = pal_resource_i2c_id(port, addr);
    WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_I2C_ADDR, res_id, owner));

    /* 7. Clear the instance data completely */
    memset(dev, 0, sizeof(dal_ssd1306_t));

    return WINK_OK;
}
