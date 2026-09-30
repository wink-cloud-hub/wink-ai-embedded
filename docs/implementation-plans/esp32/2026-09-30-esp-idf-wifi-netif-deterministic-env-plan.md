<!-- SPDX-License-Identifier: Apache-2.0 -->
# 实施计划：ESP-IDF Wi-Fi 与 Netif 确定性虚拟空口与多网卡重构战役 (v1.0)

| 字段 | 内容 |
|---|---|
| 计划编号 | PLAN-20260930-ESP-IDF-WIFI-NETIF-DETERMINISTIC-ENV-v1.0 |
| 状态 | 📝 **Ready for Review（等待用户与架构评审确认）** |
| 日期 | 2026-09-30 |
| 周期估算 | 3~4 个工作日（分 Step 1 ~ 5 递进执行） |
| 优先次序 | **场景契约与治理定义 → 声明式虚拟空口引擎 → 原厂 C-ABI 门面纠偏 → 静态防腐门禁 → 官方示例无头实证** |
| 决策依据 | [ADR-0012：契约诚实优于静默降级（Fail-Loud 原则）](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)<br>[ADR-0002：双 Target 同源编译原则](../../decisions/unisim/0002-dual-target-compilation.md)<br>[ADR-0003：生产口径与保真边界约束（永不承诺虚实恒等）](../../decisions/unisim/0003-simulation-fidelity-boundary.md)<br>[ADR-0004：编译期静态分发优于运行时函数指针](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md)<br>[ADR-0045：仿真内存配额与异常故障策略](../../decisions/unisim/0045-simulation-memory-quota-and-fault-policy.md)<br>[ADR-0053：虚拟时间因果同刻总序仲裁模型](../../decisions/unisim/0053-sim-same-timestamp-event-total-order.md)<br>[ADR-0089：分类记账堆内存与边界防御模型](../../decisions/core/0089-esp-idf-heap-caps-allocation-contract.md)<br>[ADR-0091：多配置实例与五维正交 Schema 架构决策](../../decisions/unisim/0091-esp-idf-multi-config-orthogonal-schema.md)<br>[ADR-0092：ESP-IDF 官方示例仿真治理前置筑基宪章](../../decisions/unisim/0092-esp-idf-simulation-governance-and-capability-charter.md) |
| 管辖数据源 | [`checklist.data.json`](../../../wink-micro-app/vendor/esp_idfv61/.governance/data/checklist.data.json)、[`capability-catalog.yaml`](../../../wink-micro-app/vendor/esp_idfv61/.governance/catalog/capability-catalog.yaml)、[`.governance/gates/`](../../../wink-micro-app/vendor/esp_idfv61/.governance/gates/) |
| 实施目标文件 | `frameworks/esp_idf/src/wifi/*`、`frameworks/esp_idf/include/esp_wifi.h`、`esp_wifi_types.h`、`esp_netif.h`、`esp_netif_types.h`、`.governance/gates/rules/g5_*.py` |
| 验收门禁 | `ctest -L esp_idf`、`python .governance/gates/run_gates.py --mode nightly`、`powershell run_esp32_headless_evidence.ps1 -App wifi_sta` |

---

## 一、 战略总目标与全局验收标准 (DoD)

### 1.1 现状痛点与重构要旨
当前 `wink-micro-os/frameworks/esp_idf/src/wifi/esp_wifi.c` 存在严重的“脆弱闭环与架构硬编码”问题：
1. **裸延时任务与时间失步**：内部创建 FreeRTOS 任务通过 `vTaskDelay(100ms)` 发送 `STA_CONNECTED`，再延时 100ms 发送 `GOT_IP`。在虚拟时钟快进或单步调度下，裸延时任务无法与 UniSim 调度器严格步调同步；
2. **写死虚构 IP 与单网卡死结**：代码中直接硬编码 `192.168.1.100`，且不支持真实的多网卡共存（STA + SoftAP 互斥冲突，无法双网卡并发）；
3. **异常分支与配网完全不可测**：无法模拟“密码输错（`AUTH_FAIL`）”、“找不到 AP（`NO_AP_FOUND`）”、“路由器断电（`BEACON_TIMEOUT`）”、“DHCP 租期耗尽”，导致上层业务代码的重试、配网（SmartConfig/AP 配网）与状态恢复逻辑在仿真中形同虚设；
4. **空口扫描（Scan）缺失**：`esp_wifi_scan_start()` 与 `esp_wifi_scan_get_ap_records()` 处于空桩或受限状态，无法支撑扫描周围热点的常用功能。

