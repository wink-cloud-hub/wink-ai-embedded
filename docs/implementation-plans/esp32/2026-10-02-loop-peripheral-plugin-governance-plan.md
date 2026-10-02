<!-- SPDX-License-Identifier: Apache-2.0 -->
# 实施计划：ESP-IDF 治理 Loop 外置插件外设自主闭环推进与规范约束体系 (v1.1 工业完备版)

| 字段 | 内容 |
|---|---|
| 计划编号 | PLAN-20261002-LOOP-PERIPHERAL-PLUGIN-GOVERNANCE-v1.1 |
| 状态 | **✅ Complete (Fully Implemented & Verified)** |
| 日期 | 2026-10-02 |
| 目标平台/SoC | `wasm32-unknown-emscripten` / `esp32 xtensa` 双 Target |
| 规范依据 | [外设覆盖主执行计划：00-master-execution-plan.md](../wokwi-dal-type-coverage-type/00-master-execution-plan.md)<br>[外设分类与命名 SSOT：00.1-category-type-variant-wokwi-ssot.md](../wokwi-dal-type-coverage-type/00.1-category-type-variant-wokwi-ssot.md)<br>[ADR-0004：编译期静态分发与 POD 结构体](../../design/decisions/0004-static-dispatch-vs-runtime-ops.md)<br>[ADR-0092：基础外设下沉 PAL 规范](../../design/decisions/unisim/0092-esp-idf-simulation-governance-and-capability-charter.md)<br>[自主自愈 Loop 底座主计划：2026-10-02-autonomous-self-healing-loop-plan.md](./2026-10-02-autonomous-self-healing-loop-plan.md) |
| 管辖数据源 | `wink-plugin-peripherals/builtin/`、`docs/implementation-plans/wokwi-dal-type-coverage-type/`、`wink-micro-app/vendor/esp_idfv61/`、`checklist.data.json` |
| 实施目标文件 | `.governance/tools/loop/safety_checker.py`<br>`.governance/tools/loop/agent.py`<br>`.governance/tools/loop/remediator.py`<br>`.governance/tools/loop/pipeline.py`<br>`.governance/tools/loop/runner.py`<br>`.governance/gates/tests/test_loop_peripheral_plugin.py`（新增） |
| 验收门禁 | `pytest .governance/gates/tests/test_loop_peripheral_plugin.py`（100% 离线通过）、`pytest .governance/gates/tests/test_loop_remediator.py`、`python .governance/gates/run_gates.py --gate 1`、`python .github/scripts/check_license_map.py` |

---

## 一、 战略目标与设计哲学

### 1.1 痛点与破局背景
在 ESP-IDF 官方示例仿真治理过程中，大量的用例依赖板载总线挂载的外部传感器、执行器、显示器和存储器件（如 I2C 挂载的 MPU6050/MPU9250 IMU、DS1307 RTC，SPI 挂载的 ILI9341 TFT、MicroSD，以及 1-Wire 挂载的 DHT11 等）。
在实战（如 `#023` `esp.peripherals.i2c.i2c_basic`）中暴露出现有治理 Loop 的深层缺陷：
1. **外设归因盲区导致代码作弊**：
   - 现有 Loop 的归因矩阵仅有 `Layer C (配置)`、`Layer A (C框架Mock)`、`Layer Core-B (PAL)`。
   - 当仿真因未挂载真实外部芯片从机而 NACK 时，Agent 缺乏“外设插件缺失”的概念，容易被诱导在 C 框架层硬造 `fake_i2c_read()` 空桩作弊，破坏同源编译纯洁性。
2. **缺乏总纲约束导致命名与架构分裂**：
   - Agent 自由发挥捏造诸如 `type: mpu9250` 的裸芯片型号，违背了 `00-master-execution-plan.md`“按物理语义统一为 `type: imu`、以 `variant` 消化芯片拓扑”的铁律，导致设备树 codegen 报错。
3. **安全门禁机械拦截与流程倒挂**：
   - 现行 `safety_checker.py` 的白名单封锁了 `wink-plugin-peripherals/builtin/` 与计划文档目录；
   - 现有自愈引擎只懂生成单步 C 补丁 `patch.diff`，不具备“**先编撰标准计划文档 ➔ 对抗审查 ➔ 实施 TS 插件 ➔ 编译 Manifest ➔ 应用装配**”的完整闭环生命周期。

