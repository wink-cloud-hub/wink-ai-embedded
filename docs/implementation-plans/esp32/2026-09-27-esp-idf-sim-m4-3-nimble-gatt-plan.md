# ESP-IDF 仿真拦截层实施计划 M4-3：NimBLE 虚拟 GATT 服务与特征值抽象（BLE 外设仿真拦截）

> 📋 **计划状态声明**：
> 本计划为 ESP-IDF 仿真拦截层 M4 里程碑第三阶段（蓝牙外设协议栈拦截与 GATT 虚拟化）。
> **继承总纲**：[`PLAN-20260922-ESP-IDF-SIM-MASTER`](./2026-09-22-esp-idf-simulation-interception-master-plan.md) (v3.5)
> **路线图锚定**：[`PLAN-20260927-ESP-IDF-SIM-M4-CONNECTIVITY`](./2026-09-27-esp-idf-sim-m4-wifi-ble-connectivity-roadmap.md) (v1.1) §4 Task M4-3
> **当前状态**：✅ 已完成（v1.2 全量验收闭环，65/65 测试全绿，Tier-A 语料与 Wasm 门禁通过）
> 🎯 **计划版本**：v1.2（2026-09-27）
> 📚 **关联规范**：`docs-adr.md`、`03-coding-guidelines.md`、`00-IMPLEMENTATION-PLAN-TEMPLATE.md`、[ADR-0012](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)（合约诚实原则）、[ADR-0045](../../decisions/core/0045-unified-memory-and-zero-heap-contract.md)（零运行时堆分配）、[ADR-0057](../../decisions/core/0057-pal-adc-subsystem-and-channel-3-analog-contract.md)（PAL 保持对网络与射频无知）、[ADR-0083/0084](../../decisions/core/0083-dual-target-compilation-and-license-boundaries.md)（许可分层）、[ADR-0087](../../decisions/core/0087-esp-idf-asset-channels-and-soc-data-ownership.md)（手写头文件通道）

---

## 1. 元数据表（🔴 必选）

| 字段 | 内容 |
|:---|:---|
| **计划编号** | `PLAN-20260927-ESP-IDF-SIM-M4-3-NIMBLE-GATT` |
| **创建日期** | 2026-09-27 |
| **目标平台/SoC** | `wasm32-unknown-emscripten` / `host` (x86_64, Windows/Linux)；前端交互环境：`@wink-ai/unisim` (Browser) |
| **工具链/SDK版本**| ESP-IDF v6.1@fff9895c vendored / MinGW GCC 16 / Emscripten 4.0.10 |
| **计划状态** | ✅ 已完成（v1.2 全量验收闭环） |
| **优先级** | 🟡 P1（M4-1 Wi-Fi 与 M4-2 MQTT/HTTP 闭环后的近场无线连接拓展） |
| **计划版本** | `v1.2` |
| **关联技术设计** | [`docs/zh/design/04-wasm-simulation/00-README.md`](../../zh/design/04-wasm-simulation/00-README.md)、[`docs/implementation-plans/esp32/2026-09-27-esp-idf-sim-m4-wifi-ble-connectivity-roadmap.md`](./2026-09-27-esp-idf-sim-m4-wifi-ble-connectivity-roadmap.md) |
| **关联设计规范** | [`wink-micro-os/frameworks/esp_idf/docs/01-architecture-and-governance-guide.md`](../../../wink-micro-os/frameworks/esp_idf/docs/01-architecture-and-governance-guide.md) |
| **关联 ADR** | [ADR-0012](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)（Fail-Loud）、[ADR-0045](../../decisions/core/0045-unified-memory-and-zero-heap-contract.md)（零堆分配）、[ADR-0057](../../decisions/core/0057-pal-adc-subsystem-and-channel-3-analog-contract.md)（PAL 无知射频）、[ADR-0083/0084](../../decisions/core/0083-dual-target-compilation-and-license-boundaries.md)（许可分层）、[ADR-0087](../../decisions/core/0087-esp-idf-asset-channels-and-soc-data-ownership.md)（手写头文件通道） |
| **目标里程碑** | M4-3（NimBLE 虚拟 GATT 服务与特征值抽象） |
| **前置依赖计划** | [`./2026-09-27-esp-idf-sim-m4-2-mqtt-http-plan.md`](./2026-09-27-esp-idf-sim-m4-2-mqtt-http-plan.md)（✅ 已 100% 验收结项，60/60 测试全绿） |
| **计划负责人** | 仿真拦截专项小组 & UniSim 前端引擎组 |
| **所需子代理技能** | `embedded-best-practice` |

---

## 2. 背景与目标（🔴 必选）

### 2.1 问题陈述

在物联网与智能硬件场景中，低功耗蓝牙（BLE, Bluetooth Low Energy）是近场配置、传感器直连与手机交互（App 配网、无感开锁、手环数据同步）的核心通信手段。真实 AI 生成的 ESP32 固件中大量使用 BLE Peripheral 角色对外暴露 GATT（Generic Attribute Profile）服务。

然而，在 Wasm 浏览器沙箱与 Host 仿真环境下，模拟物理蓝牙面临极为苛刻的客观障碍：
1. **原厂 Bluedroid 协议栈极其沉重且非纯 C 静态分发**：ESP-IDF 经典的 Bluedroid 源码体量达数十万行，存在大量复杂的深层动态内存分配（`malloc`/`free`）、操作系统线程交织与专有底层驱动，根本无法轻量化移植到 Wasm 协同 Fiber 环境中；
2. **射频物理连续域不可逆（[ADR-0012](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)）**：仿真环境无法也不应模拟 2.4GHz 空间跳频（FHSS）、物理层 GFSK 调制解调与天线增益；
3. **前端 UniSim 联调诉求（M4-4 前置）**：UniSim 前端需要一个纯粹、清晰的**“声明式 GATT 服务注册表”**，以树状结构直接向用户展示 Characteristic，支持用户在 Web 端像使用手机蓝牙调试助手（如 LightBlue/nRF Connect）一样进行读取、写入与接收 Notify 广播。

### 2.2 核心选型决策：全栈拥抱 Apache Mynewt NimBLE 门面

