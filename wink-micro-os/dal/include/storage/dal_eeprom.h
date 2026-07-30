#ifndef DAL_EEPROM_H
#define DAL_EEPROM_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief EEPROM 非阻塞操作状态机（参照 ultrasonic 同构设计）
 */
typedef enum {
    DAL_EEPROM_IDLE  = 0,   /**< 空闲，可接受新请求 */
    DAL_EEPROM_BUSY  = 1,   /**< 操作进行中（I2C 传输 / 页写入等待） */
    DAL_EEPROM_READY = 2,   /**< 操作完成，结果可读 */
    DAL_EEPROM_ERROR = 3,   /**< 操作失败，last_status 含具体错误码 */
} dal_eeprom_state_t;

/**
 * @brief EEPROM 配置结构体（标准化 config_t 模式）
 *
 * Phase 2 标准化：所有 DAL 外设统一采用 dal_xxx_config_t + dal_xxx_init(dev, cfg) 模式。
 * 便于代码生成器（app_codegen.py）输出结构化的初始化数据。
 *
 * @note wear-leveling = Non-goal：DAL 不做磨损均衡或 KV 语义；上层若需要用独立 storage Role/服务。
 */
typedef struct {
    const char *owner;         /**< 资源占用 owner 静态字符串（device_tree 实例名，静态存储） */
    uint32_t capacity_bytes;   /**< EEPROM 总容量（字节） */
    uint16_t i2c_addr;         /**< I2C 设备地址 (7-bit) */
    uint16_t page_size;        /**< EEPROM 页大小（单次写入最大字节数） */
    uint16_t write_time_ms;    /**< 页写入周期时间（典型值: 5ms for AT24Cxx）；0 → 默认 5ms */
    uint8_t  i2c_port;         /**< I2C 总线端口号 */
} dal_eeprom_config_t;

/**
 * @brief EEPROM 逻辑句柄（POD，ADR-0004 静态分发）
 *
 * 内嵌 config 副本，便于：
 *   1. Flash 动态覆写（ADR-0008）：从 Flash blob 读取 → 写入 config
 *   2. 运行时诊断：可直接打印当前生效的配置
 *
 * 非阻塞状态机字段用于 request_read/request_write + poll 模式。
 */
typedef struct {
    dal_eeprom_config_t config;       /**< 配置副本 */
    dal_eeprom_state_t  state;        /**< 非阻塞操作状态机 */
    wink_status_t       last_status;  /**< 上次操作结果状态（ERROR 时为具体错误码） */
    uint32_t            req_addr;     /**< 当前请求起始地址 */
    uint32_t            req_len;      /**< 当前请求长度 */
    uint8_t            *req_buf;      /**< 当前请求缓冲区指针（read: 输出; write: 输入的内部 copy 或 NULL） */
    bool                initialized;  /**< init 成功后置 true */
} dal_eeprom_t;

/* ── Non-blocking lifecycle API (always visible, STRICT-safe) ──────── */

