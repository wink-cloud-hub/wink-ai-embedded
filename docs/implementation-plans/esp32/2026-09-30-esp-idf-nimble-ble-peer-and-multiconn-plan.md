<!-- SPDX-License-Identifier: Apache-2.0 -->
# 实施计划：ESP-IDF NimBLE 蓝牙栈虚拟对端驱动器与多连接池重构战役 (v2.1 终审修订版)

| 字段 | 内容 |
|---|---|
| 计划编号 | PLAN-20260930-ESP-IDF-NIMBLE-BLE-PEER-AND-MULTICONN-v2.1 |
| 状态 | 📝 **Ready for Execution（吸收深度白盒专项评审意见后的修订终版）** |
| 日期 | 2026-09-30 |
| 周期估算 | 6~7 个工作日（分 Batch 1 ~ 3 阶段递进执行） |
| 优先次序 | **T0 语义注入与观测链路 → C-ABI 闭合审计 → 多连接池与代际仲裁 → 二维 CCCD 矩阵与 GATT 读写 → Observer 扫描子系统 → 门禁与无头实证** |
| 决策依据 | [ADR-0012：契约诚实优于静默降级（Fail-Loud 原则）](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)<br>[ADR-0002：双 Target 同源编译原则](../../decisions/unisim/0002-dual-target-compilation.md)<br>[ADR-0003：生产口径与保真边界约束（永不承诺虚实恒等）](../../decisions/unisim/0003-simulation-fidelity-boundary.md)<br>[ADR-0004：编译期静态分发优于运行时函数指针](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md)<br>[ADR-0045：仿真内存配额与异常故障策略](../../decisions/unisim/0045-simulation-memory-quota-and-fault-policy.md)<br>[ADR-0053：虚拟时间因果同刻总序仲裁模型](../../decisions/unisim/0053-sim-same-timestamp-event-total-order.md)<br>[ADR-0089：分类记账堆内存与边界防御模型](../../decisions/core/0089-esp-idf-heap-caps-allocation-contract.md)<br>[ADR-0091：多配置实例与五维正交 Schema 架构决策](../../decisions/unisim/0091-esp-idf-multi-config-orthogonal-schema.md)<br>[ADR-0092：ESP-IDF 官方示例仿真治理前置筑基宪章](../../decisions/unisim/0092-esp-idf-simulation-governance-and-capability-charter.md) |
| 管辖数据源 | [`checklist.data.json`](../../../wink-micro-app/vendor/esp_idfv61/.governance/data/checklist.data.json)、[`capability-catalog.yaml`](../../../wink-micro-app/vendor/esp_idfv61/.governance/catalog/capability-catalog.yaml)、[`.governance/gates/`](../../../wink-micro-app/vendor/esp_idfv61/.governance/gates/) |
| 实施目标文件 | `frameworks/esp_idf/src/bluetooth/*`、`frameworks/esp_idf/include/nimble/*`、`host/*`、`services/*`、`os/os_mbuf.h`、`.governance/gates/rules/g5_*.py` |
| 验收门禁 | `ctest -L esp_idf`、`python .governance/gates/run_gates.py --mode nightly`、`powershell run_esp32_headless_evidence.ps1 -App bleprph` |

---

## 一、 战略总目标与修订后全局验收标准 (DoD)

### 1.1 评审纠偏核心要旨
依据架构组白盒精审准则，对 NimBLE 蓝牙栈仿真方案进行了全维度的深度纠偏与工程落地闭环：

