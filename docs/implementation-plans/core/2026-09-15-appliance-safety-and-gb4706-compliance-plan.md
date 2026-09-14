# 【实施计划】小家电功能安全与 GB4706/IEC60335 安规合规专项计划 (v2.4 - 工业级安规定稿版)

> 📋 **计划说明**：本计划作为 [`PLAN-20260912-SIM-FIDELITY`](./2026-09-12-high-fidelity-simulation-system-hardening-plan.md) 的后续长效演进专项，专注于小家电商业级功能安全（Functional Safety）与认证级安规标准（GB 4706.19 / GB 4706.1 / IEC 60335-2-15）的落地与仿真验证。重点实现**掉电热态防重开 60s 冷却锁定**、字模扩展、安规故障注入矩阵、去 PAL 化裸机断言以及微波炉/烤箱安全模型扩展。
>
> 🎯 **关联规范**：`docs/zh/design/04-wasm-simulation/04-assurance/01-consistency-spec.md` (C1.3, C11.1, C23)
> 📚 **管理 ADR**：ADR-0001, ADR-0003, ADR-0009（物理行为与故障注入）, ADR-0067（小家电动力学 Profile，已 Accepted）, 本专项新增 ADR-0069 修订扩展（跨品类安规模型）
> 🔗 **前置依赖计划**：[`PLAN-20260912-SIM-FIDELITY`](./2026-09-12-high-fidelity-simulation-system-hardening-plan.md)（仿真高保真底座与固件基础去抖/斜率硬化）

---

## 1. 元数据表（🔴 必选）

| 字段 | 内容 |
|------|------|
| **计划编号** | `PLAN-20260915-APPLIANCE-SAFETY-AND-GB4706` |
| **创建日期** | 2026-09-12 |
| **计划执行日期**| 2026-09-13（前置基线计划已于 2026-09-13 结项验收通过，提前启动执行） |
| **目标平台/SoC** | `mcs51` (CMS8S78xx) / `wasm` (UniSim 3.0) / `host` |
| **工具链/SDK版本**| `SDCC 4.x` / `Keil-C51 Transpiler` / `Emscripten 3.1.x` / `Node.js v20+` |
| **计划状态** | 🔄 执行中（2026-09-13 启动） |
| **优先级** | 🟡 P1（商业量产合规专项） |
| **计划版本** | `v2.5`（2026-09-15：ADR-0070 经 unisim Q6/T7 评审签发 **Accepted**，Plant 侧 t=0 初值与 `HARD_RESET` 保植物已落地并回写 C14.6；通用输入面与场景 2/3 CI 验收、第二应用复用证明在 D-005a 跟踪。其余同 v2.4：D-005 拆分 a/b、反特化门禁，Task 1/2/5 固件与 host 单测、Task 3 三维安规场景 26/26 全绿、Task 4 ADR-0069 Accepted 回写 C14.5） |
| **关联前置计划** | [`PLAN-20260912-SIM-FIDELITY`](./2026-09-12-high-fidelity-simulation-system-hardening-plan.md) |
| **关联技术设计** | [`docs/zh/design/07-platform-governance/02-error-fault-model.md`](../../zh/design/07-platform-governance/02-error-fault-model.md) |
| **关联设计规范** | [`docs/zh/design/04-wasm-simulation/04-assurance/01-consistency-spec.md`](../../zh/design/04-wasm-simulation/04-assurance/01-consistency-spec.md) |
| **计划负责人** | 小家电安规与质量验证组 |
| **所需子代理技能** | `embedded-best-practice` |

---

## 2. 背景与立项目标（🔴 必选）

### 2.1 商业小家电量产安规背景

在前置基线计划 [`PLAN-20260912-SIM-FIDELITY`](./2026-09-12-high-fidelity-simulation-system-hardening-plan.md) 中，我们解决了仿真底座保真度与基础控制律问题。但在真实商业量产与安规认证（GB 4706.19、GB 4706.1、IEC 60335-2-15）中，固件必须具备工业级安全防御能力：