针对上述问题，本计划执行 [`PLAN-20260927-ESP-IDF-SIM-M4-CONNECTIVITY`](./2026-09-27-esp-idf-sim-m4-wifi-ble-connectivity-roadmap.md) §4 锚定的战略选型：
- **坚决剔除 Bluedroid**，全面拦截与实现 **Apache Mynewt NimBLE** 风格的 ESP-IDF API；
- NimBLE 本身具有极度轻量、纯 C 静态结构体声明服务、事件回调模型清晰的巨大优势，非常适合与 WinkMicroOS 的零堆内存红线契约无缝结合。

### 2.3 技术与业务目标（v1.1 强化版）

- ✅ **目标 1：零修改 C-ABI 头文件闭包（NimBLE 官方接口）**：
  - 手写实现 NimBLE 核心头文件：`host/ble_hs.h`、`host/ble_uuid.h`、`host/ble_gap.h`、`host/ble_gatt.h`、`services/gap/ble_svc_gap.h`、`services/gatt/ble_svc_gatt.h`、`nimble/nimble_port.h`、`nimble/nimble_port_freertos.h` 与轻量 `os/os_mbuf.h`；
  - 完整声明 `struct ble_gatt_svc_def`、`struct ble_gatt_chr_def`、`struct ble_gatt_dsc_def`、`struct ble_gatt_access_ctxt` 等标准结构体；
  - **补齐关键回调与上下文**：补齐 `struct ble_gatt_register_ctxt`、`ble_gatt_register_fn`、`ble_hs_cfg.gatts_register_cb`（修复官方 `bleprph` 语料编译阻断缺陷）；
  - **补齐广播与通知扩展**：补齐 `ble_gap_adv_rsp_set_fields`（扫描响应）与 `ble_gatts_notify` 门面；
  - 登记至 `channels.json`并通过 `check_harvested_headers.py` 自动化门禁。
- ✅ **目标 2：零堆静态 GATT 注册池（[ADR-0045](../../decisions/core/0045-unified-memory-and-zero-heap-contract.md)）**：
  - 严禁任何运行时动态内存分配。在 `src/bluetooth/esp_nimble.c` 内部预分配紧凑的静态 POD 槽位（**扩容至最多 8 个 Service、32 个 Characteristic、32 个 Descriptor**，从容兼容 GAP、GATT、DIS 与多自定义业务服务）；
  - `ble_gatts_count_cfg()` 遍历服务声明数组进行静态容量上限校验，超限时 Fail-Loud 报错；
  - `ble_gatts_add_svcs()` 递归/平面解析服务树，登记属性表并为 Service、Chr 声明、Chr Value、Descriptor 分配符合 BLE 规范的单调递增属性句柄；若配置了 `gatts_register_cb` 则逐项派发注册通知。
- ✅ **目标 3：GAP 广播、协同调度脉冲与虚拟连接状态机**：
  - 实现 `ble_gap_adv_start()` / `ble_gap_adv_stop()` / `ble_gap_adv_active()` / `ble_gap_adv_set_fields()` / `ble_gap_adv_rsp_set_fields()`；
  - **协同调度非阻塞脉冲（防卡死规约）**：针对 Wasm/Fiber 协同环境，`nimble_port_run()` 负责驱动单次 Host 同步脉冲，触发应用 `ble_hs_cfg.sync_cb()`，不执行死循环，确保主线程不阻塞；
  - 提供单链路虚拟连接状态机：支持触发 `BLE_GAP_EVENT_CONNECT` 与 `BLE_GAP_EVENT_DISCONNECT`；断开连接后自动复位连接句柄并支持重播；
  - 支持虚拟 MTU 协商（默认 256 字节）与 `BLE_GAP_EVENT_MTU` 派发。
- ✅ **目标 4：GATT 读写回调与订阅通知（Notify/Indicate）完整闭环**：
  - **mbuf 解包与封装**：在 `os/os_mbuf.h` 中补全 `os_mbuf_copydata()` 与 `OS_MBUF_PKTLEN()` 宏，应用层在 `OP_WRITE_CHR` 写入回调中能安全可靠地提取下发字节流；
  - 当外部发起读取时，构造 `BLE_GATT_ACCESS_OP_READ_CHR` 上下文并调用应用注册的 `access_cb`；
  - 属性权限校验：对没有 `READ` 权限的特征值拒绝读请求，对没有 `WRITE` 权限的特征值拒绝写请求；
  - **双向订阅联动**：当调用 `esp_nimble_sim_subscribe()` 时，仿真层不仅设置内部订阅标志位，同时向固件注册的 GAP 回调派发 `BLE_GAP_EVENT_SUBSCRIBE`（触发固件内部定时器或推送逻辑）；
  - 固件调用 `ble_gatts_chr_updated()` 或 `ble_gatts_notify_custom()` 时，若处于已订阅状态，向 UniSim 导出钩子（`esp_nimble_sim_set_notify_hook`）推送最新数据。
- ✅ **目标 5：UniSim 桥接与服务树发现（M4-4 联调就绪）**：
  - 导出全套仿真注入 API：`esp_nimble_sim_connect`、`esp_nimble_sim_disconnect`、`esp_nimble_sim_write_chr`、`esp_nimble_sim_read_chr`、`esp_nimble_sim_subscribe`、`esp_nimble_sim_reset`；
  - **服务树动态发现 API**：新增导出 `esp_nimble_sim_get_service_count/info` 与 `esp_nimble_sim_get_char_count/info`，彻底解除 UniSim 前端“失明”状态，支持前端组件动态遍历展开 Service 与 Characteristic；
  - 导出符号全部使用 `WINK_SIM_EXPORT` 修饰，确保在 Wasm 生产模式下不会被 DCE（死代码消除）。
- ✅ **目标 6：官方语料 Tier-A 构建验证**：
  - 引入 ESP-IDF 官方黄金例程 `examples/bluetooth/nimble/bleprph`（BLE Peripheral 例程，包含 `main.c` 与 `gatt_svr.c`）；
  - 验证该语料在 Host 与 Wasm 下零告警构建通过。

### 2.4 成功指标（分级验收出口）

