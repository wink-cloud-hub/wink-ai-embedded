#ifndef WINK_DEV_CONFIG_H
#define WINK_DEV_CONFIG_H

#include <stdint.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ──────────────────────────────────────────────────────────────────────
 * Flash 设备树覆写 blob 格式（ADR-0008 方案 C：静态 POD + Flash 动态覆写）
 *
 * 【CRC32 契约 — 前端 Codegen 将来对接的唯一依据；host 实现即权威参考】
 *   算法      : CRC-32/ISO-HDLC（同 zlib / PNG / zip / Ethernet-FCS）
 *   多项式    : 反射值 0xEDB88320（生成多项式 0x04C11DB7）
 *   初值      : 0xFFFFFFFF
 *   final XOR : 0xFFFFFFFF
 *   输入/输出 : 均按字节反射（reflected）
 *   覆盖范围  : header(8B) + items(N×20B)，不含末尾 4 字节 CRC 自身
 *   golden    : crc32("123456789",9)==0xCBF43926；crc32("",0)==0
 *
 * 【字节序】本格式假定 little-endian（host x86 / ESP32 Xtensa / wasm 均为 LE）；
 *           新增 BE target 须改用显式端序序列化（此处 memcpy f32 不再安全）。
 *
 * 【布局】offset+memcpy 逐字段读写；禁 packed 结构体指针强转（规避非对齐访问/
 *         严格别名 UB，见 ADR §3.2 实现澄清）。runtime POD 绝不 memcpy 到 wire。
 *   [magic:u32 = WINK_DEV_CONFIG_MAGIC][version:u16][count:u16]   header 8B
 *   [item]×count : [device_id:u32][params:16B]                     每 item 20B
 *   [crc32:u32]                                                     末尾 4B
 *   长度不变式 : len == HEADER_SIZE + count*ITEM_SIZE + CRC_SIZE
 * ──────────────────────────────────────────────────────────────────────
 */

#define WINK_DEV_CONFIG_MAGIC        0x57494E4Bu   /* "WINK"（见上，端序自洽即可） */
#define WINK_DEV_CONFIG_VERSION      1u
#define WINK_DEV_CONFIG_PARAMS_SIZE  16u
#define WINK_DEV_CONFIG_ITEM_SIZE    (4u + WINK_DEV_CONFIG_PARAMS_SIZE)   /* 20 */
#define WINK_DEV_CONFIG_HEADER_SIZE  8u
#define WINK_DEV_CONFIG_CRC_SIZE     4u
#define WINK_DEV_CONFIG_MAX_BYTES    256u
#define WINK_DEV_CONFIG_KEY          "dtcfg"

/**
 * @brief per-DAL 覆写回调：把 16B params 反序列化进类型化 dev（内部强转回 DAL 类型）。
 * @note 与 wink_actuator 的 void* thunk 范式一致（servo_safe_off_thunk），避免函数指针强转 UB。
 *       非法 params 须返 WINK_ERR_INVALID_ARG 且不写任何字段（与 dal_*_init 的校验纵深配合）。
 */
typedef wink_status_t (*wink_dev_override_fn)(void *dev, const uint8_t *params, uint16_t len);

/**
 * @brief 覆写注册表项：(device_id → dev* → apply_fn) 类型正确的三元组。
 *        固件侧类型安全：apply_fn 与 dev 类型由 codegen/手写 device_tree.c 保证一致。
 *        device_id 是 codegen 分配的稳定 uint32；params 布局变更须 bump blob version。
 */
typedef struct {
    uint32_t             device_id;
    void                *dev;
    wink_dev_override_fn apply;
} wink_dev_override_entry_t;

/**
 * @brief CRC-32/ISO-HDLC（无表 bitwise 实现）。
 * @note 暴露供测试 golden vector 校验，并作为前端对接的参考实现。
 */
WINK_WARN_UNUSED_RESULT
uint32_t wink_dev_config_crc32(const uint8_t *data, uint16_t len);

/**
 * @brief 解析并应用覆写 blob：校验 → 逐 item 查注册表派发。
 *
 * 损坏（magic/version/长度/CRC）→ 整体降级（返对应错误码，不写任何字段）；
 * 单 item 未命中/apply 失败 → 跳过该项不中断（该项降级，其余项照常 apply）。
 *
 * @note API Contract:
 *   - Preconditions: buf 非 NULL；len >= HEADER_SIZE+CRC_SIZE(12)；
 *                    registry_count>0 时 registry 非 NULL。
 *   - Blocking: No; Thread-safe: No; ISR-safe: No.
 *   - Error-codes: WINK_OK / WINK_ERR_INVALID_ARG(NULL/过小/registry 缺) /
 *     WINK_ERR_CONFIG_CORRUPT_DEGRADED(magic/version/长度不符) /
 *     WINK_ERR_CHECKSUM(CRC 不符)。
 *   - Postconditions: WINK_OK 时命中项字段已改写；失败时不写任何字段（降级）。
 *     count==0 为合法 no-op 成功。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_dev_config_apply(const uint8_t *buf, uint16_t len,
                                    const wink_dev_override_entry_t *registry,
                                    uint16_t registry_count);

#ifdef __cplusplus
}
#endif

#endif /* WINK_DEV_CONFIG_H */