1. **强热余温掉电重开漏洞（Power-Cycle Bypass Loophole）**：
   若单纯在 RAM 中维护 `cooldown_seconds`，用户发生干烧（E-03）或超温（E-04）后，拔掉电源插头再重新插上，RAM 清零，用户可立即开机加热。此时发热盘物理余温仍在 200°C~300°C，二次叠加蓄热极易引发熔毁起火。**必须在无 NVS 硬件下，以“上电热态侦测”封堵该漏洞**。
2. **纯裸机 8051 无 PAL 抽象的断言真理**：
   `mcs51_health_pot` 是无 OSAL、无 PAL 的轻量裸机固件。安规保护的成功出口绝非高级的 `WINK_ERR_BUSY` 错误码，而是物理级三件套：**状态锁在 ST_OFF、数码管打出 COOL、P2.0 继电器引脚硬件电平实测为 0**。
3. **真实硬件边界的诚实划分**：
   本控制板无电流互感器回路，继电器物理烧结触点粘连在仿真中无法自闭环感知，诚实列为 **HIL 独占（依靠双金属片和不可逆 TCO 熔断器硬件物理兜底）**，软件专注于能测可防的安规闭环。

### 2.2 技术/业务目标

- ✅ **目标 1（强热余温 60s 冷却锁定 + 上电热态防重开）**：干烧/超温后锁定 60s 物理冷却；拔插掉电重开时，若采得上电温度 $T \ge 45^\circ\text{C}$，强制续锁 60s 冷却，彻底消除掉电绕过漏洞。
- ✅ **目标 2（数码管 COOL 字模与声音交互）**：将 `font_table` 扩充至 17 字节加入 'C' 与 'L'；确立 `FAULT > COOL > NORMAL` 明确显示优先级；被拒开机时复用 `TONE_BUSY`（800Hz 短促否定音）。
- ✅ **目标 3（GB 4706 认证级故障注入矩阵与 Oracle 分级）**：建立 5 组认证级自动化场景，区分“仿真可测”与“HIL 独占”预言，断言纯裸机物理三件套。
- ✅ **目标 4（跨品类安全模型与规范修订）**：新建 ADR-0069 修订已 Accepted 的 ADR-0067，吸收微波炉三重门联锁（GB 4706.21）与便携式烤箱过热防护（GB 4706.14 / IEC 60335-2-9）安全动力学。

### 2.3 执行记录（2026-09-13）

| 交付 | 状态 | 落点与证据 |
|---|---|---|
| Task 1 冷却锁定 + 上电热态 | ✅ | `health_pot.c`：`font_table[17]`（'C'/'L'）、`COOLDOWN_SECONDS`/`data cooldown_seconds`、`enter_fault` 热故障重入刷新、启动 7b 同步采样 + ≥45 ℃ 续锁、OFF→HEAT / WARM 重煮双入口门控（先消费后拦截）、P2.0 输出级硬钳位、`FAULT > COOL > NORMAL` 显示交替 |
| Task 2 出厂默认保温 | ✅ | `WARM_DEFAULT_C 60u` + 三处默认替换（init / OFF→HEAT BOIL / WARM 重煮）；55/80/90 档位循环字面量不动 |
| Task 3 安规故障注入场景 | ✅ | `safety-cooldown-lock`、`safety-cold-water-injection`、`safety-relay-weld-protection`（`tags: ["HIL-Exclusive"]`）；场景 2/3 headless 按 D-005a 设计性缓落（ADR-0070 Proposed） |
| D-005 契约提案 | ✅ 契约 / 🔄 实现 | [`ADR-0070`](../../decisions/unisim/0070-scenario-power-cycle-and-t0-initial-conditions.md) 2026-09-15 **Accepted**（unisim Q6/T7 签发，C14.6 回写）：Plant 侧 t=0 初值 + `HARD_RESET` 保植物已落地（unisim，双跑 12/12 保持）；D-005a 通用输入面（通道/引脚/插件态 boot 前预置）与场景 2/3 CI 验收、第二应用复用证明仍 🔄；D-005b 复位语义（可选）排期 Phase 4.2 |
| Task 4 跨品类安规 ADR | ✅ | `docs/decisions/unisim/0069-appliance-cross-category-safety-extension.md` **Accepted**（修订 ADR-0067）；C14.5 规范与清单行回写完成 |
| Task 5 host 安规载具 | ✅ | `wink-micro-os/test/CMakeLists.txt` + `frameworks/mcs51/test/core/test_mcs51_health_pot_safety.c`：转译真实 `health_pot.c` 单镜像，覆盖冷启动/POST/热启动续锁/E-03 锁-拒-到期全流程（16.2 s） |
| 文档回写 | ✅ | `DESIGN.md` §1/§4.3/§4.4/§6/§7/§8 与 `test.md` 26 场景矩阵（含 `"C00L"` 段码与 0 共用说明） |
| 基线场景鲁棒化 | ✅ | `health-pot-power-cycle-dwell` 关机按压 2.0 s→2.1 s：消除与 1 s 遥测边界的拍点竞态，dwell 语义与断言不变 |
| 综合门禁 | ✅ | wasm 零警告构建 + **26/26 headless 全绿** + host 单测 100% + `wink lint --pack layering --pack api` clean（2026-09-13） |

