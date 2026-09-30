<!-- SPDX-License-Identifier: Apache-2.0 -->
# 实施计划：ESP-IDF NimBLE 蓝牙栈虚拟对端驱动器与多连接池重构战役 (v2.0 终审修订版)

| 字段 | 内容 |
|---|---|
| 计划编号 | PLAN-20260930-ESP-IDF-NIMBLE-BLE-PEER-AND-MULTICONN-v2.0 |
| 状态 | 📝 **Ready for Execution（吸收深度白盒专项评审意见后的修订终版）** |
| 日期 | 2026-09-30 |
| 周期估算 | 6~7 个工作日（分 Batch 1 ~ 3 阶段递进执行） |
| 优先次序 | **T0 语义注入与观测链路 → 多连接槽位池与代际仲裁 → 双向 GATT 权限与对端驱动 → 空口主动扫描子系统 → 门禁与无头实证** |
| 决策依据 | [ADR-0012：契约诚实优于静默降级（Fail-Loud 原则）](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)<br>[ADR-0002：双 Target 同源编译原则](../../decisions/unisim/0002-dual-target-compilation.md)<br>[ADR-0003：生产口径与保真边界约束（永不承诺虚实恒等）](../../decisions/unisim/0003-simulation-fidelity-boundary.md)<br>[ADR-0004：编译期静态分发优于运行时函数指针](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md)<br>[ADR-0045：仿真内存配额与异常故障策略](../../decisions/unisim/0045-simulation-memory-quota-and-fault-policy.md)<br>[ADR-0053：虚拟时间因果同刻总序仲裁模型](../../decisions/unisim/0053-sim-same-timestamp-event-total-order.md)<br>[ADR-0089：分类记账堆内存与边界防御模型](../../decisions/core/0089-esp-idf-heap-caps-allocation-contract.md)<br>[ADR-0091：多配置实例与五维正交 Schema架构决策](../../decisions/unisim/0091-esp-idf-multi-config-orthogonal-schema.md)<br>[ADR-0092：ESP-IDF 官方示例仿真治理前置筑基宪章](../../decisions/unisim/0092-esp-idf-simulation-governance-and-capability-charter.md) |
| 管辖数据源 | [`checklist.data.json`](../../../wink-micro-app/vendor/esp_idfv61/.governance/data/checklist.data.json)、[`capability-catalog.yaml`](../../../wink-micro-app/vendor/esp_idfv61/.governance/catalog/capability-catalog.yaml)、[`.governance/gates/`](../../../wink-micro-app/vendor/esp_idfv61/.governance/gates/) |
| 实施目标文件 | `frameworks/esp_idf/src/bluetooth/*`、`frameworks/esp_idf/include/nimble/*`、`host/*`、`services/*`、`.governance/gates/rules/g5_*.py` |
| 验收门禁 | `ctest -L esp_idf`、`python .governance/gates/run_gates.py --mode nightly`、`powershell run_esp32_headless_evidence.ps1 -App bleprph` |

---

## 一、 战略总目标与修订后全局验收标准 (DoD)

### 1.1 评审纠偏核心要旨
依据架构组白盒精审准则，对 NimBLE 蓝牙栈仿真进行了体系化收紧与关键纠偏：

1. **增设跨仓 T0 注入与观测基建，矫正语法规范**：
   - 彻底修复原示例中微秒单位错误（`200000µs` 违反 `time-types.ts` 正则，统一收敛为 `us/ms/s` 标准时间字面量）；
   - 废除以“供电电压”作为断言的无效验证，增加针对 `ble:gap:conn_count`（活跃连接数）与 `ble:gatt:notify_count`（通知计数）的语义观测断言；
   - 明确用例退出后的无残留清理：通过 `sim_ble_peer_reset()` 释放所有虚拟连接与属性订阅；