| 指标 | 通过标准 | 验证方法 |
|:---|:---|:---|
| **L0 编译门禁** | Host `-Wall -Wextra -Werror` 0 warning；Wasm `emcc` 编译通过 | CMake / CTest 编译检查 |
| **L0 治理门禁** | `check_harvested_headers.py` 0 error；`check_license_map.py` satisfied | Python 自动化门禁脚本 |
| **L0 架构门禁** | `winkcli lint --pack layering --pack api` 0 findings | winkcli 静态分析工具 |
| **L1 单元测试** | `test_esp_nimble`（≥18 个用例）100% 通过（TC-BLE-01 ~ TC-BLE-18 全绿） | Unity 测试套件 |
| **L1 零回归测试** | 既有 60 项 CTest **100% 零破坏通过**（测试总数提升至 64+ 项） | `ctest -L esp_idf` |
| **L2 语料编译** | `esp_idf_corpus_bleprph_obj` 0 warning 构建通过 | `OBJECT` 库编译目标 |

---

## 3. 变更范围与影响分析（🔴 必选）

### 3.1 文件变更清单

| 文件路径 | 变更类型 | 说明 |
|:---|:---:|:---|
| `wink-micro-os/frameworks/esp_idf/include/nimble/nimble_port.h` | 🆕 手写 | NimBLE Port 初始化、运行与反初始化公开 API |
| `wink-micro-os/frameworks/esp_idf/include/nimble/nimble_port_freertos.h` | 🆕 手写 | NimBLE FreeRTOS 任务绑定公开 API |
| `wink-micro-os/frameworks/esp_idf/include/host/ble_hs.h` | 🆕 手写 | NimBLE Host 核心配置、注册回调、UniSim 发现接口与错误码全集 |
| `wink-micro-os/frameworks/esp_idf/include/host/ble_uuid.h` | 🆕 手写 | 16/32/128-bit UUID 结构体、INIT/DECLARE 宏与比较函数 |
| `wink-micro-os/frameworks/esp_idf/include/host/ble_gap.h` | 🆕 手写 | GAP 广播、扫描响应、连接描述符与 GAP 事件派发模型 |
| `wink-micro-os/frameworks/esp_idf/include/host/ble_gatt.h` | 🆕 手写 | GATT 服务、特征值声明、注册上下文与访问回调上下文 |
| `wink-micro-os/frameworks/esp_idf/include/services/gap/ble_svc_gap.h` | 🆕 手写 | GAP 内置服务（Device Name）门面 |
| `wink-micro-os/frameworks/esp_idf/include/services/gatt/ble_svc_gatt.h` | 🆕 手写 | GATT 内置服务初始化门面 |
| `wink-micro-os/frameworks/esp_idf/include/os/os_mbuf.h` | 🆕 手写 | 零堆轻量静态 mbuf 结构体、追加/拷贝函数与 PKTLEN 宏 |
| `wink-micro-os/frameworks/esp_idf/src/bluetooth/esp_nimble.c` | 🆕 LGPL | 静态零堆 GATT 注册池、GAP 状态机、回调分发与 Sim 钩子 |
| `wink-micro-os/frameworks/esp_idf/channels.json` | ✏️ 修改 | 登记 9 个新增手写 NimBLE 头文件 |
| `wink-micro-os/frameworks/esp_idf/esp_idf_sources.cmake` | ✏️ 修改 | 将 `esp_nimble.c` 追加至源文件列表 |
| `wink-micro-os/frameworks/esp_idf/test/core/test_esp_nimble.c` | 🆕 GPL | NimBLE 18 项全覆盖测试套件（注册、读写回调、广播、CCCD订阅、服务树发现等） |
| `wink-micro-os/frameworks/esp_idf/test/corpus/bleprph/` | 🆕 语料 | 官方 `bleprph` 示例语料与配套 `sdkconfig.h`、`bleprph.h` 桩 |
| `wink-micro-os/test/CMakeLists.txt` | ✏️ 修改 | 注册测试目标、Tier-A 语料库与 Wasm 编译门禁 |
| `wink-micro-os/frameworks/esp_idf/docs/02-api-coverage-matrix.md` | ✏️ 修改 | 升级 v2.5，登记 NimBLE GAP/GATT API 矩阵 |
| `wink-micro-os/frameworks/esp_idf/docs/03-include-closure-inventory.md` | ✏️ 修改 | 升级 v1.6，登记新增蓝牙头文件 |
| `docs/implementation-plans/esp32/00-README.md` | ✏️ 修改 | 登记 M4-3 状态与索引 |

### 3.2 架构红线审计

1. 🚨 **PAL 绝对无知蓝牙与射频（[ADR-0057](../../decisions/core/0057-pal-adc-subsystem-and-channel-3-analog-contract.md)）**：严禁在 `pal/include/` 下增加任何蓝牙、GAP 或 GATT 相关头文件与符号。所有蓝牙拦截完全自闭环在 `frameworks/esp_idf/` 内部。
2. 🚨 **零运行时动态内存（[ADR-0045](../../decisions/core/0045-unified-memory-and-zero-heap-contract.md)）**：GATT 服务表、特征值槽位、描述符数组、连接描述符全部静态 BSS 分配，禁止使用 `malloc` / `free`。
3. 🚨 **合约诚实与 Fail-Loud（[ADR-0012](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)）**：当应用注册的服务树超出静态预设上限（如 >8 个服务或 >32 个特征值）时，`ble_gatts_count_cfg` 必须显式 `ESP_LOGE` 报警并返回 `BLE_HS_ENOMEM`，禁止静默截断。
4. 🚨 **通道资产归属（[ADR-0087](../../decisions/core/0087-esp-idf-asset-channels-and-soc-data-ownership.md)）**：所有手写新增头文件必须登记于 `channels.json` 的 `handwritten` 列表。
5. 🚨 **开源许可隔离（[ADR-0083/0084](../../decisions/core/0083-dual-target-compilation-and-license-boundaries.md)）**：`src/bluetooth/**` 严格为 `LGPL-3.0-only`，测试代码严格为 `GPL-3.0-only`，语料桩代码为 `CC0-1.0`。

---

## 4. 详细技术方案设计

### 4.1 C-ABI 头文件闭包（Task A）