---

## 3. 详细任务拆分与执行说明（🔴 必选）

---

### Task 1：强热余温 60s 冷却锁定、上电热态拦截与 UI 交互 `[ 状态: ✅ 已完成（2026-09-13） ]`

| 字段 | 内容 |
|------|------|
| **负责人** | 嵌入式安全组 |
| **预估工时** | 6 小时 |
| **前置依赖** | `PLAN-20260912-SIM-FIDELITY` Phase-A 完成 |
| **修改文件** | `wink-micro-app/mcs51_health_pot/health_pot.c`, `wink-micro-app/mcs51_health_pot/docs/DESIGN.md` |

#### 详细步骤
- [ ] **Step 1：字模扩充与防误解双相显示交互（UX 友好型冷却指示）**
  在 `health_pot.c:90`（当前 HEAD 实测；Phase-A 施工前旧行号为 82）中将 `font_table[15]` 扩充为 17 字节：
  - 添加 `'C' = 0x39u`, `'L' = 0x38u`；
  - 确立显存渲染优先级：`FAULT (E-0x) > COOL (交替实测水温) > NORMAL`；
  - **双相交替防误解设计**：在 60s 冷却期内，数码管采用 1 秒显示 `COOL`、1 秒交替显示当前实测水温（如 `75`）的双相刷新机制，直观告知用户“壶身过热，系统正在主动安全散热”，杜绝黑屏死机误解；自动化测试断言放宽为“2 秒滑动窗口内交替包含 `COOL` 与温度”。
- [ ] **Step 2：双模 60s 冷却锁定（运行时触发 + 启动时序陷阱消除）**
  - **倒计时归属顶层与防死锁设计（核心实现规约）**：
    `cooldown_seconds`（显式修饰为 `static unsigned char data cooldown_seconds;` 归属内部直接寻址 DATA 区，占用 1 字节，`DSEG <= 96B` 门禁已覆盖）必须在 `one_second_task()` 顶层无条件执行路径递减（与 `relay_off_sec` 并列）；**严禁塞进新斜率任务函数 `heat_slope_task_1s`**（因新函数包含 `state != ST_HEAT` 守卫，冷却期永远 early-return，会导致倒计时永久冻结死锁！）；
  - **热故障重入刷新**：在 `FAULT` 报警中或 60s 冷却期间，若再次触发干烧（E-03）或超温（E-04），`cooldown_seconds` 无条件刷新重置为 60s；清除故障后继续递减至 0 才能开机；
  - **启动时序链（Boot-time Sequence Pipeline）**：
    `main:914-915` 静态初值 `temp_c=25u` 会短路热态检测。必须严格按照以下不可调换的时序链排布启动代码：
    1. ADC 硬件初始化（`adc_init()`）；
    2. 主动执行同步阻塞式采样与转换（`adc_code = adc_read_filtered(); temp_c = ntc_code_to_temp(adc_code);`，耗时 $<0.5\text{ms}$，无 WDT 风险）；
    3. 热态判定：若 `temp_c >= DRYFIRE_TEMP_C (45u)`，立即强制预置 `cooldown_seconds = 60u`，彻底消灭拔插头重开 300°C 余热盘的火灾隐患；
    4. 按键 POST 卡键状态捕捉（`button_init_post()`）；
    5. 启动 Timer0 并开启总中断（`ET0=1; EA=1; TR0=1;`）。确保首个 tick 的控制逻辑具备真实温度初态；
  - **双重软件安全纵深防御（Defense-in-Depth）落点**：
    - **决策点门控（先消费、后门控时序铁律）**：在 `handle_buttons()` 的 `ST_OFF -> ST_HEAT` 入口（506 行）与 `ST_WARM -> ST_HEAT` 重煮入口（483 行）检查冷却；必须先执行 `evt_xxx = 0` 事件消费，再查 `cooldown_seconds`，拒绝路径播 `play_melody(TONE_BUSY)` 后正常落空——严禁在清标志之前 `return`，否则残留事件将在 60 秒冷却到期后幽灵自启动（行号以 Phase-A 施工前为准，施工后按符号重锚定）；
    - **输出级物理钳制**：在主循环引脚驱动处（977 行 `HEATER = heater_on;`，源码 sbit 名以此为准）执行终极硬钳位：`HEATER = (cooldown_seconds > 0) ? 0 : heater_on;`。
