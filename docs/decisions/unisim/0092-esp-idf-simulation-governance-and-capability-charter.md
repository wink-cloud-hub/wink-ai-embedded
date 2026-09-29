<!-- SPDX-License-Identifier: Apache-2.0 -->
# ADR-0092：ESP-IDF 官方示例仿真治理前置筑基与全景能力图谱宪章

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳）** |
| 日期 | 2026-09-29（提议）/ 2026-09-29（采纳） |
| 决策编号 | ADR-0092 |
| 影响范围 | `wink-micro-app/vendor/esp_idfv61/capability-catalog.yaml`（全景能力图谱扩充至 18+ 领域）；`checklist.data.json`（478 个官方用例范围与能力需求全面审计，清零 `scope_unknown`）；`.gates/`（Gate 1~4 门禁硬拦截与存量隔离区清零）；内核四大防腐安全气囊（并发记账、忙等自愈、内存 VFS、虚拟外设应答） |
| 决策者 | 架构委员会 & 用户 |
| 关联技术设计 | [`docs/zh/tech-designs/esp32/esp-idf-classification-schema-spec.md`](../../zh/tech-designs/esp32/esp-idf-classification-schema-spec.md)、[`docs/zh/tech-designs/esp32/esp-idf-classification-gate-system.md`](../../zh/tech-designs/esp32/esp-idf-classification-gate-system.md) |
| 关联实施计划 | [`docs/implementation-plans/esp32/2026-09-30-esp-idf-governance-sprint0-foundation-plan.md`](../../implementation-plans/esp32/2026-09-30-esp-idf-governance-sprint0-foundation-plan.md) |
| 关联规范 | [`CLASSIFICATION-SPEC.md`](../../../wink-micro-app/vendor/esp_idfv61/CLASSIFICATION-SPEC.md)、[`PLAYBOOK.md`](../../../wink-micro-app/vendor/esp_idfv61/PLAYBOOK.md) |
| 前序 ADR | [ADR-0012](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)、[ADR-0002](0002-dual-target-compilation.md)、[ADR-0003](0003-simulation-fidelity-boundary.md)、[ADR-0072](../../decisions/core/0072-dual-timebase-and-event-loop-scheduler.md)、[ADR-0090](0090-centralized-pluggable-gate-system.md)、[ADR-0091](0091-esp-idf-multi-config-orthogonal-schema.md) |

---

## 一、 背景与动因（Context）

在完成 [ADR-0090](0090-centralized-pluggable-gate-system.md)（门禁插件架构）与 [ADR-0091](0091-esp-idf-multi-config-orthogonal-schema.md)（多配置 Schema v2.0）的框架奠基后，系统正式进入对 ESP-IDF v6.1 官方 478 个独立示例的规模化迁移准备阶段。然而，对当前档案与代码库的深度架构审计暴露出致命的**结构性断层危机**：

1. **能力字典存在大面积盲区**：`capability-catalog.yaml` 仅定义了 31 个原子能力，而 478 个官方示例中：
   - 蓝牙（`bluetooth/`）包含 **147 个示例**（占比超 30%），但字典中 `cap.ble.*` 能力为 **0**；
   - 外设（`peripherals/`）包含 **114 个示例**，但缺乏 MCPWM、LEDC、I2S、TouchPad、ParlIO 等大量现代外设定义；
   - 系统（`system/`，68 个）、协议（`protocols/`，35 个）、存储（`storage/`，27 个）、安全（`security/`，10 个）等大类的能力定义严重不完备；