#### 4.1.1 `host/ble_uuid.h`
定义标准 16/32/128 位 UUID 格式及声明与初始化宏：
```c
/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    BLE_UUID_TYPE_16 = 16,
    BLE_UUID_TYPE_32 = 32,
    BLE_UUID_TYPE_128 = 128,
};

typedef struct ble_uuid {
    uint8_t type;
} ble_uuid_t;

typedef struct ble_uuid16 {
    ble_uuid_t u;
    uint16_t value;
} ble_uuid16_t;

typedef struct ble_uuid32 {
    ble_uuid_t u;
    uint32_t value;
} ble_uuid32_t;

typedef struct ble_uuid128 {
    ble_uuid_t u;
    uint8_t value[16];
} ble_uuid128_t;

typedef union {
    ble_uuid_t u;
    ble_uuid16_t u16;
    ble_uuid32_t u32;
    ble_uuid128_t u128;
} ble_uuid_any_t;

#define BLE_UUID16_INIT(val) { .u = { .type = BLE_UUID_TYPE_16 }, .value = (val) }
#define BLE_UUID32_INIT(val) { .u = { .type = BLE_UUID_TYPE_32 }, .value = (val) }
#define BLE_UUID128_INIT(...) { .u = { .type = BLE_UUID_TYPE_128 }, .value = { __VA_ARGS__ } }

#define BLE_UUID16_DECLARE(val) \
    ((const ble_uuid_t *) (&(const ble_uuid16_t) BLE_UUID16_INIT(val)))

#define BLE_UUID32_DECLARE(val) \
    ((const ble_uuid_t *) (&(const ble_uuid32_t) BLE_UUID32_INIT(val)))

#define BLE_UUID128_DECLARE(...) \
    ((const ble_uuid_t *) (&(const ble_uuid128_t) BLE_UUID128_INIT(__VA_ARGS__)))

int ble_uuid_cmp(const ble_uuid_t *a, const ble_uuid_t *b);
char *ble_uuid_to_str(const ble_uuid_t *uuid, char *dst);

#ifdef __cplusplus
}
#endif
```

#### 4.1.2 `host/ble_gatt.h`
声明标准 GATT 服务树、注册上下文与访问上下文：
```c
/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include "host/ble_uuid.h"
#include "os/os_mbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_GATT_SVC_TYPE_END         0
#define BLE_GATT_SVC_TYPE_PRIMARY     1
#define BLE_GATT_SVC_TYPE_SECONDARY   2

#define BLE_GATT_CHR_F_BROADCAST      0x0001
#define BLE_GATT_CHR_F_READ           0x0002
#define BLE_GATT_CHR_F_WRITE_NO_RSP   0x0004
#define BLE_GATT_CHR_F_WRITE          0x0008
#define BLE_GATT_CHR_F_NOTIFY         0x0010
#define BLE_GATT_CHR_F_INDICATE       0x0020

#define BLE_GATT_ACCESS_OP_READ_CHR   0
#define BLE_GATT_ACCESS_OP_WRITE_CHR  1
#define BLE_GATT_ACCESS_OP_READ_DSC   2
#define BLE_GATT_ACCESS_OP_WRITE_DSC  3

#define BLE_GATT_REGISTER_OP_SVC      1
#define BLE_GATT_REGISTER_OP_CHR      2
#define BLE_GATT_REGISTER_OP_DSC      3

struct ble_gatt_access_ctxt;
struct ble_gatt_chr_def;
struct ble_gatt_dsc_def;
struct ble_gatt_svc_def;

typedef int ble_gatt_access_fn(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg);

struct ble_gatt_access_ctxt {
    uint8_t op;
    union {
        struct {
            const struct ble_gatt_chr_def *chr;
        } chr;
        struct {
            const struct ble_gatt_dsc_def *dsc;
        } dsc;
    };
    struct os_mbuf *om;
};

struct ble_gatt_register_ctxt {
    uint8_t op;
    union {
        struct {
            const struct ble_gatt_svc_def *svc;
            uint16_t handle;
        } svc;
        struct {
            const struct ble_gatt_chr_def *chr;
            uint16_t handle;
            uint16_t val_handle;
        } chr;
        struct {
            const struct ble_gatt_dsc_def *dsc;
            uint16_t handle;
        } dsc;
    };
};

typedef void ble_gatt_register_fn(struct ble_gatt_register_ctxt *ctxt, void *arg);

struct ble_gatt_dsc_def {
    const ble_uuid_t *uuid;
    uint8_t att_flags;
    uint8_t min_key_size;
    ble_gatt_access_fn *access_cb;
    void *arg;
};

struct ble_gatt_chr_def {
    const ble_uuid_t *uuid;
    ble_gatt_access_fn *access_cb;
    void *arg;
    const struct ble_gatt_dsc_def *descriptors;
    uint16_t flags;
    uint8_t min_key_size;
    uint16_t *val_handle;
};

struct ble_gatt_svc_def {
    uint8_t type;
    const ble_uuid_t *uuid;
    const struct ble_gatt_svc_def **includes;
    const struct ble_gatt_chr_def *characteristics;
};

int ble_gatts_count_cfg(const struct ble_gatt_svc_def *defs);
int ble_gatts_add_svcs(const struct ble_gatt_svc_def *svcs);
int ble_gatts_start(void);
int ble_gatts_chr_updated(uint16_t chr_def_handle);
int ble_gatts_notify(uint16_t conn_handle, uint16_t chr_val_handle);
int ble_gatts_notify_custom(uint16_t conn_handle, uint16_t val_handle, struct os_mbuf *om);

#ifdef __cplusplus
}
#endif
```