**重构核心要旨**：
借鉴已落地的 HTTP/MQTT 战役经验，**将 Wi-Fi 物理空口行为抽象为“声明式虚拟空口环境引擎 (`sim_wifi_env`)”**。应用层 `esp_wifi_*` 与 `esp_netif_*` 原厂 C-ABI 保持 100% 稳定，底层通过 `.scenario.json` 中的标准 `INJECT_WIFI_FIXTURE` 步骤注入虚拟空间中存在的路由器热点与网络属性，实现真正确定性、多分支的无头自动化测试。

### 1.2 全局验收标准 (DoD)
- [ ] **G-01（C 门面彻底清除硬编码 IP 与密码）**：`src/wifi/` 下所有 C 文件严禁出现具体的硬编码 IPv4 字符串（如 `192.168.1.100`）、固定 MAC 地址或 if-else 业务特判，一律由底层虚拟环境与场景动态驱动，由 Gate 5 静态扫描规则严格阻断；
- [ ] **G-02（统一场景注入契约）**：确立 `INJECT_WIFI_FIXTURE` 标准 step 语法，支持声明虚拟 AP 列表（SSID、BSSID、RSSI、AuthMode、Password、DHCP 网段）及运行时信标中断故障；
- [ ] **G-03（全状态机与全异常路径保真）**：
  - 密码正确 $\to$ 状态机流转 `WIFI_EVENT_STA_START` $\to$ `WIFI_EVENT_STA_CONNECTED` $\to$ `IP_EVENT_STA_GOT_IP`；
  - 密码错误 $\to$ 准确触发 `WIFI_EVENT_STA_DISCONNECTED`，`reason = WIFI_REASON_AUTH_FAIL`；
  - 未知 SSID $\to$ 准确触发 `WIFI_EVENT_STA_DISCONNECTED`，`reason = WIFI_REASON_NO_AP_FOUND`；
  - 动态断网注入 $\to$ 触发 `WIFI_REASON_BEACON_TIMEOUT`，验证重连与指数退避机制；
- [ ] **G-04（真实空口扫描能力）**：`esp_wifi_scan_start()` 完整支持异步与同步扫描，`esp_wifi_scan_get_ap_records()` 正确返回场景声明的多个 AP 记录（包含精确的信道与 RSSI）；
- [ ] **G-05（STA + SoftAP 双网卡并发共存）**：`esp_netif_t` 改为标准实例句柄模型，支持同时实例化 Default STA 与 Default SoftAP，各自分配独立 IP 信息与 MAC，零互锁冲突；
- [ ] **G-06（零动态堆分配内存池）**：AP 表与 Netif 列表使用定容 POD 静态数组（上限 8 个虚拟 AP，上限 4 个 Netif），严格满足 ADR-0045 与 ADR-0089；
- [ ] **G-07（双 Target 编译与门禁全绿）**：Host Native 与 Wasm32 同源编译通过，`ctest -L esp_idf` 100% 通过，`wifi_sta` 官方 carrier 示例无头实证毫秒级确定性通过。

---

## 二、 核心架构设计方案

### 2.1 架构分层与数据流图

```
+─────────────────────────────────────────────────────────────────────────────────────────────+
│                         ESP-IDF 跨靶仿真 Wi-Fi 与 Netif 架构模型                              │
+─────────────────────────────────────────────────────────────────────────────────────────────+
│  [应用业务层]  原生业务代码 / CHECKLIST 官方示例 (wifi_sta / softAP / scan / smartconfig)   │
+─────────────────────────────────────────────────────────────────────────────────────────────+
│  [原厂 C-ABI]  frameworks/esp_idf/src/wifi/                                                  │
│                ├─ esp_wifi.c  (esp_wifi_init/start/connect/scan/set_config/disconnect)       │
│                └─ esp_netif.c (esp_netif_init/create_default_wifi_*/get_ip_info)            │
+─────────────────────────────────────────────────────────────────────────────────────────────+
│                                              │
│                                              ▼
│  [虚拟环境层]  sim_wifi_env (声明式虚拟空口与网络协议引擎)                                   │
│                ├─ 虚拟 AP 列表 (POD 定长表，最大 8 个热点，存储 SSID/BSSID/RSSI/AuthMode/Pwd)  │
│                ├─ 虚拟 DHCP 引擎 (根据网段自动派发 IP、掩码、网关、DNS)                      │
│                ├─ 状态机与事件泵 (基于虚拟时钟精准触发 esp_event_post，废除裸任务延时)        │
│                └─ 故障注入拦截器 (动态注入断网、丢包、弱信号 -95dBm、握手失败)               │
+─────────────────────────────────────────────────────────────────────────────────────────────+
                                               ▲
                                               │ (场景剧本注入)
+─────────────────────────────────────────────────────────────────────────────────────────────+
│  [场景剧本层]  unisim-scenarios/<app>.scenario.json                                          │
│                └─ steps: [ { "type": "INJECT_WIFI_FIXTURE", "accessPoints": [...] } ]       │
+─────────────────────────────────────────────────────────────────────────────────────────────+
```