- [ ] **Step 3：被拒开机声音反馈与遥测协议界定**
  - 冷却锁定期用户按 ON 键，触发 `play_melody(TONE_BUSY)`（API 严格对齐 `health_pot.c:144`），发出标准操作否定音（800Hz / 30ms）；
  - **遥测协议拍板（方案 a）**：现有 6 字节串口遥测协议不变，冷却期保持 `TLM_STATE = ST_OFF`；冷却锁定属于 UI 与底层驱动安全门控行为，由数码管双相显示、否定音和 P2.0 引脚电平提供完整可观测性。

#### 验证步骤（裸机物理三件套与无死锁证明）
1. **死锁自由度证明（Deadlock-Freedom）**：60s 冷却期内发热盘常关，`relay_off_sec` 必然连续累加至 60s，远超继电器防拉弧 dwell 门限（`RELAY_DWELL_SECONDS = 3u`，源码实测值；量产若需加大需另立变更并重写 dwell 三场景）；冷却到期放行瞬间 dwell 互锁条件恒满足，绝对不存在“冷却放行、dwell 再锁”的二次等待死锁。
2. 触发 E-03 后短延时按开关机，断言：`state == ST_OFF`、数码管 2 秒内交替呈现 `COOL` 与温度、`HEATER (P2.0) == 0`。
3. 模拟掉电并保持探头在 50°C，重新上电，断言立即进入 60s `COOL` 锁定，继电器输出电平恒为 0。

---

### Task 2：掉电默认安全态与启动安规不变量 `[ 状态: ✅ 已完成（2026-09-13） ]`

| 字段 | 内容 |
|------|------|
| **负责人** | 嵌入式安全组 |
| **预估工时** | 4 小时 |
| **修改文件** | `wink-micro-app/mcs51_health_pot/health_pot.c` |

#### 详细步骤
- [ ] **Step 1：固化掉电默认关机安全不变量（GB 4706 通用要求）**
  器具掉电再通电后，严禁恢复加热，`main` 初始化无条件锁死进入 `ST_OFF`。
- [ ] **Step 2：出厂保温默认设定值**
  新增单点宏 `#define WARM_DEFAULT_C 60u`（置于 `health_pot.c` 现有 `WARM_HYST_C` 附近），仅替换三处默认 60u 字面量：init 默认（`health_pot.c:995`）、OFF→HEAT BOIL 入口（`:626`）、WARM 重煮入口（`:595`）；保温档位循环赋值（`:583`/`:585`）与直热目标（55/80）保持字面量不动。

---

### Task 3：GB 4706 认证级自动化故障注入场景矩阵 `[ 状态: ✅ 已完成（2026-09-13，场景 2/3 headless 按 D-005 设计性缓落）；🔄 2026-09-15 更新：ADR-0070 已 Accepted，Plant 侧能力落地，场景 2/3 待 D-005a 通用输入面（或 Plant boot 前初值输出发布）解锁 ]`