1. **重写 `bleprph` 验收场景与广播状态契约**：
   - 彻底纠正原示例中写入 `DEAD`（4 字节）与订阅 `2A37` 的失真假设：核对官方 carrier [`gatt_svr.c:37-43`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/bluetooth/bleprph/gatt_svr.c#L37-L43)，其真实特征值采用 128 位 UUID（`00000000-1111-1111-2222-222233333333`），且在第 163 行限制 `min_len=1, max_len=1`（只接受 1 字节写入，写入 4 字节直接报 `0x0D` 错误）；场景统一修正为写入 1 字节载荷（如 `0x42`）；
   - 纠正连接后广播行为：核对官方 carrier [`main.c:253-270`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/bluetooth/bleprph/main.c#L253-L270)，`bleprph` 在首个连接建立后**不再继续广播**。在未重新开启广播时强行注入第二次连接在真实协议栈中必定被拒；四连接并发能力改由支持连接后持续广播的专用测试 App（`ble_multiconn_app`）进行验证；
   - 杜绝假绿灯断言：`gatt_svr.c:167` 即使写入失败也会无条件调用 `ble_gatts_chr_updated()`，断言通知计数不能证明写入成功。验收场景必须断言**对端写入返回码 `0` 与后续显式读取返回的有效值 `0x42`**；
2. **跨仓 T0 场景解析、注入与语义观测完整闭环**：
   - 统一收敛时间语法为合规的 `us/ms/s`；
   - 纠偏断言契约：`ASSERT_POINT` 必须使用 UniSim 规范的 `matcher` 语法；通过 Wasm 导出 C-ABI 读取应用产生的实际读回值与状态；
   - 明确测试用例退出清理钩子：通过 `sim_ble_peer_reset()` 释放所有连接槽位、订阅位图与虚拟信标，保证同 seed 多次运行的严格幂等；
3. **NimBLE C-ABI 闭合审计与编译门禁**：
   - **紧急纠偏重大编译阻断 Bug**：现存 [`include/host/ble_gap.h:60`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/include/host/ble_gap.h#L60) 错将字段定义为结构体 `struct ble_gap_conn_desc conn;`，而原生 carrier [`main.c:259`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/bluetooth/bleprph/main.c#L259) 原生访问 `event->connect.conn_handle`，必须立即纠正对齐；
   - 纠正 [`include/host/ble_gatt.h:112`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/include/host/ble_gatt.h#L112) 将 `ble_gatts_chr_updated()` 错写为返回 `int` 的问题，恢复上游 Mynewt 原厂规范 `void ble_gatts_chr_updated(uint16_t chr_def_handle);`；
   - 补齐上游标准 ATT 错误码（`BLE_ATT_ERR_READ_NOT_PERMITTED` 0x02, `BLE_ATT_ERR_WRITE_NOT_PERMITTED` 0x03, `INSUFFICIENT_AUTHEN` 0x05, `INSUFFICIENT_ENC` 0x0F）；
   - 增设“原生 carrier 零修改编译通过”门禁，防止仅凭简化测试通过产生伪兼容；
4. **收窄交付范围：聚焦 Observer 扫描，后置完整 Central 链路**：
   - 原计划声称交付 `blecent` 官方示例，但官方 `blecent` 在扫描发现设备后还会调用 `ble_gap_disc_cancel()` $\to$ `ble_gap_connect()` $\to$ GATT Client 服务/特征值枚举；
   - 本期诚实收窄：第三批交付聚焦 **Observer / iBeacon 扫描发现**（实现 `ble_gap_disc()`、超时、主动取消、去重与 `BLE_GAP_EVENT_DISC_COMPLETE`），将复杂的 GATT 客户端发现与访问流水线后置为独立专项；
5. **订阅状态按“连接 × 特征”二维 CCCD 矩阵保存**：
   - 彻底废除当前在特征值结构体上挂单布尔值 `is_subscribed_notify` 的简陋设计；
   - 建立二维 `CCCD[SIM_BLE_MAX_CONNS][SIM_BLE_MAX_CHRS]` 订阅矩阵：连接 A 订阅、连接 B 未订阅时，`chr_updated` 仅向 A 投递通知；`ble_gatts_notify(conn_handle, ...)` 严格检查指定连接的 CCCD 权限，杜绝跨连接严重串包；
6. **MBUF 消费语义与生命周期安全**：
   - 遵循原厂 Mynewt 契约：`ble_gatts_notify_custom()` 无论成功失败都会消费/释放传入的 `os_mbuf`，模拟真实所有权转移；按连接实测协商的 MTU 严格检查载荷尺寸；
   - 虚拟时间事件队列入队：连接槽位携带 `generation` 代际号，连接断开后立即作废在途事件，覆盖槽位复用时的迟到通知拦截；
7. **契约诚实与容量承诺对齐（ADR-0012）**：
   - 本期未实现 SMP 密码学握手前，绝不虚假承诺“独立鉴权”；对要求加密或认证的特征值访问，严格返回 `BLE_ATT_ERR_INSUFFICIENT_ENC` 或 `INSUFFICIENT_AUTHEN`（Fail-Loud）；
   - 四连接槽位与目标固件的 `CONFIG_BT_NIMBLE_MAX_CONNECTIONS=4` 内存预算严格对齐。

### 1.2 全局验收标准 (DoD)
- [ ] **G-01（原生 carrier 零修改编译通过）**：`event->connect.conn_handle` 字段纠偏，`ble_gatts_chr_updated()` 恢复为 `void`；`wink-micro-app/vendor/esp_idfv61/bluetooth/bleprph/` 原生代码零修改同源编译通过；
- [ ] **G-02（`bleprph` 官方示例真实闭环实证）**：使用 128 位实际 UUID 进行连接、订阅与单字节（`0x42`）写入，断言写入成功返回码 `0` 与后续显式读取返回的 `0x42`，无头运行毫秒级确定性通过；
- [ ] **G-03（二维 CCCD 订阅矩阵与防串包隔离）**：连接 A 订阅特征值、连接 B 未订阅时，调用 `chr_updated` 严格只向连接 A 派发通知回调，连接 B 零事件泄漏；
- [ ] **G-04（四连接并发与广播互斥仲裁）**：在支持持续广播的多从机 App 中跑通 4 连接并发接入与独立 MTU 协商；设备未广播时连接注入立即失败，第 5 个并发连接被拒绝（返回 `BLE_HS_ENOMEM` / `BLE_HS_ECONGESTION`）；
- [ ] **G-05（Observer 空口主动扫描）**：支持在场景中声明虚拟广播信标池（Virtual Air Beacons），支持 `ble_gap_disc()` 主动扫描、超时退出、去重枚举与 `BLE_GAP_EVENT_DISC_COMPLETE` 准确派发；
- [ ] **G-06（代际防悬挂与 MBUF 生命周期）**：连接断开或 deinit 时递增代际号，在途事件与通知全部安全作废；槽位复用后无迟到事件；MBUF 模拟原生所有权释放，超 MTU 载荷显式报错；
- [ ] **G-07（契约诚实与反异变门禁）**：加密/认证特征值越权访问严格返回标准 ATT 错误码；部署 Gate 5 Rule 505/506；反异变测试（篡改对端写入预期值或连接状态）必报红灯；`checklist.data.json` 严格分列 `wasm_node` 证据通道，`ctest -L esp_idf` 100% 绿灯。

---

## 二、 核心架构设计方案 (修订版)

### 2.1 架构分流与双向数据流图

```
+─────────────────────────────────────────────────────────────────────────────────────────────+
│                       ESP-IDF NimBLE 蓝牙栈双向交互架构 (v2.1 修订版)                         │
+─────────────────────────────────────────────────────────────────────────────────────────────+
│  [应用业务层]  官方 carrier 示例 (bleprph 从机 / 专用 ble_multiconn_app 多连接验证)          │
+─────────────────────────────────────────────────────────────────────────────────────────────+
│  [原厂 C-ABI]  frameworks/esp_idf/src/bluetooth/esp_nimble.c                                │
│                ├─ GAP 接口 (ble_gap_adv_start / ble_gap_conn_find / ble_gap_disc)           │
│                ├─ GATT 接口 (ble_gatts_add_svcs / void ble_gatts_chr_updated / notify)       │
│                └─ 回调泵 (event->connect.conn_handle 对齐 / ble_gatt_access_fn 驱动)          │
+───────────────────────────────────────────────────────┬─────────────────────────────────────+
                                                        │
                         ┌──────────────────────────────┴──────────────────────────────┐
                         ▼                                                             ▼
+───────────────────────────────────────────────────────+ +───────────────────────────────────+
│ sim_ble_peer (声明式虚拟蓝牙对端引擎)                  │ │ sim_ble_beacons (虚拟信标池)      │
│ ├─ 多连接槽位池: conns[4] (conn_handle 1..4, MTU, 代际)│ │ ├─ 8 槽位虚拟 Beacon 静态数组     │
│ ├─ 二维 CCCD 矩阵: cccd[4][32] (连接 × 特征防串包)     │ │ ├─ Observer 扫描检索与去重过滤    │
│ ├─ 对端动作执行器: Connect / Disconnect / Write / Read │ │ └─ DISC_COMPLETE 事件生命周期管理 │
│ ├─ 权限仲裁: Read/Write/Encrypted 标志判定，标准 ATT  │ +───────────────────────────────────+
│ └─ 事件调度队列: 虚拟时钟优先队列，代际失效检查      │
+───────────────────────────────────────────────────────+
                         ▲
                         │ (Wasm C-ABI 注入链路)
+─────────────────────────────────────────────────────────────────────────────────────────────+
│  [场景剧本层]  unisim-scenarios/<app>.scenario.json                                          │
│                ├─ steps: [ { "type": "INJECT_BLE_PEER_ACTION", "action": "WRITE_CHR" } ]     │
│                ├─ 语义信号观测: ble:gap:conn_count / 读取应用写回值                          │
│                └─ 幂等清理钩子: sim_ble_peer_reset()                                         │
+─────────────────────────────────────────────────────────────────────────────────────────────+
```

### 2.2 统一场景注入链路契约 (`scenario.json`)

针对 `bleprph` 原生 128 位 UUID、单字节容量限制与 Zod Matcher 规范重写：

```json
{
  "header": {
    "version": "1.0.0",
    "name": "ESP-IDF v6.1 NimBLE bleprph Characteristic Access Proof",
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
      "matcher": { "equals": 1 }
    },
    {
      "type": "INJECT_BLE_PEER_ACTION",
      "timeUs": "500ms",
      "connHandle": 1,
      "action": "SUBSCRIBE_NOTIFY",
      "charUuid128": "00000000-1111-1111-2222-222233333333",
      "enable": true
    },
    {
      "type": "INJECT_BLE_PEER_ACTION",
      "timeUs": "800ms",
      "connHandle": 1,
      "action": "WRITE_CHR",
      "charUuid128": "00000000-1111-1111-2222-222233333333",
      "payloadHex": "42"
    },
    {
      "type": "ASSERT_POINT",
      "timeUs": "900ms",
      "target": "ble:gatt:last_write_status",
      "matcher": { "equals": 0 }
    },
    {
      "type": "INJECT_BLE_PEER_ACTION",
      "timeUs": "1200ms",
      "connHandle": 1,
      "action": "READ_CHR",
      "charUuid128": "00000000-1111-1111-2222-222233333333"
    },
    {
      "type": "ASSERT_POINT",
      "timeUs": "1300ms",
      "target": "ble:gatt:last_read_val_byte",
      "matcher": { "equals": 66 }
    }
  ]
}
```

### 2.3 C-ABI 头文件纠偏与结构体定义

#### 1. 修复 `ble_gap.h` 字段名
```c
/* 彻底纠正结构体命名，与原厂 carrier main.c:259 保持完全一致 */
struct ble_gap_event {
    uint8_t type;
    union {
        struct {
            int status;
            uint16_t conn_handle;           /* 修正：原为 struct ble_gap_conn_desc conn，严重阻断编译 */
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
            int status;
            struct ble_gap_disc_desc disc;
        } disc;
    };
};
```

#### 2. 修复 `ble_gatt.h` 返回值与补齐 ATT 错误码
```c
/* 原厂标准 ATT 错误码补全 */
#define BLE_ATT_ERR_INVALID_HANDLE          0x01
#define BLE_ATT_ERR_READ_NOT_PERMITTED      0x02
#define BLE_ATT_ERR_WRITE_NOT_PERMITTED     0x03
#define BLE_ATT_ERR_INVALID_PDU             0x04
#define BLE_ATT_ERR_INSUFFICIENT_AUTHEN     0x05
#define BLE_ATT_ERR_REQ_NOT_SUPPORTED       0x06
#define BLE_ATT_ERR_INVALID_OFFSET          0x07
#define BLE_ATT_ERR_INSUFFICIENT_AUTHOR     0x08
#define BLE_ATT_ERR_PREPARE_QUEUE_FULL      0x09
#define BLE_ATT_ERR_ATTR_NOT_FOUND          0x0A
#define BLE_ATT_ERR_ATTR_NOT_LONG           0x0B
#define BLE_ATT_ERR_INSUFFICIENT_KEY_SZ     0x0C
#define BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN  0x0D
#define BLE_ATT_ERR_UNLIKELY                0x0E
#define BLE_ATT_ERR_INSUFFICIENT_ENC        0x0F
#define BLE_ATT_ERR_INSUFFICIENT_RES        0x11

/* 修正返回值类型为 void (原厂 Mynewt 契约) */
void ble_gatts_chr_updated(uint16_t chr_def_handle);
```

### 2.4 二维 CCCD 订阅矩阵与事件分发管道

彻底废除单布尔值，构建二维订阅与权限矩阵：

```c
#define SIM_BLE_MAX_CONNS       4
#define SIM_BLE_MAX_CHRS        32

typedef struct {
    bool notify_enabled;
    bool indicate_enabled;
} sim_ble_cccd_state_t;

typedef struct {
    /* 核心二维状态矩阵: [连接槽位 0..3] x [特征值槽位 0..31] */
    sim_ble_cccd_state_t cccd_matrix[SIM_BLE_MAX_CONNS][SIM_BLE_MAX_CHRS];
} sim_ble_subscription_mgr_t;
```

- **`ble_gatts_notify(conn_handle, chr_val_handle)` 时序**：
  检索传入 `conn_handle` 对应的连接槽位，仅当 `cccd_matrix[slot][chr].notify_enabled == true` 时才派发，否则直接返回 `BLE_HS_EINVAL`；
- **`ble_gatts_chr_updated(chr_def_handle)` 时序**：
  遍历 `0..SIM_BLE_MAX_CONNS-1`，针对每一个处于活跃且订阅了该特征值的连接槽位，独立生成并向其投递通知，绝不波及未订阅的对端连接。

---

## 三、 任务拆分与执行路线图 (WBS 修订版)

```
┌────────────────────────────────────────────────────────────────────────┐
│                        NimBLE 战役执行依赖图 (v2.1)                    │
├────────────────────────────────────────────────────────────────────────┤
│ 【阶段 0：T0 注入与观测基建】规范时间语法与断言 Matcher 链路             │
│       │                                                                │
│       ▼                                                                │
│ 【阶段 1：C-ABI 闭合审计】修复 conn_handle 字段与 void chr_updated     │
│       │                                                                │
│       ▼                                                                │
│ 【阶段 2：第一批交付 (多连接)】实现多连接槽位池、代际仲裁与广播互斥    │
│       │                                                                │
│       ▼                                                                │
│ 【阶段 3：第二批交付 (GATT 双向)】二维 CCCD 矩阵与 128 位对端读写驱动  │
│       │                                                                │
│       ▼                                                                │
│ 【阶段 4：第三批交付 (Observer)】空口主动扫描与虚拟 Beacon 信标池      │
│       │                                                                │
│       ▼                                                                │
│ 【阶段 5：门禁、反异变与全量实证】Gate 5 规则 505/506 与 bleprph 实据  │
└────────────────────────────────────────────────────────────────────────┘
```

### 阶段 0：T0 注入与语义观测基建 (Infrastructure & Observability)
- [ ] **任务 T0.1**：规范场景时间语法，全面收敛为 `us/ms/s`；
- [ ] **任务 T0.2**：定义 Wasm 导出 C-ABI 对端动作注入接口：
  - `sim_ble_peer_inject_connect(uint16_t conn_handle, const char* addr_str, uint16_t mtu)`
  - `sim_ble_peer_inject_write_chr128(uint16_t conn_handle, const char* uuid128, const uint8_t* val, uint16_t len)`
  - `sim_ble_peer_inject_read_chr128(uint16_t conn_handle, const char* uuid128)`
  - `sim_ble_peer_inject_subscribe128(uint16_t conn_handle, const char* uuid128, bool notify)`
  - `sim_ble_peer_reset(void)`
- [ ] **任务 T0.3**：建立语义观测探针，通过 Wasm 导出符号向无头运行器暴露 `ble:gap:conn_count`、`ble:gatt:last_write_status` 与 `ble:gatt:last_read_val_byte`，配置对应 Zod Matcher。

### 阶段 1：NimBLE C-ABI 闭合审计与原样 carrier 编译门禁 (Header ABI Alignment)
- [ ] **任务 T1.1**：纠偏 `frameworks/esp_idf/include/host/ble_gap.h`：
  - 将 `connect` 联合体成员恢复为 `int status; uint16_t conn_handle;`；
- [ ] **任务 T1.2**：纠偏 `frameworks/esp_idf/include/host/ble_gatt.h`：
  - 修正 `void ble_gatts_chr_updated(uint16_t chr_def_handle);`；
  - 完整补齐标准 ATT 错误码（`BLE_ATT_ERR_READ_NOT_PERMITTED` 等）；
- [ ] **任务 T1.3**：增设 CTest / 编译门禁，确保未修改的官方 carrier `bluetooth/bleprph/` 原生源文件能够直接同源编译通过；更新 `channels.json` 保持资产登记一致。

### 阶段 2：【第一批交付】多连接槽位池与代际仲裁 (Batch 1: Multi-Conn Core)
- [ ] **任务 T2.1**：在 `frameworks/esp_idf/src/bluetooth/` 下实现 `sim_ble_peer.h/.c`：
  - 4 槽位定容 POD 静态连接池，0 动态堆分配；
  - 槽位独立分配 `conn_handle 1..4`，独立跟踪 MTU 与代际号；
  - 建立广播互斥逻辑：外设连接建立后若未显式调用 `ble_gap_adv_start()`，拒绝新的连接注入；
- [ ] **任务 T2.2**：重构 `src/bluetooth/esp_nimble.c` GAP 状态机：
  - 移除硬编码单例 `is_connected` 与写死 `conn_handle=1`；
  - 非法 `conn_handle` 严格返回 `BLE_HS_ENOTCONN`；
  - 编写专用多连接测试 App（`ble_multiconn_app`），完成 4 连接并发建立与第 5 连接拒绝的验收。

### 阶段 3：【第二批交付】二维 CCCD 订阅矩阵与 128 位对端读写驱动 (Batch 2: GATT CCCD)
- [ ] **任务 T3.1**：在 `sim_ble_peer` 中构建二维 `cccd[4][32]` 订阅状态矩阵：
  - 隔离不同连接的订阅状态；
  - `chr_updated` 仅向已订阅的活跃连接投递通知；
  - `ble_gatts_notify(conn_handle, ...)` 校验指定连接的 CCCD 开关；
- [ ] **任务 T3.2**：实现 128 位 UUID 查找与权限仲裁器：
  - 严格校验只读、只写与加密标志；
  - 对加密要求明确返回 `BLE_ATT_ERR_INSUFFICIENT_ENC`（Fail-Loud）；
- [ ] **任务 T3.3**：重构 `ble_gatts_notify_custom` 的 MBUF 生命周期管理：
  - 遵守原厂规范，消费/释放传入的 `os_mbuf`；
  - 按各连接协商的 MTU 检查载荷，超长抛出错误。

### 阶段 4：【第三批交付】Observer 空口主动扫描与虚拟信标池 (Batch 3: Observer Scan)
- [ ] **任务 T4.1**：在 `sim_ble_peer` 中实现虚拟广播信标池（上限 8 个 Beacon）：
  - 支持注册广播数据包（包含 Flags、Name、Service UUIDs）；
- [ ] **任务 T4.2**：在 `esp_nimble.c` 中落地 Observer 扫描 API：
  - 实现 `ble_gap_disc()`，支持主动/被动扫描枚举与去重；
  - 模拟扫描超时与主动取消（`ble_gap_disc_cancel()`），派发 `BLE_GAP_EVENT_DISC_COMPLETE`；
  - 严格收窄边界：后置 GATT Client 完整流水线，本期不承诺直接打勾 `blecent`。

### 阶段 5：门禁强化、反异变防御与全量无头实证 (Verification & Governance)
- [ ] **任务 T5.1**：在 `.governance/gates/rules/` 部署：
  - `g5_no_ble_inline_mock.py`（Rule 505）：阻断 C 代码硬编码 peer 动作或针对特定业务 UUID 的硬编码特判；
  - `g5_ble_assertion_quality.py`（Rule 506）：检查蓝牙场景断言有效性；
- [ ] **任务 T5.2**：编写反异变单元测试：篡改对端写入特征值的 Payload 预期或期望返回值，验证断言能够 100% 触发红灯拦截；
- [ ] **任务 T5.3**：在 `checklist.data.json` 中为 `#010 bleprph` 明确区分 `wasm_node` 证据条目；
- [ ] **任务 T5.4**：更新 `test_esp_nimble.c`，执行 `ctest -L esp_idf` 与 `run_gates.py --mode nightly` 达成全绿；通过 `run_esp32_headless_evidence.ps1 -App bleprph` 输出确定性实据，全部达成 100% 通过后更新 `capability-catalog.yaml` 为 `implemented`。

---

## 四、 风险分析与规避对策 (修订版)

| 风险场景 | 风险等级 | 诱发原因 | 预防与规避对策 |
|---|:---:|---|---|
| **R-1：NimBLE 事件队列溢出与时序重入** | 🟡 中 | 对端动作密集注入导致事件队列堆积 | 引入容量为 16 的固定环形事件 FIFO，满队列时触发背压阻断；所有注入走统一的虚拟时间事件循环，杜绝多线程重入死锁 |
| **R-2：MBUF 消费后二次释放** | 🟡 中 | 业务层与仿真层对 `os_mbuf` 归属权不一致产生 double-free | 严格遵守 Mynewt 契约：仿真层接收后接管所有权并统一释放，外部严禁重复引用已传递的 mbuf 指针 |
| **R-3：多连接代际失效竞争** | 🟡 中 | 连接 1 断开后快速复用同一槽位，旧事件错误投递给新设备 | 槽位分配严格递增 32 位 `generation` 代际号；排队事件入队时打上当前代际戳，出队分发前校验代际一致性，不符则静默丢弃 |

---

## 五、 结论与后续里程碑规划

本实施计划（v2.1 终审版）彻底吸纳了架构组的深度白盒评审意见，消除了 C-ABI 字段阻断、单字节载荷误判、二维 CCCD 串包、广播互斥失真与范围夸大等全部隐患。

- **Milestone 1（当前计划）**：交付多连接槽位池、二维 CCCD 订阅矩阵、`bleprph` 128 位对端读写驱动与 Observer 空口扫描；
- **Milestone 2（后续规划）**：推进 SMP 蓝牙安全配对（Passkey / Just Works）与完整的 GATT Client 发现流水线（支撑 `blecent` 官方示例）。
