<!-- SPDX-License-Identifier: Apache-2.0 -->
# 实施计划：ESP-IDF Wi-Fi 与 Netif 确定性虚拟空口与多网卡重构战役 (v2.0 终审修订版)

| 字段 | 内容 |
|---|---|
| 计划编号 | PLAN-20260930-ESP-IDF-WIFI-NETIF-DETERMINISTIC-ENV-v2.0 |
| 状态 | 📝 **Ready for Execution（吸收深度白盒专项评审意见后的修订终版）** |
| 日期 | 2026-09-30 |
| 周期估算 | 6~7 个工作日（分 Batch 1 ~ 3 阶段递进执行） |
| 优先次序 | **T0 语义注入与观测链路 → v6.1 C-ABI 头文件纠偏 → STA 三阶段确定性状态机 → 空口扫描子系统 → SoftAP与网络就绪级联联动 → 门禁与无头实证** |
| 决策依据 | [ADR-0012：契约诚实优于静默降级（Fail-Loud 原则）](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)<br>[ADR-0002：双 Target 同源编译原则](../../decisions/unisim/0002-dual-target-compilation.md)<br>[ADR-0003：生产口径与保真边界约束（永不承诺虚实恒等）](../../decisions/unisim/0003-simulation-fidelity-boundary.md)<br>[ADR-0004：编译期静态分发优于运行时函数指针](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md)<br>[ADR-0045：仿真内存配额与异常故障策略](../../decisions/unisim/0045-simulation-memory-quota-and-fault-policy.md)<br>[ADR-0053：虚拟时间因果同刻总序仲裁模型](../../decisions/unisim/0053-sim-same-timestamp-event-total-order.md)<br>[ADR-0089：分类记账堆内存与边界防御模型](../../decisions/core/0089-esp-idf-heap-caps-allocation-contract.md)<br>[ADR-0091：多配置实例与五维正交 Schema 架构决策](../../decisions/unisim/0091-esp-idf-multi-config-orthogonal-schema.md)<br>[ADR-0092：ESP-IDF 官方示例仿真治理前置筑基宪章](../../decisions/unisim/0092-esp-idf-simulation-governance-and-capability-charter.md) |
| 管辖数据源 | [`checklist.data.json`](../../../wink-micro-app/vendor/esp_idfv61/.governance/data/checklist.data.json)、[`capability-catalog.yaml`](../../../wink-micro-app/vendor/esp_idfv61/.governance/catalog/capability-catalog.yaml)、[`.governance/gates/`](../../../wink-micro-app/vendor/esp_idfv61/.governance/gates/) |
| 实施目标文件 | `frameworks/esp_idf/src/wifi/*`、`frameworks/esp_idf/include/esp_wifi.h`、`esp_wifi_types.h`、`esp_netif.h`、`esp_netif_types.h`、`src/network/esp_mqtt.c`、`.governance/gates/rules/g5_*.py` |
| 验收门禁 | `ctest -L esp_idf`、`python .governance/gates/run_gates.py --mode nightly`、`powershell run_esp32_headless_evidence.ps1 -App wifi_sta` |

---

## 一、 战略总目标与修订后全局验收标准 (DoD)

### 1.1 评审纠偏核心要旨
依据架构组对 v1.0 草案的深度白盒评审，本方案彻底纠正了原计划中的**六大核心漏洞与工程盲区**：

1. **增设跨仓 T0 注入与观测基建，矫正语法规范**：
   - 彻底修复原示例中微秒单位错误（`1500000µs` 违反 `time-types.ts` 正则，统一收敛为 `us/ms/s` 标准时间字面量）；
   - 明确 UniSim 场景注入路径：在 UniSim Runner 中打通向 Wasm 导出 C-ABI 注入虚拟 AP 列表的适配通道，并在用例运行结束时通过 `sim_wifi_env_reset()` 进行彻底清理；
   - 废除“电压断言证明网络”的无效断言：增加针对 `wifi:state`（关联状态）与 `netif:ip`（IP 地址）的可观测信号断言体系；
