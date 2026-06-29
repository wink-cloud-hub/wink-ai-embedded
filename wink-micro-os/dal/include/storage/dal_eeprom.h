#ifndef DAL_EEPROM_H
#define DAL_EEPROM_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief EEPROM 配置结构体（标准化 config_t 模式）
 *
 * Phase 2 标准化：所有 DAL 外设统一采用 dal_xxx_config_t + dal_xxx_init(dev, cfg) 模式。
 * 便于代码生成器（app_codegen.py）输出结构化的初始化数据。
 */
typedef struct {
    uint8_t  i2c_port;         /**< I2C 总线端口号 */
    uint16_t i2c_addr;         /**< I2C 设备地址 (7-bit) */
    uint32_t capacity_bytes;   /**< EEPROM 总容量（字节） */
    uint16_t page_size;        /**< EEPROM 页大小（单次写入最大字节数） */
    uint16_t write_time_ms;    /**< 页写入周期时间（典型值: 5ms for AT24Cxx） */
} dal_eeprom_config_t;

/**
 * @brief EEPROM 逻辑句柄（POD，ADR-0004 静态分发）
 *
 * 内嵌 config 副本，便于：
 *   1. Flash 动态覆写（ADR-0008）：从 Flash blob 读取 → 写入 config
 *   2. 运行时诊断：可直接打印当前生效的配置
 */
typedef struct {
    dal_eeprom_config_t config;  /**< 配置副本 */
    bool initialized;            /**< init 成功后置 true */
} dal_eeprom_t;

/**
 * @brief 初始化 EEPROM：校验配置参数、配置 I2C 总线、置 initialized。
 *
 * Phase 2 标准化：统一采用 config_t 模式，简化 Codegen 设备树生成。
 *
 * @param dev EEPROM 实例句柄
 * @param cfg 配置结构体指针（内部深拷贝到 dev->config）
 * @return wink_status_t
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；cfg 非 NULL；cfg->i2c_addr 为有效 7-bit 地址。
 *   - Blocking: Yes（I2C 总线初始化 + EEPROM 存在性探测）。
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_FOUND / WINK_ERR_IO
 *   - Postconditions: WINK_OK 时 dev->initialized=true；cfg 的内容已深拷贝到 dev->config。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_eeprom_init(dal_eeprom_t *dev, const dal_eeprom_config_t *cfg);

/**
 * @brief 从 EEPROM 读取数据
 * @param dev EEPROM 实例句柄
 * @param addr 起始地址（字节偏移）
 * @param buf 数据缓冲区（输出）
 * @param len 读取字节数
 * @return wink_status_t
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；dal_eeprom_init() 已成功；buf 非 NULL；addr+len 不越界。
 *   - Blocking: Yes（I2C 总线传输）。
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED /
 *                  WINK_ERR_OUT_OF_RANGE / WINK_ERR_IO / WINK_ERR_BUSY
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_eeprom_read(dal_eeprom_t *dev, uint16_t addr, uint8_t *buf, uint16_t len);

/**
 * @brief 写入数据到 EEPROM（自动分页处理）
 * @param dev EEPROM 实例句柄
 * @param addr 起始地址（字节偏移）
 * @param buf 数据缓冲区
 * @param len 写入字节数
 * @return wink_status_t
 * @note API Contract:
 *   - Preconditions: dev 非 NULL；dal_eeprom_init() 已成功；buf 非 NULL；addr+len 不越界。
 *   - Blocking: Yes（I2C 总线传输 + 页写入等待）。
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG / WINK_ERR_NOT_INITIALIZED /
 *                  WINK_ERR_OUT_OF_RANGE / WINK_ERR_IO / WINK_ERR_BUSY
 *   - Implementation Note: 跨页边界时自动拆分写入，并在每页写入后等待 write_time_ms。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_eeprom_write(dal_eeprom_t *dev, uint16_t addr, const uint8_t *buf, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* DAL_EEPROM_H */