#### 4.1.3 `host/ble_gap.h`
定义广播参数、扫描响应、连接事件与描述符：
```c
/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "host/ble_uuid.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_GAP_EVENT_CONNECT        0
#define BLE_GAP_EVENT_DISCONNECT     1
#define BLE_GAP_EVENT_ADV_COMPLETE   4
#define BLE_GAP_EVENT_SUBSCRIBE      5
#define BLE_GAP_EVENT_MTU            6

typedef struct {
    uint8_t type;
    uint8_t val[6];
} ble_addr_t;

struct ble_gap_conn_desc {
    uint16_t conn_handle;
    uint16_t conn_itvl;
    uint16_t conn_latency;
    uint16_t supervision_timeout;
    uint8_t role;
    uint8_t master_id;
    ble_addr_t peer_id_addr;
    ble_addr_t peer_ota_addr;
    ble_addr_t our_id_addr;
    ble_addr_t our_ota_addr;
};

struct ble_gap_event {
    uint8_t type;
    union {
        struct {
            int status;
            struct ble_gap_conn_desc conn;
        } connect;
        struct {
            int reason;
            struct ble_gap_conn_desc conn;
        } disconnect;
        struct {
            int reason;
        } adv_complete;
        struct {
            uint16_t conn_handle;
            uint16_t attr_handle;
            uint8_t reason;
            uint8_t prev_notify:1;
            uint8_t cur_notify:1;
            uint8_t prev_indicate:1;
            uint8_t cur_indicate:1;
        } subscribe;
        struct {
            uint16_t conn_handle;
            uint16_t channel_id;
            uint16_t value;
        } mtu;
    };
};

typedef int ble_gap_event_fn(struct ble_gap_event *event, void *arg);

struct ble_gap_adv_params {
    uint8_t conn_mode;
    uint8_t disc_mode;
    uint16_t itvl_min;
    uint16_t itvl_max;
    uint8_t channel_map;
    uint8_t filter_policy;
    uint8_t high_duty_cycle:1;
};

struct ble_hs_adv_fields {
    uint8_t flags;
    const uint8_t *name;
    uint8_t name_len;
    uint8_t name_is_complete:1;
    const ble_uuid16_t *uuids16;
    uint8_t num_uuids16;
    uint8_t uuids16_is_complete:1;
    int8_t tx_pwr_lvl;
    uint8_t tx_pwr_lvl_is_present:1;
};

int ble_gap_adv_start(uint8_t own_addr_type, const ble_addr_t *direct_addr,
                      int32_t duration_ms, const struct ble_gap_adv_params *adv_params,
                      ble_gap_event_fn *cb, void *cb_arg);
int ble_gap_adv_stop(void);
int ble_gap_adv_active(void);
int ble_gap_adv_set_fields(const struct ble_hs_adv_fields *adv_fields);
int ble_gap_adv_rsp_set_fields(const struct ble_hs_adv_fields *rsp_fields);
int ble_gap_conn_find(uint16_t conn_handle, struct ble_gap_conn_desc *out_desc);

#ifdef __cplusplus
}
#endif
```

#### 4.1.4 `host/ble_hs.h` & `nimble/nimble_port.h`
声明 Host 核心配置结构、错误码、启动接口与 UniSim 前端服务树遍历导出：
```c
/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "os/os_mbuf.h"
#include "host/ble_gatt.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__EMSCRIPTEN__)
#  include <emscripten.h>
#  define WINK_SIM_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#  define WINK_SIM_EXPORT
#endif

#define BLE_HS_EAGAIN       1
#define BLE_HS_EALREADY     2
#define BLE_HS_EINVAL       3
#define BLE_HS_ENOMEM       4
#define BLE_HS_ENOTCONN     5
#define BLE_HS_ENOTSUP      6
#define BLE_HS_EAPP         7
#define BLE_HS_EDONE        8

typedef void ble_hs_sync_fn(void);
typedef void ble_hs_reset_fn(int reason);

struct ble_hs_cfg {
    ble_hs_sync_fn *sync_cb;
    ble_hs_reset_fn *reset_cb;
    ble_gatt_register_fn *gatts_register_cb;
    void *gatts_register_arg;
    void *store_status_cb;
};

extern struct ble_hs_cfg ble_hs_cfg;

int ble_hs_init(void);
int ble_hs_is_enabled(void);
int ble_hs_mbuf_to_flat(const struct os_mbuf *om, void *flat, uint16_t max_len, uint16_t *out_len);

esp_err_t nimble_port_init(void);
esp_err_t nimble_port_deinit(void);
void nimble_port_run(void);
esp_err_t nimble_port_stop(void);
void nimble_port_freertos_init(void (*host_task_fn)(void *param));
void nimble_port_freertos_deinit(void);

/* UniSim 仿真与前端交互导出类型 */
typedef struct {
    uint16_t handle;
    uint8_t  type;
    uint8_t  uuid_type;
    uint8_t  uuid_bytes[16];
} sim_ble_service_info_t;

typedef struct {
    uint16_t handle;
    uint16_t val_handle;
    uint16_t flags;
    uint8_t  uuid_type;
    uint8_t  uuid_bytes[16];
    bool     is_subscribed_notify;
    bool     is_subscribed_indicate;
} sim_ble_chr_info_t;

typedef void (*esp_nimble_sim_notify_hook_t)(uint16_t conn_handle, uint16_t attr_handle, const uint8_t *data, uint16_t len);

void esp_nimble_sim_set_notify_hook(esp_nimble_sim_notify_hook_t hook);
WINK_SIM_EXPORT int esp_nimble_sim_get_service_count(void);
WINK_SIM_EXPORT int esp_nimble_sim_get_service_info(uint8_t index, sim_ble_service_info_t *out_info);
WINK_SIM_EXPORT int esp_nimble_sim_get_char_count(uint8_t svc_index);
WINK_SIM_EXPORT int esp_nimble_sim_get_char_info(uint8_t svc_index, uint8_t chr_index, sim_ble_chr_info_t *out_info);

#ifdef __cplusplus
}
#endif
```

#### 4.1.5 `os/os_mbuf.h`
零堆轻量静态 mbuf 实现（带解包与取长功能）：
```c
/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OS_MBUF_DATA(om, type) ((type)((om)->om_data))
#define OS_MBUF_PKTLEN(om)     ((om)->om_len)

struct os_mbuf {
    uint8_t *om_data;
    uint16_t om_len;
    uint8_t om_databuf[256];
};

static inline int os_mbuf_append(struct os_mbuf *om, const void *data, uint16_t len) {
    if (!om || !data) return -1;
    if ((uint32_t)om->om_len + len > sizeof(om->om_databuf)) return -1;
    memcpy(om->om_databuf + om->om_len, data, len);
    om->om_len += len;
    om->om_data = om->om_databuf;
    return 0;
}

static inline int os_mbuf_copydata(const struct os_mbuf *om, int off, int len, void *dst) {
    if (!om || !dst || off < 0 || len < 0) return -1;
    if ((uint32_t)(off + len) > (uint32_t)om->om_len) return -1;
    memcpy(dst, om->om_databuf + off, len);
    return 0;
}

static inline void os_mbuf_free_chain(struct os_mbuf *om) {
    (void)om;
}

#ifdef __cplusplus
}
#endif
```

---

### 4.2 NimBLE 内存态零堆 GATT Server 拦截架构（Task B）

在 `wink-micro-os/frameworks/esp_idf/src/bluetooth/esp_nimble.c` 中落地全套仿真逻辑：

