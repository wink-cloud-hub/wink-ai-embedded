# 【实施计划】嵌入式高保真仿真系统真机一致性整改与演进计划 (v2.5 - 施工定稿版)

> 📋 **计划说明**：本计划针对当前 MCS-51 / 养生壶（`mcs51_health_pot`）及 UniSim 仿真运行环境中暴露出的“仿真与真机不一致性”现象，在经过五轮严谨的专家级架构、硅片事实与小家电安规评审后最终定稿。彻底消除了旧定时器重复自增与两级干烧打架、8051 xdata 隔离、冷水 40 码扰动守卫、10ms 守卫双向去抖、波形原子对与场景时序等全部 P0 阻断性风险。
>
> 🎯 **关联规范**：`docs/zh/design/04-wasm-simulation/04-assurance/01-consistency-spec.md` (C1~C25)、`packages/unisim/docs/roadmap/design/high-fidelity-simulation-architecture.review.md`
> 📚 **管理 ADR**：ADR-0001, ADR-0002, ADR-0003, ADR-0014, ADR-0019, ADR-0040, ADR-0042, ADR-0047, ADR-0053, ADR-0055
> 🏷️ **本计划交付新 ADR**：[`docs/decisions/unisim/0067-appliance-plant-profile-architecture.md`](../../decisions/unisim/0067-appliance-plant-profile-architecture.md)、[`docs/decisions/unisim/0068-waveform-edge-and-virtual-timestamp.md`](../../decisions/unisim/0068-waveform-edge-and-virtual-timestamp.md)
> 🔗 **后续专项计划**：[`PLAN-20260915-APPLIANCE-SAFETY-AND-GB4706`](./2026-09-15-appliance-safety-and-gb4706-compliance-plan.md)（已立项，涵盖 60s 冷却锁定、掉电热态防重开与安规故障矩阵）

---

## 1. 元数据表（🔴 必选）