### 2.2 统一场景注入链路契约 (`scenario.json`)

在 `unisim-scenarios/<app>.scenario.json` 的 `steps` 中引入标准 `INJECT_WIFI_FIXTURE` 步骤：

```json
{
  "header": {
    "version": "1.0.0",
    "name": "ESP-IDF v6.1 Wi-Fi STA Connection & Scan Deterministic Proof",
    "templateId": "esp_idfv61_wifi_sta",
    "accuracyMode": "behavioral",
    "timeoutUs": "3000000",
    "failurePolicy": "fail-fast",
    "determinism": { "prngSeed": 42 }
  },
  "steps": [
    {
      "type": "INJECT_WIFI_FIXTURE",
      "timeUs": "0ms",
      "accessPoints": [
        {
          "ssid": "Target_AP",
          "bssid": "00:11:22:33:44:55",
          "rssi": -55,
          "channel": 6,
          "authMode": "WPA2_PSK",
          "password": "valid_password",
          "dhcp": {
            "assignedIp": "192.168.10.125",
            "netmask": "255.255.255.0",
            "gateway": "192.168.10.1",
            "dns": "192.168.10.1"
          }
        },
        {
          "ssid": "Guest_Free_WiFi",
          "bssid": "00:11:22:33:44:56",
          "rssi": -78,
          "channel": 1,
          "authMode": "OPEN",
          "password": ""
        }
      ]
    },
    {
      "type": "INJECT_WIFI_FIXTURE",
      "timeUs": "1500000µs",
      "fault": {
        "targetSsid": "Target_AP",
        "action": "DROP_BEACON",
        "reason": "BEACON_TIMEOUT"
      }
    },
    {
      "type": "ASSERT_POINT",
      "timeUs": "2000000µs",
      "target": "power:VCC_WIFI",
      "condition": "equals",
      "expected": 3.3
    }
  ]
}
```

### 2.3 核心数据结构与 C-ABI 抽象 (`sim_wifi_env.h`)

所有内部结构严格遵从 POD 原则，完全规避 `malloc`，内联定长静态槽位管理：

```c
/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_wifi_types.h"
#include "esp_netif_types.h"

#define SIM_WIFI_MAX_APS        8
#define SIM_WIFI_MAX_NETIFS     4
#define SIM_WIFI_SSID_MAX_LEN   32
#define SIM_WIFI_PASS_MAX_LEN   64

/* 虚拟 AP 实体 */
typedef struct {
    char ssid[SIM_WIFI_SSID_MAX_LEN + 1];
    uint8_t bssid[6];
    int8_t rssi;
    uint8_t channel;
    wifi_auth_mode_t authmode;
    char password[SIM_WIFI_PASS_MAX_LEN + 1];
    esp_netif_ip_info_t dhcp_ip_info;
    bool in_use;
} sim_wifi_ap_entry_t;

/* 虚拟 Netif 实体 */
typedef struct {
    uint8_t id;
    bool is_active;
    wifi_interface_t wifi_if;
    esp_netif_ip_info_t ip_info;
    uint8_t mac[6];
    char if_key[16];
} sim_netif_slot_t;

/* 虚拟 Wi-Fi 全局上下文 */
typedef struct {
    bool is_initialized;
    wifi_mode_t mode;
    wifi_config_t sta_cfg;
    wifi_config_t ap_cfg;
    
    /* 虚拟 AP 池 */
    sim_wifi_ap_entry_t aps[SIM_WIFI_MAX_APS];
    uint8_t ap_count;
    
    /* Netif 槽位池 */
    sim_netif_slot_t netifs[SIM_WIFI_MAX_NETIFS];
    
    /* 运行态属性 */
    bool is_connected;
    uint8_t connected_ap_index;
    uint32_t state_generation_token;
} sim_wifi_env_t;

/* C 接口声明 */
void sim_wifi_env_reset(void);
esp_err_t sim_wifi_env_register_ap(const sim_wifi_ap_entry_t *ap);
esp_err_t sim_wifi_env_trigger_fault(const char *ssid, wifi_err_reason_t reason);
sim_wifi_env_t* sim_wifi_env_get_instance(void);
```

### 2.4 原厂 C-ABI 门面行为与状态机模型