2. **多连接槽位池与严格代际（Generation Token）仲裁**：
   - 废除死板的全局单例 `is_connected` 与写死 `conn_handle=1`；
   - 建立 4 槽位定容连接池（`conn_handle 1..4`），支持多连接独立 MTU 协商、独立地址与独立断开事件；
   - 每个连接槽位分配独立代际号（`generation`），连接断开后立即作废在途通知与对端延时操作，彻底消除悬挂指针与越权回调；
3. **分批递进式交付，确保全双向保真**：
   - **第一批（多连接池与对端连接/断开/MTU）**；
   - **第二批（全双向 GATT 权限检查与对端读/写/订阅）**：支持 `BLE_GATT_CHR_F_READ / WRITE / NOTIFY` 权限判定，未开放标志严格返回标准 `BLE_ATT_ERR_READ_NOT_PERMITTED`；
   - **第三批（空口主动扫描与虚拟信标池）**：落地 `ble_gap_disc()`，支持以 ESP32 为主机扫描环境信标，支撑 `blecent` 官方示例；
4. **校准治理证据与反异变测试**：
   - 针对 `checklist.data.json` 严格分列 `wasm_node` 证据通道，杜绝无头偷换浏览器；
   - 部署反异变测试：人为篡改对端写入特征值的 Payload 预期或篡改连接数量，验证断言 100% 变红报警。

### 1.2 全局验收标准 (DoD)
- [ ] **G-01（支持真正的并发多连接）**：实现最大 4 个并发连接槽位管理（`conn_handle 1..4`），支持多连接独立鉴权、独立 MTU 协商与独立断开事件（`BLE_GAP_EVENT_DISCONNECT`）；
- [ ] **G-02（统一虚拟对端交互契约）**：标准时间格式（`us/ms/s`），支持标准 `INJECT_BLE_PEER_ACTION` step，涵盖 `CONNECT`、`DISCONNECT`、`EXCHANGE_MTU`、`READ_CHR`、`WRITE_CHR`、`SUBSCRIBE_NOTIFY`；
- [ ] **G-03（全双向 GATT 权限与事件闭环）**：
  - 对端写特征值 $\to$ 正确触发用户注册的 `ble_gatt_access_fn`（`BLE_GATT_ACCESS_OP_WRITE_CHR`）；
  - 业务调用 `ble_gatts_chr_updated` / `ble_gatts_notify` $\to$ 正确捕获并验证对端订阅状态；
  - 越权读写未开放标志的特征值 $\to$ 准确返回 `BLE_ATT_ERR_READ_NOT_PERMITTED` / `BLE_ATT_ERR_WRITE_NOT_PERMITTED`；
- [ ] **G-04（空口主动扫描子系统）**：支持在场景中声明虚拟广播信标池（Virtual Air Beacons），支持 `ble_gap_disc()` 主动扫描与持续派发 `BLE_GAP_EVENT_DISC` 事件；
- [ ] **G-05（代际取消与防内存悬挂安全）**：连接断开或 deinit 时递增连接代际号，安全丢弃对端或本端排队的延时通知，杜绝空指针解引用与内存泄漏；
- [ ] **G-06（静态防腐与反异变门禁）**：Gate 5 规则 505 阻断在 C 代码中写死 peer 行为或特定业务 UUID；反异变测试验证篡改对端 payload 必报红灯；
- [ ] **G-07（双 Target 编译与门禁全绿）**：Host Native 与 Wasm32 同源编译通过，`test_esp_nimble.c` 20+ 项测试全绿，`bleprph` 官方 carrier 示例无头实证毫秒级确定性通过。

---

## 二、 核心架构设计方案 (修订版)

### 2.1 架构分层与数据流图

