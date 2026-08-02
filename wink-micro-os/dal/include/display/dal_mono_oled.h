#ifndef DAL_MONO_OLED_H
#define DAL_MONO_OLED_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 单色 OLED 最大帧缓冲（128×64 / 8 = 1024 字节） */
#define MONO_OLED_FB_SIZE 1024

/** @brief 紧凑字体宽度（5 像素宽 + 1 像素间距 = 6 像素有效宽度） */
#define MONO_OLED_FONT_WIDTH  6
#define MONO_OLED_FONT_GLYPH_W 5

/**
 * @brief 单色 OLED 同族变体（屏驱芯片）— 强制 1 字节存储。
 *
 * ``typedef uint8_t`` 保持 layout；取值见下方枚举常量。
 * 今日成员本已是裸 ``uint8_t``，本改动引入强类型别名 + 绑定常量。
 */
typedef uint8_t dal_mono_oled_variant_t;

enum {
    DAL_MONO_OLED_VARIANT_SSD1306 = 0,  /**< SSD1306（默认，Zero-as-Default） */
    DAL_MONO_OLED_VARIANT_SH1106  = 1,  /**< SH1106（列偏移；未实现 → UNSUPPORTED） */
};

#ifdef __cplusplus
static_assert(sizeof(dal_mono_oled_variant_t) == 1,
              "variant must stay 1 byte to keep dal_mono_oled_config_t layout stable");
#else
_Static_assert(sizeof(dal_mono_oled_variant_t) == 1,
               "variant must stay 1 byte to keep dal_mono_oled_config_t layout stable");
#endif

/**
 * @brief 单色 OLED 构造期配置（dal_mono_oled_init 输入）
 */
typedef struct {
    const char *owner;      /* 资源占用 owner 静态字符串（device_tree 实例名） */
    uint16_t i2c_addr;      /* 7 位 I2C 地址（常见 0x3C 或 0x3D） */
    uint16_t width;         /* 屏幕宽度像素（常见 128） */
    uint16_t height;        /* 屏幕高度像素（常见 64） */
    uint8_t  i2c_port;      /* 逻辑 I2C 端口号 */
    dal_mono_oled_variant_t variant; /* 0=SSD1306; values from enum above */
} dal_mono_oled_config_t;

/**
 * @brief 单色 OLED 实例（运行期状态；POD，零动态分配）
 *
 * 帧缓冲内嵌在结构体内（128×64/8 = 1024 B），避免堆分配。
 *
 * Phase 2 标准化：内嵌 `.config` 副本（与 led/button/ultrasonic/servo 一致），
 * 便于 codegen 统一遍历、Flash 覆写（ADR-0008）和运行时诊断。
 *
 * `pages` 是从 config.height 派生的运行期值（height/8），init 时计算一次并缓存于
 * 顶层字段，避免每次 draw/flush 重复右移；不进 config 副本，避免"输入 vs 派生态"
 * 语义混淆。
 *
 * config MUST be the first member (DAL-S-011, offsetof==0). The 1024-byte
 * framebuffer follows it (uint8 array has no alignment constraint); trailing
 * uint8/bool need no tail padding on either ILP32 or 64-bit host.
 */
typedef struct {
    dal_mono_oled_config_t config;    /* 配置副本，由 init 从 cfg 深拷贝（首成员，offsetof==0） */
    uint8_t                framebuffer[MONO_OLED_FB_SIZE]; /* 帧缓冲（页式：每页 8 行 × 128 列） */
    uint8_t                pages;     /* 派生：config.height / 8 */
    bool                   initialized;
} dal_mono_oled_t;

/* ABI stability: config MUST remain the first member (DAL-S-011). */
_Static_assert(offsetof(dal_mono_oled_t, config) == 0, "config must be the first member");

/**
 * @brief 初始化单色 OLED：I2C 地址冲突治理、下发 init 命令序列。
 * @note API Contract:
 *   - Preconditions: dev/cfg 非 NULL；cfg->owner 非 NULL（静态存储）。
 *   - Blocking: No（pal_i2c_transfer 不阻塞）。
 *   - Thread-safe: No; ISR-safe: No.
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_BUSY(地址冲突) /
 *     透传 PAL 错误（WINK_ERR_IO 等）。
 *   - Postconditions: WINK_OK 时 dev->initialized=true；帧缓冲已清零；显示已开启。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_mono_oled_init(dal_mono_oled_t *dev, const dal_mono_oled_config_t *cfg);

/**
 * @brief 清空帧缓冲（纯内存操作，无 I2C）。
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；initialized。
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_mono_oled_clear(dal_mono_oled_t *dev);

/**
 * @brief 在帧缓冲中绘制文本（不触发 I2C；渲染后须 dal_mono_oled_flush 刷到屏幕）。
 * @param col 起始列（0 ~ width-1）
 * @param page 起始页（0 ~ pages-1；每页 8 像素高）
 * @param str 以 null 结尾的 ASCII 字符串。
 *   默认字库（WINK_MONO_OLED_FONT=ascii_upper）：空格、0-9、A-Z/a-z、!；
 *   其它字符渲染为空格。minimal 字库仅含 demo 常用字母子集。
 * @note API Contract:
 *   - Preconditions: dev/str 非 NULL；initialized。
 *   - Blocking: No。
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_mono_oled_draw_text(dal_mono_oled_t *dev, uint16_t col, uint8_t page,
                                      const char *str);

/**
 * @brief 将帧缓冲刷到单色 OLED（按页经 pal_i2c_transfer 写出）。
 * @note 每页使用 ≤129 B 栈缓冲（1 控制字节 + 128 数据字节）；无 VLA，无 malloc。
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；initialized。
 *   - Blocking: No。
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED / 透传 PAL 错误。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_mono_oled_flush(dal_mono_oled_t *dev);

/**
 * @brief 反初始化单色 OLED：熄灭屏幕、释放 I2C 地址资源。
 *
 * 共享 I2C 契约（ADR-0024 §4 #6）：deinit 仅卸本设备 client 的 I2C 地址 claim，
 * **不** 调用 pal_i2c_bus_deinit()——总线生命周期由 bus-owner（device_tree）管理。
 * 与 eeprom 等同 bus 器件共存时安全。
 *
 * @param dev 单色 OLED 实例句柄
 * @return wink_status_t
 * @note API Contract:
 *   - Preconditions: dev 非 NULL。
 *   - Blocking: No（best-effort 屏幕关闭命令，失败忽略）。
 *   - Thread-safe: No; ISR-safe: No.
 *   - Idempotent: 未 init 时返回 WINK_OK。
 *   - ADR-0024: 只释放 I2C_ADDR claim，不销毁共享 bus。
 */
wink_status_t dal_mono_oled_deinit(dal_mono_oled_t *dev);

#ifdef __cplusplus
}
#endif

/* ── Compile-time pruning stubs (P2-1 2026-07-06) ────────────────────── */
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
