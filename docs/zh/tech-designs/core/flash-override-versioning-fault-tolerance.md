# Flash Override (ADR-0008) 版本化与容错策略技术规范

| 项 | 内容 |
|---|---|
| **状态** | **Accepted（已采纳）** |
| **日期** | 2026-07-30 |
| **关联规范** | [ADR-0008](../../decisions/core/0008-dynamic-device-tree-config-flash.md)、[ADR-0012](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)、[DAL 评审](../../reviews/core/2026-07-30-dal-type-semantic-and-function-sufficiency-review.md) §4 1.6 |

---

## 1. 概述与设计原则

Flash Override (ADR-0008) 是 `wink-micro-os` 提供的免编译快速调试逃生通道。前端 Codegen 或调试工具将参数打包为微型二进制 Block (`dev_tree.bin`) 写入 NVS / Storage，系统启动时在 `dal_*_init` 之前尝试覆盖静态 POD 配置。

为应对 DAL `config_t` 结构体变动、新增字段或 ABI 演进，本规范定义 Flash Wire 二进制协议的版本控制机制与容错降级策略。

### 核心纪律：
1. **绝不越界/不误解析 (Zero Corrupt Parsing)**：Wire 布局变更时，旧版本 Flash blob 严禁被强行按新布局解析。
2. **优雅降级 (Graceful Degradation)**：任何校验失败（版本不匹配、CRC32 错误、长度越界）必须**静默降级**为编译期 `device_tree.c` 静态配置，绝对禁止 Panic、挂起或崩塌。
3. **显式版本门控 (Strict Version Gating)**：不做复杂的前向/后向自动迁移；版本不匹配直接放弃 Override，回退到代码默认值。

---

## 2. Flash Wire 二进制帧头与版本定义

### 2.1 帧头结构 (`wink_dev_config_header_t`)

```c
#include <stdint.h>

#define WINK_DEV_CONFIG_MAGIC    0x474E4957u  /* "WING" / "WINK" 校验魔数 */
#define WINK_DEV_CONFIG_VERSION  1u          /* 当前全局 Wire 协议版本号 */

typedef struct {
    uint32_t magic;         /**< 魔数：0x474E4957 ("WINK") */
    uint16_t version;       /**< 协议版本号：当前固件固定校验 == WINK_DEV_CONFIG_VERSION */
    uint16_t total_devices; /**< 包含覆写配置的器件条目总数 */
} wink_dev_config_header_t;
```

### 2.2 Wire Item 结构与逐字段反序列化 (ADR-0008 §5.2)

```c
typedef struct {
    uint32_t device_id;     /**< 设备静态 ID/Hash（Codegen 稳定生成） */
    uint8_t  param_len;     /**< 本条目 params 有效负载长度 */
    uint8_t  params[16];    /**< 变长/固定负载缓冲（逐字段拷贝，禁止底层指针直转） */
} wink_dev_config_item_t;
```

---

## 3. 版本演进与容错处理流程

```
 ┌──────────────────────────────────────────────────────────┐
 ├─► 1. 从 pal_storage 读取 NVS/Flash 数据 (dev_tree.bin)   │
 └────────────────────────────┬─────────────────────────────┘
                              │
                    ┌─────────▼─────────┐
                    │  Length >= Header? │ ──NO──┐
                    └─────────┬─────────┘       │
                              │ YES             │
                    ┌─────────▼─────────┐       │
                    │ Magic == 0x474E4957│ ──NO──┤
                    └─────────┬─────────┘       │
                              │ YES             │
                    ┌─────────▼─────────┐       │
                    │ CRC32 Valid ISO?  │ ──NO──┤
                    └─────────┬─────────┘       │
                              │ YES             │
                    ┌─────────▼─────────┐       │
                    │ Version == CURRENT│ ──NO──┤
                    └─────────┬─────────┘       │
                              │ YES             │
                    ┌─────────▼─────────┐       │
                    │ Dispatch items to │       │
                    │ dal_*_apply_override      │
                    └─────────┬─────────┘       │
                              │                 │
                              ▼                 ▼
                     [ 覆写成功生效 ]    [ 优雅降级：退回编译期默认配置 ]
```

### 3.1 校验四重防护门禁

1. **长度校验 (Length Check)**：`blob_size < sizeof(header) + 4 (CRC)` → 忽略覆写。
2. **魔数校验 (Magic Check)**：`header.magic != WINK_DEV_CONFIG_MAGIC` → 忽略覆写。
3. **CRC32 ISO-HDLC 校验 (Integrity Check)**：
   - 覆盖范围：Header + Items 内容（不含末尾 4 字节 CRC 字段）。
   - 计算结果不一致 → 忽略覆写（防 Flash 掉电点阵损坏）。
4. **版本精确匹配 (Version Gating)**：
   - `header.version != WINK_DEV_CONFIG_VERSION` → 忽略覆写并记录 Log：
     `"Flash config version mismatch (blob=%d, fw=%d); fallback to static device_tree."`

---

## 4. DAL params 变更时的 Bump 规则

当任意 DAL `config_t` 结构体或 `apply_override` 序列化格式变更时：

1. **版本 Bump 规则**：
   - 只要任何 DAL 的 `apply_override` params 布局（字段顺序、大小、类型）发生突破性变更（例如 `rc_servo` 增加 16 位角度限制、`dc_motor` 增加 `invert` 字段等），必须将全局 `WINK_DEV_CONFIG_VERSION` 递增（`version++`）。
2. **前端 Codegen 同步**：
   - 编译器/工具链与 Web 画布前端在导出 `dev_tree.bin` 时使用匹配的 `version`。
3. **降级保障**：
   - 烧录新固件后，若 Flash NVS 中留存有旧版前端写入的旧 `version` 配置文件，固件启动时自动识别版本号不匹配，跳过旧 Override 块，直接以新固件内嵌的 `device_tree.c` POD 默认值初始化。
   - 用户通过 Web 重新保存/连线下发后，新 `dev_tree.bin` 会覆盖旧 NVS 槽位，恢复动态覆写能力。

---

## 5. 各 DAL type 当前 Override 支持状态

| DAL Type | `apply_override` 状态 | Wire Params 字节长 | 说明 |
|---|---|---|---|
| `ultrasonic` | 已支持 | 8 B | `(uint16_t trig, uint16_t echo, uint32_t timeout)` |
| `rc_servo` | 已支持 | 9 B | `(uint16_t pin, uint8_t pwm_ch, uint16_t min_us, uint16_t max_us, uint16_t default_angle)` |
| `led` | 可选 / 保留 API | 3 B | `(uint16_t pin, uint8_t active_high)` |
| `button` | 可选 / 保留 API | 4 B | `(uint16_t pin, uint8_t active_low, uint8_t pull)` |
| 其余 Type | 不适用 / 走静态 config | — | 通过静态编译期 `device_tree.c` 配置 |

---

## 6. 验证与单测要求

1. **Host 单元测试 (`test_pal_resource_wire.c` / `test_flash_override.c`)**：
   - 验证 CRC 错误静默降级；
   - 验证 Version 故意改成 `0xFFFF` 时静默降级且不改动 POD 字段；
   - 验证合法 Version 下 `apply_override` 正确写回 POD 实例。