2. **数据档案处于严重悬空态**：全量 478 个示例中，**469 个条目的 `required_capabilities` 为空（`[]`）**，**285 个条目的产品范围处于未决状态（`scope_unknown` 占比达 59.6%）**，仅有 10 个示例经过审计并暂时处于 14 天 TTL 隔离区；
3. **架构反噬与腐化风险**：若在缺乏前置全景规划与防御安全气囊的情况下直接盲目推进 Checklist 迁移，基建将迅速遭遇四大反噬：
   - 底层 PAL 失去轻量跨平台特性，被 ESP32 专用外设挤压膨胀；
   - 单核协作调度掩盖多核自旋锁竞态，产生“仿真全绿、硬件烧录即死锁”的并发假阳性；
   - 驱动级纯微秒忙等死循环导致虚拟时钟冻结死锁；
   - 官方示例因缺失外部硬件应答而在初始化握手阶段崩溃，诱发开发者编写返回 `ESP_OK` 的虚假伪桩。

为防止基建腐化、杜绝人工梳理的遗漏与主观实施的偏差，必须确立一份**不可妥协的治理宪章与前置筑基方案**。

---

## 二、 架构决策（Decision）

架构委员会决定全面采纳**“零遗漏机器推导、零偏差门禁闭环、四大安全气囊筑基、前置冲刺清障”**的治理体系，确立以下五大支柱：

### 1. 零遗漏原则：基于静态分析逆向推导与全景字典扩充
- **严禁纯人工拍脑袋枚举需求**：引入机器级静态依赖提取器（`extract_example_dependencies.py`），遍历 478 个示例的源码与构建配置，提取客观存在的 `#include` 头文件闭包、核心 API 符号与 Kconfig 宏开关；
- **全景扩充能力字典**：将 `capability-catalog.yaml` 扩充覆盖至 18+ 个完整能力领域（新增 `cap.ble.*`、`cap.pm.*`、`cap.crypto.*`、`cap.usb.*`、`cap.storage.*` 等），使每一个上游关键技术模块在字典中有法可依；
- **建立显式符号所有权矩阵**：原子能力不仅具有名称与图层归属，还必须绑定 `owned_headers`（管辖头文件）与 `owned_symbols`（管辖 API），实现机器可自动判别的依赖映射。

### 2. 零偏差原则：三层防腐归宿分流防火墙
在引入和实现任何新能力时，必须严格执行三层归宿分流红线，杜绝架构越界：
- **Tier 1（通用基础外设 -> 下沉 PAL）**：ADC、DAC、PWM、Timer、I2C Master、SPI Master、UART。保持轻量、跨平台（8051/STM32/ESP32 共享），严禁带入 ESP32 私有结构体；
- **Tier 2（专用总线与协议流 -> 提升 Model / 虚拟语义通道）**：WS2812(RMT)、CAN(TWAI)、I2S 音频流、USB-CDC。严禁直接塞入 PAL，必须在门面/模型层抽象为虚拟流通道，通过环形管道直接与 UniSim 宿主或前端交互；
- **Tier 3（物理硬件不可逆特性 -> 诚实剪枝 Fail-Loud）**：Efuse 物理熔丝烧录、物理射频测距（FTM）、硬件级 MMU 换页。坚决依据 [ADR-0012](../core/0012-contract-honesty-over-silent-degradation.md) 标记为 `out_of_scope`，在编译期或运行期触发 `WINK_SLA_ERROR` 显式阻断，**严禁手写返回 `ESP_OK` 的静默空桩**。

### 3. 内核四大防腐“安全气囊”前置注入
在大规模官方示例迁移执行前，仿真内核必须预先具备四大防御机制：
1. **并发自旋锁记账（Anti-False-Positive Spinlock Bookkeeping）**：
   在 FreeRTOS 门面层跟踪 `portMUX_TYPE` 持有状态。一旦任务在持有自旋锁期间试图调用引发纤程让步的 API（如 `vTaskDelay`、`xQueueReceive`），**立即触发不可屏蔽的运行时断言 `assert(!in_critical_section)`**，彻底根除单核串行化掩盖并发缺陷的假阳性；
2. **纯微秒忙等自愈（Yield Injection & Time Catch-up）**：
   依据 [ADR-0072](../core/0072-dual-timebase-and-event-loop-scheduler.md)，在 `esp_rom_delay_us()` 及总线忙等宏内埋入计数器。累计忙等超过阈值（如 1000 微秒）时，**强制触发调度器让步并推进虚拟时间**，化解调度器冻结死锁；