| 字段 | 内容 |
|------|------|
| **负责人** | 质量与验证组 |
| **预估工时** | 8 小时 |
| **修改文件** | `wink-micro-app/mcs51_health_pot/unisim-scenarios/`（5 个 JSON 平铺，以 `safety-` 名前缀区分，不设 `safety/` 子目录） |
| **前置依赖** | D-005a（外仓 runner：t=0 物理环境预置）为场景 2/3 必需；D-005b（中途复位语义）可选；契约见 ADR-0070（**Accepted 2026-09-15**）。unisim 已交付 Plant 侧 t=0 初值 + `HARD_RESET` 保植物；场景 2/3 仍待通用输入面（通道/引脚/插件态 boot 前预置）或 Plant boot 前初值输出发布（见 unisim 应用面清单）。核心断言由 Task 5 host 单测先行承载；场景 1、4 与 L1 单测无外部阻塞 |

#### 详细步骤与 Oracle 分级（含超时与阶段解耦）

> **D-005 口径（外仓 runner 能力需求，已拆分为 D-005a/b，契约：[ADR-0070](../../decisions/unisim/0070-scenario-power-cycle-and-t0-initial-conditions.md) Accepted 2026-09-15）**：
> - **D-005a（必需）**：**t=0 物理环境预置**——在固件启动前应用模拟通道初值/引脚初值/插件输入态，时序为"校验 → 应用初值 → 复位/启动 MCU → 时间轴起跑"。养生壶只是首个消费者，能力必须对所有应用通用（禁止应用专属字段/引擎业务知识）。场景 2（热初值续锁）与场景 3（t=0 卡键）都只需这一项即可闭环。
> - **D-005a 现状（2026-09-15）**：unisim 已落地 Plant 侧（`header.initialConditions.plants.<id>.{parameters,inputs}` + `HARD_RESET` 保植物 + 报告回显）；通用输入面（通道/引脚/插件态 boot 前预置）与两个场景 CI 断言、第二无关应用复用证明仍待补；场景 2 亦可选 Plant boot 前初值输出发布路径（二选一，见 unisim `docs/review/artifacts/2026-09-15-adr-0070-application-surface.md`）。
> - **D-005b（可选）**：中途 **HARD_RESET/SOFT_RESET** 语义（当前 headless 为空实现，Plant 状态保留已由 T7 契约测试覆盖）；仅"运行→掉电重上电"单场景全序列需要，不阻塞场景 2/3。
> - 本计划处置：核心断言先由 Task 5 host 单测承载；D-005a 落地（含 runner 单测 + 第二个无关应用的复用证明）后再补两个 JSON；落地前严禁硬写（硬写只会空过/假阳性）。

- [ ] **场景 1：`safety-cooldown-lock.scenario.json`（仿真可验，Phase-B 闭环）**
  配置 `timeoutUs: 95000000`（95s：21s 干烧＋确认延时＋60s 锁定＋解锁断言裕量）；触发干烧后短延时按开机，验证锁定期 P2.0 恒为 0 且数码管 `COOL`/温度双相交替；冷却到期后一次开机进入 HEAT 即收尾——严禁拖入 ~105s 第二次斜率 E-03 窗口（fail-fast 下未断言 fault 会掀翻成绩）。
- [ ] **场景 2：上电热态拦截（核心断言 L1 host 单测先行，headless 版待 D-005）**
  host 单测打桩 `adc_read_filtered()` 返回 55°C 等效码并走完整启动时序链，断言 `cooldown_seconds == 60` 且首 tick 前无加热输出（host 载具见 Task 5）；headless 版 `safety-power-cycle-hot-reboot.scenario.json`（`timeoutUs: 100000000`）待 **D-005a** 落地后补齐（unisim 推荐形态：Plant 热态初值 + boot 前初值输出发布；备选：插件/通道初值——见 unisim 应用面清单）。原因：现行 runner 的输入/断言表达能力不足以在 boot 前预置初值，硬写只会空过（口径见上方 D-005a/b 说明）。