```text
┌────────────────────────────────────────────────────────────────────────┐
│                   ESP-IDF 应用层业务代码 (100% 原文零修改)               │
│        (bleprph / ble_gatts_add_svcs / ble_gap_adv_start)              │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ C-ABI
┌───────────────────────────────────▼────────────────────────────────────┐
│         src/bluetooth/esp_nimble.c 仿真拦截门面 (LGPL-3.0-only)        │
│                                                                        │
│  ┌───────────────────────┐  ┌───────────────────────┐                  │
│  │ 静态 GATT 服务注册表   │  │ 静态 Characteristic 表 │ (Max 32 Chrs)    │
│  │ (Max 8 Services)      │  │ (UUID, flags, handle) │                  │
│  └───────────────────────┘  └───────────────────────┘                  │
│  ┌───────────────────────┐  ┌───────────────────────┐                  │
│  │ GAP 广播/响应状态机    │  │ 虚拟单链路连接描述符   │ (conn_handle = 1)│
│  └───────────────────────┘  └───────────────────────┘                  │
│                                                                        │
│  - nimble_port_run(): 协同调度单次同步脉冲，驱动 ble_hs_cfg.sync_cb()   │
│  - ble_gatts_add_svcs(): 零堆句柄分配，驱动 gatts_register_cb 回调     │
│  - access_cb(OP_READ/WRITE): 属性权限校验、安全分发与 mbuf 解包        │
│  - ble_gatts_chr_updated() / notify_custom(): Notify/Indicate 推送流   │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ WINK_SIM_EXPORT
┌───────────────────────────────────▼────────────────────────────────────┐
│            UniSim 测试与前端桥接 API (Web / CTest / TypeScript)         │
│  - esp_nimble_sim_connect() / disconnect()                             │
│  - esp_nimble_sim_write_chr() / read_chr()                             │
│  - esp_nimble_sim_subscribe(): 内部状态置位 + 派发 GAP SUBSCRIBE 事件 │
│  - esp_nimble_sim_get_service_count/info(): 前端 GATT 树动态发现       │
│  - esp_nimble_sim_get_char_count/info(): 前端特征值动态遍历展开        │
│  - esp_nimble_sim_set_notify_hook(): 前端 Notify 实时推流钩子          │
└────────────────────────────────────────────────────────────────────────┘
```

#### 4.2.1 静态内存预算与槽位池定义（[ADR-0045](../../decisions/core/0045-unified-memory-and-zero-heap-contract.md)）

```c
#define SIM_BLE_MAX_SERVICES    8
#define SIM_BLE_MAX_CHRS        32
#define SIM_BLE_MAX_DSCS        32
#define SIM_BLE_MAX_NAME_LEN    32
#define SIM_BLE_MAX_VALUE_LEN   128

typedef struct {
    uint16_t handle;
    uint16_t val_handle;
    const ble_uuid_t *uuid;
    ble_gatt_access_fn *access_cb;
    void *arg;
    uint16_t flags;
    uint16_t *val_handle_ptr;
    uint8_t value[SIM_BLE_MAX_VALUE_LEN];
    uint16_t value_len;
    bool is_subscribed_notify;
    bool is_subscribed_indicate;
    uint8_t svc_index;
} sim_ble_chr_record_t;

typedef struct {
    uint8_t type;
    const ble_uuid_t *uuid;
    uint16_t start_handle;
    uint16_t end_handle;
} sim_ble_svc_record_t;

typedef struct {
    bool is_initialized;
    bool is_advertising;
    bool is_connected;
    char device_name[SIM_BLE_MAX_NAME_LEN];
    
    // GAP 广播与响应数据
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields adv_fields;
    struct ble_hs_adv_fields rsp_fields;
    ble_gap_event_fn *adv_cb;
    void *adv_cb_arg;
    
    // GATT 资源
    sim_ble_svc_record_t svcs[SIM_BLE_MAX_SERVICES];
    uint8_t num_svcs;
    sim_ble_chr_record_t chrs[SIM_BLE_MAX_CHRS];
    uint8_t num_chrs;
    uint16_t next_handle;
    
    // 连接
    struct ble_gap_conn_desc conn;
    
    // UniSim 外部钩子
    esp_nimble_sim_notify_hook_t notify_hook;
} sim_ble_state_t;

static sim_ble_state_t s_ble_state;
```

#### 4.2.2 核心实现逻辑与边界防御

1. **`ble_gatts_count_cfg` 与 `ble_gatts_add_svcs`（标准句柄分配与注册回调）**：
   - 遍历传入的服务数组（以 `type == 0` 终结）；
   - 对每一个 Service，遍历其 `characteristics` 数组（以 `uuid == NULL` 终结）；
   - 严格检查 `num_svcs <= SIM_BLE_MAX_SERVICES` 与 `num_chrs <= SIM_BLE_MAX_CHRS`，超出即返回 `BLE_HS_ENOMEM`；
   - **规范属性句柄分配**：每个 Service 递增分配 Service Handle；每个 Characteristic 递增分配 Declaration Handle、Value Handle 与（若有描述符）Descriptor Handle；
   - 回填应用指针：`*chr->val_handle = val_handle`；
   - **注册回调派发**：若应用配置了 `ble_hs_cfg.gatts_register_cb`，在注册每个 Service 与 Characteristic 时，构造 `struct ble_gatt_register_ctxt` 并调用该回调，完全对齐真实 NimBLE 行为。
2. **`nimble_port_run` 协同调度单次脉冲（防主线程卡死）**：
   - 在协同调度环境下，`nimble_port_run()` 不执行阻塞式的 `while (1)`；
   - 执行一次初始化同步脉冲：若已配置 `ble_hs_cfg.sync_cb`，直接调用 `ble_hs_cfg.sync_cb()` 驱动应用启动广播和初始化业务；随后平稳返回，保障 Wasm/Host 主线程不被独占。
3. **`ble_gap_adv_start` 与连接生命周期闭环**：
   - 将 `s_ble_state.is_advertising = true`，记录回调函数 `adv_cb`；
   - 当调用测试/前端注入接口 `esp_nimble_sim_connect()` 时：
     1. 停止广播（`is_advertising = false`）；
     2. 构造 `struct ble_gap_event ev = { .type = BLE_GAP_EVENT_CONNECT, .connect = { .status = 0, .conn = ... } }`；
     3. 触发应用的 `adv_cb(&ev, adv_cb_arg)`；
     4. 随后派发默认 MTU 事件 `BLE_GAP_EVENT_MTU`（`mtu = 256`）。
   - 当调用 `esp_nimble_sim_disconnect()` 时：
     1. 构造 `struct ble_gap_event ev = { .type = BLE_GAP_EVENT_DISCONNECT, .disconnect = { .reason = 0, .conn = ... } }`；
     2. 触发应用的 `adv_cb(&ev, adv_cb_arg)`；
     3. 复位连接状态 `is_connected = false`，允许应用再次调用 `ble_gap_adv_start()` 重新广播。