/**
 * @brief 非阻塞初始化 EEPROM：校验配置参数、claim I2C 地址资源、置 initialized。
 *
 * **不进行 I2C 总线探测**（非阻塞）；总线初始化由 bus-owner 负责。
 * 此函数在所有构建模式下均可见（包括 WINK_STRICT_NONBLOCKING=1）。
 *
 * @experimental Stub: returns WINK_ERR_UNSUPPORTED until I2C EEPROM backend lands.
 *
 * @param dev EEPROM 实例句柄
 * @param cfg 配置结构体指针（内部深拷贝到 dev->config）
 * @return wink_status_t
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；cfg 非 NULL；cfg->owner 非 NULL；cfg->i2c_addr 为有效 7-bit 地址。
 *   - Blocking: No。
 *   - Thread-safe: No; ISR-safe: No.
 *   - Error-codes: WINK_ERR_UNSUPPORTED (stub) / WINK_ERR_INVALID_ARG / WINK_ERR_BUSY (addr conflict)
 *   - Postconditions: 当前 stub 实现下 *dev 被清零，dev->initialized=false，不 claim I2C 资源。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_eeprom_init(dal_eeprom_t *dev, const dal_eeprom_config_t *cfg);

/**
 * @brief 请求一次非阻塞读操作。
 *
 * 提交读请求后立即返回。通过 dal_eeprom_poll() 推进 I2C 传输，
 * dal_eeprom_get_status() 检查完成态，dal_eeprom_get_read_result() 获取数据。
 *
 * @experimental Stub: returns WINK_ERR_UNSUPPORTED until backend lands.
 *
 * @param dev   EEPROM 实例句柄
 * @param addr  起始地址（字节偏移）
 * @param len   读取字节数
 * @return wink_status_t
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；initialized；addr+len 不越界 capacity_bytes；
 *                    state == IDLE（BUSY 时返回 WINK_ERR_BUSY）。
 *   - Blocking: No。
 *   - Thread-safe: No; ISR-safe: No.
 *   - Error-codes: WINK_ERR_UNSUPPORTED (stub) / WINK_OK / WINK_ERR_INVALID_ARG /
 *                  WINK_ERR_NOT_INITIALIZED / WINK_ERR_OUT_OF_RANGE / WINK_ERR_BUSY
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_eeprom_request_read(dal_eeprom_t *dev, uint32_t addr, uint32_t len);

/**
 * @brief 请求一次非阻塞写操作。
 *
 * 提交写请求后立即返回。通过 dal_eeprom_poll() 推进 I2C 传输（含自动分页处理），
 * dal_eeprom_get_status() 检查完成态。
 *
 * @experimental Stub: returns WINK_ERR_UNSUPPORTED until backend lands.
 *
 * @param dev   EEPROM 实例句柄
 * @param addr  起始地址（字节偏移）
 * @param buf   数据缓冲区（调用方须保证在操作完成前 buf 内容不变且不释放）
 * @param len   写入字节数
 * @return wink_status_t
 * @note API Contract:
 *   - Preconditions: dev/buf 非 NULL；initialized；addr+len 不越界；state == IDLE。
 *   - Blocking: No。
 *   - Thread-safe: No; ISR-safe: No.
 *   - Error-codes: 同 request_read。
 *   - Implementation Note: 跨页边界时 poll 内部自动拆分写入，并在每页写入后等待 write_time_ms。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_eeprom_request_write(dal_eeprom_t *dev, uint32_t addr,
                                        const uint8_t *buf, uint32_t len);

/**
 * @brief 非阻塞轮询：推进 I2C 传输状态机。
 *
 * 每 tick 调用一次。IDLE 或 READY/ERROR 时为 no-op。
 *
 * @experimental Stub: returns WINK_ERR_UNSUPPORTED until backend lands.
 *
 * @param dev EEPROM 实例句柄
 * @return wink_status_t
 * @note API Contract:
 *   - Blocking: No。
 *   - Thread-safe: No; ISR-safe: No.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_eeprom_poll(dal_eeprom_t *dev);

/**
 * @brief 查询当前操作状态。
 *
 * @param dev       EEPROM 实例句柄
 * @param out_state 输出：当前状态机状态
 * @return wink_status_t
 * @note API Contract:
 *   - Blocking: No。
 *   - Thread-safe: No; ISR-safe: No.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_eeprom_get_status(const dal_eeprom_t *dev, dal_eeprom_state_t *out_state);

/**
 * @brief 获取读操作结果数据。
 *
 * 仅在 state == READY 且上次操作是 read 时有效。
 * 调用后状态机回到 IDLE。
 *
 * @experimental Stub: returns WINK_ERR_UNSUPPORTED until backend lands.
 *
 * @param dev EEPROM 实例句柄
 * @param buf 输出缓冲区（长度须 ≥ 请求时的 len）
 * @param len 期望读取的长度（须与 request_read 时一致）
 * @return wink_status_t
 * @note API Contract:
 *   - Blocking: No。
 *   - Thread-safe: No; ISR-safe: No.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_eeprom_get_read_result(dal_eeprom_t *dev, uint8_t *buf, uint32_t len);

/* ── Blocking convenience API (STRICT-guarded) ─────────────────────── */
#ifndef WINK_STRICT_NONBLOCKING
/**
 * @brief 阻塞式从 EEPROM 读取数据。
 * @experimental Stub: returns WINK_ERR_UNSUPPORTED until backend lands; buf filled with 0xFF.
 * @note API Contract:
 *   - Blocking: Yes（I2C 总线传输）。
 *     Not available under WINK_STRICT_NONBLOCKING (ADR-0017 层 2).
 *   - Thread-safe: No; ISR-safe: No.
 */
WINK_BLOCKING
WINK_WARN_UNUSED_RESULT
wink_status_t dal_eeprom_read_blocking(dal_eeprom_t *dev, uint32_t addr,
                                        uint8_t *buf, uint32_t len);

