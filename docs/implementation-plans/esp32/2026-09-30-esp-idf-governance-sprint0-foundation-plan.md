<!-- SPDX-License-Identifier: Apache-2.0 -->
# 实施计划：ESP-IDF 官方示例治理前置筑基战役 (Sprint 0)

| 字段 | 内容 |
|---|---|
| 计划编号 | PLAN-20260930-ESP-IDF-GOVERNANCE-SPRINT0-FOUNDATION-v1.0 |
| 状态 | **Completed（已完成收官）** |
| 日期 | 2026-09-30 |
| 周期估算 | 5~7 个工作日（分 Step 1 ~ 5 递进执行） |
| 决策依据 | [ADR-0092：ESP-IDF 官方示例仿真治理前置筑基与全景能力图谱宪章](../../decisions/unisim/0092-esp-idf-simulation-governance-and-capability-charter.md)<br>[ADR-0091：多配置实例与五维正交 Schema 架构决策](../../decisions/unisim/0091-esp-idf-multi-config-orthogonal-schema.md)<br>[ADR-0090：集中式可插拔门禁系统](../../decisions/unisim/0090-centralized-pluggable-gate-system.md)<br>[ADR-0072：双时钟域与配额片调度器](../../decisions/core/0072-dual-timebase-and-event-loop-scheduler.md)<br>[ADR-0012：契约诚实优于静默降级](../../decisions/core/0012-contract-honesty-over-silent-degradation.md) |
| 管辖数据源 | [`checklist.data.json`](../../../wink-micro-app/vendor/esp_idfv61/checklist.data.json)、[`capability-catalog.yaml`](../../../wink-micro-app/vendor/esp_idfv61/capability-catalog.yaml)、[`.gates/quarantine.yaml`](../../../wink-micro-app/vendor/esp_idfv61/.gates/quarantine.yaml) |
| 验收门禁 | `.gates/run_gates.py` (Gate 1~4 全量规则)、`winkcli lint --pack layering --pack api`、`check_license_map.py` |

---

## 一、 战略总目标与全局验收标准

在正式拉开 478 个官方示例的规模化迁移大幕前，本计划旨在执行**筑基攻坚战（Sprint 0）**，彻底消除“能力字典盲区”、“285 项未知范围”与“469 项能力空白”，并在仿真内核中植入四大防腐安全气囊，形成**零遗漏、零偏差、自愈型**的工程底座。

### 全局验收标准（DoD）

- [x] **G-01（能力字典全景覆盖）**：[`capability-catalog.yaml`](../../../wink-micro-app/vendor/esp_idfv61/capability-catalog.yaml) 从 31 项扩充至 58 项，覆盖 15+ 个完整技术领域（彻底消灭蓝牙、休眠、加密、存储等大类盲区），每项能力严格绑定 Tier 1/2/3 归宿、`owned_headers` 与 `owned_symbols`；
- [x] **G-02（范围盲区完全清零）**：`checklist.data.json` 中的 `scope_unknown: 285` **彻底清零（0 unknown）**，全量 478 个条目基于物理介质与功能决策树完成终审确权（划分为 `in_scope`、`out_of_scope` 或 `deferred`）；
- [x] **G-03（能力依赖 100% 闭环）**：全量 478 个条目的 `required_capabilities` 均由静态依赖提取器结合人工审核完成打标，**彻底消除空能力数组（0 empty entries）**；
- [x] **G-04（并发安全气囊注入）**：在 FreeRTOS 仿真门面中落地自旋锁 `portMUX_TYPE` 记账，持锁期间调用让步 API 触发运行时不可屏蔽断言，彻底阻断并发假阳性；
- [x] **G-05（忙等死锁自愈注入）**：在 `esp_rom_delay_us()` 及总线忙等宏内落地配额让步注入（Yield Injection），微秒死循环自动推进系统虚拟时间，杜绝调度器冻结死锁；
- [x] **G-06（纯内存虚拟 VFS 基础模型）**：落地纯 RAM Inode 树与 Partition Table 虚拟分区块映射沙箱，支撑后续 27 个存储用例；
- [x] **G-07（虚拟外设应答器规范）**：实现 I2C / SPI 通用外设应答桩接口及基础 AT24C02 EEPROM 应答模型，解决高级外设无物理响应启动即崩溃痛点；
- [x] **G-08（存量隔离区债务清零）**：补齐当前处于 `.gates/quarantine.yaml` 隔离区中的 10 个条目的真实验证凭证与哈希，**隔离区白名单完全清空（0 quarantined debts）**；
- [x] **G-09（CI 门禁 100% 全绿）**：`python .gates/run_gates.py --mode nightly` 全量通过（0 errors, 0 warnings），许可门禁与分层 Lint 零缺陷。