彻底淘汰依赖真实时钟的后台任务，接入 UniSim 虚拟时钟与 `esp_event_post` 同刻总序队列：

1. **`esp_wifi_connect()` 时序**：
   - 检查 `sim_wifi_env` 的虚拟 AP 池；
   - **命中且密码匹配**：
     立即通过 `esp_event_post` 派发 `WIFI_EVENT / WIFI_EVENT_STA_CONNECTED`；
     虚拟 DHCP 引擎为默认 STA netif 写入配置的 IP 信息，紧接着派发 `IP_EVENT / IP_EVENT_STA_GOT_IP`；
   - **命中但密码不匹配**：
     派发 `WIFI_EVENT / WIFI_EVENT_STA_DISCONNECTED`，事件携带 `wifi_event_sta_disconnected_t`，其中 `reason = WIFI_REASON_AUTH_FAIL`；
   - **未找到目标 SSID**：
     派发 `WIFI_EVENT / WIFI_EVENT_STA_DISCONNECTED`，其中 `reason = WIFI_REASON_NO_AP_FOUND`；
2. **`esp_wifi_scan_start()` 时序**：
   - 同步或异步遍历 `sim_wifi_env` 中的所有有效 AP 条目；
   - 触发 `WIFI_EVENT_SCAN_DONE`；
   - `esp_wifi_scan_get_ap_records()` 依序填充 `wifi_ap_record_t` 结构体数组（包含 SSID、BSSID、RSSI、AuthMode、Channel）。

---

## 三、 任务拆分与执行路线图 (WBS)

```
┌────────────────────────────────────────────────────────────────────────┐
│                        Wi-Fi 与 Netif 战役实施路线图                    │
├────────────────────────────────────────────────────────────────────────┤
│ 【阶段一：治理图谱与契约规整】扩展 capability-catalog 与 Schema 规范   │
│       │                                                                │
│       ▼                                                                │
│ 【阶段二：虚拟空口与 DHCP 引擎】实现 sim_wifi_env.h/.c 与全状态机      │
│       │                                                                │
│       ▼                                                                │
│ 【阶段三：门面双向重构与 ABI 纠偏】重写 esp_wifi.c 与 esp_netif.c       │
│       │                                                                │
│       ▼                                                                │
│ 【阶段四：门禁强化与反异变防御】扩展 Gate 5 (Rule 503/504) 与测试套件  │
│       │                                                                │
│       ▼                                                                │
│ 【阶段五：官方用例实证与全量回归】wifi_sta/softAP 无头实证与 CTest 闭环 │
└────────────────────────────────────────────────────────────────────────┘
```

### 阶段一：治理图谱与契约规整 (Governance & Schema Alignment)
- [ ] **任务 T1.1**：在 [`capability-catalog.yaml`](../../../wink-micro-app/vendor/esp_idfv61/.governance/catalog/capability-catalog.yaml) 中追加并规范能力定义：
  - `cap.wifi.sta_mock`（layer: facade, status: implemented, owned_paths: `frameworks/esp_idf/src/wifi/esp_wifi.c`）
  - `cap.wifi.ap_mock`（layer: facade, status: implemented, owned_paths: `frameworks/esp_idf/src/wifi/esp_wifi.c`）
  - `cap.wifi.scan_mock`（layer: facade, status: implemented, owned_paths: `frameworks/esp_idf/src/wifi/esp_wifi.c`）
  - `cap.net.netif_multi`（layer: facade, status: implemented, owned_paths: `frameworks/esp_idf/src/wifi/esp_netif.c`）
- [ ] **任务 T1.2**：在 `checklist.data.json` 中审视并更新所有 Wi-Fi 关联条目（如 `#006 wifi_sta`、`#007 softAP`、`#008 scan`）的 `required_capabilities`；
- [ ] **任务 T1.3**：在 `PLAYBOOK.md` 中增补 `INJECT_WIFI_FIXTURE` 的标准 step 规范与字段校验逻辑。

### 阶段二：虚拟空口与 DHCP 引擎 (Virtual Air & DHCP Core)
- [ ] **任务 T2.1**：在 `frameworks/esp_idf/src/wifi/` 下实现 `sim_wifi_env.h` 与 `sim_wifi_env.c`：
  - 实现定长 8-AP POD 存储与增删查清空接口；
  - 实现基于网段的虚拟 DHCP 分配器（默认网关、子网掩码、DNS、静态 IP 支持）；
  - 实现主动故障注入接口（`DROP_BEACON`、`AUTH_FAIL`）；
