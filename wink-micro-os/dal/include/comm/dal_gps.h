#ifndef DAL_GPS_H
#define DAL_GPS_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief GPS 定位数据结构体（NMEA 解析结果）
 *
 * 坐标使用微度整数（1e-6°）而非浮点：
 *   - float 仅 ~7 位有效数字，经纬度精度退化至 ~±11m（赤道）；
 *   - ESP32 soft-double 成本高，跨 target 对齐风险大；
 *   - int32_t 微度可表示 ±2147°，精度 ~±0.11m，覆盖全球。
 *
 * 字段按对齐需求降序排列（4B → 2B → 1B），减少内部 padding。
 */
typedef struct {
    int32_t  lat_udeg;       /**< 纬度（微度，1e-6°），正=北纬，负=南纬。如 39908712 = 39.908712°N */
    int32_t  lon_udeg;       /**< 经度（微度），正=东经，负=西经 */
    int32_t  alt_mm;         /**< 海拔高度（毫米）；0=未知/海平面 */
    float    speed_kmh;      /**< 地面速度（公里/小时） */
    float    course_deg;     /**< 航向（度，0-360°，正北为 0°） */
    uint32_t timestamp_ms;   /**< 数据更新时间戳（毫秒，pal_os_get_ms） */
    uint16_t hdop;           /**< HDOP × 100（0=未知；如 120 = HDOP 1.20）; Zero-as-Default */
    uint8_t  satellites;     /**< 可见卫星数 */
    uint8_t  fix_quality;    /**< 定位质量：0=无定位, 1=GPS, 2=DGPS, 6=估算; 0=默认 */
    bool     fix_valid;      /**< 定位有效（有 3D 定位） */
    bool     time_valid;     /**< UTC 时间是否有效（NMEA RMC/GGA 时间戳已解析） */
} dal_gps_position_t;

/**
 * @brief GPS 配置结构体（标准化 config_t 模式）
 *
 * Phase 2 标准化：所有 DAL 外设统一采用 dal_xxx_config_t + dal_xxx_init(dev, cfg) 模式。
 * 便于代码生成器（app_codegen.py）输出结构化的初始化数据。
 */
typedef struct {
    const char *owner;       /**< 资源占用 owner 静态字符串（device_tree 实例名，静态存储） */
    uint8_t  uart_port;      /**< UART 总线端口号 */
    uint32_t baudrate;       /**< 波特率（典型值: 9600, 115200）；0 → 默认 9600 */
    uint16_t rx_buffer_size; /**< NMEA 接收缓冲区大小（字节）；0 → 默认 256 */
} dal_gps_config_t;

/**
 * @brief GPS 逻辑句柄（POD，ADR-0004 静态分发）
 *
 * 内嵌 config 副本，便于：
 *   1. Flash 动态覆写（ADR-0008）：从 Flash blob 读取 → 写入 config
 *   2. 运行时诊断：可直接打印当前生效的配置
 */
typedef struct {
    dal_gps_config_t config;        /**< 配置副本 */
    dal_gps_position_t last_position; /**< 最近一次有效定位结果 */
    uint32_t last_fix_time_ms;      /**< 最近一次定位有效时间戳 */
    bool initialized;               /**< init 成功后置 true */
} dal_gps_t;

/**
 * @brief 非阻塞初始化 GPS：校验配置参数、配置 UART、初始化 NMEA 解析器状态。
 *
 * **不等待首帧 NMEA**——数据在 dal_gps_poll() 中推进。
 * 此函数在所有构建模式下均可见（包括 WINK_STRICT_NONBLOCKING=1）。
 *
 * @experimental Stub: returns WINK_ERR_UNSUPPORTED until UART backend
 * (pal_uart, PAL_RESOURCE_UART_PORT backend, see P2-P6) and NMEA parser land.
 * 不要依赖返回 WINK_OK 做业务逻辑；不要假设 dev->initialized 会被置 true。
 *
 * @param dev GPS 实例句柄
 * @param cfg 配置结构体指针（内部深拷贝到 dev->config）
 * @return wink_status_t
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；cfg 非 NULL；cfg->owner 非 NULL。
 *   - Blocking: No（仅配置 UART 参数 + 初始化解析器状态机，不等待数据）。
 *   - Thread-safe: No; ISR-safe: No.
 *   - Error-codes: WINK_ERR_UNSUPPORTED (stub) / WINK_ERR_INVALID_ARG / WINK_ERR_IO
 *   - Postconditions: 当前 stub 实现下 *dev 被清零，dev->initialized=false，不 claim UART 资源。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_gps_init(dal_gps_t *dev, const dal_gps_config_t *cfg);

#ifndef WINK_STRICT_NONBLOCKING
/**
 * @brief 阻塞初始化 GPS：配置 UART + 等待首帧 NMEA 数据。
 *
 * 在 dal_gps_init 基础上额外等待首个有效 NMEA 语句（超时约 1 秒）。
 * 适用于一次性脚本/单测，不适用于协作式实时循环。
 *
 * @experimental Stub: returns WINK_ERR_UNSUPPORTED until backend lands.
 *
 * @param dev GPS 实例句柄
 * @param cfg 配置结构体指针
 * @return wink_status_t
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；cfg 非 NULL。
 *   - Blocking: Yes（UART 初始化 + 等待首个 NMEA 语句，超时约 1 秒）。
 *     Not available under WINK_STRICT_NONBLOCKING (ADR-0017 层 2).
 *   - Thread-safe: No; ISR-safe: No.
 *   - Error-codes: WINK_ERR_UNSUPPORTED (stub) / WINK_ERR_INVALID_ARG / WINK_ERR_IO / WINK_ERR_TIMEOUT
 */
