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
 */
typedef struct {
    float latitude;          /**< 纬度（度），正数为北纬(N)，负数为南纬(S) */
    float longitude;         /**< 经度（度），正数为东经(E)，负数为西经(W) */
    float altitude_m;        /**< 海拔高度（米） */
    float speed_kmh;         /**< 地面速度（公里/小时） */
    float course_deg;        /**< 航向（度，0-360°，正北为 0°） */
    uint8_t satellites;      /**< 可见卫星数 */
    bool fix_valid;          /**< 定位有效（有 3D 定位） */
    uint32_t timestamp_ms;   /**< 数据更新时间戳（毫秒） */
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
    uint32_t baudrate;       /**< 波特率（典型值: 9600, 115200） */
    uint16_t rx_buffer_size; /**< NMEA 接收缓冲区大小（字节，默认 256） */
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
 * @brief 初始化 GPS：校验配置参数、配置 UART 波特率、初始化 NMEA 解析器
 *
 * Phase 2 标准化：统一采用 config_t 模式，简化 Codegen 设备树生成。
 *
 * @param dev GPS 实例句柄
 * @param cfg 配置结构体指针（内部深拷贝到 dev->config）
 * @return wink_status_t
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；cfg 非 NULL。
 *   - Blocking: Yes（UART 初始化 + 等待首个 NMEA 语句，超时约 1 秒）。
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_IO / WINK_ERR_TIMEOUT
 *   - Postconditions: WINK_OK 时 dev->initialized=true；cfg 的内容已深拷贝到 dev->config。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_gps_init(dal_gps_t *dev, const dal_gps_config_t *cfg);

/**
 * @brief 非阻塞轮询：接收 UART 数据并解析 NMEA 语句（需在 app_loop 中每 tick 调用）
 * @param dev GPS 实例句柄
 * @return wink_status_t
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；dal_gps_init() 已成功。
 *   - Blocking: No（仅读取 UART 接收缓冲区，不等待数据）。
 *   - Error-codes: WINK_OK / WINK_ERR_NOT_INITIALIZED / WINK_ERR_INVALID_ARG。
 *   - Postconditions: 若解析到有效 GGA/RMC 语句，则 dev->last_position 被更新。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_gps_poll(dal_gps_t *dev);

/**
 * @brief 获取最近一次定位结果
 * @param dev GPS 实例句柄
 * @param pos 输出定位结果（拷贝）
 * @return wink_status_t
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；dal_gps_init() 已成功；pos 非 NULL。
 *   - Blocking: No。
 *   - Error-codes: WINK_OK / WINK_ERR_NOT_INITIALIZED / WINK_ERR_INVALID_ARG /
 *                  WINK_ERR_EMPTY（从未获得有效定位）。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_gps_get_position(const dal_gps_t *dev, dal_gps_position_t *pos);

#ifdef __cplusplus
}
#endif

#endif /* DAL_GPS_H */
