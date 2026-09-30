<!-- SPDX-License-Identifier: Apache-2.0 -->
# 实施计划：ESP-IDF NimBLE 蓝牙栈虚拟对端驱动器与多连接池重构战役 (v1.0)

| 字段 | 内容 |
|---|---|
| 计划编号 | PLAN-20260930-ESP-IDF-NIMBLE-BLE-PEER-AND-MULTICONN-v1.0 |
| 状态 | 📝 **Ready for Review（等待用户与架构评审确认）** |
| 日期 | 2026-09-30 |
| 周期估算 | 4~5 个工作日（分 Step 1 ~ 5 递进执行） |
| 优先次序 | **场景契约与治理定义 → 虚拟对端驱动引擎 → 原厂 C-ABI 门面纠偏 → 静态防腐门禁 → 官方示例无头实证** |
| 决策依据 | [ADR-0012：契约诚实优于静默降级（Fail-Loud 原则）](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)<br>[ADR-0002：双 Target 同源编译原则](../../decisions/unisim/0002-dual-target-compilation.md)<br>[ADR-0003：生产口径与保真边界约束（永不承诺虚实恒等）](../../decisions/unisim/0003-simulation-fidelity-boundary.md)<br>[ADR-0004：编译期静态分发优于运行时函数指针](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md)<br>[ADR-0045：仿真内存配额与异常故障策略](../../decisions/unisim/0045-simulation-memory-quota-and-fault-policy.md)<br>[ADR-0053：虚拟时间因果同刻总序仲裁模型](../../decisions/unisim/0053-sim-same-timestamp-event-total-order.md)<br>[ADR-0089：分类记账堆内存与边界防御模型](../../decisions/core/0089-esp-idf-heap-caps-allocation-contract.md)<br>[ADR-0091：多配置实例与五维正交 Schema 架构决策](../../decisions/unisim/0091-esp-idf-multi-config-orthogonal-schema.md)<br>[ADR-0092：ESP-IDF 官方示例仿真治理前置筑基宪章](../../decisions/unisim/0092-esp-idf-simulation-governance-and-capability-charter.md) |
| 管辖数据源 | [`checklist.data.json`](../../../wink-micro-app/vendor/esp_idfv61/.governance/data/checklist.data.json)、[`capability-catalog.yaml`](../../../wink-micro-app/vendor/esp_idfv61/.governance/catalog/capability-catalog.yaml)、[`.governance/gates/`](../../../wink-micro-app/vendor/esp_idfv61/.governance/gates/) |
| 实施目标文件 | `frameworks/esp_idf/src/bluetooth/*`、`frameworks/esp_idf/include/nimble/*`、`host/*`、`services/*`、`.governance/gates/rules/g5_*.py` |
| 验收门禁 | `ctest -L esp_idf`、`python .governance/gates/run_gates.py --mode nightly`、`powershell run_esp32_headless_evidence.ps1 -App bleprph` |

---

## 一、 战略总目标与全局验收标准 (DoD)

### 1.1 现状痛点与重构要旨
当前 `wink-micro-os/frameworks/esp_idf/src/bluetooth/esp_nimble.c` 实现了基础的 GATT 服务注册与单例广播，但存在明显的“单例受限与被动单向”瓶颈：
1. **死板单例连接（`conn_handle=1`）**：全局仅维护一个 `is_connected` 标记，连接句柄永远写死为 `1`。无法模拟真实物联网网关/中继场景下的“多设备并发接入（如同时连接 3 个传感器从机）”；
2. **缺乏外部对端行为驱动（Virtual Peer Phone）**：蓝牙开发是高度非对称的交互过程。当前代码仅支持单元测试在本地调用 C 辅助函数修改值，无法在 `.scenario.json` 声明式剧本中模拟“手机端在指定虚拟时间主动发起 Connect、发起 MTU 交换、主动 Write Characteristic、订阅 Notify”；
3. **主动扫描（Central / Observer）断层**：`ble_gap_disc()`（空口扫描）缺少虚拟信标支持，无法跑通任何以 ESP32 为主机扫描周边 iBeacon 或外设的官方用例（如 `blecent`）；
4. **权限与安全校验缺失**：GATT 特征值读写未校验 `BLE_GATT_CHR_F_READ` / `BLE_GATT_CHR_F_WRITE` 权限，对未授权操作无法如实返回 `BLE_ATT_ERR_READ_NOT_PERMITTED` 等原生错误码。