- [ ] **场景 3：`safety-post-jammed.scenario.json`（核心断言 L1 host 单测先行，headless 版待 D-005）**
  host 单测在 `button_init_post()` 前将按键 GPIO 打桩为低，断言 `held` 预置且 100ms 内无 `evt`、加热不启动；headless 版（`timeoutUs: 15000000`）待 **D-005a** 的 t=0 引脚/插件输入态预置能力（通用输入面）。在 runner 能力确认前禁止落地 headless 版，避免假阳性。
- [ ] **场景 4：`safety-cold-water-injection.scenario.json`（仿真可验，🟢 Phase-A 即可提前闭环）**
  配置 `timeoutUs: 60000000`（60s 虚拟时间）；加热中途注入冷水（ADC 突变跳变），覆盖注入后继续加热 20s+，断言单向加冷水上升沿守卫生效不误报干烧；脚本 authoring 约束：注入后脚本必须按掩模规则复温爬坡（若保持平坦＝合法干烧，跳闸算固件对，非固件缺陷），45°C 以下爬坡段必须保证任意 16 秒窗口下跌 ≥6 码，否则 slope 中途触发属于脚本违规（掩模规则）。
- [ ] **场景 5：`safety-relay-weld-protection.scenario.json`（HIL 独占声明）**
  标注为 `Oracle: HIL-Exclusive (TCO/Bimetal Hardware Fallback)`，仿真侧记录事件日志。

---

### Task 4：跨品类安规模型扩展（新 ADR-0069 修订 ADR-0067） `[ 状态: ✅ 已完成（2026-09-13，ADR-0069 已 Accepted 并回写 C14.5） ]`

| 字段 | 内容 |
|------|------|
| **负责人** | 系统架构组 |
| **预估工时** | 6 小时 |
| **修改文件** | `docs/decisions/unisim/0069-appliance-cross-category-safety-extension.md`（新建）, `docs/zh/design/`（Accepted 后按流程回写） |

#### 详细步骤
- [ ] **Step 1：流程修正与新 ADR 立项**
  ADR-0067 已于 2026-09-12 随 `PLAN-20260912-SIM-FIDELITY` 评审 **Accepted**，按仓库惯例（先例：ADR-0010 修订 ADR-0007）旧 ADR 保持只读、不原地增改。本专项改为新建 **ADR-0069**（标题注明“修订 ADR-0067”），初始 Proposed；将微波炉三重门开关互锁（GB 4706.21）与便携式烤箱过热防护（GB 4706.14）并入后签发 Accepted，并按流程回写设计规范。
  > 完成（2026-09-13）：ADR-0069 已签发 **Accepted**；跨品类条款已回写 `04-assurance/01-consistency-spec.md` §C14.5 与 `02-consistency-checklist.md` C14.5 行；ADR-0067 保持只读，由 ADR-0069 元数据注明修订关系。
- [ ] **Step 2：定义 Plant Override 与 Resume 恢复契约**
  在 ADR-0069 中明确契约：当测试脚本临时覆写 ADC 码值（模拟冷水注入或探头扰动）并释放后，Plant 热力学演算核心基于当前实测温度平滑恢复微分求解，严禁产生滞后冲击鬼影。

---

### Task 5：health_pot 安规 host 单测载具 `[ 状态: ✅ 已完成（2026-09-13） ]`

| 字段 | 内容 |
|------|------|
| **负责人** | 质量与验证组 |
| **预估工时** | 6 小时 |
| **前置依赖** | 无外部阻塞；断言随 Task 1/2 实现增量追加 |
| **修改文件** | `wink-micro-os/frameworks/mcs51/test/core/test_mcs51_health_pot_safety.c`（新增）, `wink-micro-os/test/CMakeLists.txt` |

#### 背景（决策）
`mcs51_health_pot` 为 `wasm-sim only`（`wink-micro-app/mcs51_health_pot/CMakeLists.txt:60-62` 不产出 host target），计划原 L0 的 `wink.py test --app mcs51_health_pot --target host` 无落点。采用仓库既有载具（先例：`test_mcs51_iron_ntc_e2e`）：`wink-micro-os/test/CMakeLists.txt` 的 `add_mcs51_host_test()` + `transpile_app_keil_c51.py` 转译应用源码；应用源码不复制、不 fork，单一镜像直接引用 `wink-micro-app/mcs51_health_pot/health_pot.c`。

