/**
 * @file wasm_dev_ssd1306.c
 * @brief Wasm 仿真侧 SSD1306 OLED 虚拟外设模型 (C-side Model)。
 */
#include "wasm_sim_registry.h"
#include <string.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

#define SSD1306_WIDTH  128
#define SSD1306_HEIGHT 64
#define SSD1306_FB_SIZE (SSD1306_WIDTH * SSD1306_HEIGHT / 8) // 1024 bytes

// 虚拟 SSD1306 状态
static uint8_t s_virtual_fb[SSD1306_FB_SIZE];
static uint8_t s_height = 64;

static uint8_t s_col_start  = 0;
static uint8_t s_col_end    = 127;
static uint8_t s_col_cursor = 0;

static uint8_t s_page_start  = 0;
static uint8_t s_page_end    = 7;
static uint8_t s_page_cursor = 0;

static uint8_t s_addr_mode = 0; // 0=Horizontal, 1=Vertical, 2=Page

void wasm_dev_ssd1306_reset(void) {
    memset(s_virtual_fb, 0, sizeof(s_virtual_fb));
    s_height      = 64;
    s_col_start   = 0;
    s_col_end     = 127;
    s_col_cursor  = 0;
    s_page_start  = 0;
    s_page_end    = 7;
    s_page_cursor = 0;
    s_addr_mode   = 0;
}

// 供 JS 侧轮询渲染的导出接口
EMSCRIPTEN_KEEPALIVE const uint8_t* pal_wasm_get_ssd1306_fb(uint32_t *width, uint32_t *height) {
    if (width)  *width  = SSD1306_WIDTH;
    if (height) *height = s_height;
    return s_virtual_fb;
}

// 解析 I2C 指令及数据流
wink_status_t wasm_dev_ssd1306_transfer(const uint8_t *write_buf, uint32_t write_len) {
    if (write_len < 2) {
        return WINK_OK;
    }

    uint8_t control_byte = write_buf[0];

    // control_byte = 0x00 / 0x80 表示命令包，0x40 表示数据包
    if (control_byte == 0x00 || control_byte == 0x80) {
        uint32_t i = 1;
        while (i < write_len) {
            uint8_t cmd = write_buf[i];
            if (cmd == 0x21 && i + 2 < write_len) { // Set Column Address
                s_col_start  = write_buf[i + 1];
                s_col_end    = write_buf[i + 2];
                s_col_cursor = s_col_start;
                i += 3;
            } else if (cmd == 0x22 && i + 2 < write_len) { // Set Page Address
                s_page_start  = write_buf[i + 1];
                s_page_end    = write_buf[i + 2];
                s_page_cursor = s_page_start;
                i += 3;
            } else if (cmd == 0x20 && i + 1 < write_len) { // Set Memory Addressing Mode
                s_addr_mode = write_buf[i + 1];
                i += 2;
            } else if (cmd == 0xA8 && i + 1 < write_len) { // Set Multiplex Ratio (决定高度)
                uint8_t ratio = write_buf[i + 1];
                s_height = (ratio == 31) ? 32 : 64;
                s_page_end = (s_height >> 3) - 1;
                i += 2;
            } else {
                // 其他普通指令忽略
                i++;
            }
        }
    } else if (control_byte == 0x40) {
        // 数据包写入虚拟 Framebuffer
        for (uint32_t i = 1; i < write_len; i++) {
            uint16_t offset = (uint16_t)s_page_cursor * SSD1306_WIDTH + s_col_cursor;
            if (offset < SSD1306_FB_SIZE) {
                s_virtual_fb[offset] = write_buf[i];
            }

            // 水平寻址模式 (Mode 0) 递增逻辑
            if (s_addr_mode == 0) {
                s_col_cursor++;
                if (s_col_cursor > s_col_end) {
                    s_col_cursor = s_col_start;
                    s_page_cursor++;
                    if (s_page_cursor > s_page_end) {
                        s_page_cursor = s_page_start;
                    }
                }
            } else {
                // 其他寻址模式暂做简易的列自增
                s_col_cursor++;
                if (s_col_cursor >= SSD1306_WIDTH) {
                    s_col_cursor = 0;
                }
            }
        }
    }

    return WINK_OK;
}