---

## 二、 任务拆分与执行路线图

```
┌────────────────────────────────────────────────────────────────────────┐
│                        Sprint 0 实施阶段依赖图                         │
├────────────────────────────────────────────────────────────────────────┤
│ 【阶段一：自动化工具】实现静态依赖提取器 (extract_example_dependencies.py)│
│       │                                                                │
│       ▼                                                                │
│ 【阶段二：字典重构】扩充 capability-catalog.yaml (覆盖 18+ 领域/65+能力) │
│       │                                                                │
│       ▼                                                                │
│ 【阶段三：数据治理】478 示例范围清障 (消灭 unknown) + 能力全面打标      │
│       │                                                                │
│       ▼                                                                │
│ 【阶段四：内核气囊】注入自旋锁记账 + 忙等让步 + 内存 VFS + 虚拟应答器  │
│       │                                                                │
│       ▼                                                                │
│ 【阶段五：隔离区清零】回填 10 个存量凭据，quarantine.yaml 归零，全线验收 │
└────────────────────────────────────────────────────────────────────────┘
```

---

### 阶段一：自动化工具建设 — 机器静态依赖与符号提取器

**目标**：杜绝人工猜测导致的遗漏，编写专门的自动化脚本，提取 478 个示例的客观头文件依赖与关键 API 符号。

#### 任务 T1.1：编写 `tools/esp32/extract_example_dependencies.py`
- [x] 解析 `checklist.data.json` 获取 478 个官方示例的 `upstream_path` 与分类；
- [x] 基于已收割的头文件知识库与上游目录结构，提取每个用例的核心特征：
  - 顶层业务技术分类（Bluetooth、Peripherals、System、Protocols、Storage、Security、WiFi 等）；
  - 核心包含的头文件（如 `esp_wifi.h`、`nvs_flash.h`、`driver/i2c_master.h`、`esp_bt.h`）；
  - 调用的代表性 API 符号模式（如 `esp_ble_*`、`vTaskDelay`、`nvs_get_*`）；
  - 是否依赖不可逆物理介质（物理以太网 PHY、Efuse 烧录、射频微波测距、外接特定摄像头）；
- [x] 输出机器推导报告与候选映射草稿 `output_dependency_draft.json`。

---

### 阶段二：能力图谱字典（SSOT）全景扩充与依赖图拓扑化

**目标**：将 `capability-catalog.yaml` 从 31 项扩充至 58 项，彻底覆盖 ESP-IDF v6.1 官方示例涉及的全部技术模块。

#### 任务 T2.1：扩充补齐缺失核心能力域
在 [`capability-catalog.yaml`](../../../wink-micro-app/vendor/esp_idfv61/capability-catalog.yaml) 中新增以下原子能力定义，并严格标定 Tier 1/2/3 归宿层级：
- [x] **蓝牙与无线协议（`cap.ble.*`, `cap.mesh.*`）**：
  - `cap.ble.gap_adv`（广播/扫描状态机，layer: model）
  - `cap.ble.gatt_server`（GATT 服务端属性表与回调，layer: model）
  - `cap.ble.gatt_client`（GATT 客户端发现与读写，layer: model）
  - `cap.ble.smp_security`（安全配对与密钥加密，layer: model）
  - `cap.mesh.esp_now`（ESP-NOW 局域无连接通信帧通道，layer: model）
- [x] **电源管理与低功耗（`cap.pm.*`）**：
  - `cap.pm.light_sleep`（CPU 纤程暂停与微秒定时唤醒，layer: core_sim）
  - `cap.pm.deep_sleep`（上下文断电与 RTC GPIO 唤醒存根，layer: core_sim）
  - `cap.pm.dynamic_freq`（动态时钟频率调节存根，layer: facade）
- [x] **现代外设与总线流（`cap.pulse.*`, `cap.bus.*`, `cap.media.*`）**：
  - `cap.pulse.mcpwm_motor`（六通道死区互补电机控制，layer: model）
  - `cap.pulse.ledc_fade`（LEDC 渐变与占空比定点调制，layer: pal）
  - `cap.pulse.touch_pad`（电容触摸按键状态机，layer: pal）
  - `cap.bus.parlio`（并行输入输出总线，layer: model）
  - `cap.bus.temp_sensor`（芯片内置温度传感器采样，layer: pal）
  - `cap.media.camera_dma`（虚拟图像帧捕获与注入管道，layer: model）
- [x] **存储与文件系统深化（`cap.vfs.*`, `cap.storage.*`）**：
  - `cap.vfs.fatfs_vfs`（FATFS 文件系统挂载与操作，layer: model）
  - `cap.storage.wear_levelling`（Flash 磨损均衡层模拟，layer: model）
  - `cap.storage.sdmmc_host`（SD 卡与 SDIO 主机协议模拟，layer: model）
  - `cap.storage.partition_api`（多分区表寻址与只读映射，layer: model）