#### 详细步骤
- [ ] **Step 1：转译与注册**：在 OS 测试 CMake 增设应用源码转译规则（参照现有 `mcs51_transpile_sample`，源路径指向 app 内 `health_pot.c`），并以 `add_mcs51_host_test(test_mcs51_health_pot_safety ...)` 注册。运行入口：`cmake -B build-host -DTARGET_PLATFORM=host -DWINK_BUILD_TESTS=ON` + `ctest -R health_pot_safety`（`wink.py test` 全量亦覆盖）。
- [ ] **Step 2：掉电热态拦截（场景 2 核心断言）**：post-init hook 在首 tick/首条控制逻辑前预置片上 ADC 等效码（≥45°C）与按键电平，`wink_runtime_run()` 走完启动时序链，断言 `cooldown_seconds == 60`（静态变量经只读访问钩子或行为等价断言观测）且首秒内无加热输出。
- [ ] **Step 3：POST 卡键（场景 3 核心断言）**：`button_init_post()` 前将按键 GPIO 打桩为低，断言 held 预置、100ms 内无 `evt`、加热不启动；松开＋重按后事件恢复正常。
- [ ] **Step 4：冷却锁定（场景 1 补强）**：E-03 触发后 60s 内注入按键事件，断言拒绝、`P2.0 == 0`、遥测保持 `S=0`；冷却到期后一次开机进入 HEAT。

---

## 4. 测试策略与验收门禁（🔴 必选）

### L0 编译门禁
- [x] `python wink-tools/wink.py build --app mcs51_health_pot --target wasm` 零错误零警告（2026-09-13 施工验收通过）
- [x] 架构与内存门禁：`font_table` 扩充且直接寻址区 `DSEG` $\le 96$ 字节，调用栈预留充分（`wink lint --pack layering --pack api` → No lint findings）
- [x] 安全单测（host 载具见 Task 5）：`ctest -R health_pot_safety` 100% 通过（2026-09-13 实测 16.2 s）

### L1 单元测试（去 PAL 化物理三件套）
- [x] **冷却锁定断言**：E-03 触发后 60s 内，按键事件均保持 `state == ST_OFF`、P2.0 == 0、冻结 `S=0`（host 实测；`display == "COOL"` 双相由 host P1 帧环 + 场景正则共同覆盖）。
- [x] **掉电热态断言（host 单测）**：打桩 ADC 返回 50°C 等效码并走完整启动时序链，确认 60s 续锁（拒绝开机 + 63s 后放行）（headless 版待 D-005）。
- [x] **POST 卡键断言（host 单测）**：`button_init_post()` 前 GPIO 打桩为低，确认无 phantom 事件、加热不启动，释放+重按后恢复（headless 版待 D-005）。

### L2 集成仿真测试（回归全绿）
- [x] **基线 23 + 安规 3 = 26 个场景全量回归**：`health-pot-dryfire`、`health-pot-boil-warm` 等既有 23 个场景与 3 个 `safety-*` 场景（`wink-micro-app/mcs51_health_pot/unisim-scenarios/`）**26/26 全绿**通过（2026-09-13）。
- [x] **安规场景自动化验证**：场景 1（`safety-cooldown-lock`）、场景 4（`safety-cold-water-injection`）100% 通过；场景 5（`safety-relay-weld-protection`）以 `tags: ["HIL-Exclusive"]` 声明 HIL 独占、仅记录指令事件日志（runner 支持按 tag 过滤）；场景 2、3 的 headless 版待 D-005，核心断言已由 L1 覆盖。

---

## 5. 小家电安规条款映射表（标准条目标题）