2. **将 ESP-IDF v6.1 兼容头文件列为独立前置任务，根除二级指针重大 Bug**：
   - **紧急纠偏**：修正 [`esp_netif_types.h`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/include/esp_netif_types.h) 将 `esp_netif_t` 误定义为指针（`typedef struct esp_netif_obj* esp_netif_t;`）导致的 `ip_event_got_ip_t.esp_netif` 沦为二级指针（`**`）的严重扭曲；统一恢复为乐鑫原厂规范的不透明对象 `typedef struct esp_netif_obj esp_netif_t;` 并配合 `esp_netif_t *` 签名；
   - 补齐官方 v6.1 规范缺失的 `WIFI_EVENT_SCAN_DONE`、`esp_wifi_scan_get_ap_num()`、`wifi_ap_record_t`、`wifi_event_sta_disconnected_t` 结构体及 `esp_netif_create_default_wifi_ap()`；
3. **纠正时间调度根因，建立可取消的“三阶段确定性状态机”**：
   - 纠正原计划“`vTaskDelay` 导致时间失步”的不准确表述（`freertos_task.c:218` 实际已支持虚拟时钟）；
   - 针对真正缺陷——“固定的 100ms 成功时序、缺乏取消保护导致断开后抛出迟到的‘幽灵 GOT_IP’”——建立 **Association（关联） $\to$ DHCP（地址协商） $\to$ Disconnect（断开）** 三阶段显式状态机；引入**代际令牌（Generation Token）**，在每次状态迁移前严格核验，发生 `disconnect`、`stop` 或 `reset` 时立即注销在途事件；
4. **收窄保真度断言，支持原厂细分原因码**：
   - 纠正“密码错误必为 `AUTH_FAIL`”的片面定义，支持真实的四次握手超时（`WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT`）等原厂原因码建模；
   - 完善扫描子系统规范：支持 SSID/BSSID 过滤、RSSI 降序排列、结果一次性消费（`consumed` 机制）、`scan_get_ap_num()` 及同步阻塞扫描中断连接的行为验收；