```
+─────────────────────────────────────────────────────────────────────────────────────────────+
│                       ESP-IDF NimBLE 蓝牙栈双向交互架构 (v2.0 修订版)                         │
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
│                ├─ 代际令牌控制器 (generation 仲裁，防止迟到事件污染)                         │
│                ├─ 虚拟广播信道池 (Virtual Air Beacons，支持扫描检索与过滤)                   │
│                ├─ 属性权限仲裁器 (Read/Write/Notify 权限匹配，标准 ATT 错误码生成)           │
│                └─ 对端动作执行器 (响应 scenario.json 中的对端动作，驱动 GAP/GATT 回调)       │
+─────────────────────────────────────────────────────────────────────────────────────────────+
                                               ▲
                                               │ (Wasm C-ABI 注入链路)
+─────────────────────────────────────────────────────────────────────────────────────────────+
│  [场景剧本层]  unisim-scenarios/<app>.scenario.json                                          │
│                ├─ steps: [ { "type": "INJECT_BLE_PEER_ACTION", "action": "WRITE_CHR" } ]     │
│                ├─ 语义信号观测: ble:gap:conn_count / ble:gatt:notify_count                   │
│                └─ 复位钩子: sim_ble_peer_reset()                                             │
+─────────────────────────────────────────────────────────────────────────────────────────────+
```

### 2.2 统一场景注入链路契约 (`scenario.json`)

```json
{
  "header": {
    "version": "1.0.0",
    "name": "ESP-IDF v6.1 NimBLE Multi-Connection & Peer Interaction Proof",
    "templateId": "esp_idfv61_bleprph",
    "accuracyMode": "behavioral",
    "timeoutUs": "3000ms",
    "failurePolicy": "fail-fast",
    "determinism": { "prngSeed": 42 }
  },
  "steps": [
    {
      "type": "INJECT_BLE_PEER_ACTION",
      "timeUs": "200ms",
      "connHandle": 1,
      "action": "CONNECT",
      "peerAddress": "11:22:33:44:55:66",
      "initialMtu": 256
    },
    {
      "type": "ASSERT_POINT",
      "timeUs": "300ms",
      "target": "ble:gap:conn_count",
      "condition": "equals",
      "expected": 1
    },
    {
      "type": "INJECT_BLE_PEER_ACTION",
      "timeUs": "500ms",
      "connHandle": 1,
      "action": "SUBSCRIBE_NOTIFY",
      "charUuid": "2A37",
      "enable": true
    },
    {
      "type": "INJECT_BLE_PEER_ACTION",
      "timeUs": "800ms",
      "connHandle": 1,
      "action": "WRITE_CHR",
      "charUuid": "DEAD",
      "payloadHex": "01020304"
    },
    {
      "type": "INJECT_BLE_PEER_ACTION",
      "timeUs": "1500ms",
      "connHandle": 2,
      "action": "CONNECT",
      "peerAddress": "AA:BB:CC:DD:EE:FF",
      "initialMtu": 128
    },
    {
      "type": "ASSERT_POINT",
      "timeUs": "1600ms",
      "target": "ble:gap:conn_count",
      "condition": "equals",
      "expected": 2
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
    uint32_t generation;                    /* 代际编号，消除迟到通知 */
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
    uint32_t total_notify_dispatched;
} sim_ble_peer_ctx_t;

/* C 接口声明 */
void sim_ble_peer_reset(void);
esp_err_t sim_ble_peer_inject_connect(uint16_t conn_handle, const ble_addr_t *peer_addr, uint16_t mtu);
esp_err_t sim_ble_peer_inject_disconnect(uint16_t conn_handle, uint8_t reason);
esp_err_t sim_ble_peer_inject_write(uint16_t conn_handle, uint16_t attr_handle, const uint8_t *data, uint16_t len);
esp_err_t sim_ble_peer_inject_subscribe(uint16_t conn_handle, uint16_t val_handle, bool notify, bool indicate);
esp_err_t sim_ble_peer_register_beacon(const ble_addr_t *addr, int8_t rssi, const uint8_t *adv_data, uint8_t len);
uint8_t sim_ble_peer_get_conn_count(void);
uint32_t sim_ble_peer_get_notify_count(void);
```