| 软件与硬件安全防护 | 物理硬件双保险 | 对应安规标准与条款标题（需第三方认证签核） | 仿真测试验证方式 |
|---|---|---|---|
| **E-03 斜率干烧切断** | 壶底双金属片突跳器（115°C 机械断开） | **GB 4706.19 / IEC 60335-2-15**<br>第 19 章非正常工作（19.101 无水通电，待签核） | `health-pot-dryfire` (21s 报警) |
| **E-04 软件超温保护** | 不可逆热熔断体 TCO（130°C~150°C 熔断） | **GB 4706.1 / IEC 60335-1**<br>第 19 章非正常工作（19.11 保护电子线路评估） | `health-pot-overtemp` (105°C 停机) |
| **60s 强热余温冷却锁定** | 继电器防拉弧延寿、防止干烧二次升温 | **GB 4706.1 / IEC 60335-1**<br>第 19 章非正常工作与防二次复热要求 | `safety-cooldown-lock` (COOL 锁定) |
| **上电热态防重开锁定** | 防止拔插插头绕过余热冷却锁定 | **GB 4706.1 / IEC 60335-1**<br>第 19 章非正常工作与电源中断后重新通电 | `safety-power-cycle-hot-reboot` |
| **上电 POST 按键抑制** | 防止汤汁渗透致微动开关短路自加热 | **GB 4706.1 / IEC 60335-1**<br>第 19 章保护电子线路（元器件单点故障分析） | `safety-post-jammed` |
| **微波炉三重门联锁** | 门监控开关直接短路高压变压器初级 | **GB 4706.21 / IEC 60335-2-25**<br>微波泄漏联锁保护 | ADR-0069 跨品类安规模型（修订 ADR-0067） |
| **烤箱发热与风机联锁** | 炉腔超温热保护熔断器 | **GB 4706.14 / IEC 60335-2-9**<br>烘烤器具表面温度与过热防护（台式便携） | ADR-0069 跨品类安规模型（修订 ADR-0067） |

---

## 6. 工业级量产演进路线图（P2 阶段储备与认证背书）

为使本系统从“实验级仿真”迈向“工业级量产”，在 Phase-B 之后规划以下 4 项储备技术：

1. **可制造性与出厂修调（Factory Trim Spike）**：
   - 排期调研 CMS8S78xx 的 Data-Flash / EEPROM，用于固化出厂 NTC 单点校准阻值（消除 $\pm 1\%$ 上拉与分压漂移）、故障历史黑匣子以及继电器吸合次数累计（10 万次触点寿命预警）；
   - **传感器容差分段模型**：针对 NTC 在 98°C 沸腾段斜率平缓、1 LSB 对应近 1°C 的物理事实，将高温段公差优化为码域绝对容差（$\pm 3\text{ LSB}$），避免全温区单一 $\pm 5\%$ 导致沸腾误判。
2. **四角极值工况验证矩阵（Four-Corner Stress Matrix）**：
   在 ADR-0067 物理模型中引入 `lid_state`（开盖/闭盖换热系数 $h$ 与潜热蒸发差异，对齐 GB 4706.19 第 19.4 条）与电压降额参数，组合出“满水量 $\times$ 冬季低温 5°C $\times$ 市电 -10%（实际 648W） $\times$ 开盖”极限工况，检验温控算法鲁棒性。
3. **IEC 60730 Class B 软件安全自愈与无死锁论证（Safety Case for Certification）**：
   - **Table H.1 Item 4（CPU 寄存器与数据流动态刷新）**：固件 `heater_on` 在 `health_pot.c` 中每 100ms 由状态机根据实测条件无状态重算，无粘性全局锁存。即使在强 EMI/ESD 干扰或单粒子翻转（SEU）下引脚被强行置高，系统在 100ms 内确定性自愈关断，满足家电安全控制器认证规范；
   - **Table H.1 Item 5（模拟量输入单点故障防御）**：结合单秒 40 码单向加冷水上升沿守卫与 16 拍滑动时域一致性检验，消除传感器跳变与微动断续误触发；
   - **死锁自由度严格形式证明（Deadlock-Freedom Proof）**：60s 冷却锁定期内发热盘恒关，`relay_off_sec` 在秒任务中无条件自增至 60s（远超继电器防拉弧 dwell 门限），冷却解锁放行瞬间 dwell 互锁条件恒满足，形式化证明系统绝对无二次等待死锁。
4. **烧录选项字安全审计门禁（Option-Byte Audit Gate）**：
   在 L0 门禁中增加对烧录配置文件（如 LVR 低压复位使能、硬件看门狗硬使能、内部 24MHz 高精度振荡器选择）的静态文本扫描，防止固件逻辑正确但因单片机选项字配置失误导致裸跑死机。