3. **纯内存虚拟 VFS 与 Partition Table 沙箱**：
   建立不触碰宿主硬盘文件的纯内存 Inode 树状文件系统，提供虚拟 Flash 分区表解析器，为 27 项存储与多任务用例提供安全、独立的运行沙箱；
4. **可插拔虚拟外设应答模型（Virtual Peripheral Responders）**：
   为 I2C、SPI 等复杂总线构建标准外设应答器（如虚拟 AT24C02 EEPROM、虚拟传感器寄存器映射），确保高级示例在无物理硬件环境下能够正常完成握手。

### 4. 478 项示例全量范围清障与热力图驱动
- **消灭 285 个 `scope_unknown`**：基于物理介质事实与功能范围判定树，在 Sprint 0 结束前将全量 478 个示例一次性审定划分为 `in_scope`、`out_of_scope` 或 `deferred`，彻底消灭范围盲区；
- **依赖热力图编排**：依据静态扫描计算出的各能力复用热度（ROI），按“高复用公共能力优先”编排执行波次，实行“聚类攻坚、以案促建”，严禁按目录字母顺序盲目推进。

### 5. 门禁硬拦截与防伪密码学闭环
- **Gate 1~4 全面生效**：CI 自动化执行 Gate 1（SSOT 格式与引用合法性）、Gate 2（PAL 防膨胀红线）、Gate 3（分层架构 Lint）、Gate 4（依赖闭包与回归防御）；
- **六位一体打勾闭包**：看板 `CHECKLIST.md` 严禁人工编辑，必须由生成脚本依据真实运行产出的防伪哈希证据链（`assets_sha256`, `scenario_sha256`, `run_id`, `verified_commit`）单向渲染更新。

---

## 三、 后果与约束（Consequences）

### 正向收益
1. **架构长治久安**：从机制上切断了底层 PAL 被污染、门面层充满假桩的技术债务通道；
2. **迁移效率倍增**：通过能力复用热力图与虚拟应答器，攻克 1 项通用能力即可成批点亮数十个官方示例；
3. **真实保真度飞跃**：消除了并发假阳性与时钟冻结死锁，仿真绿灯的代码具备极高的硬件烧录成功率；
4. **资产全量受管**：478 个示例拥有 100% 明确的产品范围与能力需求，CI 门禁形成自愈型防护网。

### 约束与代价
1. **前置投入不可压缩**：在开始迁移业务示例前，必须安排专门的 **Sprint 0（基建筑基战役，预估 1~2 周）** 完成字典扩充、静态分析与安全气囊注入，严禁提前抢跑业务示例；
2. **裁决严肃性**：任何示例判定为 `out_of_scope` 必须在 PR 中附带架构师签署的物理硬件不可逆依据；
3. **门禁只读约束**：门禁运行期间严禁自动改写数据源，任何状态流转必须由带执行凭证的显式 PR 触发。

---

## 四、 实施落地映射

| 阶段 | 核心任务 | 交付产物 |
|---|---|---|
| **Sprint 0 - Phase A** | 全景能力图谱字典扩充 | 扩充至 18+ 领域的 [`capability-catalog.yaml`](../../../wink-micro-app/vendor/esp_idfv61/capability-catalog.yaml) |
| **Sprint 0 - Phase B** | 机器静态提取与范围打标 | `extract_example_dependencies.py`，全量审定打标的 `checklist.data.json`（0 unknown） |
| **Sprint 0 - Phase C** | 内核四大防腐安全气囊注入 | 自旋锁记账断言、忙等让步自愈、内存 VFS 基础模型、虚拟总线应答桩 |
| **Sprint 0 - Phase D** | 存量隔离区债务清零 | 10 个存量条目凭证补齐，`.gates/quarantine.yaml` 归零，Gate 1~4 全绿 |
| **Sprint 1+** | 按热力图波次聚类迁移官方示例 | 阶段一：模拟量与外设聚类（M5 里程碑） |
