#include "dal_ssd1306.h"
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

/* ---- 极简 5×7 字体（MVP 范围：空格、数字 0-9、大写字母 A,H,I,O,P,S,T,W、感叹号） ----
 * 每个字形 5 字节（5 列 × 7 行），LSB=顶行像素。不支持的字符渲染为空格。
 * 字体数据可后续由 codegen / 外部工具扩展，不影响 API 契约。 */
#define FONT_ENTRIES 20

static const uint8_t s_font[FONT_ENTRIES][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, /*  0: space       */
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, /*  1: '0'         */
    {0x00, 0x42, 0x7F, 0x40, 0x00}, /*  2: '1'         */
    {0x42, 0x61, 0x51, 0x49, 0x46}, /*  3: '2'         */
    {0x21, 0x41, 0x45, 0x4B, 0x31}, /*  4: '3'         */
    {0x18, 0x14, 0x12, 0x7F, 0x10}, /*  5: '4'         */
    {0x27, 0x45, 0x45, 0x45, 0x39}, /*  6: '5'         */
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, /*  7: '6'         */
    {0x01, 0x71, 0x09, 0x05, 0x03}, /*  8: '7'         */
    {0x36, 0x49, 0x49, 0x49, 0x36}, /*  9: '8'         */
    {0x06, 0x49, 0x49, 0x29, 0x1E}, /* 10: '9'         */
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, /* 11: 'A'         */
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, /* 12: 'H'         */
    {0x00, 0x41, 0x7F, 0x41, 0x00}, /* 13: 'I'         */
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, /* 14: 'O'         */
    {0x7F, 0x09, 0x09, 0x09, 0x06}, /* 15: 'P'         */
    {0x26, 0x49, 0x49, 0x49, 0x32}, /* 16: 'S'         */
    {0x01, 0x01, 0x7F, 0x01, 0x01}, /* 17: 'T'         */
    {0x2F, 0x48, 0x48, 0x48, 0x3F}, /* 18: 'W'         */
    {0x00, 0x00, 0x5F, 0x00, 0x00}, /* 19: '!'         */
};

static int font_lookup(char c) {
    if (c == ' ')  return 0;
    if (c >= '0' && c <= '9') return 1 + (c - '0');
    switch (c) {
        case 'A': return 11;
        case 'H': return 12;
        case 'I': return 13;
        case 'O': return 14;
        case 'P': return 15;
        case 'S': return 16;
        case 'T': return 17;
        case 'W': return 18;
        case '!': return 19;
        default:  return -1; /* 不支持 → 空格 */
    }
}

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
        int idx = font_lookup(*p);
        if (idx < 0) { idx = 0; } /* 不支持的字符 → 空格 */
        uint16_t base = (uint16_t)page * dev->config.width + x;
        for (int c = 0; c < SSD1306_FONT_GLYPH_W; c++) {
            dev->framebuffer[base + c] = s_font[idx][c];
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