4. **特征值读写分发与安全解包**：
   - **读取**（`esp_nimble_sim_read_chr`）：
     - 校验特征值标志位是否包含 `BLE_GATT_CHR_F_READ`，不包含则返回 `BLE_HS_ENOTSUP`；
     - 构造 `struct os_mbuf om = {0}`；
     - 构造 `struct ble_gatt_access_ctxt ctxt = { .op = BLE_GATT_ACCESS_OP_READ_CHR, .om = &om }`；
     - 调用 `chr->access_cb(conn_handle, chr->val_handle, &ctxt, chr->arg)`；
     - 将 `om` 中产生的数据安全拷贝至输出缓冲。
   - **写入**（`esp_nimble_sim_write_chr`）：
     - 校验特征值标志位是否包含 `BLE_GATT_CHR_F_WRITE` 或 `BLE_GATT_CHR_F_WRITE_NO_RSP`，不包含则返回 `BLE_HS_ENOTSUP`；
     - 构造预填数据的 `os_mbuf`（数据写入 `om_databuf`，更新 `om_len` 与 `om_data`）；
     - 构造 `ctxt = { .op = BLE_GATT_ACCESS_OP_WRITE_CHR, .om = &om }`；
     - 调用 `chr->access_cb(...)`（应用可在回调中使用 `ble_hs_mbuf_to_flat` 或 `os_mbuf_copydata` 提取数据）；
     - 返回执行状态码。
5. **订阅联动与主动推送（Notify / Indicate）**：
   - 当调用 `esp_nimble_sim_subscribe(conn_handle, val_handle, notify, indicate)` 时：
     1. 更新特征值槽位的 `is_subscribed_notify` / `is_subscribed_indicate`；
     2. **构造并派发 `BLE_GAP_EVENT_SUBSCRIBE`** 给应用的 `adv_cb`，通知应用“客户端已订阅”，触发固件内部数据采集或定时上报逻辑；
   - 当固件主动调用 `ble_gatts_chr_updated(val_handle)` 或 `ble_gatts_notify_custom(conn_handle, val_handle, om)` 时：
     1. 校验特征值是否处于已订阅状态，未订阅则忽略或报错；
     2. 若注册了 `notify_hook`，直接向前端/测试宿主推送特征值句柄与最新数据载荷；
     3. 返回 `0`。
6. **UniSim 服务树动态发现接口实现**：
   - `esp_nimble_sim_get_service_count()`：返回当前已注册的服务数量 `s_ble_state.num_svcs`；
   - `esp_nimble_sim_get_service_info(index, out_info)`：填入该服务的起始句柄、类型、UUID；
   - `esp_nimble_sim_get_char_count(svc_index)`：统计属于该服务的特征值数量；
   - `esp_nimble_sim_get_char_info(svc_index, chr_index, out_info)`：输出特征值句柄、值句柄、UUID、权限 Flags 及当前订阅状态。

---

### 4.3 官方语料 Tier-A 验证（Task C）

选定乐鑫官方经典语料：
**`examples/bluetooth/nimble/bleprph`**
（包含外设广播、GAP 服务、自定义 GATT Heart Rate / Device Info 服务）。

- 放置于 `wink-micro-os/frameworks/esp_idf/test/corpus/bleprph/`；
- 配备轻量 `sdkconfig.h` 与 `bleprph.h` 桩文件；
- 在 `CMakeLists.txt` 中配置为 `OBJECT` 库并绑定 `-Wall -Wextra -Werror` 门禁。

---

### 4.4 单元测试矩阵（Task D）

创建 `wink-micro-os/frameworks/esp_idf/test/core/test_esp_nimble.c`，实现 **18 项全绿单测**：

| 用例 ID | 测试项 | 验证内容与断言 |
|:---|:---|:---|
| **TC-BLE-01** | Port 生命周期与 Sync 回调 | `nimble_port_init` 成功，注册 `sync_cb` 并在任务循环/脉冲中成功触发 |
| **TC-BLE-02** | GAP 设备名称读写 | `ble_svc_gap_device_name_set("Wink-BLE")` 后获取名称完全一致 |
| **TC-BLE-03** | GATT 资源静态计数与越界防护 | 注册 3 个服务通过；注册 9 个服务超过上限返回 `BLE_HS_ENOMEM` |
| **TC-BLE-04** | 服务与特征值句柄规范分配 | 注册含 2 个特征值的服务，`val_handle` 分别被正确赋值且严格单调自增 |
| **TC-BLE-05** | UUID 16-bit 与 128-bit 比较 | `ble_uuid_cmp` 正确区分相同/不同 16 位与 128 位 UUID |
| **TC-BLE-06** | GAP 广播与扫描响应流转 | `adv_start` 后 `adv_active() == 1`；设置扫描响应成功；`adv_stop` 后为 `0` |
| **TC-BLE-07** | 虚拟连接事件建立与 MTU 协商 | `esp_nimble_sim_connect` 触发 `BLE_GAP_EVENT_CONNECT` 与 `MTU=256` |
| **TC-BLE-08** | 虚拟断开连接事件与重播流转 | `esp_nimble_sim_disconnect` 触发 `BLE_GAP_EVENT_DISCONNECT` 且允许重新广播 |
| **TC-BLE-09** | GATT 特征值读回调执行 | 注入读请求，`access_cb(OP_READ_CHR)` 正确填充 mbuf 并由仿真层完整读出 |
| **TC-BLE-10** | GATT 特征值写回调与 mbuf 提取 | 注入写请求，应用回调通过 `os_mbuf_copydata` 正确解析传入的字节流 |
| **TC-BLE-11** | 特征值读写权限守卫 | 对只读特征值执行写操作返回 `BLE_HS_ENOTSUP`；只写特征值读操作被拒绝 |
| **TC-BLE-12** | 订阅与 `chr_updated` 主动推送 | 订阅 Notify 后调用 `ble_gatts_chr_updated`，`notify_hook` 成功捕获更新 |
| **TC-BLE-13** | 非法句柄防御（Fail-Loud） | 对未注册的 `0x9999` 句柄读写，返回 `BLE_HS_EINVAL` 并报错 |
| **TC-BLE-14** | 仿真符号导出保全（Wasm DCE） | 验证 `esp_nimble_sim_*` 符号带有 `WINK_SIM_EXPORT` 属性 |
| **TC-BLE-15** | `gatts_register_cb` 注册钩子通知 | 注册服务时，验证配置的回调函数被逐个 Service 和 Characteristic 调用 |
| **TC-BLE-16** | `BLE_GAP_EVENT_SUBSCRIBE` 双向事件 | 仿真注入订阅时，应用 GAP 事件回调成功收到 `SUBSCRIBE` 且 `cur_notify == 1` |
| **TC-BLE-17** | `ble_gatts_notify_custom` 数据直推 | 构造 mbuf 数据调用 `ble_gatts_notify_custom`，Hook 端完整校验数据载荷 |
| **TC-BLE-18** | UniSim 前端 GATT 树遍历发现 | 调用 `esp_nimble_sim_get_*` 接口能完整枚举已注册服务数与全部特征值元数据 |