WINK_BLOCKING
WINK_WARN_UNUSED_RESULT
wink_status_t dal_gps_init_blocking(dal_gps_t *dev, const dal_gps_config_t *cfg);
#endif /* WINK_STRICT_NONBLOCKING */

/**
 * @brief 非阻塞轮询：接收 UART 数据并解析 NMEA 语句（需在 app_loop 中每 tick 调用）
 * @experimental Stub: returns WINK_ERR_UNSUPPORTED until backend lands.
 * @param dev GPS 实例句柄
 * @return wink_status_t
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；dal_gps_init() 已成功。
 *   - Blocking: No（仅读取 UART 接收缓冲区，不等待数据）。
 *   - Thread-safe: No; ISR-safe: No.
 *   - Error-codes: WINK_ERR_UNSUPPORTED (stub) / WINK_OK / WINK_ERR_NOT_INITIALIZED / WINK_ERR_INVALID_ARG。
 */
wink_status_t dal_gps_poll(dal_gps_t *dev);

/**
 * @brief 获取最近一次定位结果
 * @experimental Stub: returns WINK_ERR_UNSUPPORTED until backend lands; *pos is zeroed on entry.
 * @param dev GPS 实例句柄
 * @param pos 输出定位结果（拷贝）
 * @return wink_status_t
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；dal_gps_init() 已成功；pos 非 NULL。
 *   - Blocking: No。
 *   - Thread-safe: No; ISR-safe: No.
 *   - Error-codes: WINK_ERR_UNSUPPORTED (stub) / WINK_OK / WINK_ERR_NOT_INITIALIZED / WINK_ERR_INVALID_ARG /
 *                  WINK_ERR_EMPTY（从未获得有效定位）。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_gps_get_position(const dal_gps_t *dev, dal_gps_position_t *pos);

/**
 * @brief 反初始化 GPS：释放 UART 资源、清空句柄。
 * @param dev GPS 实例句柄
 * @return wink_status_t
 * @note API Contract:
 *   - Preconditions: dev 非 NULL。
 *   - Blocking: No（SW-only resource release）。
 *   - Thread-safe: No; ISR-safe: No.
 *   - Idempotent: 未 init 或 NULL 时返回 WINK_OK / WINK_ERR_INVALID_ARG。
 *   - ADR-0024: 释放 UART 资源 claim；未来须停 UART RX DMA/ISR、≤50ms。
 */
wink_status_t dal_gps_deinit(dal_gps_t *dev);

#ifdef __cplusplus
}
#endif

/* ── Compile-time pruning stubs (P2-1 2026-07-06, guard symmetry fix 2026-07-30) ──
 *
 * Guard symmetry rule (追加项 6a):
 *   stub guard ⊇ 本体 guard。
 *   - dal_gps_init_blocking is WINK_BLOCKING + STRICT-guarded in body →
 *     stub must also be #ifndef WINK_STRICT_NONBLOCKING guarded.
 *   - dal_gps_init (non-blocking) is always visible → stub always visible.
 */
#if !defined(WINK_USE_GPS) || !WINK_USE_GPS
#define WINK_GPS_DISABLED_MSG \
    "GPS driver not enabled; add a \"gps\" device to wink-app.json " \
    "(or set -DWINK_USE_GPS=ON)."
WINK_UNAVAILABLE_MSG(WINK_GPS_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_gps_init(dal_gps_t *dev, const dal_gps_config_t *cfg);
#ifndef WINK_STRICT_NONBLOCKING
WINK_UNAVAILABLE_MSG(WINK_GPS_DISABLED_MSG) WINK_BLOCKING WINK_WARN_UNUSED_RESULT
wink_status_t dal_gps_init_blocking(dal_gps_t *dev, const dal_gps_config_t *cfg);
#endif /* WINK_STRICT_NONBLOCKING */
WINK_UNAVAILABLE_MSG(WINK_GPS_DISABLED_MSG)
wink_status_t dal_gps_poll(dal_gps_t *dev);
WINK_UNAVAILABLE_MSG(WINK_GPS_DISABLED_MSG) WINK_WARN_UNUSED_RESULT
wink_status_t dal_gps_get_position(const dal_gps_t *dev, dal_gps_position_t *pos);
WINK_UNAVAILABLE_MSG(WINK_GPS_DISABLED_MSG)
wink_status_t dal_gps_deinit(dal_gps_t *dev);
#endif /* !WINK_USE_GPS */

#endif /* DAL_GPS_H */