- [x] **加密与安全（`cap.crypto.*`）**：
  - `cap.crypto.mbedtls_shim`（mbedTLS 软算法跨平台桥接，layer: host_bridge）
  - `cap.crypto.hw_sha_aes`（硬件加速器只读校验，layer: facade）
- [x] **系统与通用通信（`cap.system.*`, `cap.usb.*`）**：
  - `cap.system.ota_update`（OTA 双分区回滚与固件校验，layer: model）
  - `cap.system.app_trace`（系统运行时跟踪与内存分析，layer: core_sim）
  - `cap.usb.cdc_acm`（USB-CDC 虚拟串口流，layer: model）
  - `cap.usb.serial_jtag`（USB-Serial/JTAG 硬件桥接，layer: model）

#### 任务 T2.2：构建 `depends_on` 强拓扑链与管辖符号索引
- [x] 为每个新增能力梳理前置依赖（`mandatory` 数组）；
- [x] 为每个能力补全 `owned_headers`（头文件列表）与 `owned_symbols`（核心 API 前缀）；
- [x] 运行 Gate 1 校验确保 Catalog YAML 格式与依赖引用拓扑无循环依赖。

---

### 阶段三：478 项官方示例范围清障与全量依赖打标

**目标**：彻底消除 `scope_unknown: 285`，消灭 469 项空能力需求，形成可指导全局推进的“能力复用热力图”。

#### 任务 T3.1：消灭 285 个 `scope_unknown`（终审确权）
基于客观物理介质与产品策略决策树，批量更新 `checklist.data.json`：
- [x] **划定不可逆物理硬件排除（`out_of_scope`）**：
  - 物理射频测距（`wifi/ftm`、微波 CSI 探针）；
  - 物理以太网外部 PHY 芯片（`ethernet/basic`、`ethernet/eth2ap` 等）；
  - 物理 Efuse 激光熔丝安全烧录与只读防篡改硬件；
  - 射频功率校准与硬件级天线分集；
  - 必须写入明确的 `exclusion_reason`（引用 ADR-0012 物理边界）；
- [x] **圈定高价值产品范围（`in_scope`，活跃排期 `schedule: active`）**：
  - 基础外设（GPIO, Timer, PWM, ADC, DAC, I2C, SPI, UART）；
  - 常用存储（NVS, SPIFFS, FATFS）；
  - 核心并发系统（FreeRTOS, 队列, 信号量, 事件组）；
  - 通用网络协议（Wi-Fi Station, HTTP Client, MQTT, WebSocket, SNTP）；
  - 核心 BLE（GAP 广播, GATT 属性服务器）；
- [x] **挂起多板级联与超重型用例（`deferred`）**：
  - 依赖两块真实硬件互连通信、外部专用工业总线卡（如某些大型 Zigbee/OpenThread 网关集群）；
- [x] 确保 `checklist.data.json` 中的 `summary.scope_unknown` 归零。

#### 任务 T3.2：打标 478 个条目的 `required_capabilities`
- [x] 运行 `extract_example_dependencies.py` 将推导出的原子能力数组批量灌入 `checklist.data.json`；
- [x] 组织架构合规性校验，确保：
  - 无任何条目遗留空的 `required_capabilities: []`；
  - 引用的每个能力均存在于 `capability-catalog.yaml` 中；
- [x] 生成能力复用热力图与波次攻坚报表，明确统计出各能力的引用次数（ROI 指数）。

---

### 阶段四：内核四大防腐“安全气囊”前置注入

**目标**：在官方示例涌入前，向仿真内核注入四大防御机制，根除并发假阳性、忙等死锁与外设缺失崩溃。

#### 任务 T4.1：并发自旋锁记账与持锁让步断言
- [x] 在 `wink-micro-os/frameworks/esp_idf/src/freertos/` 中完善并发簿记结构体：
  ```c
  typedef struct {
      uint32_t owner_fiber_id;
      uint32_t nesting_count;
  } esp_sim_spinlock_t;
  ```
- [x] 在 `taskENTER_CRITICAL()` / `taskEXIT_CRITICAL()` 中进行持锁计数；
- [x] 在所有可能触发纤程让步或阻塞的 API（`vTaskDelay`、`xQueueReceive`、`xSemaphoreTake`、`vTaskSuspend` 等）入口处植入防御断言：
  ```c
  WINK_ASSERT_MSG(!in_critical_section(), "Deadlock Hazard: Task called blocking/yield API while holding spinlock!");
  ```