---

## 三、 任务拆分与执行路线图 (WBS 修订版)

```
┌────────────────────────────────────────────────────────────────────────┐
│                        NimBLE 蓝牙战役执行路线图                        │
├────────────────────────────────────────────────────────────────────────┤
│ 【阶段 0：T0 注入与观测基建】规范时间语法与暴露 ble:gap 语义探针         │
│       │                                                                │
│       ▼                                                                │
│ 【阶段 1：第一批交付 (多连接)】实现 sim_ble_peer.h/.c 与代际槽位池      │
│       │                                                                │
│       ▼                                                                │
│ 【阶段 2：第二批交付 (GATT 双向)】权限校验、对端读写驱动与通知捕获     │
│       │                                                                │
│       ▼                                                                │
│ 【阶段 3：第三批交付 (Scan)】空口主动扫描与虚拟 Beacon 信标池          │
│       │                                                                │
│       ▼                                                                │
│ 【阶段 4：门禁强化与反异变防御】Gate 5 规则 505/506 与测试套件          │
│       │                                                                │
│       ▼                                                                │
│ 【阶段 5：全量实证与无头闭环】bleprph/blecent 确定性运行与 CTest 全绿   │
└────────────────────────────────────────────────────────────────────────┘
```

### 阶段 0：T0 注入与语义观测基建 (Infrastructure & Observability)
- [ ] **任务 T0.1**：规范场景时间语法，全面收敛为合规的 `us/ms/s`；
- [ ] **任务 T0.2**：定义 Wasm 导出 C-ABI 对端动作注入接口：
  - `sim_ble_peer_inject_action(const char* json_str)`
  - `sim_ble_peer_reset(void)`
- [ ] **任务 T0.3**：建立语义观测探针，通过 Wasm 导出符号向无头运行器暴露 `ble:gap:conn_count` 与 `ble:gatt:notify_count`，废除以电压作为网络断言的无效做法。

### 阶段 1：【第一批交付】多连接槽位池与代际仲裁 (Batch 1: Multi-Conn Core)
- [ ] **任务 T1.1**：在 `frameworks/esp_idf/src/bluetooth/` 下实现 `sim_ble_peer.h/.c`：
  - 4 槽位定容 POD 静态连接池，0 动态堆分配；
  - 槽位独立分配 `conn_handle 1..4`，独立跟踪 MTU 与加密状态；
  - 引入 `generation` 代际号，断开时立即作废槽位；
- [ ] **任务 T1.2**：重构 `src/bluetooth/esp_nimble.c` GAP 状态机：
  - 移除硬编码单例 `is_connected` 与写死 `conn_handle=1`；
  - 严格校验传入的 `conn_handle` 有效性，非法句柄直接返回 `BLE_HS_ENOTCONN`；
  - 正确分发 `BLE_GAP_EVENT_CONNECT` 与 `BLE_GAP_EVENT_DISCONNECT`。

### 阶段 2：【第二批交付】双向 GATT 权限仲裁与对端驱动 (Batch 2: GATT Bi-Directional)
- [ ] **任务 T2.1**：在 `sim_ble_peer` 中实现特征值访问仲裁器：
  - 严格核验标志位（`BLE_GATT_CHR_F_READ / WRITE / NOTIFY`）；
  - 越权操作严格返回 `BLE_ATT_ERR_READ_NOT_PERMITTED` / `BLE_ATT_ERR_WRITE_NOT_PERMITTED`；
- [ ] **任务 T2.2**：实现对端主动读写驱动器：
  - 响应 `WRITE_CHR` 动作，驱动用户绑定的 `access_cb(conn_handle, attr_handle, ctxt, arg)`；
  - 响应 `SUBSCRIBE_NOTIFY` 动作，维护客户端配置描述符（CCCD）状态；
  - 捕获业务层 `ble_gatts_notify` 投递，正确统计 `notify_count`。