**重构核心要旨**：
将蓝牙仿真升级为**“多连接槽位池 + 虚拟对端（Virtual Peer）事件驱动引擎”**。应用层原厂 NimBLE C-ABI 保持 100% 稳定，底层通过 `INJECT_BLE_PEER_ACTION` 标准 step 模拟外部 Central/Phone 的交互动作，精准派发原生 `ble_gap_event` 与 `ble_gatt_access_fn`，彻底实现蓝牙应用全双向、多连接的高保真确定性闭环。

### 1.2 全局验收标准 (DoD)
- [ ] **G-01（支持真正的并发多连接）**：实现最大 4 个并发连接槽位管理（`conn_handle 1..4`），支持多连接独立鉴权、独立 MTU 协商与独立断开事件（`BLE_GAP_EVENT_DISCONNECT`）；
- [ ] **G-02（统一虚拟对端交互契约）**：支持标准 `INJECT_BLE_PEER_ACTION` step，涵盖 `CONNECT`、`DISCONNECT`、`EXCHANGE_MTU`、`READ_CHR`、`WRITE_CHR`、`SUBSCRIBE_NOTIFY` 六大核心对端动作；
- [ ] **G-03（全双向 GATT 权限与事件闭环）**：
  - 对端写特征值 $\to$ 正确触发用户注册的 `ble_gatt_access_fn`（`BLE_GATT_ACCESS_OP_WRITE_CHR`）；
  - 业务调用 `ble_gatts_chr_updated` / `ble_gatts_notify` $\to$ 正确捕获并验证对端订阅状态；
  - 越权读写未开放标志的特征值 $\to$ 准确返回 `BLE_ATT_ERR_READ_NOT_PERMITTED` / `BLE_ATT_ERR_WRITE_NOT_PERMITTED`；
- [ ] **G-04（空口扫描与虚拟信标池）**：支持在场景中声明虚拟广播信标池（Virtual Air Beacons），支持 `ble_gap_disc()` 主动扫描与持续派发 `BLE_GAP_EVENT_DISC` 事件；
- [ ] **G-05（代际取消与防内存悬挂安全）**：连接断开或 deinit 时递增连接代际号，安全丢弃对端或本端排队的延时通知，杜绝空指针解引用与内存泄漏；
- [ ] **G-06（静态防腐与反异变门禁）**：Gate 5 增加规则阻断在 C 代码中写死 peer 行为或特定业务 UUID；反异变测试验证篡改对端 payload 必报红灯；
- [ ] **G-07（双 Target 编译与门禁全绿）**：Host Native 与 Wasm32 同源编译通过，`test_esp_nimble.c` 20+ 项测试全绿，`bleprph` 官方 carrier 示例无头实证毫秒级确定性通过。

---

## 二、 核心架构设计方案

### 2.1 架构分层与数据流图

```
+─────────────────────────────────────────────────────────────────────────────────────────────+
│                        ESP-IDF 跨靶仿真 NimBLE 蓝牙栈双向交互架构                            │
+─────────────────────────────────────────────────────────────────────────────────────────────+
│  [应用业务层]  原生业务代码 / CHECKLIST 官方示例 (bleprph 从机 / blecent 主机 / iBeacon)     │
+─────────────────────────────────────────────────────────────────────────────────────────────+
│  [原厂 C-ABI]  frameworks/esp_idf/src/bluetooth/esp_nimble.c                                │
│                ├─ GAP 接口 (ble_gap_adv_start / ble_gap_connect / ble_gap_disc)             │
│                ├─ GATT 接口 (ble_gatts_count_cfg / ble_gatts_add_svcs / ble_gatts_notify)   │
│                └─ 回调泵 (ble_gap_event_fn 回调分发 / ble_gatt_access_fn 读写驱动)          │
+─────────────────────────────────────────────────────────────────────────────────────────────+
│                                              │
│                                              ▼
│  [虚拟对端层]  sim_ble_peer (声明式虚拟蓝牙对端与空口引擎)                                    │
│                ├─ 多连接槽位池 (支持 conn_handle 1..4 并发跟踪、MTU、加密、对端地址)        │
│                ├─ 虚拟广播信道池 (Virtual Air Beacons，支持扫描检索与过滤)                   │
│                ├─ 属性权限仲裁器 (Read/Write/Notify 权限匹配，标准 ATT 错误码生成)           │
│                └─ 对端动作执行器 (响应 scenario.json 中的对端动作，驱动 GAP/GATT 回调)       │
+─────────────────────────────────────────────────────────────────────────────────────────────+
                                               ▲
                                               │ (场景剧本注入)
+─────────────────────────────────────────────────────────────────────────────────────────────+
│  [场景剧本层]  unisim-scenarios/<app>.scenario.json                                          │
│                └─ steps: [ { "type": "INJECT_BLE_PEER_ACTION", "action": "WRITE_CHR" } ]     │
+─────────────────────────────────────────────────────────────────────────────────────────────+
```