### 1.2 核心设计哲学（五大钢铁纪律）
1. **SSOT 唯一权威原则（The Master Plan as Law）**：
   - 凡涉及外置器件，[`00-master-execution-plan.md`](../wokwi-dal-type-coverage-type/00-master-execution-plan.md) 与 [`00.1-category-type-variant-wokwi-ssot.md`](../wokwi-dal-type-coverage-type/00.1-category-type-variant-wokwi-ssot.md) 是最高法律；
   - 严禁用具体芯片型号命名裸 `type`，必须归一化为标准外设大类与 Type。
2. **文档先行与对抗评审闭环（Plan-First with Adversarial Gate）**：
   - 严禁未经计划文档背书直接生成外设插件代码；
   - 必须先在 `docs/implementation-plans/wokwi-dal-type-coverage-type/` 下产出详尽的子计划，并经 Role B（独立裁判）依据《外设 8 大军规》挑刺修正后，方可进入实施阶段。
3. **物理 1:1 对齐与真实时序（Wokwi Pinout & Class 4 Accuracy）**：
   - 仿真插件的引脚数量、排布、命名必须与 Wokwi 物理器件 1:1 精确对齐（如 MPU6050 8 引脚拓扑），实现微秒级 Class 4 总线时序与寄存器全状态机。
4. **生命周期完整隔离（Zero State Leakage Across Runs）**：
   - 插件必须严格实现初始化与重置接口，严禁前序场景的寄存器脏数据污染后续场景。
5. **双轨事务一致性（Atomic Delivery & Verification）**：
   - 插件 TS 源码、`manifest.json`、编译产物 `dist/simulation.js`、`wink-app.json` 拓扑与子计划文档必须作为一个原子整体通过 Canary 变异击杀验证。

---

## 二、 架构规格与接口设计

### 2.1 归因矩阵扩展：引入 Layer D (Peripheral Plugin)

在根因排查（RCA）阶段，确立五级归因判定标准：

| 归因层级 | 缺陷范畴 | 判定信号/特征 | 自主修复策略 |
|---|---|---|---|
| **Layer C** | 应用配置层 | 引脚定义缺失、Kconfig 宏关闭、场景超时短 | Agent 修改 `wink-app.json` / `scenario.json` |
| **Layer A** | C 框架协议层 | MQTT/HTTP/WiFi 网络状态机响应桩缺失 | 修改 `wink-micro-os/frameworks/esp_idf/`，跑零回归 |
| **Layer Core-B** | PAL 基础外设层 | 缺失 ADC/DAC/WDT/Touch 等通用片上外设 HAL | `pal/include/hal/` 纯增量演进，严禁破坏既有 ABI |
| **Layer D (新增)** | **外置插件外设层** | **I2C/SPI 从机无应答、读 WHO_AM_I 为 0/FF、缺少传感器/显示/执行器器件** | **启动外设专项工作流：查总纲 ➔ 判别已有/全新 ➔ 写子计划(若全新) ➔ 对抗评审 ➔ 实现与编译 ➔ 拓扑挂载** |
| **Layer Core-A / B1** | 核心调度/UniSim 内核 | 协程调度死锁、UniSim 引擎底层虚拟时间异常 | **立即触发熔断** `[BLOCKED_ARCH_CHANGE_ESCALATION]` |

---

### 2.2 外设专项 5 阶段工作流（Peripheral SOP Lifecycle）

#### 1. 与主计划 13 状态机的衔接拓扑
当 RCA 判定缺陷归因为 **Layer D** 时，主计划状态机在 `PLAN_AUTHORED` 之后分叉进入外设专项流程（D1~D4），完成后汇合回主计划的 `HEURISTIC_PRECHECK` 节点。外设插件产出物为 TS 源码 + `manifest.json` + 编译产物（非 C `patch.diff`），因此 `HEURISTIC_PRECHECK` 阶段执行的是外设专有规则（P-1~P-4 + 外设军规 5~8）而非 H-1~H-8 C 代码规则。