### 阶段 3：【第三批交付】空口主动扫描与虚拟信标池 (Batch 3: Active Scan Pipeline)
- [ ] **任务 T3.1**：在 `sim_ble_peer` 中实现虚拟广播信标池（上限 8 个 Beacon）：
  - 支持注册外部 iBeacon / 传感器广播包（包含 Flags、Name、Service UUIDs）；
- [ ] **任务 T3.2**：在 `esp_nimble.c` 中落地主动扫描 API：
  - 实现 `ble_gap_disc()`，根据扫描参数枚举信标池；
  - 步进时向业务回调派发 `BLE_GAP_EVENT_DISC`，打通 Observer / Central 官方示例。

### 阶段 4：门禁强化与反异变防御 (Gate 5 & Mutation Tests)
- [ ] **任务 T4.1**：在 `.governance/gates/rules/` 部署：
  - `g5_no_ble_inline_mock.py`（Rule 505）：阻断 C 代码硬编码 peer 动作或针对特定业务 UUID 的硬编码判断；
  - `g5_ble_assertion_quality.py`（Rule 506）：检查蓝牙场景断言有效性；
- [ ] **任务 T4.2**：编写反异变单元测试：篡改对端写入特征值的 Payload 预期，验证断言能够 100% 触发红灯拦截。

### 阶段 5：用例矩阵实证与全量无头实证 (Verification & Governance)
- [ ] **任务 T5.1**：为 `bluetooth/nimble/bleprph` 编写增强的 `bleprph.scenario.json`；
- [ ] **任务 T5.2**：在 `checklist.data.json` 中为 `#010 bleprph` 明确区分 `wasm_node` 证据条目；
- [ ] **任务 T5.3**：扩充 `test_esp_nimble.c` 测试套件，覆盖并发多从机连接、未授权读写拒绝、主动扫描发现；
- [ ] **任务 T5.4**：执行 `ctest -L esp_idf` 与 `run_gates.py --mode nightly` 达成全绿；通过 `run_esp32_headless_evidence.ps1 -App bleprph` 输出确定性实据，全部达成 100% 通过后更新 `capability-catalog.yaml` 为 `implemented`。

---

## 四、 风险分析与规避对策 (修订版)

| 风险场景 | 风险等级 | 诱发原因 | 预防与规避对策 |
|---|:---:|---|---|
| **R-1：NimBLE 多任务线程死锁** | 🟡 中 | 原厂 NimBLE 在 `nimble_port_run()` 中起专用线程，若对端注入直接同步调用可能产生死锁 | 所有对端动作注入必须通过内部轻量 FIFO 投递至 NimBLE 事件处理上下文，由事件循环统一派发，保证线程模型与原厂完全一致 |
| **R-2：MBUF 链表内存泄漏** | 🟡 中 | 构造 `os_mbuf` 传递载荷给 `access_cb` 时若未释放会导致泄漏 | 使用只读零拷贝内存切片包装器，并在 `access_cb` 返回后立即重置复用内部静态临时缓冲 |
| **R-3：多连接并发句柄竞争** | 🟡 中 | 同时存在多个连接时，上层如果以固定 `1` 传递会误操作其他连接 | 门面接口严格校验 `conn_handle` 的有效性与代际匹配，非法句柄直接返回 `BLE_HS_ENOTCONN` |

---

## 五、 结论与后续里程碑规划

本实施计划（v2.0 终审版）消除了单例受限、被动单向、无法模拟多设备与扫描缺失等痛点。

- **Milestone 1（当前计划）**：按 Batch 1 $\to$ 2 $\to$ 3 递进交付多连接槽位池、双向 GATT 权限仲裁、虚拟对端动作执行器与主动扫描信标池；
- **Milestone 2（后续规划）**：推进 SMP 蓝牙安全配对（Passkey / Just Works）与 L2CAP 原生高吞吐数据流通道仿真。
