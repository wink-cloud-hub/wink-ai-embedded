/**
 * @file pal_resource.h
 * @brief 资源占用治理（静态表，零动态分配）。host/debug + esp32 真机双 target 接入；wasm no-op。
 *
 * 检测 GPIO 引脚 / PWM 通道 / I2C 端口/地址的重复占用冲突（review P0-3 / Phase 2 Task 2-3）。
 * host/debug 与 esp32 真机均编译接入（esp32 在 FreeRTOS 临界区内维护静态表）；wasm 单线程
 * 沙箱无需冲突治理，claim/release 退化为 no-op。
 *
 * ⚠ owner 生命周期契约：静态表**持有 owner 指针（不拷贝字符串）**。owner 必须指向
 *    生命周期 ≥ 资源占用期的静态存储——实践中仅接受**字符串字面量**或 device_tree 中的
 *    静态名。传栈上/临时字符串会产生悬垂指针。若需放宽，应在 claim 时 strncpy 到表内
 *    固定缓冲（增加每项体积，权衡，本阶段未做）。
 */
#ifndef PAL_RESOURCE_H
#define PAL_RESOURCE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 受治理的资源类型 */
typedef enum {
    PAL_RESOURCE_GPIO_PIN    = 1,
    PAL_RESOURCE_PWM_CHANNEL = 2,
    PAL_RESOURCE_I2C_PORT    = 3,
    PAL_RESOURCE_I2C_ADDR    = 4,   /* Phase 2：(port,7位地址) 粒度，见 pal_resource_i2c_id */
    PAL_RESOURCE_UART_PORT   = 5,   /* Track A（M1）：UART 端口独占（GPS/串行外设） */
    PAL_RESOURCE_ADC_CHANNEL = 6,   /* PAL ADC 子系统逻辑通道 */
} pal_resource_type_t;

/**
 * @brief 把 I2C (port, 地址) 编码为 PAL_RESOURCE_I2C_ADDR 的资源 id。
 * @note 粒度语义（Device Model Registry §5「同一 I2C port 可共享地址，不可地址冲突」）：
 *       同 port 不同地址 → 不同 id → 不冲突（合法共享总线）；同 (port,addr) 不同 owner →
 *       WINK_ERR_BUSY（地址冲突）。port 占高 16 位、addr 占低 16 位（7/10 位地址均可容纳）。
 */
static inline uint32_t pal_resource_i2c_id(uint8_t port, uint16_t addr) {
    return ((uint32_t)port << 16) | (uint32_t)addr;
}

/** @brief 静态占用表容量（可按平台 -D 调整） */
#ifndef PAL_RESOURCE_MAX_CLAIMS
#define PAL_RESOURCE_MAX_CLAIMS 32
#endif

/**
 * @brief 占用一个资源
 * @param type 资源类型
 * @param id 资源标识（GPIO pin / PWM channel / I2C port）
 * @param owner 占用方静态字符串（须为字面量/静态存储，见文件头生命周期契约）
 * @return WINK_OK / WINK_ERR_BUSY(已被不同 owner 占用) / WINK_ERR_RESOURCE_EXHAUSTED(表满)
 * @note 幂等：同 (type,id) 同 owner → WINK_OK；不同 owner → WINK_ERR_BUSY。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_resource_claim(pal_resource_type_t type, uint32_t id, const char *owner);

/**
 * @brief 释放一个已占用的资源（与 claim 配对，支持 deinit/设备树变更回收占位）。
 * @param type 资源类型
 * @param id 资源标识
 * @param owner 占用方静态字符串（须与 claim 时一致）
 * @return WINK_OK / WINK_ERR_INVALID_ARG(owner 不匹配或未占用) / WINK_ERR_UNSUPPORTED(wasm no-op 视实现)
 * @note 仅当 (type,id) 存在且 owner 完全匹配时才释放；不同 owner 视为未占用返回 INVALID_ARG，
 *       防止误释放他人占位。wasm 单线程沙箱恒返回 WINK_OK（无表）。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_resource_release(pal_resource_type_t type, uint32_t id, const char *owner);

/**
 * @brief 检查一个资源是否已被独占
 * @param type 资源类型
 * @param id 资源标识
 * @return true 表示已被独占，false 表示未被独占
 */
bool pal_resource_is_claimed(pal_resource_type_t type, uint32_t id);

/** @brief 清空资源占用表（测试隔离 / 启动重置用） */
void pal_resource_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* PAL_RESOURCE_H */