```text
主计划状态机分叉点:
  [PLAN_AUTHORED] ──(Layer D)──> [D1: SSOT查表与已有/全新判别]
                                       │
                     ┌─────────────────┴─────────────────┐
                     ▼ (分支 D1.A: 插件已存在)              ▼ (分支 D1.B: 全新器件类型)
            [读取已有 Manifest 契约]                 [编制子计划草案]
                     │                                   │
                     │                                   ▼
                     │                              [D2: Role B 对抗审查]
                     │                                   │
                     │                                   ▼
                     │                              [D3: TS 插件实现与构建]
                     │                               (含 Pre-flight 环境自检)
                     │                                   │
                     └─────────────────┬─────────────────┘
                                       ▼
                                [D4: 拓扑装配与基线重跑]
                                 (更新 wink-app.json / 重新生成 device-tree)
                                       │
                                       ▼ (汇合回主计划)
                                [HEURISTIC_PRECHECK] (执行 P-1~P-4 + 军规 5~8)
                                       │
                                       ▼
                                [PATCH_APPLYING] ──> ... (后续编译/回归/Canary 不变)
                                       │
                                       ▼ (Phase 5 凭证签署)
                                [总纲 00-master 双向自动回写]
```

#### 2. 外设专项 5 阶段执行细则

##### Phase D1: SSOT 查表与二元分支分流（Avoid Redundant Reinvention）
- 读取 `00-master-execution-plan.md` 与 `00.1-category-type-variant-wokwi-ssot.md`，确定标准 `category`, `type`, `variant`, `wokwi-tag`。
- **分支判别**：检查 `wink-plugin-peripherals/builtin/<type>/dist/manifest.json` 是否已就绪：
  - **分支 D1.A（插件已就绪，纯拓扑/连线缺失）**：直接复用已有插件，跳过 D2 与 D3，直达 D4 修正 `wink-app.json` 的引脚、总线与从机地址绑定；
  - **分支 D1.B（全新外设）**：在 `docs/implementation-plans/wokwi-dal-type-coverage-type/` 下按标准模版建立外设子计划文档。

##### Phase D2: 双盲对抗审查与计划深度融合（Adversarial Gate）
- Role B (独立裁判) 依据军规 **P-5 ~ P-8** 对子计划进行无情找茬（审查引脚是否 1:1 对齐 Wokwi 原生拓扑、是否有 Class 4 微秒级时序、是否保障多实例隔离与重置）；
- Role A 吸收修改意见，深度重写子计划正文（严禁简单尾部追加），生成定稿。

##### Phase D3: 插件源码实现与构建（Build Pipeline with Pre-flight Check）
- **构建环境前置健康自检（Build Pre-flight Check）**：
  - 执行 `npm run build:sim` 前，先自检 `node` 与 `npm` 可用性，并确认 `vite` 构建依赖就绪；
  - 若环境缺失，抛出结构化 `[BUILD_ENV_ERROR]` 阻断并提示人类安装依赖，严禁误判为业务代码逻辑缺陷。
- **源码与构建产物**：
  - 编写 `builtin/<type>/1.0.0/src/simulation.ts`、`vite.config.sim.ts` 与 `package.json`；
  - 触发 `npm run build:sim` 产出 `dist/simulation.js` 与 `dist/manifest.json`；
  - **运行时隔离**：外设插件为独立 npm 包，由 UniSim 运行时通过动态 import 加载，**无需重编 UniSim 主 bundle**，保全闭盒边界。

##### Phase D4: 应用拓扑编排与 DAL 剪枝豁免（Wiring & Driver Boundary）
- 更新目标应用的 `wink-app.json`，声明外设挂载；
- 重新触发设备树生成与 Wasm 仿真编译；
- **Vendor Example 驱动边界保障**：针对原厂 SDK 官方例程（属于 `vendor_example` 目录），确保 `wink-micro-os/CMakeLists.txt` 的 DAL 剪枝豁免机制生效，允许例程直接调用原厂总线驱动。

##### Phase D5: 凭据签署与总纲双向自动回写（Master Plan SSOT Bidirectional Sync）
- 通过 Canary 变异击杀后，签署 SLA 凭证；
- **总纲双向自动回写**：自动更新 `docs/implementation-plans/wokwi-dal-type-coverage-type/00-master-execution-plan.md`：
  - 将该外设行的进度状态由 `🆕 Planned` 更新为 `🟢 Implemented (Sim Plugin Delivered)`；
  - 在目录树索引中登记子计划文档超链接；
- 实施原子 Git 提交。

---

### 2.3 安全门禁强化与整树安全回滚

#### 1. 物理写入白名单扩展
```python
SAFE_WRITE_WHITELIST = [
    "wink-micro-app/vendor/esp_idfv61/",
    "wink-micro-os/frameworks/esp_idf/src/",
    "wink-micro-os/frameworks/esp_idf/include/",
    "wink-micro-os/pal/include/hal/",
    "wink-micro-os/targets/wasm/",
    "wink-micro-os/targets/esp32/",
    # 新增外设插件与计划目录白名单：
    "wink-plugin-peripherals/builtin/",
    "docs/implementation-plans/wokwi-dal-type-coverage-type/",
]
```