### 2.2 统一场景注入链路契约 (`scenario.json`)

在 `unisim-scenarios/<app>.scenario.json` 中引入标准 `INJECT_BLE_PEER_ACTION` 与 `INJECT_BLE_BEACON`：

```json
{
  "header": {
    "version": "1.0.0",
    "name": "ESP-IDF v6.1 NimBLE Multi-Connection & Peer Interaction Proof",
    "templateId": "esp_idfv61_bleprph",
    "accuracyMode": "behavioral",
    "timeoutUs": "3000000",
    "failurePolicy": "fail-fast",
    "determinism": { "prngSeed": 42 }
  },
  "steps": [
    {
      "type": "INJECT_BLE_PEER_ACTION",
      "timeUs": "200000µs",
      "connHandle": 1,
      "action": "CONNECT",
      "peerAddress": "11:22:33:44:55:66",
      "initialMtu": 256
    },
    {
      "type": "INJECT_BLE_PEER_ACTION",
      "timeUs": "500000µs",
      "connHandle": 1,
      "action": "SUBSCRIBE_NOTIFY",
      "charUuid": "2A37",
      "enable": true
    },
    {
      "type": "INJECT_BLE_PEER_ACTION",
      "timeUs": "800000µs",
      "connHandle": 1,
      "action": "WRITE_CHR",
      "charUuid": "DEAD",
      "payloadHex": "01020304"
    },
    {
      "type": "INJECT_BLE_PEER_ACTION",
      "timeUs": "1500000µs",
      "connHandle": 2,
      "action": "CONNECT",
      "peerAddress": "AA:BB:CC:DD:EE:FF",
      "initialMtu": 128
    },
    {
      "type": "ASSERT_POINT",
      "timeUs": "2000000µs",
      "target": "power:VCC_3V3",
      "condition": "equals",
      "expected": 3.3
    }
  ]
}
```

### 2.3 核心数据结构与 C-ABI 抽象 (`sim_ble_peer.h`)

严格遵从无动态堆分配准则（ADR-0045 / ADR-0089），定长静态槽位管理：

```c
/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"

#define SIM_BLE_MAX_CONNS       4
#define SIM_BLE_MAX_BEACONS     8
#define SIM_BLE_MAX_PAYLOAD_LEN 256

/* 对端动作枚举 */
typedef enum {
    SIM_BLE_ACTION_CONNECT = 0,
    SIM_BLE_ACTION_DISCONNECT,
    SIM_BLE_ACTION_EXCHANGE_MTU,
    SIM_BLE_ACTION_READ_CHR,
    SIM_BLE_ACTION_WRITE_CHR,
    SIM_BLE_ACTION_SUBSCRIBE
} sim_ble_peer_action_t;

/* 单个连接槽位 */
typedef struct {
    uint16_t conn_handle;
    bool is_active;
    ble_addr_t peer_addr;
    uint16_t mtu;
    bool encrypted;
    uint32_t generation;
} sim_ble_conn_slot_t;

/* 虚拟信标条目（支撑扫描） */
typedef struct {
    bool in_use;
    ble_addr_t addr;
    int8_t rssi;
    uint8_t adv_data[31];
    uint8_t adv_data_len;
} sim_ble_beacon_entry_t;

/* 虚拟对端全局管理引擎 */
typedef struct {
    sim_ble_conn_slot_t conns[SIM_BLE_MAX_CONNS];
    uint8_t active_conn_count;
    sim_ble_beacon_entry_t beacons[SIM_BLE_MAX_BEACONS];
    uint8_t beacon_count;
} sim_ble_peer_ctx_t;

/* C 接口声明 */
void sim_ble_peer_reset(void);
esp_err_t sim_ble_peer_inject_connect(uint16_t conn_handle, const ble_addr_t *peer_addr, uint16_t mtu);
esp_err_t sim_ble_peer_inject_disconnect(uint16_t conn_handle, uint8_t reason);
esp_err_t sim_ble_peer_inject_write(uint16_t conn_handle, uint16_t attr_handle, const uint8_t *data, uint16_t len);
esp_err_t sim_ble_peer_inject_subscribe(uint16_t conn_handle, uint16_t val_handle, bool notify, bool indicate);
esp_err_t sim_ble_peer_register_beacon(const ble_addr_t *addr, int8_t rssi, const uint8_t *adv_data, uint8_t len);
```