---

## 5. 逐步实施与分阶段计划（🔴 可执行）

```mermaid
graph TD
    Step1[Step 1: C-ABI 头文件落地与 channels.json 登记]
    Step2[Step 2: esp_nimble.c 门面实现与零堆 GATT 注册池]
    Step3[Step 3: test_esp_nimble.c 单元测试与 18 项 TC 验证]
    Step4[Step 4: Tier-A 官方 bleprph 语料接入与 CMake 集成]
    Step5[Step 5: 治理门禁回归、文档更新与原子归档]

    Step1 --> Step2
    Step2 --> Step3
    Step3 --> Step4
    Step4 --> Step5
```

### 步骤 1：C-ABI 头文件闭包落地与 channels 登记
- 创建 `host/ble_hs.h`、`host/ble_uuid.h`、`host/ble_gap.h`、`host/ble_gatt.h`、`services/gap/ble_svc_gap.h`、`services/gatt/ble_svc_gatt.h`、`nimble/nimble_port.h`、`nimble/nimble_port_freertos.h`、`os/os_mbuf.h`；
- 更新 `channels.json`，运行 `python .github/scripts/check_harvested_headers.py` 确保 0 errors；
- 检查 `python .github/scripts/check_license_map.py` 确保 SPDX 声明合规。

### 步骤 2：`esp_nimble.c` 门面与静态零堆 GATT 状态机实现
- 在 `wink-micro-os/frameworks/esp_idf/src/bluetooth/esp_nimble.c` 中实现静态服务池（8 svc / 32 chr）、GAP 广播逻辑、注册通知派发、读写回调调度器与 UniSim 仿真发现接口；
- 更新 `esp_idf_sources.cmake` 将源码纳入构建。

### 步骤 3：编写单测与执行主机回归
- 创建 `wink-micro-os/frameworks/esp_idf/test/core/test_esp_nimble.c`；
- 在 `wink-micro-os/test/CMakeLists.txt` 中注册 `test_esp_nimble`；
- 运行 `ctest --test-dir build_esp_idf -R test_esp_nimble --output-on-failure` 确保 18/18 测试全部通过。

### 步骤 4：接入官方 `bleprph` 语料与 Wasm 门禁
- 导入官方例程源码至 `test/corpus/bleprph/`；
- 在 CMake 中添加 `esp_idf_corpus_bleprph_obj` 与 Wasm 编译检查；
- 运行 CTest 验证 Host GCC 与 Wasm `emcc` 零告警通过。

### 步骤 5：全量回归、文档更新与原子提交
- 运行全量 `ctest --test-dir build_esp_idf -L esp_idf` 确保 64+ 项测试全绿；
- 更新 `02-api-coverage-matrix.md`（v2.5）与 `03-include-closure-inventory.md`（v1.6）；
- 按原子提交规则分批次 Commit。

---

## 6. 风险评估与应对措施

| 风险项 | 严重级 | 应对策略 |
|:---|:---:|:---|
| **语料包含未声明的宏或复杂驱动桩** | 中 | 提取 `bleprph.h` 并在桩目录提供最小合法结构体与宏定义，不污染全局头文件 |
| **`os_mbuf` 宏展开与类型强转引发 Clang 告警** | 低 | 编写类型安全的内联辅助函数与宏，统一通过 `-Wall -Wextra -Werror` 校验 |
| **GATT 递归服务解析导致栈溢出** | 低 | 采用平面数组遍历算法，最大调用深度限制为 1，杜绝深层递归 |
| **协同调度下 `nimble_port_run` 阻塞** | 高 | 严格执行单次同步脉冲规约，调用 `sync_cb` 后立即返回，杜绝死循环 |

---

## 7. 结项声明与签署

- ✅ **Step 1: C-ABI 头文件落地与 channels.json 登记**：9 个手写头文件已落地，`channels.json` 登记完毕，`check_harvested_headers.py` 与 `check_license_map.py` 门禁 0 error 完美通过；
- ✅ **Step 2: esp_nimble.c 门面与静态零堆 GATT 注册池**：静态服务/特征值/描述符池完成，GAP 广播状态机、单链路虚拟连接、非阻塞单次协同脉冲、订阅推流闭环落地，纳入 `esp_idf_sources.cmake`；
- ✅ **Step 3: test_esp_nimble.c 单元测试与 18 项 TC 验证**：18/18 单元测试全部 PASS，覆盖率 100%；
- ✅ **Step 4: Tier-A 官方 bleprph 语料接入与 CMake 集成**：官方语料 `main.c` + `gatt_svr.c` 0 warning 构建通过，Wasm compile checks 全绿；
- ✅ **Step 5: 治理门禁回归、文档更新与原子归档**：
  - `ctest -L esp_idf` 65/65 全绿通过（原 60 项 + 新增 5 项，零破坏零回归）；
  - `02-api-coverage-matrix.md`（v2.5）与 `03-include-closure-inventory.md`（v1.6）更新就绪；
  - `winkcli lint --pack layering --pack api` 0 findings；
  - 本计划正式升至 **v1.2（验收结项版）**，宣告 M4-3 圆满结项！