#### 2. 外设 8 大军规（完整编号 P-1 ~ P-8）

**静态硬检查（由 `safety_checker.py` 执行，零额外依赖）：**
- **P-1（计划文档前置原则）**：
  若变更包含新外设插件目录 `builtin/<type>/`，工作区或补丁中必须**同时包含**对应的 `docs/implementation-plans/wokwi-dal-type-coverage-type/*-<type>-plan.md`，否则一票否决；
- **P-2（Type 收拢护城河原则）**：
  严禁以芯片具体型号作为插件目录名或 Type 名称（如 `builtin/mpu6050/` 或 `type: mpu9250` 属于严重违规），必须收拢为统一规范语义（如 `builtin/imu/`，`type: imu`）；
- **P-3（Manifest 引脚完备性原则）**：
  插件输出的 `manifest.json` 必须包含合法的 `pinType`（如 `i2c_scl`, `i2c_sda`, `power_vcc`, `power_gnd`, `gpio`），严禁未定义引脚类型；
- **P-4（生命周期与重置保障原则）**：
  `simulation.ts` 中必须实现设备状态重置机制。检测方式：正则扫描 `reset\s*\(` 或 `onAttach` 中的状态清零赋值模式（如 `= 0`、`= []`、`= {}`），防止多用例执行时的状态串扰。

**行为正确性审查（由 Role B 裁判在对抗审查时执行）：**
- **P-5（SSOT 命名合规性）**：方案是否违反了 `00-master-execution-plan.md`？是否存在自创裸 type 行为？
- **P-6（引脚物理 1:1 对齐）**：仿真模型引脚是否与 Wokwi 物理规格 1:1 对齐？是否有遗漏中断引脚或辅助总线引脚？
- **P-7（时序与寄存器诚实度）**：是否实现了真实的寄存器映射与时序响应？是否存在无状态伪造假桩？
- **P-8（多用例状态隔离）**：插件是否提供了清理与重置机制？是否会泄漏状态给下一个测试用例？

#### 3. 双轨事务回滚器的整树安全清理（Directory-Tree Safe Rollback）
- 现有 `TransactionalGitTracker.rollback` 针对单个文件 `unlink`；
- **外设专项扩展**：若自愈过程新建了 `wink-plugin-peripherals/builtin/<type>/` 目录，在自愈失败或触发回滚时，必须使用 `shutil.rmtree` 将新建插件子目录**整树彻底清理**，严禁遗留空目录、未完成源码或中间编译产物污染后续任务。

---

### 2.4 Agent Prompt 契约升级

#### 1. Role A (Proposer) RCA Prompt 增补：
```text
【Layer D: 外置插件外设判定与实施军规 (SSOT)】
若排查确认仿真失败原因为缺少外部芯片/传感器（如 I2C/SPI 从机无应答、读 ID 失败）：
1. 必须将缺陷明确归类为: 【Layer D (Peripheral Plugin)】；
2. 必须先行读取权威文档:
   - D:\workspaces\ai-coding\wink-ai\wink-ai-embedded\docs\implementation-plans\wokwi-dal-type-coverage-type\00-master-execution-plan.md
   - docs/implementation-plans/wokwi-dal-type-coverage-type/00.1-category-type-variant-wokwi-ssot.md
3. 查表确定标准命名:
   * 严禁裸芯片型号作为 type (如严禁 type: mpu9250，必须为 type: imu, variant: mpu9250_i2c)
   * 确认对应的 Wokwi 原生组件标识 (如 <wokwi-mpu6050>) 与 8 引脚拓扑
4. 区分已有插件 vs 全新插件:
   * 若 builtin/<type>/dist/manifest.json 已就绪，直接编排 wink-app.json 引脚拓扑 (分支 D1.A)
   * 若为全新器件，在 docs/implementation-plans/wokwi-dal-type-coverage-type/ 下编撰标准子计划 (分支 D1.B)。
```