5. **分批递进式交付，建立上层网络级联联动**：
   - 将原计划一锅端的臃肿交付拆分为三批清晰路线：**第一批（STA 连接/断开/IP） $\to$ 第二批（空口扫描子系统） $\to$ 第三批（SoftAP/APSTA 与网络级联）**；
   - 解决 [`esp_mqtt.c:50`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/network/esp_mqtt.c#L50) 硬编码 `s_network_ready = true` 的脱节问题，实现 Wi-Fi 掉线时联动传输层使 MQTT/HTTP 自然感知网络故障并触发重连；
6. **校准现有工程资产与治理证据归属**：
   - 将虚拟 AP Fixture 配置与官方示例现有配置 [`sdkconfig.h`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/wifi/wifi_sta/include/sdkconfig.h) 严格对齐（`CONFIG_ESP_WIFI_SSID="myssid"`，`CONFIG_ESP_WIFI_PASSWORD="mypassword"`）；
   - 严格执行 ADR-0091，禁止用 Node.js 无头执行给 `checklist.data.json` 中的 `wasm_browser` 打勾，明确分列 `wasm_node` 证据通道；待真实场景证据闭环后才更新能力目录状态为 `implemented`。

### 1.2 全局验收标准 (DoD)
- [ ] **G-01（C 代码零业务数据硬编码）**：`src/wifi/` 下所有 C 文件严禁出现具体的硬编码 IPv4 字符串（如 `192.168.1.100`）、固定 MAC 或 if-else 业务特判，违者由 Gate 5 Rule 503 静态门禁直接阻断；
- [ ] **G-02（T0 规范与跨用例完全复位）**：标准时间格式（`us/ms/s`），通过标准 Wasm 导出 C-ABI 注入虚拟 AP，每次测试退出后通过 `sim_wifi_env_reset()` 释放所有虚拟 AP 与 Netif 状态，同 seed 重跑轨迹 100% 一致；
- [ ] **G-03（C-ABI 签名与结构体 100% 对齐 v6.1）**：`esp_netif_t` 恢复为标准对象类型，`ip_event_got_ip_t.esp_netif` 纠正为一级指针；官方 `wifi_sta` 示例源文件在未做任何修改的情况下一次性同源编译成功；
- [ ] **G-04（三阶段状态机与代际防幽灵保障）**：
  - 密码正确 $\to$ 完整经历 `STA_START` $\to$ `STA_CONNECTED` $\to$ `STA_GOT_IP`；
  - 密码错误 / 握手超时 $\to$ 准确触发 `STA_DISCONNECTED`，携带对应 `reason`；
  - 在 Association 或 DHCP 阶段调用 `esp_wifi_disconnect()` 或 `esp_wifi_stop()` $\to$ 立即取消后续在途事件，**绝不产生迟到的幽灵 `GOT_IP` 事件**；
- [ ] **G-05（扫描子系统质量）**：`esp_wifi_scan_start()` 与 `esp_wifi_scan_get_ap_records()` 严格按 RSSI 降序返回，支持单次消费失效，`esp_wifi_scan_get_ap_num()` 准确返回候选数量；
- [ ] **G-06（网络就绪级联联动）**：Wi-Fi 取得 IP 时标记网络就绪，Wi-Fi 掉线时置低网络就绪状态并向传输层广播网络中断，使处于连接中的 MQTT 客户端能够自然触发 `MQTT_EVENT_DISCONNECTED`；
- [ ] **G-07（反异变防御与证据合规）**：部署 Rule 503 与 Rule 504 门禁；反异变测试（篡改场景中 AP 密码或期望分配 IP）必报红灯；证据严格归属 `wasm_node`，`ctest -L esp_idf` 100% 绿灯。

---

## 二、 核心架构设计方案 (修订版)

### 2.1 架构分流与级联数据流图

```
+─────────────────────────────────────────────────────────────────────────────────────────────+
│                       ESP-IDF Wi-Fi / Netif 跨靶仿真架构模型 (v2.0 修订版)                    │
+─────────────────────────────────────────────────────────────────────────────────────────────+
│  [应用业务层]  原生业务代码 / 官方 carrier 示例 (wifi_sta / softAP / scan)                    │
+─────────────────────────────────────────────────────────────────────────────────────────────+
│  [原厂 C-ABI]  frameworks/esp_idf/src/wifi/                                                  │
│                ├─ esp_wifi.c  (原厂 v6.1 API: init/start/connect/scan/set_config/stop)       │
│                └─ esp_netif.c (原厂 v6.1 API: esp_netif_t* 对象管理/get_ip_info)            │
+───────────────────────────────────────────────────────┬─────────────────────────────────────+
                                                        │
                         ┌──────────────────────────────┴──────────────────────────────┐
                         ▼                                                             ▼
+───────────────────────────────────────────────────────+ +───────────────────────────────────+
│ sim_wifi_env (声明式虚拟空口与 DHCP 引擎)              │ │ sim_network_broker (传输就绪仲裁) │
│ ├─ Virtual AP 表: SSID / BSSID / RSSI / Channel / Pwd │ │ ├─ network_is_ready 物理级联标志  │
│ ├─ 三阶段状态机: Association -> DHCP -> Disconnect   │ │ └─ 联动 esp_mqtt / esp_http 管道  │
│ ├─ 代际令牌: generation_token (彻底消除幽灵事件)      │ +───────────────────────────────────+
│ ├─ 扫描排序器: 按 RSSI 稳定降序，支持一次性消费      │
│ └─ 虚拟 DHCP: 基于 AP 网段分配 IP/Mask/GW/DNS         │
+───────────────────────────────────────────────────────+
                         ▲
                         │ (Wasm C-ABI 注入链路)
+─────────────────────────────────────────────────────────────────────────────────────────────+
│  [场景与运行器] UniSim Headless Runner (Node.js / Browser)                                  │
│                ├─ scenario.json (标准 us/ms/s 语法)                                          │
│                ├─ 信号观测探针: 捕获 wifi:state / netif:ip                                   │
│                └─ 清理钩子: 运行结束调用 sim_wifi_env_reset()                                │
+─────────────────────────────────────────────────────────────────────────────────────────────+
```

### 2.2 统一场景注入链路契约 (`scenario.json`)

针对现有 `time-types.ts` 正则限制与断言可信度，场景规范全面修正为：

```json
{
  "header": {
    "version": "1.0.0",
    "name": "ESP-IDF v6.1 Wi-Fi STA Connection & Scan Deterministic Proof",
    "templateId": "esp_idfv61_wifi_sta",
    "accuracyMode": "behavioral",
    "timeoutUs": "3000ms",
    "failurePolicy": "fail-fast",
    "determinism": { "prngSeed": 42 }
  },
  "steps": [
    {
      "type": "INJECT_WIFI_FIXTURE",
      "timeUs": "0ms",
      "accessPoints": [
        {
          "ssid": "myssid",
          "bssid": "00:11:22:33:44:55",
          "rssi": -45,
          "channel": 1,
          "authMode": "WPA2_PSK",
          "password": "mypassword",
          "dhcp": {
            "assignedIp": "192.168.1.100",
            "netmask": "255.255.255.0",
            "gateway": "192.168.1.1",
            "dns": "192.168.1.1"
          }
        },
        {
          "ssid": "Nearby_AP",
          "bssid": "00:11:22:33:44:56",
          "rssi": -75,
          "channel": 6,
          "authMode": "OPEN",
          "password": ""
        }
      ]
    },
    {
      "type": "ASSERT_POINT",
      "timeUs": "1500ms",
      "target": "netif:sta:ip",
      "condition": "equals",
      "expected": "192.168.1.100"
    },
    {
      "type": "INJECT_WIFI_FIXTURE",
      "timeUs": "2000ms",
      "fault": {
        "targetSsid": "myssid",
        "action": "DROP_BEACON",
        "reason": "BEACON_TIMEOUT"
      }
    },
    {
      "type": "ASSERT_POINT",
      "timeUs": "2500ms",
      "target": "wifi:sta:state",
      "condition": "equals",
      "expected": "DISCONNECTED"
    }
  ]
}
```

### 2.3 C-ABI 头文件纠偏与结构体定义

#### 1. 根治 `esp_netif_types.h` 二级指针 Bug
```c
/* 彻底恢复乐鑫 v6.1 原厂规范 */
struct esp_netif_obj;
typedef struct esp_netif_obj esp_netif_t;  /* esp_netif_t 是对象，不是指针 */

typedef struct {
    esp_netif_t *esp_netif;                 /* 标准一级指针 */
    esp_netif_ip_info_t ip_info;
    bool ip_changed;
} ip_event_got_ip_t;

/* 补齐多 Netif 实例化 API */
esp_netif_t* esp_netif_create_default_wifi_sta(void);
esp_netif_t* esp_netif_create_default_wifi_ap(void);
esp_err_t    esp_netif_get_ip_info(esp_netif_t *netif, esp_netif_ip_info_t *ip_info);
```

#### 2. 补齐 `esp_wifi.h` 与 `esp_wifi_types.h`
```c
/* 补齐扫描完成事件 */
typedef enum {
    WIFI_EVENT_WIFI_READY = 0,
    WIFI_EVENT_STA_START,
    WIFI_EVENT_STA_STOP,
    WIFI_EVENT_STA_CONNECTED,
    WIFI_EVENT_STA_DISCONNECTED,
    WIFI_EVENT_SCAN_DONE,
    WIFI_EVENT_AP_START,
    WIFI_EVENT_AP_STOP,
    WIFI_EVENT_MAX
} wifi_event_t;

/* 补齐断开事件载荷 */
typedef struct {
    uint8_t ssid[32];
    uint8_t ssid_len;
    uint8_t bssid[6];
    uint8_t reason;                         /* wifi_err_reason_t */
    int8_t  rssi;
} wifi_event_sta_disconnected_t;

/* 补齐扫描记录结构体 */
typedef struct {
    uint8_t bssid[6];
    uint8_t ssid[33];
    uint8_t primary;                        /* channel */
    wifi_second_chan_t second;
    int8_t  rssi;
    wifi_auth_mode_t authmode;
    wifi_cipher_type_t pairwise_cipher;
    wifi_cipher_type_t group_cipher;
    wifi_ant_t ant;
    uint32_t phy_11b:1;
    uint32_t phy_11g:1;
    uint32_t phy_11n:1;
    uint32_t phy_lr:1;
    uint32_t wps:1;
    uint32_t ftm_responder:1;
    uint32_t ftm_initiator:1;
    uint32_t reserved:25;
    wifi_country_t country;
} wifi_ap_record_t;

esp_err_t esp_wifi_scan_get_ap_num(uint16_t *number);
```

### 2.4 三阶段状态机与代际防幽灵机制

为了杜绝异步延迟过程中的时序悬挂，建立严格的阶段代际仲裁模型：

```
                    【STA 三阶段状态机与代际校验】
  esp_wifi_connect()
         │
         ▼ (生成 state_token)
  [Phase 1: Association (关联阶段)]
         │
         ├─ 核对 state_token：若已调用 disconnect / stop / reset ──> [丢弃，直接退出]
         ├─ 密码不匹配 ──> 投递 STA_DISCONNECTED (4WAY_HANDSHAKE_TIMEOUT / AUTH_FAIL)
         ├─ 找不到 SSID ──> 投递 STA_DISCONNECTED (NO_AP_FOUND)
         │
         ▼ 匹配成功
  [投递 WIFI_EVENT_STA_CONNECTED]
         │
         ▼
  [Phase 2: DHCP (地址协商阶段)]
         │
         ├─ 核对 state_token：若状态已非 CONNECTED ──> [丢弃，彻底消除幽灵 GOT_IP]
         │
         ▼ 取得虚拟 IP
  [写入 Netif IP 信息，投递 IP_EVENT_STA_GOT_IP，联动更新 network_is_ready = true]
         │
         ▼
  [Phase 3: Connection Established (稳定运行)]
         │
         └─ 外部注入断网 / 故障 ──> 递增 state_token ──> 投递 STA_DISCONNECTED ──> network_is_ready = false
```

### 2.5 扫描算法模型 (Scan Pipeline)
1. **参数提取**：支持 `wifi_scan_config_t` 中的 `ssid` 过滤与信道过滤；
2. **稳定降序排列**：根据虚拟 AP 表中各 AP 的 `rssi` 字段进行稳定快排（强信号优先返回）；
3. **一次性消费（Consumed 机制）**：扫描完成后产生 `WIFI_EVENT_SCAN_DONE`；调用 `esp_wifi_scan_get_ap_records()` 读取后，将当前扫描快照标记为已消费，再次读取返回空或 `ESP_ERR_WIFI_NOT_INIT`；
4. **数量统计**：`esp_wifi_scan_get_ap_num()` 准确返回快照中符合过滤条件的 AP 总数。

---

## 三、 任务拆分与执行路线图 (WBS 修订版)

```
┌────────────────────────────────────────────────────────────────────────┐
│                        Wi-Fi 战役五阶段执行依赖图                       │
├────────────────────────────────────────────────────────────────────────┤
│ 【阶段 0：T0 注入与观测基建】打通跨仓 Wasm 接口与时间语法规范           │
│       │                                                                │
│       ▼                                                                │
│ 【阶段 1：C-ABI 头文件纠偏】修复 netif 二级指针与补齐 v6.1 结构体      │
│       │                                                                │
│       ▼                                                                │
│ 【阶段 2：第一批交付 (STA)】三阶段确定性状态机与代际防幽灵机制          │
│       │                                                                │
│       ▼                                                                │
│ 【阶段 3：第二批交付 (Scan)】空口扫描、RSSI 降序与消费模型             │
│       │                                                                │
│       ▼                                                                │
│ 【阶段 4：第三批交付 (SoftAP & 级联)】双网卡并存与 MQTT 掉线联动       │
│       │                                                                │
│       ▼                                                                │
│ 【阶段 5：门禁、反异变与全量实证】Gate 5 规则 503/504 与 wifi_sta 实据 │
└────────────────────────────────────────────────────────────────────────┘
```

### 阶段 0：T0 注入与语义观测基建 (Infrastructure & Observability)
- [ ] **任务 T0.1**：规范场景时间语法，全面废除 `µs` 符号，收敛为合规的 `us/ms/s`；
- [ ] **任务 T0.2**：定义 Wasm 导出 C-ABI 注入接口：
  - `sim_wifi_env_inject_ap(const char* json_str)`
  - `sim_wifi_env_inject_fault(const char* json_str)`
  - `sim_wifi_env_reset(void)`
- [ ] **任务 T0.3**：建立语义观测探针，通过 Wasm 导出符号向无头运行器暴露 `wifi:state` 与 `netif:sta:ip`，废除以电压作为网络断言的无效做法。

### 阶段 1：ESP-IDF v6.1 头文件契约彻底纠偏 (Header ABI Alignment)
- [ ] **任务 T1.1**：纠偏 `frameworks/esp_idf/include/esp_netif_types.h`：
  - 恢复 `typedef struct esp_netif_obj esp_netif_t;`；
  - 修正 `ip_event_got_ip_t.esp_netif` 为 `esp_netif_t *` 一级指针；
- [ ] **任务 T1.2**：补齐 `include/esp_wifi.h` 与 `include/esp_wifi_types.h`：
  - 添加 `WIFI_EVENT_SCAN_DONE` 枚举；
  - 补全 `wifi_event_sta_disconnected_t` 结构体；
  - 补全 `wifi_ap_record_t` 与 `esp_wifi_scan_get_ap_num()` 原型声明；
- [ ] **任务 T1.3**：维护 `channels.json` 与 `check_harvested_headers.py`，确保资产一致性校验 0 error。

### 阶段 2：【第一批交付】STA 三阶段确定性状态机与 Netif 驱动 (Batch 1: STA Core)
- [ ] **任务 T2.1**：在 `frameworks/esp_idf/src/wifi/` 下实现 `sim_wifi_env.h/.c`：
  - 8-AP POD 静态表驱动，0 动态堆分配；
  - 支持虚拟 DHCP 分配器（依据 AP 网段分配 IP、掩码、网关、DNS）；
- [ ] **任务 T2.2**：重构 `src/wifi/esp_wifi.c` STA 状态机：
  - 移除硬编码 `192.168.1.100`；
  - 实现 Association $\to$ DHCP $\to$ Disconnect 三阶段状态推进；
  - 引入 `state_generation_token`，在 `disconnect`/`stop` 时使在途事件失效，彻底杜绝“幽灵 GOT_IP”；
  - 对齐 `CONFIG_ESP_WIFI_SSID="myssid"` / `"mypassword"`；支持握手超时等细分原因码。

### 阶段 3：【第二批交付】空口扫描（Scan）子系统 (Batch 2: Scan Pipeline)
- [ ] **任务 T3.1**：在 `sim_wifi_env` 中实现扫描检索逻辑：
  - 实现基于入参 `wifi_scan_config_t` 的 SSID 与 Channel 过滤；
  - 实现基于 RSSI 的稳定降序排序算法；
- [ ] **任务 T3.2**：在 `esp_wifi.c` 中落地扫描 API：
  - `esp_wifi_scan_start()` 触发扫描并派发 `WIFI_EVENT_SCAN_DONE`；
  - `esp_wifi_scan_get_ap_num()` 返回候选总数；
  - `esp_wifi_scan_get_ap_records()` 实现一次性安全消费拷贝。

### 阶段 4：【第三批交付】SoftAP / APSTA 与底层网络状态联动 (Batch 3: SoftAP & Linkage)
- [ ] **任务 T4.1**：重构 `src/wifi/esp_netif.c` 为多槽位数组（支持最大 4 个实例）：
  - 独立实例化 `esp_netif_create_default_wifi_sta()` 与 `esp_netif_create_default_wifi_ap()`；
  - 保证 STA 与 SoftAP 独立句柄、独立 MAC 与独立 IP 空间；
- [ ] **任务 T4.2**：实现网络状态级联就绪总线（`sim_network_broker`）：
  - 当 STA 取得 IP 时，广播网络物理就绪；
  - 当 STA 掉线时，置低网络就绪状态；
  - 联动 `src/network/esp_mqtt.c`：当网络断开时，使处于连接态的 MQTT 客户端正确触发断链回调，消除假连。

### 阶段 5：门禁强化、反异变防御与全量无头实证 (Verification & Governance)
- [ ] **任务 T5.1**：在 `.governance/gates/rules/` 部署：
  - `g5_no_wifi_inline_mock.py`（Rule 503）：阻断 C 代码硬编码 IP 与密码；
  - `g5_wifi_assertion_quality.py`（Rule 504）：检查 Wi-Fi 语义断言有效性；
- [ ] **任务 T5.2**：编写反异变单元测试：篡改虚拟 AP 密码与期望分配 IP，验证断言能够 100% 触发红灯；
- [ ] **任务 T5.3**：在 `checklist.data.json` 中为 `#006 wifi_sta` 明确区分 `wasm_node` 证据条目；
- [ ] **任务 T5.4**：更新 `test_esp_wifi.c` 并执行 `ctest -L esp_idf`，运行 `run_esp32_headless_evidence.ps1 -App wifi_sta` 输出确定性实据，全部达成 100% 通过后更新 `capability-catalog.yaml` 为 `implemented`。

---

## 四、 风险分析与规避对策 (修订版)

| 风险场景 | 风险等级 | 诱发原因 | 预防与规避对策 |
|---|:---:|---|---|
| **R-1：应用因缺少异步调度间隙导致事件错过** | 🟡 中 | 原厂代码依赖 FreeRTOS 任务调度间隙注册事件处理器 | 事件派发必须走 `esp_event_post()` 投递至系统默认事件循环任务队列，由调度器按虚拟时间总序仲裁，严禁同步调用用户回调 |
| **R-2：扫描数据并发消费争用** | 🟡 中 | 多任务同时调用 `esp_wifi_scan_get_ap_records` | 引入单写单读标记与原子快照锁，二次读取直接返回 `ESP_ERR_WIFI_NOT_INIT` 或空记录 |
| **R-3：MQTT 级联过敏** | 🟡 中 | Wi-Fi 偶发重连抖动导致 MQTT 频繁重置 | 状态联动增加 50ms 滞后滤波缓冲区，确保真正确认断网后再广播失效 |

---

## 五、 结论与后续里程碑规划

本实施计划（v2.0 终审版）彻底吸纳了架构组的深度白盒评审意见，消除了二级指针扭曲、时间调度误判、幽灵事件、证据偷换与断言失真等全部结构性缺陷。

- **Milestone 1（当前计划）**：按 Batch 1 $\to$ 2 $\to$ 3 递进交付 STA 三阶段状态机、扫描子系统、SoftAP 与传输层就绪联动；
- **Milestone 2（后续规划）**：推进 SmartConfig / WPS 配网握手仿真与多 STA 动态漫游网络拓扑。