### 2.4 原厂 C-ABI 门面行为与状态机模型

1. **连接与多连接管理**：
   - 当收到对端 `CONNECT` 动作时，在 `conns` 槽位中寻找空闲位；
   - 生成对应的 `ble_gap_event`（`type = BLE_GAP_EVENT_CONNECT`，填充 `conn_handle`、`peer_addr` 等）；
   - 调用业务注册的全局 GAP 事件回调 `s_nimble.adv_cb`；
2. **对端读写特征值驱动**：
   - 对端发起 `WRITE_CHR` 动作时，检索注册的特征值结构；
   - 检查权限标志（必须包含 `BLE_GATT_CHR_F_WRITE` 或 `BLE_GATT_CHR_F_WRITE_NO_RSP`）；
   - 构造 `struct ble_gatt_access_ctxt`，直接驱动特征值定义中用户绑定的 `access_cb(conn_handle, attr_handle, ctxt, arg)`；
3. **空口扫描与发现**：
   - 业务调用 `ble_gap_disc(discovery_params, cb, cb_arg)`；
   - 虚拟对端引擎遍历 `beacons` 数组，在每次步进时向 `cb` 派发 `BLE_GAP_EVENT_DISC` 事件；
   - 业务能够完整解析广播包中的 Flags、Name、16/128-bit Service UUIDs。

---

## 三、 任务拆分与执行路线图 (WBS)

```
┌────────────────────────────────────────────────────────────────────────┐
│                        NimBLE 蓝牙战役实施路线图                        │
├────────────────────────────────────────────────────────────────────────┤
│ 【阶段一：治理图谱与契约规整】扩展 capability-catalog 与 Schema 规范   │
│       │                                                                │
│       ▼                                                                │
│ 【阶段二：虚拟对端驱动引擎】实现 sim_ble_peer.h/.c 与多连接槽位池      │
│       │                                                                │
│       ▼                                                                │
│ 【阶段三：门面双向重构与 ABI 纠偏】重写 esp_nimble.c 权限与扫描模块    │
│       │                                                                │
│       ▼                                                                │
│ 【阶段四：门禁强化与反异变防御】扩展 Gate 5 (Rule 505/506) 与测试套件  │
│       │                                                                │
│       ▼                                                                │
│ 【阶段五：官方用例实证与全量回归】bleprph/blecent 无头实证与 CTest 闭环 │
└────────────────────────────────────────────────────────────────────────┘
```

### 阶段一：治理图谱与契约规整 (Governance & Schema Alignment)
- [ ] **任务 T1.1**：在 [`capability-catalog.yaml`](../../../wink-micro-app/vendor/esp_idfv61/.governance/catalog/capability-catalog.yaml) 中追加并规范能力定义：
  - `cap.ble.nimble_peripheral_mock`（layer: facade, status: implemented, owned_paths: `frameworks/esp_idf/src/bluetooth/esp_nimble.c`）
  - `cap.ble.nimble_central_mock`（layer: facade, status: implemented, owned_paths: `frameworks/esp_idf/src/bluetooth/esp_nimble.c`）
  - `cap.ble.nimble_multiconn`（layer: facade, status: implemented, owned_paths: `frameworks/esp_idf/src/bluetooth/esp_nimble.c`）
- [ ] **任务 T1.2**：在 `checklist.data.json` 中审视并更新所有蓝牙关联条目（如 `#010 bleprph`、`#011 blecent`）的 `required_capabilities`；
- [ ] **任务 T1.3**：在 `PLAYBOOK.md` 中增补 `INJECT_BLE_PEER_ACTION` 与 `INJECT_BLE_BEACON` 的标准 step 规范与字段校验逻辑。