#### 2. Role B (Auditor) 对抗审查 Prompt 增补：
```text
【外设插件专项找茬红线 (Role B) — 对照军规 P-5 ~ P-8 逐条审查】:
若方案涉及 Layer D 外设插件开发，裁判必须执行以下专项无情审查：
- P-5 (SSOT 命名合规性): 方案是否违反了 00-master-execution-plan.md？是否存在自创裸 type 行为？
- P-6 (引脚物理 1:1 对齐): 仿真模型引脚是否与 Wokwi 物理规格 1:1 对齐？是否有遗漏中断引脚或辅助总线引脚？
- P-7 (时序与寄存器诚实度): 是否实现了真实的寄存器映射与时序响应？是否存在无状态伪造假桩？
- P-8 (多用例状态隔离): 插件是否提供了清理与重置机制？是否会泄漏状态给下一个测试用例？
凡违反上述任一红线，必须判定为 REVISE_REQUIRED 或 REJECTED！
```

---

## 三、 实施任务拆分（WBS）与时间线

### 阶段 1：安全防腐、白名单与整树回滚筑基（Safety & Rollback Foundation）
- [x] **Task 1.1**：在 `safety_checker.py` 中扩充 `SAFE_WRITE_WHITELIST`，放行 `wink-plugin-peripherals/builtin/` 和 `docs/implementation-plans/wokwi-dal-type-coverage-type/`；
- [x] **Task 1.2**：在 `safety_checker.py` 中实现外设插件静态防腐规则 `check_peripheral_plugin_rules(patch_or_files)`（P-1 至 P-4 正则扫描）；
- [x] **Task 1.3**：在 `remediator.py` 的 `TransactionalGitTracker` 中增加目录级整树安全递归清理（Directory-Tree Safe Rollback）；
- [x] **Task 1.4**：编写离线单测 `test_loop_safety_checker.py` 覆盖外设白名单、P-1~P-4 拦截与整树回滚逻辑。

### 阶段 2：Prompt 契约升级与 D1.A/D1.B 双分支分流（Agent Synthesizer Upgrade）
- [x] **Task 2.1**：修改 `agent.py`，在 `build_root_cause_prompt` 中挂载 `00-master-execution-plan.md`、Layer D 判定与 D1.A/D1.B 二元分支逻辑；
- [x] **Task 2.2**：修改 `agent.py`，在 `build_adversarial_review_prompt` 中注入 Role B 外设 P-5 ~ P-8 专项对抗挑刺红线；
- [x] **Task 2.3**：增加外设子计划模板加载器与 TS 仿真插件代码提取器。

### 阶段 3：外设工作流串联、编译构建与总纲双向回写（Remediator & Pipeline）
- [x] **Task 3.1**：在 `remediator.py` 的 `RemediatorState` 中扩充外设专项状态，完成主计划 13 状态机与外设 D1~D4 流程的分叉与汇合对接；
- [x] **Task 3.2**：在 `pipeline.py` 中实现 `compile_peripheral_plugin`，包含 Node/npm/vite 构建环境前置健康自检与 `[BUILD_ENV_ERROR]` 结构化诊断；
- [x] **Task 3.3**：实现 `wink-app.json` 自动装配更新与 `device-tree.json` 校验；
- [x] **Task 3.4**：实现 `sync_master_execution_plan(type, plan_path)` 总纲表格状态与目录树索引双向自动回写。

### 阶段 4：单测验收与端到端实证（Verification & Dogfooding）
- [x] **Task 4.1**：编写 `test_loop_peripheral_plugin.py`，模拟 Layer D 场景全流程离线单测（覆盖已有插件复用 D1.A 与全新插件创建 D1.B）；
- [x] **Task 4.2**：运行全量 pytest 与 ESP 门禁验证（Gate 1~4 全绿，`run_gates.py` Exit Code 0）；
- [x] **Task 4.3**：以一个未落地的传感器用例（如 PIR 移动侦测 `#15` 或 DS1307 RTC `#20`）作为实证验收。

---

## 四、 验收标准与防御准则（Definition of Done）

1. **零白名单误伤与整树无残留**：自愈修复外设时，`safety_checker.py` 放行合规外设插件，坚决拦截无计划裸写；若中途失败，新增插件目录 100% 彻底清理；
2. **规范遵守率 100%**：所有新生成的外设插件，其目录、`type` 与 `manifest` 100% 符合 `00-master-execution-plan.md` 的 SSOT 定义；
3. **环境故障透明化**：若宿主缺少构建依赖，准确抛出 `[BUILD_ENV_ERROR]`，不发生假阴性业务归因；
4. **总纲双向自洽率 100%**：外设交付后，总纲文档状态与子计划链接自动同步对齐；
5. **单测覆盖率 100%**：新增模块在离线单测套件下 100% 通过；
6. **门禁零破坏**：不破坏既有 ESP-IDF Gate 1~4 门禁系统，保持 Exit Code 0。