| 字段 | 内容 |
|------|------|
| **计划编号** | `PLAN-20260912-SIM-FIDELITY` |
| **创建日期** | 2026-09-12 |
| **版本演进** | `v2.5`（扰动计数先判定后复位、Task5 前置 D-004、测试分级对账） |
| **目标平台/SoC** | `wasm` (UniSim 3.0) / `mcs51` (CMS8S78xx) / `host`（ESP32 仅限通用 PAL 回归） |
| **工具链/SDK版本**| `Emscripten 3.1.x` / `SDCC 4.x` / `Node.js v20+` / `Keil-C51 Transpiler` |
| **计划状态** | 📋 评审通过 / 正式施工 |
| **优先级** | 🔴 P0（Phase-A 固件去抖与定点斜率）+ 🟡 P1（Phase-B 跨仓契约与模型演进） |
| **关联技术设计** | [`docs/zh/design/04-wasm-simulation/00-README.md`](../../zh/design/04-wasm-simulation/00-README.md) |
| **关联设计规范** | [`docs/zh/design/04-wasm-simulation/04-assurance/01-consistency-spec.md`](../../zh/design/04-wasm-simulation/04-assurance/01-consistency-spec.md) |
| **关联评审记录** | [`packages/unisim/docs/roadmap/design/high-fidelity-simulation-architecture.review.md`](file:///d:/workspaces/ai-coding/wink-ai/unisim/docs/roadmap/design/high-fidelity-simulation-architecture.review.md) |
| **前置依赖计划** | 无，本仓 Phase-A 零外部阻塞 |
| **后续演进计划** | [`PLAN-20260915-APPLIANCE-SAFETY-AND-GB4706`](./2026-09-15-appliance-safety-and-gb4706-compliance-plan.md) |
| **替代/废弃** | 替代 `v1.0` ~ `v2.4` 草案 |
| **计划负责人** | 嵌入式与仿真架构联合组 |
| **所需子代理技能** | `embedded-best-practice` |

---

## 2. 架构决策与执行策略（两阶段切分）

```
【Phase-A：本仓独立闭环交付 (P0，立即施工)】
├── Task 1: health_pot.c 10ms Tick 守卫的双向 20ms 去抖 + POST 上电卡键抑制 + WARM_HYST_C=2u
├── Task 2: 彻底重构干烧逻辑：
│           ├── a) 彻底删除 one_second_task() 原 728~734 行的旧计数与两级超时（杜绝双倍自增）
│           ├── b) 8051 xdata 定点斜率算法 (adc_hist_16s[16] + &0x0F + 冷水 40 码守卫 + HEAT 门)
│           └── c) 固件常量 Stage 1 放宽至 650s、Stage 2 放宽至 550s + HEAT 入口全环预填
├── Task 3: 修正既有 15 个场景断言 (boil-warm 58°C 断言 + dryfire 21s 注释 + hold-time 审计)
└── Task 4: 产出 ADR-0067 (Profile 架构) 与 ADR-0068 (原子对时戳/Pin映射/Generation/迟到钳位)
    └── 验收基线：Host 纯单测 100% 覆盖 + 既有 15 个场景全绿回归

【Phase-B：跨仓闭环联调 (P1，依赖外仓)】
├── 前置阻塞依赖：D-003 (UniSim TS plant-profile 实现) + D-004 (Frontend 原子波形注入)
└── 联调目标：拉起 fast-boil (0.3L/1000W) 94s 自发烧开全自动闭环集成测试 (timeoutUs: 150s)
```

---

## 3. 核心物理参数与场景联动基线（含激励源隔离）

> ⚠️ **固件单镜像原则与安全硬边界（Safety Hard Invariants）**：单镜像固件无法感知外部 JSON，固件常量一律采用标称宽容限（Stage 1: 650s，Stage 2: 550s，斜率 16s/6 码）。下表中的耗时与超时仅为场景物理预期与测试上限（如 fast-boil 场景 timeoutUs: 150s）。**安全硬边界防篡改原则**：固件内部的干烧斜率判据与两级超时常数属于绝对固化的物理安全防线，严禁暴露为外部 JSON 可篡改参数；外部场景脚本只能配置物理输入环境（如水质、功率、气压），绝不可篡改安全硬常数。

| 场景配置文件 | 标称功率 $P$ | 水质量 $m$ | 升温速率 $\frac{dT}{dt}$ | $25 \to 100^\circ\text{C}$ 耗时 | 激励源类别 (C14.5) | 固件干烧判据 / 场景断言 | 验证目的与阶段 |
|---|---|---|---|---|---|---|---|
| **旧场景兼容档** (`health-pot-dryfire.scenario.json`) | 1000 W | 0.05 kg (极少水) | $\approx 4.0^\circ\text{C/s}$ | $\approx 18\text{s}$ | **脚本固定输入** (INPUT_ANALOG，关闭 Plant) | 固件斜率法在第 20 个 tick 秒（场景墙钟约 21s）触发 E-03 | **Phase-A 验收**：老资产 100% 通过（20~31.5s 窗口有效） |
| **快速冒烟档** (`health-pot-fast-boil.scenario.json`) | 1000 W | 0.30 kg (小水量) | $\approx 0.79^\circ\text{C/s}$ | $\approx 94\text{s}$ | **Plant 自激演算** (场景内零外部模拟注入) | 固件正常升温不报警，场景 timeoutUs: 150s 闭环跳断 | **Phase-B 联调**：外仓到位后闭环 |
| **标称量产档** (`default_plant_config`) | 800 W | 1.00 kg (标称满壶) | $\approx 0.19^\circ\text{C/s}$ ($\eta=0.85 \to 462\text{s}$) | $\approx 462\text{s}$ | **Plant 自激演算** | 固件常量 650s/550s 兜底，斜率法全周期监控 | **全生命周期**：商用高保真数字孪生 |

---

## 4. 关键算法与硅片事实细化（施工级定稿）

### 4.1 8051 定点 ADC 码域温升斜率算法（抗冷水扰动 + 状态门控 + xdata 隔离）

```c
/* 内存隔离：显式放置于片外 XRAM，杜绝挤爆直接寻址区 DATA (128B) */
static unsigned int xdata adc_hist_16s[16]; 
static unsigned char adc_hist_idx;
static unsigned int  adc_prev_1s;
static unsigned char glitch_cnt;

/* 进入 ST_HEAT 或扰动复位瞬间执行全环预填与基线重填。
 * 时序铁律：本函数不碰 glitch_cnt（扰动分支先计数判定、后调本函数；若在此清零则计数恒为1、E-01 永不可达）。
 * glitch_cnt 仅在三处清零：HEAT 入口三落点（FUNC 切档 / WARM 重煮 / OFF 开机）显式清零，以及干净采样清零。 */
void heat_slope_reset(void) {
    unsigned char i;
    for (i = 0; i < 16u; i++) {
        adc_hist_16s[i] = adc_code;
    }
    adc_hist_idx = 0u;
    adc_prev_1s = adc_code;
}

/* 1000ms 任务中顺序严格固定：1. 采样与双向突变守卫 -> 2. 自增 heat_seconds -> 3. 评估 */
void heat_slope_task_1s(void) {
    unsigned int adc_old;
    
    /* 严格状态门控：仅在 ST_HEAT 且加热中执行，杜绝 WARM 态被误杀 */
    if (state != ST_HEAT || !heater_on) {
        return;
    }

    /* 1. 采样与对称突变扰动守卫 (40 码约合 9°C 骤变，物理上不可能在 1 秒内自加热产生) */
    /* 上升沿防加冷水误杀干烧；下降沿防探头接触不良/EMI 虚假暴热欺骗斜率门 */
    /* 时序铁律：先计数判定、后复位基线——heat_slope_reset() 不清 glitch_cnt，否则 E-01 永不可达 */
    unsigned int adc_diff = (adc_code >= adc_prev_1s) ? (adc_code - adc_prev_1s) : (adc_prev_1s - adc_code);
    if (adc_diff > 40u) {
        /* 扰动计数：连续 3 次突变判定探头微动磨损/接触不良（物理阻抗瞬时剧增），统一报警 E-01 开路类故障 */
        if (++glitch_cnt >= 3u) {
            enter_fault(1u); /* 报 E-01，防止误导维修至短路排查 */
            return;
        }
        heat_slope_reset(); /* 只重填环形基线，不碰计数；计数靠干净采样清零 */
        return;
    }
    glitch_cnt = 0u; /* 干净采样清零：孤立跳变不累计 */
    adc_prev_1s = adc_code;

    /* 单周期位与推进 16 槽环形队列 (实际跨度整 16 秒) */
    adc_hist_idx = (adc_hist_idx + 1u) & 0x0Fu;
    adc_old = adc_hist_16s[adc_hist_idx];
    adc_hist_16s[adc_hist_idx] = adc_code;

    /* 2. 唯一自增点：严格在此自增，旧 one_second_task 中的自增块必须删除！ */
    heat_seconds++;

    /* 3. 斜率评估：加热满 20 秒且当前水温 < DRYFIRE_TEMP_C (45°C) 时激活 */
    if (heat_seconds >= 20u && temp_c < DRYFIRE_TEMP_C) {
        /* 16 秒前码值减当前码值：正常升温码值下降，差值为正；不足 6 码判定干烧 */
        if (adc_old <= adc_code || (adc_old - adc_code) < 6u) {
            enter_fault(3u); /* E-03 干烧报警 */
            return;
        }
    }

    /* 两级超时兜底 (固件单镜像常量) */
    if (heat_seconds > 650u && temp_c < DRYFIRE_TEMP_C) {
        enter_fault(3u); /* 一级超时 */
    }
    if (heat_mode == MODE_BOIL_100 && heat_seconds > 550u && temp_c < BOIL_TEMP_C) {
        enter_fault(3u); /* 二级超时 */
    }
}
```

---

### 4.2 10ms Tick 守卫的双向 20ms 去抖与上电 POST 卡键抑制

```c
/* 初始化块 (main:889-918) 增加 POST 上电卡键抑制 */
void button_init_post(void) {
    btn_onoff_held = (BTN_ONOFF == 0) ? 1u : 0u;
    btn_func_held  = (BTN_FUNC == 0)  ? 1u : 0u;
    db_onoff_press = 0u; db_onoff_release = 0u;
    db_func_press  = 0u; db_func_release  = 0u;
}

/* 严格受 10ms tick 守卫调用的双向 2 拍状态机 */
static void button_scan_10ms(void) {
    /* ---- ON/OFF 键 (低有效) ---- */
    if (BTN_ONOFF == 0) {
        db_onoff_release = 0u;
        if (!btn_onoff_held && ++db_onoff_press >= 2u) {
            btn_onoff_held = 1u;
            evt_onoff = 1u; /* 连续 2 拍 10ms 采 0 -> 确认按下 */
        }
    } else {
        db_onoff_press = 0u;
        if (btn_onoff_held && ++db_onoff_release >= 2u) {
            btn_onoff_held = 0u; /* 连续 2 拍 10ms 采 1 -> 确认释放 */
        }
    }
    /* ---- FUNC 键完全对称处理 ---- */
}
```

---

### 4.3 波形通道与绝对时戳契约（ADR-0068 落地细化）

1. **源端绝对时戳与单一真相（SSOT）**：
   `t_now = pal_wasm_get_virtual_clock_us()` 读取底层 `s_virtual_us`。
2. **Generation 与原子对保对策略**：
   - 每次物理按下生成自增 `generation++`；
   - 投递按下沿（`level=0, t_press=t_now`）与释放沿（`level=1, t_release=t_now + real_press_us`）；
   - **迟到与丢弃策略**：
     - 按下沿迟到：软钳位至 `t_now`，且必须保证与释放沿之间至少保留 $30,000\mu s$（30ms）物理宽度，确保能穿透 20ms 固件去抖；
     - 释放沿迟到：软钳位至 `t_now` 立即交付；
     - 若因异常整体丢弃，必须**整对取消**（调 `cancel_waveform_generation`），严禁产生悬空释放造成“按键卡死”。
3. **引脚映射**：Header `P04` $\to$ 引脚编号 `4`，`P05` $\to$ `5`。

---

## 5. 详细任务拆分与执行说明（🔴 必选）

---

### Task 1（P0，Phase-A）：固件按键双向去抖、POST 卡键抑制与文档纠偏 `[ 状态: ⏳ 待开始 ]`

| 字段 | 内容 |
|------|------|
| **负责人** | 嵌入式固件组 |
| **预估工时** | 4 小时 |
| **优先级** | 🔴 P0（即刻施工） |
| **修改文件** | `wink-micro-app/mcs51_health_pot/health_pot.c`, `docs/DESIGN.md` |

#### 详细步骤
- [ ] **Step 1：重构按键去抖与初始化 POST**
  在 `health_pot.c` 中实现 `button_init_post()` 与 `button_scan_10ms()`，消除死变量 `db_onoff/db_func`。
- [ ] **Step 2：修正保温迟滞为 `WARM_HYST_C:68 = 2u`**
  放宽至 $\pm 2^\circ\text{C}$ 保护继电器。
- [ ] **Step 3：纠偏 `DESIGN.md`**
  更新 `DESIGN.md:19` 响应延迟为 20~30ms，记录 2°C 迟滞与 POST 卡键抑制。

---

### Task 2（P0，Phase-A）：彻底清理旧干烧块、16 槽 xdata 斜率算法与放宽超时 `[ 状态: ⏳ 待开始 ]`

| 字段 | 内容 |
|------|------|
| **负责人** | 嵌入式固件组 |
| **预估工时** | 6 小时 |
| **优先级** | 🔴 P0（即刻施工） |
| **修改文件** | `wink-micro-app/mcs51_health_pot/health_pot.c`, `unisim-scenarios/` |

#### 详细步骤
- [ ] **Step 1：彻底删除旧干烧计数与判据块（核心防回归）**
  **干烧逻辑唯一归属 `heat_slope_task_1s`**；在 `health_pot.c` 的 `one_second_task()` 中彻底删除原 728~734 行的 `heat_seconds++` 与两级判据，严禁两处自增！
- [ ] **Step 2：实现 `xdata` 16 槽环形斜率算法与对称扰动守卫**
  实现 `adc_hist_16s[16]` 算法，加入双向对称 40 码突变扰动守卫（上升沿防加水误判，下降沿防虚假暴热欺骗斜率门）；扰动分支严格采用“先计数判定、后环复位”时序（`heat_slope_reset()` 不清 `glitch_cnt`，`glitch_cnt` 仅在 HEAT 入口三落点与干净采样处清零），连续 3 次突变统一报警 E-01 开路/接触不良类故障，并加入 `state == ST_HEAT && heater_on` 状态门。
- [ ] **Step 3：放宽固件单镜像两级超时常量**
  Stage 1 设为 650s，Stage 2 设为 550s；并在进入 HEAT 时全环预填（预填落点：FUNC 切档 / WARM 重煮 / OFF 开机三处，每处同步 `glitch_cnt = 0u`）。
- [ ] **Step 4：场景文件时序注释更新与 Hold-time 审计**
  更新 `health-pot-dryfire.scenario.json` 注释（斜率机制在第 20 个 tick 秒、对应场景墙钟约 21s 触发报警）；全面扫描 15 个场景，确保无非预期的 $T < 45^\circ\text{C}$ 长保持死区。

---

### Task 3（P0，Phase-A）：既有 15 个场景断言同步更新与 ADR 交付 `[ 状态: ⏳ 待开始 ]`

| 字段 | 内容 |
|------|------|
| **负责人** | 系统架构组 |
| **预估工时** | 6 小时 |
| **优先级** | 🔴 P0（即刻施工） |
| **修改文件** | `unisim-scenarios/health-pot-boil-warm.scenario.json`, `docs/decisions/unisim/0067-*.md`, `0068-*.md` |

#### 详细步骤
- [ ] **Step 1：同步修正 `health-pot-boil-warm.scenario.json`**
  将回跳断言点由 59°C 调整为 58°C（对齐 2°C 迟滞）。
- [ ] **Step 2：合入 ADR-0067 与 ADR-0068 正式技术决策**
  在 `docs/decisions/unisim/` 固化 Profile 架构与绝对时戳波形契约。

---

### Task 4（P1，Phase-B）：快速闭环新场景联调 `[ 状态: ⏳ 待前置完成 ]`

| 字段 | 内容 |
|------|------|
| **负责人** | 跨仓联调组 |
| **前置依赖** | **D-003**（UniSim TS plant-profile）、**D-004**（Frontend 原子波形注入） |
| **预估工时** | 6 小时 |
| **修改文件** | `unisim-scenarios/health-pot-fast-boil.scenario.json` |

#### 详细步骤
- [ ] **Step 1：配置并运行 `health-pot-fast-boil.scenario.json`**
  配置 `timeoutUs: 150000000`（150s），指定固定 `prngSeed`，依据 ADR-0055 设定公差，验证 94s 自发烧开断电。

---

### Task 5（P1，Phase-B）：4COM 显示时延与 Bounce TDD 门禁 `[ 状态: ⏳ 待前置完成 ]`

| 字段 | 内容 |
|------|------|
| **负责人** | 质量组 |
| **前置依赖** | Task 1（固件去抖）＋ D-004（原子波形注入，外仓；Bounce 注入能力来自 D-004） |
| **预估工时** | 6 小时 |
| **优先级** | 🟡 P1 |
| **修改文件** | `unisim-scenarios/` 回归断言与 host 单测（本仓）；注入侧能力随 D-004 验收 |

#### 详细步骤
- [ ] **Step 1：数码管稳定帧 $\le 50\text{ms}$ 门禁**
- [ ] **Step 2：8ms 触点抖动（Bounce）注入回归（注入能力依赖 D-004，本任务只做断言与门禁）**

---

## 6. 测试策略与验收门禁（🔴 必选）

### L0 编译门禁（必须 100% 通过）
- [ ] 架构分层扫描：`python wink-tools/wink.py lint arch --pack layering --pack api` 零违规
- [ ] 内存映射检查（Map Gate）：`adc_hist_16s` 显式分配于 XDATA；直接寻址区 `DSEG` $\le 96$ 字节，预留至少 32 字节保障中断嵌套调用栈安全
- [ ] 转译子集检查：无浮点、无软除法、无未转译宏
- [ ] mcs51 应用构建：`python wink-tools/wink.py build --app mcs51_health_pot --target wasm` 零错误零警告
- [ ] 固件单测：`python wink-tools/wink.py test --app mcs51_health_pot --target host` 100% 通过

### L1 单元测试（量产公差与时序）
- [ ] **测温公差带**：NTC 测温公差放宽至量产合理的 **$\pm 5\%$**。
- [ ] **去抖健壮性**：8ms Bounce 注入下，单次按键识别率 100%，无重入。
- [ ] **双向扰动抗性**：单测注入 +50 码（加水）与 −50 码（瞬态抖动），断言基线复位且不误报 E-03；注入连续 3 次跳变，断言正确报警 E-01。
- [ ] **时基漂移定量**：接受 UART 阻塞导致的 ~1% tick 漂移，断言以“第 20 个 tick 秒（场景约 21s）”为基准。

### L2 集成仿真测试（分阶段出口）
- [ ] **Phase-A 出口**：全量 15 个既有场景（含 `dryfire` 与 `boil-warm`）**100% 全绿**通过。
- [ ] **Phase-B 出口**：外仓到位后，`health-pot-fast-boil` 94s 自发烧开断电，全自动无头通过。

---

## 7. 风险登记册（🔴 必须纳入管理）

| 风险ID | 风险描述 | 严重度 | 缓解措施 | 责任人 |
|--------|----------|--------|----------|--------|
| R-001 | 传感器双向突变与微动间歇接触不良 | 🔴 高 | 对称 40 码突变先计数后复位（复位不清计数）；连续 3 次报警 E-01 | 嵌入式组 |
| R-002 | `adc_hist_16s` 挤爆 8051 直接寻址区 | 🔴 高 | 显式修饰 `xdata`，L0 门禁硬性检查 `DSEG <= 96B` | 架构组 |
| R-003 | 脚本注入与 Plant 自激演算打架 (C14.5) | 🟡 中 | 场景表显式隔离激励源，禁止混用 | 质量组 |
| R-004 | UART 阻塞导致 tick 漂移 | 🟢 低 | 定量分析证实影响 $<1\%$，接受并纳入时基分析界 | 固件组 |
| R-005 | （残余风险）周期性离散扰动延迟斜率检出 | 🟡 中 | ① 650s 绝对超时兜底；② 硬件双金属片 115°C 独立物理熔断；③ **持续干烧检出上限声明**：无扰动干烧检出 $\le 36\text{s}$（20s 门限 + 16s 历史窗），已由 dryfire 场景覆盖 | 嵌入式与安规组 |

---

## 8. 附录：非目标与后续独立计划声明 (C11.1)

下列高阶小家电功能安全与电气特性属于**显式非目标**，将在随后的独立专项计划中展开：
1. **后续独立专项**：[`PLAN-20260915-APPLIANCE-SAFETY-AND-GB4706`](./2026-09-15-appliance-safety-and-gb4706-compliance-plan.md)（已立项，涵盖 E-03/E-04 后 60s 强热余温冷却锁定与继电器防拉弧延寿、GB4706 认证级故障注入矩阵）；
2. **真机/HIL 独占非目标**：继电器物理粘连由 TCO 硬件保险熔断；水垢热阻累计与高原气压沸点修正由真机标定承担。
