#ifndef PAL_I2C_H
#define PAL_I2C_H

#include <stdint.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 极简 I2C bus 生命周期 API —— 仅供 codegen 生成的 bus-owner 静态节点调用。
 *
 * 单器件 DAL _init/_deinit 直接用 pal_i2c_transfer(port, dev_addr, ...)，
 * 不感知 bus 句柄；bus 生命周期由 device_tree 拓扑序管理。
 */

/**
 * @brief 初始化指定的 I2C 物理总线
 * @param port I2C 端口号 [0, PAL_I2C_PORTS)
 * @param sda SDA 物理引脚号
 * @param scl SCL 物理引脚号
 * @param hz 总线频率 (例如 100000 表示 100kHz)
 * @return wink_status_t WINK_OK 表示初始化成功，其他为错误码
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_i2c_bus_init(uint8_t port, uint8_t sda, uint8_t scl, uint32_t hz);

/**
 * @brief 反初始化并注销指定的 I2C 物理总线，包含 SCL 9-pulse 总线恢复
 * @param port I2C 端口号 [0, PAL_I2C_PORTS)
 */
void pal_i2c_bus_deinit(uint8_t port);

#ifdef __cplusplus
}
#endif

#endif // PAL_I2C_H