/**
 * @brief 阻塞式写入数据到 EEPROM（自动分页处理）。
 * @experimental Stub: returns WINK_ERR_UNSUPPORTED until backend lands.
 * @note API Contract:
 *   - Blocking: Yes（I2C 总线传输 + 页写入等待）。
 *     Not available under WINK_STRICT_NONBLOCKING (ADR-0017 层 2).
 *   - Thread-safe: No; ISR-safe: No.
 */
WINK_BLOCKING
WINK_WARN_UNUSED_RESULT
wink_status_t dal_eeprom_write_blocking(dal_eeprom_t *dev, uint32_t addr,
                                         const uint8_t *buf, uint32_t len);
#endif /* WINK_STRICT_NONBLOCKING */

/**
 * @brief 反初始化 EEPROM：释放 I2C 地址资源、清空句柄。
 *
 * 共享 I2C 契约（ADR-0024 §4 #6）：deinit 仅卸本设备 client 的 I2C 地址 claim，
 * **不** 调用 pal_i2c_bus_deinit()——总线生命周期由 bus-owner（device_tree）管理。
 * 与 mono_oled 等同 bus 器件共存时安全。
 *
 * @param dev EEPROM 实例句柄
 * @return wink_status_t
 * @note API Contract:
 *   - Preconditions: dev 非 NULL。
 *   - Blocking: No（SW-only resource release）。
 *   - Thread-safe: No; ISR-safe: No.
 *   - Idempotent: 未 init 时返回 WINK_OK；NULL 返回 WINK_ERR_INVALID_ARG。
 *   - ADR-0024: 仅释放 I2C_ADDR claim；不销毁共享 bus。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_eeprom_deinit(dal_eeprom_t *dev);

#ifdef __cplusplus
}
#endif

/* ── Compile-time pruning stubs (guard symmetry fix 2026-07-30) ────────
 *
 * Guard symmetry rule (追加项 6a):
 *   - Non-blocking APIs (init, request_*, poll, get_*, deinit) → stub always visible.
 *   - Blocking APIs (read_blocking, write_blocking) → stub #ifndef WINK_STRICT_NONBLOCKING.
 */
#if !defined(WINK_USE_EEPROM) || !WINK_USE_EEPROM
#define WINK_EEPROM_DISABLED_MSG \
    "EEPROM driver not enabled; add an \"eeprom\" device to wink-app.json " \
    "(or set -DWINK_USE_EEPROM=ON)."
WINK_UNAVAILABLE_MSG(WINK_EEPROM_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_eeprom_init(dal_eeprom_t *dev, const dal_eeprom_config_t *cfg);
WINK_UNAVAILABLE_MSG(WINK_EEPROM_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_eeprom_request_read(dal_eeprom_t *dev, uint32_t addr, uint32_t len);
WINK_UNAVAILABLE_MSG(WINK_EEPROM_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_eeprom_request_write(dal_eeprom_t *dev, uint32_t addr,
                                        const uint8_t *buf, uint32_t len);
WINK_UNAVAILABLE_MSG(WINK_EEPROM_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_eeprom_poll(dal_eeprom_t *dev);
WINK_UNAVAILABLE_MSG(WINK_EEPROM_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_eeprom_get_status(const dal_eeprom_t *dev, dal_eeprom_state_t *out_state);
WINK_UNAVAILABLE_MSG(WINK_EEPROM_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_eeprom_get_read_result(dal_eeprom_t *dev, uint8_t *buf, uint32_t len);
#ifndef WINK_STRICT_NONBLOCKING
WINK_UNAVAILABLE_MSG(WINK_EEPROM_DISABLED_MSG) WINK_BLOCKING WINK_WARN_UNUSED_RESULT
wink_status_t dal_eeprom_read_blocking(dal_eeprom_t *dev, uint32_t addr,
                                        uint8_t *buf, uint32_t len);
WINK_UNAVAILABLE_MSG(WINK_EEPROM_DISABLED_MSG) WINK_BLOCKING WINK_WARN_UNUSED_RESULT
wink_status_t dal_eeprom_write_blocking(dal_eeprom_t *dev, uint32_t addr,
                                         const uint8_t *buf, uint32_t len);
#endif /* WINK_STRICT_NONBLOCKING */
WINK_UNAVAILABLE_MSG(WINK_EEPROM_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_eeprom_deinit(dal_eeprom_t *dev);
#endif /* !WINK_USE_EEPROM */

#endif /* DAL_EEPROM_H */
