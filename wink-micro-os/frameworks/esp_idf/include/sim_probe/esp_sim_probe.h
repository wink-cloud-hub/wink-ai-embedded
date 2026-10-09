/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef ESP_SIM_PROBE_H_
#define ESP_SIM_PROBE_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#if defined(WINK_TARGET_SIMULATION) || defined(SIMULATION) || defined(WINK_SIM_TEST)

#define WINK_SIM_PROBE_ABI_VERSION_1_1 0x0101

/**
 * @brief 白盒硬件探针快照结构体（AFG-Engine 契约 v1.1）
 *
 * 强制 ABI 版本化：首两字段固定为 probe_abi_version 与 probe_size_bytes。
 * 读取方必须校验版本号与大小，不匹配时返回 ERR_PROBE_ABI_MISMATCH。
 */
typedef struct {
    uint32_t probe_abi_version;    /* 首字段，固定偏移 0；格式 0xMAJOR_MINOR，初版 0x0101 */
    uint32_t probe_size_bytes;     /* sizeof(pal_sim_hardware_probe_t)，前向兼容校验 */
    uint32_t generation_token;     /* 代际 Token，句柄失效后变为 0 */
    uint32_t allocated_bytes;      /* Heap Caps 追踪字节数 */
    uint16_t fifo_watermark;       /* 硬件 FIFO 水位线 */
    uint8_t  state_machine_stage;  /* 状态机内部阶段枚举 */
    bool     is_hardware_busy;     /* 硬件总线忙标志 */
    bool     in_isr_context;       /* 模拟 ISR 上下文标志 */
    uint8_t  power_domain_state;   /* 电源域状态 */
    uint32_t pending_irq_mask;     /* 挂起中断掩码 */
    uint64_t virtual_timestamp_us; /* 当前虚拟时间戳（微秒） */
    uint32_t validity_mask;        /* 字段有效性位图 */
} pal_sim_hardware_probe_t;

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#define WINK_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define WINK_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 获取外设白盒探针快照（纯只读，无时钟推进副作用）
 */
int pal_sim_get_probe(uint32_t domain_id, uint32_t instance_id, pal_sim_hardware_probe_t *out_probe);

/**
 * @brief Wasm 线性内存穿越导出函数（C-ABI），供宿主安全复制探针快照
 */
WINK_EXPORT uint32_t wink_sim_copy_probe(
    uint32_t domain_id,
    uint32_t instance_id,
    uint8_t *out_buffer,
    uint32_t max_len
);

#ifdef __cplusplus
}
#endif

#else /* 物理硬件编译 (xtensa-esp32-elf 等): 硬隔离零 Flash、零 RAM 开销退化 */

typedef struct {
    uint32_t _unused;
} pal_sim_hardware_probe_t;

#define pal_sim_get_probe(domain, inst, out) (-1)
#define wink_sim_copy_probe(domain, inst, buf, max_len) (0)

#endif

#endif /* ESP_SIM_PROBE_H_ */