- [ ] **任务 T2.2**：在 `frameworks/esp_idf/include/` 中导出公共测试/驱动桩头 `sim_wifi_env.h`，并同步在 `channels.json` 的 `handwritten` 清单中完成资产登记（符合 ADR-0087）。

### 阶段三：门面双向重构与 ABI 纠偏 (Facade & ABI Alignment)
- [ ] **任务 T3.1**：重构 `frameworks/esp_idf/src/wifi/esp_wifi.c`：
  - 彻底删除 `wifi_connect_task` 裸延时任务，消除 `vTaskDelay`；
  - 接入 `sim_wifi_env`，使 `esp_wifi_connect()` 按照密码与 SSID 匹配真实触发成功或断开事件；
  - 完整实现 `esp_wifi_scan_start()` 与 `esp_wifi_scan_get_ap_records()`，支持被动/主动空口多 AP 扫描枚举；
  - 支持 `WIFI_MODE_AP` 与 `WIFI_MODE_APSTA` 模式切换；
- [ ] **任务 T3.2**：重构 `frameworks/esp_idf/src/wifi/esp_netif.c`：
  - 将单例静态结构重构为多槽位数组（支持最大 4 个 Netif）；
  - 真实实现 `esp_netif_create_default_wifi_sta()` 与 `esp_netif_create_default_wifi_ap()` 的独立句柄返回与信息绑定；
  - 确保 `esp_netif_get_ip_info()` 精确返回对应 Netif 的 IP/掩码/网关。

### 阶段四：门禁强化与反异变防御 (Gate 5 & Mutation Tests)
- [ ] **任务 T4.1**：在 `.governance/gates/rules/` 下新增 `g5_no_wifi_inline_mock.py`（Rule 503）：
  - 静态 AST 检查：严禁在 `src/wifi/` 中出现具体硬编码 IP 字符串（如 `192.168.`）、固定密码字面量；
- [ ] **任务 T4.2**：在 `.governance/gates/rules/` 下新增 `g5_wifi_assertion_quality.py`（Rule 504）：
  - 检查 Wi-Fi 场景剧本的断言深度与有效性；
- [ ] **任务 T4.3**：编写反异变单元测试：篡改虚拟 AP 密码与期望 IP，验证断言能够 100% 触发红灯拦截。

### 阶段五：用例矩阵实证与全量回归 (Verification & Sign-off)
- [ ] **任务 T5.1**：为 `wifi/getting_started/station` 编写合规的 `wifi_sta.scenario.json`；
- [ ] **任务 T5.2**：为 `wifi/getting_started/softAP` 编写合规的 `softap.scenario.json`；
- [ ] **任务 T5.3**：更新/扩充 `test_esp_wifi.c` 核心单元测试，覆盖正常连接、密码错误断开、找不到 AP、主动扫描、STA+SoftAP 共存等 10+ 测试用例；
- [ ] **任务 T5.4**：执行 `ctest -L esp_idf` 与 `run_gates.py --mode nightly` 达成全绿；通过 `run_esp32_headless_evidence.ps1` 输出官方示例无头确定性实据。

---

## 四、 风险分析与规避对策

| 风险场景 | 风险等级 | 诱发原因 | 预防与规避对策 |
|---|:---:|---|---|
| **R-1：应用因依赖任务异步延时导致的时序竞争** | 🟡 中 | 某些官方示例可能依赖 connect 后数十毫秒的调度间隙来注册 handler | 在 `esp_wifi_connect()` 中使用标准 `esp_event_post`（投递至默认事件循环队列），由调度器总序分发，确保与真实 FreeRTOS 异步语义严格等价 |
| **R-2：扫描记录内存溢出** | 🟡 中 | 用户传入的 `ap_records` 缓冲区小于虚拟空口中的 AP 总数 | 严格遵守原厂规范：入参 `number` 既是输入缓冲区最大容量也是输出有效条数，取 `min(*number, actual_count)` 截断拷贝，绝不越界 |
| **R-3：多 Netif 实例指针混淆** | 🟡 中 | STA 与 SoftAP 销毁时未成对操作导致悬挂句柄 | 为 Netif 句柄引入带代际编号的紧凑 Handle 校验，销毁后置空槽位并标记无效 |

---

## 五、 结论与后续衔接

完成本计划后，ESP-IDF 的 Wi-Fi 与 Netif 将**彻底摆脱脆弱闭环与写死 IP 的初级状态**，升级为具备企业级保真度的声明式虚拟空口系统，能够支撑全品类 Wi-Fi 业务代码、异常重试与配网逻辑的严密验证，并与上层的 HTTP/MQTT 管道形成无缝的端到端联调闭环。