### 阶段二：虚拟对端驱动引擎 (Virtual Peer Core)
- [ ] **任务 T2.1**：在 `frameworks/esp_idf/src/bluetooth/` 下实现 `sim_ble_peer.h` 与 `sim_ble_peer.c`：
  - 实现定长 4-连接槽位管理池（支持独立的代际编号与 MTU 跟踪）；
  - 实现对端动作分发器（`CONNECT`, `DISCONNECT`, `WRITE_CHR`, `SUBSCRIBE`）；
  - 实现虚拟广播信标池（支持信标注册与扫描枚举）；
- [ ] **任务 T2.2**：在 `frameworks/esp_idf/include/` 中导出公共测试/驱动桩头 `sim_ble_peer.h`，并同步在 `channels.json` 的 `handwritten` 清单中完成资产登记（符合 ADR-0087）。

### 阶段三：门面双向重构与 ABI 纠偏 (Facade & ABI Alignment)
- [ ] **任务 T3.1**：重构 `frameworks/esp_idf/src/bluetooth/esp_nimble.c`：
  - 接入 `sim_ble_peer`，移除死板的全局单例 `is_connected`，绑定至具体的 `conn_handle`；
  - 增强 `ble_gap_disc()` 实现，接入虚拟信标池，派发 `BLE_GAP_EVENT_DISC`；
  - 特征值读写增加标志位权限校验（`BLE_GATT_CHR_F_READ / WRITE`），越权精确返回标准 ATT 错误码；
  - 完善 `ble_gatts_notify_custom()` 与 `ble_gatts_indicate()` 的对端订阅状态校验。

### 阶段四：门禁强化与反异变防御 (Gate 5 & Mutation Tests)
- [ ] **任务 T4.1**：在 `.governance/gates/rules/` 下新增 `g5_no_ble_inline_mock.py`（Rule 505）：
  - 静态 AST 检查：严禁在 `src/bluetooth/` 中写死 peer 动作或针对特定业务 UUID 的硬编码判断；
- [ ] **任务 T4.2**：在 `.governance/gates/rules/` 下新增 `g5_ble_assertion_quality.py`（Rule 506）：
  - 检查蓝牙场景剧本的断言深度与有效性；
- [ ] **任务 T4.3**：编写反异变单元测试：篡改对端写入特征值的 Payload 预期，验证断言能够 100% 触发红灯拦截。

### 阶段五：用例矩阵实证与全量回归 (Verification & Sign-off)
- [ ] **任务 T5.1**：为 `bluetooth/nimble/bleprph` 编写增强的 `bleprph.scenario.json`（涵盖对端连接、订阅心率通知与特征值写入）；
- [ ] **任务 T5.2**：更新 `test_esp_nimble.c` 核心测试套件，新增多从机并发、主动扫描发现、非法权限拒绝等测试用例；
- [ ] **任务 T5.3**：执行 `ctest -L esp_idf` 与 `run_gates.py --mode nightly` 达成全绿；通过 `run_esp32_headless_evidence.ps1` 输出官方示例无头确定性实据。

---

## 四、 风险分析与规避对策

| 风险场景 | 风险等级 | 诱发原因 | 预防与规避对策 |
|---|:---:|---|---|
| **R-1：NimBLE 多任务线程死锁** | 🟡 中 | 原厂 NimBLE 在 `nimble_port_run()` 中起专用线程，若对端注入直接同步调用可能产生死锁 | 所有对端动作注入必须通过内部轻量 FIFO 投递至 NimBLE 事件处理上下文，由事件循环统一派发，保证线程模型与原厂完全一致 |
| **R-2：MBUF 链表内存泄漏** | 🟡 中 | 构造 `os_mbuf` 传递载荷给 `access_cb` 时若未释放会导致泄漏 | 使用只读零拷贝内存切片包装器，并在 `access_cb` 返回后立即重置复用内部静态临时缓冲 |
| **R-3：多连接并发句柄竞争** | 🟡 中 | 同时存在多个连接时，上层如果以固定 `1` 传递会误操作其他连接 | 门面接口严格校验 `conn_handle` 的有效性与代际匹配，非法句柄直接返回 `BLE_HS_ENOTCONN` |

---

## 五、 结论与后续衔接

完成本计划后，ESP-IDF 的 NimBLE 蓝牙栈将**从被动单例的初级状态，飞跃至支持双向交互、并发多连接与主动扫描的工业级协议栈**。这不仅能完美支撑智能穿戴、工业传感器从机、网关主机等多样化蓝牙应用的同源运行，更为后续建立多设备协同网络仿真奠定了坚实的底层架构。