- [x] 编写单测验证持锁让步被立即捕获。

#### 任务 T4.2：微秒忙等自愈（Yield Injection & Time Catch-up）
- [x] 依据 ADR-0072，在 `esp_rom_delay_us()` 及常用总线轮询宏（如等待传输完成）内部植入忙等累加器；
- [x] 当累计忙等时间超过安全阈值（如 1000 微秒）且无硬件事件到达时，强制调用调度器：
  ```c
  sim_scheduler_yield_and_advance_time(accumulated_us);
  ```
- [x] 确保虚拟时间平滑推进，其他就绪纤程与定时器事件能够切入执行，从根本上化解调度器冻结。

#### 任务 T4.3：纯内存沙箱 VFS 与 Partition Table 基础模型
- [x] 实现纯 RAM 树状 Inode 索引，支持标准的 `open`、`read`、`write`、`close`、`mkdir` 虚拟操作，不触碰宿主硬盘；
- [x] 实现内存 Flash 分区表解析器，虚拟映射 `nvs`、`spiffs`、`phy_init` 扇区，提供分区擦写桩；
- [x] 确保多示例仿真隔离，互不污染宿主磁盘文件。

#### 任务 T4.4：可插拔虚拟外设应答器体系（Virtual Responders）
- [x] 定义可插拔外设应答器抽象接口 `sim_responder_t`；
- [x] 落地首个标准应答器：`sim_i2c_eeprom_at24c02`（256 字节虚拟 EEPROM，支持 I2C 寻址、写时序与读返回 ACK）；
- [x] 确保官方外设示例在无真实芯片时能够顺利通过初始化探测。

---

### 阶段五：存量隔离区债务清零与门禁最终闭环

**目标**：彻底消除存量历史债务，清空白名单，实现 Gate 1~4 全量零告警、零错误运行。

#### 任务 T5.1：补齐 10 个存量条目凭证
- [x] 针对 `wink-micro-app/vendor/esp_idfv61/.gates/quarantine.yaml` 中的 10 个存量用例（`blink_gpio`、`gptimer_alarm` 等），运行仿真并捕获最新的执行日志；
- [x] 回填 `assets_sha256`、`scenario_sha256`、`run_id` 与 `verified_commit`；
- [x] 从 `quarantine.yaml` 中移除上述条目，使 `quarantine.yaml` 的 `quarantined_items` 变为由空列表构成的纯净基线。

#### 任务 T5.2：全量门禁流水线回归验证
- [x] 运行 `python .gates/run_gates.py --mode nightly`：
  - Gate 1：478 条数据结构合法、0 条引用不存在能力、0 条未决范围；
  - Gate 2：PAL 保持轻量，无任何器件协议污染；
  - Gate 3：`winkcli lint --pack layering --pack api` 全过；
  - Gate 4：反向影响闭包计算正常，回归测试 100% 通过；
- [x] 运行 `python generate_checklist_v1_1.py` 重新生成 `CHECKLIST.md`，确认看板数据与统计图表 100% 对齐；
- [x] 运行 `python .github/scripts/check_license_map.py` 确认所有新增文件 Apache-2.0 许可无误；
- [x] 执行原子 Git Commit，正式宣告 Sprint 0 筑基战役胜利收官！

---

## 三、 风险矩阵与应急预案

| 风险项 | 严重度 | 触发场景 | 缓解措施与应急预案 |
|---|:---:|---|---|
| **R-01：静态提取器推导过度碎片化** | 中 | 扫描器为每个微小的子功能都提取出一个能力，导致 Catalog 膨胀到数百项。 | 设定“技术模块/特性”粒度上限，将细粒度寄存器参数归并为模块级能力（如将各种 ADC 参数统一归属为 `cap.analog.adc_oneshot` / `adc_dma`）。 |
| **R-02：范围判定出现内部争议** | 中 | 某高难度示例在判定为 `in_scope` 还是 `out_of_scope` 时出现分歧。 | 依据宪章铁律五走架构仲裁，必须在 3 个工作日内出具裁定，以物理介质是否存在真实软件模拟价值为客观准绳。 |
| **R-03：忙等让步注入导致定时精度漂移** | 低 | 自动让步注入如果步长过大，可能破坏微秒级脉冲协议时序。 | 依据 ADR-0072 设置自适应动态步长（微秒级累积，未超时前保持就地空转，超限后再行触发调度让步）。 |
| **R-04：TTL 到期阻断** | 高 | 存量隔离区条目若未在 14 天 TTL（2026-10-13）前补齐凭证，门禁将硬阻断 PR。 | 在 Sprint 0 第五阶段优先将这 10 个条目的哈希回填完毕，彻底解除 TTL 危机。 |
