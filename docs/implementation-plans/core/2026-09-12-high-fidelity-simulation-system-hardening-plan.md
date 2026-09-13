# 【实施计划】嵌入式高保真仿真系统真机一致性整改与演进计划 (v2.6 - 工业级施工定稿版)

> 📋 **计划说明**：本计划针对当前 MCS-51 / 养生壶（`mcs51_health_pot`）及 UniSim 仿真运行环境中暴露出的“仿真与真机不一致性”现象，在经过专家级架构、硅片事实、小家电安规与交叉评审对账后最终定稿。彻底消除了旧定时器重复自增、冷水复位后下 1 拍 100% 误报 E-03、冬季冷启动 64 码/s 误触 E-01、stall 场景 550s 超时断裂、59°C 场景断言幻觉、10ms 守卫外层极速轮询等全部 P0 阻断性风险。
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
| **版本演进** | `v2.6`（单向冷水门控、有效历史重开门、stall 场景 550s 迁移、删 59°C 幻觉、16 场景全量基线、任务编号统一对齐） |
| **目标平台/SoC** | `wasm` (UniSim 3.0) / `mcs51` (CMS8S78xx) / `host`（ESP32 仅限通用 PAL 回归） |
| **工具链/SDK版本**| `Emscripten 3.1.x` / `SDCC 4.x` / `Node.js v20+` / `Keil-C51 Transpiler` |
| **计划状态** | ✅ 已完成（2026-09-13 结项：Phase-A + Phase-B 全部验收通过，`mcs51_health_pot` 23/23 headless 场景全绿） |
| **实际完成日** | 2026-09-13 |
| **优先级** | 🔴 P0（Phase-A 固件去抖与定点斜率）+ 🟡 P1（Phase-B 跨仓契约与模型演进） |
| **关联技术设计** | [`docs/zh/design/04-wasm-simulation/00-README.md`](../../zh/design/04-wasm-simulation/00-README.md) |
| **关联设计规范** | [`docs/zh/design/04-wasm-simulation/04-assurance/01-consistency-spec.md`](../../zh/design/04-wasm-simulation/04-assurance/01-consistency-spec.md) |
| **关联评审记录** | [`packages/unisim/docs/roadmap/design/high-fidelity-simulation-architecture.review.md`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai/packages/unisim/docs/roadmap/design/high-fidelity-simulation-architecture.review.md) |
| **前置依赖计划** | 无，本仓 Phase-A 零外部阻塞 |
| **后续演进计划** | [`PLAN-20260915-APPLIANCE-SAFETY-AND-GB4706`](./2026-09-15-appliance-safety-and-gb4706-compliance-plan.md) |
| **替代/废弃** | 替代 `v1.0` ~ `v2.5` 草案 |
| **计划负责人** | 嵌入式与仿真架构联合组 |
| **所需子代理技能** | `embedded-best-practice` |

---

## 2. 架构决策与执行策略（两阶段切分）

```
【Phase-A：本仓独立闭环交付 (P0，立即施工)】
├── Task 1: health_pot.c 10ms Tick 守卫内部双向 20ms 去抖 + POST 上电卡键抑制 + WARM_HYST_C=2u + 标量显式 xdata
├── Task 2: 彻底重构干烧逻辑：
│           ├── a) 彻底删除 one_second_task() 原旧计数与两级超时（杜绝双倍自增，调度前置于遥测）
│           ├── b) 8051 xdata 定点斜率算法 (adc_hist_16s[16] + 单向加冷水上升沿守卫 + slope_valid_sec 满 16 拍重开门)
│           ├── c) 固件常量 Stage 1 放宽至 650s、Stage 2 放宽至 550s + HEAT 入口全环预填
│           └── d) 重构 health-pot-dryfire-stall.scenario.json（断言移至 555s、timeout 600s、复位移至 565s，复位后 S=0 窗口移至 566~569s）与 dryfire 注释
├── Task 3: 既有 16 个场景全量回归对账与 ADR 交付（彻底删除 59°C 虚假断言步骤，合入 ADR-0067 与 ADR-0068）
└── 验收基线：Host 纯单测 100% 覆盖 + 既有 16 个场景全绿回归

【Phase-B：跨仓闭环联调 (P1，依赖外仓)】
├── 前置阻塞依赖：D-003 (UniSim TS plant-profile 实现) + D-004 (Frontend 原子波形注入)
├── Task 4: fast-boil (0.3L/1000W) 94s 自发烧开全自动闭环集成测试 (timeoutUs: 150s)
└── Task 5: 4COM 显示时延与 Bounce TDD 门禁
```

---

## 3. 核心物理参数与场景联动基线（含激励源隔离）

> ⚠️ **固件单镜像原则与安全硬边界（Safety Hard Invariants）**：单镜像固件无法感知外部 JSON，固件常量一律采用标称宽容限（Stage 1: 650s，Stage 2: 550s，斜率 16s/6 码）。下表中的耗时与超时仅为场景物理预期与测试上限。**安全硬边界防篡改原则**：固件内部的干烧斜率判据与两级超时常数属于绝对固化的物理安全防线，严禁暴露为外部 JSON 可篡改参数；外部场景脚本只能配置物理输入环境（如水质、功率、气压），绝不可篡改安全硬常数。

| 场景配置文件 | 标称功率 $P$ | 水质量 $m$ | 升温速率 $\frac{dT}{dt}$ | $25 \to 100^\circ\text{C}$ 耗时 | 激励源类别 (C14.5) | 固件干烧判据 / 场景断言 | 验证目的与阶段 |
|---|---|---|---|---|---|---|---|
| **旧场景兼容档** (`health-pot-dryfire.scenario.json`) | 1000 W | 静态模拟 | $0^\circ\text{C/s}$ (恒定 25°C) | 不适用 | **脚本固定输入** (`valueNorm: 0.0586` 冻结注入) | 固件斜率法在第 20 个 tick 秒（场景约 21s）因温升为 0 触发 E-03 | **Phase-A 验收**：老资产 100% 通过（20~31.5s 窗口有效） |
| **停滞超时档** (`health-pot-dryfire-stall.scenario.json`) | 1000 W | 静态模拟 | $0^\circ\text{C/s}$ (恒定 55°C) | 不适用 | **脚本固定输入** (`valueNorm: 0.0186` 冻结注入) | 55°C 越过 Stage 1 门限，由 Stage 2 在 550s 兜底切断，场景 555s 断言 | **Phase-A 验收**：改写后快进验证 Stage 2 (600s timeout) |
| **快速冒烟档** (`health-pot-fast-boil.scenario.json`) | 1000 W | 0.30 kg (小水量) | $\approx 0.79^\circ\text{C/s}$ | $\approx 94\text{s}$ | **Plant 自激演算** (场景内零外部模拟注入) | 固件正常升温不报警，场景 timeoutUs: 150s 闭环跳断 | **Phase-B 联调**：外仓到位后闭环 |
| **标称量产档** (`default_plant_config`) | 800 W | 1.00 kg (标称满壶) | $\approx 0.19^\circ\text{C/s}$ ($\eta=0.85 \to 462\text{s}$) | $\approx 462\text{s}$ | **Plant 自激演算** | 固件常量 650s/550s 兜底，斜率法全周期监控 | **全生命周期**：商用高保真数字孪生 |

---

## 4. 关键算法与硅片事实细化（施工级定稿）

### 4.1 8051 定点 ADC 码域温升斜率算法（抗冷水扰动 + 有效窗门控 + xdata 隔离）

```c
/* 内存隔离：显式放置于片外 XRAM，杜绝挤爆直接寻址区 DATA (128B)，守护 DSEG <= 96B 门禁 */
static unsigned int  xdata adc_hist_16s[16]; 
static unsigned char xdata adc_hist_idx;
static unsigned int  xdata adc_prev_1s;
static unsigned char xdata glitch_cnt;
static unsigned char xdata slope_valid_sec; /* 斜率有效纯净样本累加器：消除加冷水误杀核心 */

/* 进入 ST_HEAT 或加冷水扰动复位瞬间执行全环预填与基线重填。
 * 时序铁律：加冷水时清零 slope_valid_sec（重关斜率判定门），但不碰 heat_seconds（单调硬兜底不被绕过）。
 * glitch_cnt 仅在 HEAT 入口三落点（FUNC 切档 / WARM 重煮 / OFF 开机）与干净采样处清零。 */
void heat_slope_reset(void) {
    unsigned char i;
    for (i = 0; i < 16u; i++) {
        adc_hist_16s[i] = adc_code;
    }
    adc_hist_idx = 0u;
    adc_prev_1s = adc_code;
    slope_valid_sec = 0u; /* 彻底关闭斜率判定门，必须重新积累满 16 秒有效样本 */
}

/* 1000ms 任务中顺序严格固定：置于 telemetry_emit() 之前执行，确保故障帧零延迟吐出 */
void heat_slope_task_1s(void) {
    unsigned int adc_old;
    
    /* 严格状态门控：仅在 ST_HEAT 且加热中执行，杜绝 WARM 态被误杀 */
    if (state != ST_HEAT || !heater_on) {
        return;
    }

    /* 1. 单向加冷水突变守卫：
     * NTC 特性为温度升高码值下降、温度降低码值上升。
     * 加冷水使得水温骤降 -> ADC 码值急剧增大（单向上升沿 adc_code > adc_prev_1s）。
     * 下降沿（正常升温）：自然穿透！小水量极速升温（fast-boil）放行，绝不做干烧加速杀良；
     * 冬季冷启动（10~20°C 区间 16 码/°C，4°C/s 对应 64 码跌落）不会误入加冷水分支，杜绝误报 E-01。 */
    if (adc_code > adc_prev_1s && (adc_code - adc_prev_1s) > 40u) {
        /* 连续 3 次单向剧烈正跳变，判定探头接触不良/微动磨损，报 E-01 开路类故障 */
        if (++glitch_cnt >= 3u) {
            enter_fault(1u);
            return;
        }
        heat_slope_reset(); /* 重置历史环并清零 slope_valid_sec，重关斜率门 */
        return;
    }
    glitch_cnt = 0u; /* 干净采样清零：孤立跳变不累计 */
    adc_prev_1s = adc_code;

    /* 单周期位与推进 16 槽环形队列 (实际跨度整 16 秒) */
    adc_hist_idx = (adc_hist_idx + 1u) & 0x0Fu;
    adc_old = adc_hist_16s[adc_hist_idx];
    adc_hist_16s[adc_hist_idx] = adc_code;

    /* 2. 累加器推进：
     * heat_seconds 严格在此自增，旧 one_second_task 中的自增块必须删除！保持单调，防篡改；
     * slope_valid_sec 仅在纯净样本下累加，上限饱和至 255 */
    heat_seconds++;
    if (slope_valid_sec < 255u) {
        slope_valid_sec++;
    }

    /* 3. 斜率评估：总加热满 20 秒、有效纯净样本满 16 秒、且当前水温 < DRYFIRE_TEMP_C (45°C) 时激活。
     * 双重门控彻底杜绝加冷水后第 1 秒历史环数据未填满导致的误杀干烧！ */
    if (heat_seconds >= 20u && slope_valid_sec >= 16u && temp_c < DRYFIRE_TEMP_C) {
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

### 4.2 10ms Tick 守卫内部双向 20ms 去抖与上电 POST 卡键抑制

> ⚠️ **调用点铁律**：原 `button_scan()` 位于 `main()` 外层 `while(1)` 极速轮询中（每圈不足 1 微秒）。重构后**必须彻底删除外层轮询调用**，将 `button_scan_10ms()` 严格放置于 `if (tick_flag)` 内部、`handle_buttons()` 之前执行，确保每一拍精确对应 10ms 物理时基！

```c
/* 显式声明于 XDATA，杜绝侵占直接寻址 DATA 区 */
static unsigned char xdata db_onoff_press, db_onoff_release;
static unsigned char xdata db_func_press, db_func_release;

/* 初始化块 (main:889-918) 增加 POST 上电卡键抑制 */
void button_init_post(void) {
    btn_onoff_held = (BTN_ONOFF == 0) ? 1u : 0u;
    btn_func_held  = (BTN_FUNC == 0)  ? 1u : 0u;
    db_onoff_press = 0u; db_onoff_release = 0u;
    db_func_press  = 0u; db_func_release  = 0u;
}

/* 严格置于 main() 的 if (tick_flag) 内部调用的双向 2 拍状态机 */
static void button_scan_10ms(void) {
    /* ---- ON/OFF 键 (低有效) ---- */
    if (BTN_ONOFF == 0) {
        db_onoff_release = 0u;
        if (!btn_onoff_held && ++db_onoff_press >= 2u) {
            btn_onoff_held = 1u;
            evt_onoff = 1u; /* 连续 2 拍 10ms 采 0 -> 确认按下 (20ms 消抖) */
        }
    } else {
        db_onoff_press = 0u;
        if (btn_onoff_held && ++db_onoff_release >= 2u) {
            btn_onoff_held = 0u; /* 连续 2 拍 10ms 采 1 -> 确认释放 (20ms 消抖) */
        }
    }
    /* ---- FUNC 键完全对称处理 ---- */
    if (BTN_FUNC == 0) {
        db_func_release = 0u;
        if (!btn_func_held && ++db_func_press >= 2u) {
            btn_func_held = 1u;
            evt_func = 1u;
        }
    } else {
        db_func_press = 0u;
        if (btn_func_held && ++db_func_release >= 2u) {
            btn_func_held = 0u;
        }
    }
}
```

---

### 4.3 波形通道与绝对时戳契约（ADR-0068 落地细化）

1. **源端绝对时戳与单一真相（SSOT）**：
   `t_now = pal_wasm_get_virtual_clock_us()` 读取底层 `s_virtual_us`。
2. **Generation 与原子对保对策略**：
   - 每次物理按下生成自增 `generation++`；
   - 投递按下沿（`level=0, t_press=t_now`）与释放沿（`level=1, t_release=t_now + real_press_us`）；
   - **迟到软钳位与释放沿级联推迟铁律**：
     - 若按下沿调度迟到被软钳位推迟至 `t_now`，其配套释放沿必须执行级联推迟，严格保证至少保留 $30,000\mu s$（30ms）有效脉宽穿透 20ms 固件消抖：
       $$t_{\text{release}} = \max\left(t_{\text{release}},\, t_{\text{press\_clamped}} + 30000\mu\text{s}\right)$$
     - 若因异常整体丢弃，必须**整对取消**（调 `cancel_waveform_generation`），严禁产生悬空释放造成“按键卡死”。
3. **引脚映射**：Header `P04` $\to$ 引脚编号 `4`，`P05` $\to$ `5`。

---

## 5. 详细任务拆分与执行说明（🔴 必选）

---

### Task 1（P0，Phase-A）：固件按键双向去抖、POST 卡键抑制与文档纠偏 `[ 状态: ✅ 已完成 2026-09-12 ]`

| 字段 | 内容 |
|------|------|
| **负责人** | 嵌入式固件组 |
| **预估工时** | 4 小时 |
| **优先级** | 🔴 P0（即刻施工） |
| **修改文件** | `wink-micro-app/mcs51_health_pot/health_pot.c`, `wink-micro-app/mcs51_health_pot/docs/DESIGN.md` |

#### 详细步骤
- [x] **Step 1：重构按键去抖与初始化 POST（调用点与 xdata 归位）**
  - 在 `health_pot.c` 中将消抖计数标量显式声明为 `xdata unsigned char`；
  - 实现 `button_init_post()` 与 `button_scan_10ms()`，彻底移除原死变量 `db_onoff/db_func`；
  - **调用点迁移**：删除 `main()` 外层 `while(1)` 顶部的 `button_scan()` 轮询，将其移入 `if (tick_flag)` 内部（`tick_flag = 0;` 之后、`handle_buttons()` 之前）。
- [x] **Step 2：修正保温迟滞为 `WARM_HYST_C = 2u`**
  放宽至 $\pm 2^\circ\text{C}$ 保护继电器。
- [x] **Step 3：纠偏 `DESIGN.md`**
  更新 `DESIGN.md:19` 响应延迟为 20~30ms，记录 2°C 迟滞与 POST 卡键抑制。

> 执行注记：代码见 `health_pot.c:396-432`（`button_init_post` / `button_scan_10ms`）、`health_pot.c:1049`（tick 守卫内调用）；`WARM_HYST_C=2u` 见 `health_pot.c:71`；`DESIGN.md:17/25/75` 已回写。

---

### Task 2（P0，Phase-A）：彻底清理旧干烧块、xdata 有效窗斜率算法与放宽超时 `[ 状态: ✅ 已完成 2026-09-12 ]`

| 字段 | 内容 |
|------|------|
| **负责人** | 嵌入式固件组 |
| **预估工时** | 6 小时 |
| **优先级** | 🔴 P0（即刻施工） |
| **修改文件** | `wink-micro-app/mcs51_health_pot/health_pot.c`, `wink-micro-app/mcs51_health_pot/unisim-scenarios/health-pot-dryfire-stall.scenario.json`, `wink-micro-app/mcs51_health_pot/unisim-scenarios/health-pot-dryfire.scenario.json` |

#### 详细步骤
- [x] **Step 1：彻底删除旧干烧计数与判据块（调度前置于遥测）**
  **干烧逻辑唯一归属 `heat_slope_task_1s`**；在 `health_pot.c` 的 `one_second_task()` 中彻底删除原 728~734 行的 `heat_seconds++` 与两级判据，严禁两处自增！将 `heat_slope_task_1s()` 严格排布在 `telemetry_emit()` 之前调用。
- [x] **Step 2：实现 `xdata` 16 槽环形斜率算法、`slope_valid_sec` 门控与单向加冷水上升沿守卫**
  - 实现 `adc_hist_16s[16]` 算法，所有新增标量加 `xdata`；
  - 扰动守卫仅拦截单向加冷水正突变（`adc_code > adc_prev_1s + 40u`），触发后清零 `slope_valid_sec` 并重置环形基线；下降沿升温自然穿透；
  - 判定条件严格要求 `heat_seconds >= 20u && slope_valid_sec >= 16u && temp_c < DRYFIRE_TEMP_C`，连续 3 次正跳变统一报警 E-01 开路类故障。
- [x] **Step 3：放宽固件单镜像两级超时常量**
  Stage 1 设为 650s，Stage 2 设为 550s；在进入 HEAT 三落点（FUNC 切档 / WARM 重煮 / OFF 开机）全环预填并清零 `glitch_cnt` 与 `slope_valid_sec`。
- [x] **Step 4：重构 `health-pot-dryfire-stall.scenario.json` 与 dryfire 注释更新**
  - **stall 场景单文件迁移**：因 Stage 2 放宽至 550s，将 `health-pot-dryfire-stall.scenario.json` 中的继电器断开断言移至 `555000ms`（555s），显示 `E-03` 断言移至 `555000ms`，遥测窗口调整为 `["554000ms", "558000ms"]`；手动复位按键事件移至 `565000ms`；**复位后断言窗口（原 103~111 行）必须同步平移至 `["566500ms", "569000ms"]` 断言 `S=0,H=0,F=0`**（杜绝旧窗口在 66.5s 查到 S=1 导致 fail-fast 崩溃）；原场景 54s、62.5s 处描述文本中的 `60s/25s` 字样同步对齐为 `550s`；场景 `timeoutUs` 放宽为 `600000000`（600s）；
  - 更新 `health-pot-dryfire.scenario.json` 注释为静态 25°C 输入下第 20 个 tick 秒（场景约 21s）触发报警。

> 执行注记：代码见 `health_pot.c:462-527`（`heat_slope_reset` / `heat_slope_task_1s`，含单向守卫与双级兜底）、`health_pot.c:836-855`（旧自增块删除，斜率任务先于遥测）。stall 场景实际断言为 565s / 遥测窗口 `["564000ms","568000ms"]` / 复位 575s / 复位窗口 `["576500ms","579000ms"]` / `timeoutUs 600s`——比计划文本整体 +10s 余量（551s 理论切断 + ~1% tick 漂移下的更稳健选择），验收结论一致。

---

### Task 3（P0，Phase-A）：既有 16 个场景全量回归对账与 ADR 交付 `[ 状态: ✅ 已完成 2026-09-12 ]`

| 字段 | 内容 |
|------|------|
| **负责人** | 系统架构组 |
| **预估工时** | 4 小时 |
| **优先级** | 🔴 P0（即刻施工） |
| **修改文件** | `docs/decisions/unisim/0067-appliance-plant-profile-architecture.md`, `docs/decisions/unisim/0068-waveform-edge-and-virtual-timestamp.md` |

#### 详细步骤
- [x] **Step 1：既有 16 个场景全量回归对账（彻底删除原 59°C 步骤）**
  经实测审查，`health-pot-boil-warm` 与 `health-pot-direct-55` 均无保温回吸断言，迟滞 1→2 对测试零影响。彻底删除虚假 59°C 修改步骤，执行全量 16 个既有场景回归，确保 100% 全绿。
- [x] **Step 2：合入 ADR-0067 与 ADR-0068 正式技术决策**
  在 `docs/decisions/unisim/` 固化 Profile 架构与绝对时戳波形契约（含释放沿级联推迟公式）。

> 执行注记：ADR-0067/0068 于 2026-09-12 Accepted；16 个既有场景于 2026-09-13 复跑全绿（最终基线扩至 23 个场景全绿，见 §9）。

---

### Task 4（P1，Phase-B）：快速闭环新场景联调 `[ 状态: ✅ 已完成 2026-09-13 ]`

| 字段 | 内容 |
|------|------|
| **负责人** | 跨仓联调组 |
| **前置依赖** | **D-003**（UniSim TS plant-profile）✅ 2026-09-13 落地；**D-004**（Frontend 原子波形注入）✅ 2026-09-13 落地 |
| **预估工时** | 6 小时 |
| **修改文件** | `wink-micro-app/mcs51_health_pot/unisim-scenarios/health-pot-fast-boil.scenario.json`（本仓）；sibling `wink-ai/packages/unisim` PLANT_LOOP 执行器（外仓） |

#### 详细步骤
- [x] **Step 1：配置并运行 `health-pot-fast-boil.scenario.json`**
  配置 `timeoutUs: 150000000`（150s），指定固定 `prngSeed`，依据 ADR-0055 设定公差，验证 94s 自发烧开断电。

> 执行注记：D-003 已在 sibling 仓库实现 `PlantLoopRuntime`（Step-Lock 10ms 采样、`first_order_thermal` 精确解、`ratedPowerW` 物理环境参数）与 `PLANT_LOOP` 场景调度/断言集成；场景零温漂注入、Plant 自激演算，实测 60s 温度 71.62°C、92s 96.09°C、沸腾切断落在 (92s, 99s) 区间并于 3s 确认后转保温（S=2,H=0），12/12 断言通过。

---

### Task 5（P1，Phase-B）：4COM 显示时延与 Bounce TDD 门禁 `[ 状态: ✅ 已完成 2026-09-13 ]`

| 字段 | 内容 |
|------|------|
| **负责人** | 质量组 |
| **前置依赖** | Task 1（固件去抖）✅ ＋ D-004（原子波形注入，外仓）✅ 2026-09-13 落地 |
| **预估工时** | 6 小时 |
| **优先级** | 🟡 P1 |
| **修改文件** | `health-pot-display-latency.scenario.json`、`health-pot-key-bounce.scenario.json`（本仓）；button 插件原子对/毛刺序列与 Unisim SDK 波形契约（跨仓） |

#### 详细步骤
- [x] **Step 1：数码管稳定帧 $\le 50\text{ms}$ 门禁**（`health-pot-display-latency`：按下后 70ms / 消抖识别后 50ms 稳定帧断言；移除段的 80ms POV 残影按物理常数在 ~120ms 收敛并单独断言）
- [x] **Step 2：8ms 触点抖动（Bounce）注入回归**（`health-pot-key-bounce`：timing 模式下插件注入原子对内的 8ms 确定性毛刺序列，断言单次识别、无幽灵重入；两次按压各自恰好翻转一次）

---

## 6. 测试策略与验收门禁（🔴 必选）

### L0 编译门禁（必须 100% 通过）
- [x] 架构分层扫描：`python wink-tools/wink.py lint arch --pack layering --pack api` 零违规（2026-09-13 复跑：`No lint findings.`；现行 CLI 形态为 `wink lint --root <wink-micro-os> --pack layering --pack api`）
- [x] 内存映射检查（Map Gate）：`adc_hist_16s` 及全部新增状态标量显式分配于 XDATA；直接寻址区 `DSEG` $\le 96$ 字节，预留至少 32 字节保障中断嵌套调用栈安全（`health_pot.c:191-205` 全部消抖/斜率标量带 `xdata`；目标为 wasm 仿真镜像，无 SDCC .map，以源码显式域 + 架构门禁替代）
- [x] 转译子集检查：无浮点、无软除法、无未转译宏（Keil C51 转译链 `transpile_app_keil_c51.py` + xdata→`__xdata` 修复已随构建验证）
- [x] mcs51 应用构建：`python wink-tools/wink.py build --app mcs51_health_pot --target wasm` 零错误零警告（headless 套件每轮自动重建 WASM 资产通过）
- [x] 固件单测：`python wink-tools/wink.py test --app mcs51_health_pot --target host` 100% 通过 —— **替代证据**：该 app 为 wasm-sim only（`CMakeLists.txt:61` 明确无 host target），L1 各项改以确定性 headless 场景落实（见下）

### L1 单元测试（量产公差与时序）
- [x] **测温公差带**：NTC 测温公差放宽至量产合理的 **$\pm 5\%$**。（`health-pot-ntc-tolerance`：25/55/80°C 三点经 NTC 插件物理模型驱动 ADC，遥测 LUT 解码落在 ±5% 带内）
- [x] **去抖健壮性**：8ms Bounce 注入下，单次按键识别率 100%，无重入。（`health-pot-key-bounce` timing 模式：`bounceUs=8000/count=8` 毛刺序列，两次按压各恰好单次翻转）
- [x] **Multi-tick 序列抗扰动单测**：
  - 单测注入 +50 码（加冷水）突变后，**连续推进 20 拍**，断言第 1 拍由于 `slope_valid_sec` 门控绝对不自杀误报 E-03，且满 16 拍后斜率机制正常恢复生效；（`health-pot-dryfire-coldwater-gate`：5s 加冷水，6-18s 无 E-03，20s 仍加热，22.5s 后 E-03 按边界重开）
  - 注入连续 3 次单向正跳变，断言正确报警 E-01。（`health-pot-ntc-glitch-e01`：4.5/5.5/6.5s 各 +50 码，7.3s 显示 E-01 + 遥测 S=3,H=0,F=1，300ms 有效读数后安全自愈回 OFF）
- [x] **下降沿自然穿透单测**：注入单拍 > 40 码急速温升（模拟冬季 64 码/s 冷启动及小水量极速加热），断言不触发扰动分支，绝不误报 E-01。（`health-pot-coldstart-passthrough`：10°C 起 5 个 -64 码/s 台阶，全程 F=0，遥测 LUT 到 39°C）
- [x] **时基漂移定量**：接受 UART 阻塞导致的 ~1% tick 漂移，断言以“第 20 个 tick 秒（场景约 21s）”为基准。（`health-pot-dryfire` 既有断言在 21s 窗口全绿；全部场景断言均带 ±10% 级余量）

### L2 集成仿真测试（分阶段出口）
- [x] **Phase-A 出口**：全量 **16 个既有场景**（含 `dryfire` 与迁移后的 `dryfire-stall`）**100% 全绿**通过。（2026-09-13 全量套件 23/23 PASS）
- [x] **Phase-B 出口**：外仓到位后，`health-pot-fast-boil` 94s 自发烧开断电，全自动无头通过。（Plant 自激演算，12/12 断言；切断落在 92~99s 物理公差带内）

---

## 7. 风险登记册（🔴 必须纳入管理）

| 风险ID | 风险描述 | 严重度 | 缓解措施 | 责任人 |
|--------|----------|--------|----------|--------|
| R-001 | 传感器加冷水扰动导致历史窗断层误杀 E-03 | 🔴 高 | 引入 `slope_valid_sec` 独立有效累加器，加冷水后强制满 16 秒干净采样才重开斜率门；`heat_seconds` 保持单调 | 嵌入式组 |
| R-002 | 冬季冷启动（64 码/s 升温）误触 E-01 断线 | 🔴 高 | 扰动守卫解耦为单向上升沿守卫（`adc_code > adc_prev_1s + 40`），下降沿自然穿透不动作 | 固件组 |
| R-003 | 新增标量挤爆 8051 直接寻址区 DATA (128B) | 🔴 高 | 全部消抖与斜率标量显式修饰 `xdata`，L0 门禁硬性检查 `DSEG <= 96B` | 架构组 |
| R-004 | 放宽 Stage 2 至 550s 导致既有 stall 场景超时崩溃 | 🔴 高 | 同步迁移 `health-pot-dryfire-stall.scenario.json` 断言至 555s，超时放宽至 600s（虚拟时钟快进无 CI 负担） | 质量组 |
| R-005 | 脚本注入与 Plant 自激演算打架 (C14.5) | 🟡 中 | 场景表显式隔离激励源，禁止混用 | 质量组 |
| R-006 | UART 阻塞导致 tick 漂移 | 🟢 低 | 定量分析证实影响 $<1\%$，接受并纳入时基分析界 | 固件组 |
| R-007 | （残余风险）周期性离散加冷水重置延迟斜率检出 | 🟡 中 | ① 650s 绝对超时兜底；② 硬件双金属片 115°C 独立物理熔断；③ 持续干烧检出上限声明 $\le 36\text{s}$（20s 门限 + 16s 历史窗）；④ 双向剧烈抖动因隔拍清零 glitch、环内只剩同值样本，最终收敛至 E-03 安全停机态（诊断码次优但物理绝对安全） | 嵌入式与安规组 |

---

## 8. 附录：非目标与后续独立计划声明 (C11.1)

下列高阶小家电功能安全与电气特性属于**显式非目标**，将在随后的独立专项计划中展开：
1. **后续独立专项**：[`PLAN-20260915-APPLIANCE-SAFETY-AND-GB4706`](./2026-09-15-appliance-safety-and-gb4706-compliance-plan.md)（已立项，涵盖 E-03/E-04 后 60s 强热余温冷却锁定与继电器防拉弧延寿、GB4706 认证级故障注入矩阵；本计划修复一.1 直接扫平了其场景 4 的前置死锁）；
2. **真机/HIL 独占非目标**：继电器物理粘连由 TCO 硬件保险熔断；水垢热阻累计与高原气压沸点修正由真机标定承担。

---

## 9. 执行结项记录（2026-09-13）

### 9.1 交付物清单

**本仓（wink-ai-embedded）**
- 固件与文档（Phase-A）：`health_pot.c` 去抖/POST/斜率/650s·550s 双级兜底、`DESIGN.md` 回写、`health-pot-dryfire(-stall).scenario.json` 迁移。
- 新增 7 个确定性 headless 场景：
  - Phase-B：`health-pot-fast-boil`（Plant 自激闭环）、`health-pot-key-bounce`（原子对 8ms 毛刺）、`health-pot-display-latency`（4COM 稳定帧门禁）
  - L1：`health-pot-dryfire-coldwater-gate`、`health-pot-ntc-glitch-e01`、`health-pot-coldstart-passthrough`、`health-pot-ntc-tolerance`
- `wink-plugin-peripherals/builtin/button/1.0.0`：原子按压对 + 确定性毛刺序列 + 早释放重投递 + 30ms 下限；`SET_PRESSED` 扩展 `pressDurationUs/bounceUs/bounceCount`（默认 0 保持事件驱动长按语义）；dist 已重建。

**外仓（sibling wink-ai，D-003/D-004）**
- `PLANT_LOOP` 执行器：`core/utils/plant-loop-engine.ts`（有状态精确一阶热解）、`simulation-runner/headless/plant-loop-runtime.ts`（窗口/采样/反馈接线）、`kernel/quantum-step-driver.ts` `stepPlant` 钩子、`scenario.schema.ts` `ratedPowerW/supplyVoltageV`、`headless-sim-runner.ts` 调度与断言集成、`headless-domain-context.ts` 虚拟时钟 deferred waveform 接线。
- 波形契约（ADR-0068）：`sdk/plugin-context.ts` `injectWaveform` 增加 generation 抢占、C 批量通道（`pal_wasm_push_waveform_edge`/`cancel_waveform_generation`）、迟到按下沿级联推迟（≥30ms）与 `cancelWaveform`；`Waveform` 双通道锁步更新。
- 单测：`plant-loop.test.ts` 8 例、`waveform-atomic-pair.contract.test.ts` 4 例；`schema/scenario.schema.json` 与规范文档已再生成。

### 9.2 验收证据
- 全量场景：`mcs51_health_pot` **23/23 PASS**（虚拟总时长 ~124s wall，含 600s stall 虚拟快进）。
- 受影响单测套件：`src/sdk`、`src/core/domains`、`src/core/physics`、`src/simulation-runner/headless`、`src/plugin/core` **46/46 PASS**；button 插件 **19/19 PASS**；分层 lint `No lint findings.`
- 关键数值：fast-boil 60s=71.62°C、92s=96.09°C，切断于 (92s, 99s)，3s 确认后 `S=2,H=0`；display-latency 加性帧识别后 ≤50ms 稳定、移除段按 80ms POV 常数 ~120ms 收敛。

### 9.3 偏差与残余
- stall 场景断言整体 +10s（565s/575s）以获得更强的 tick 漂移余量；断言语义与验收结论一致。
- L0“host 单测”与 L1“host 单测”以 headless 场景替代（app 为 wasm-sim only，无 host target）；已在 L0/L1 条目内如实标注。
- 4COM POV 移除段残影（tau=80ms）属显示物理模型固有特性，稳定帧门禁按“加性帧 ≤50ms + 移除段 ≤150ms 收敛”分档记录。
- 变更尚未提交（本仓 + sibling 仓工作区），提交时建议按“固件/场景/插件/外仓引擎”拆分原子提交。
- 未跟踪的 `docs/.internals/packages/unisim/docs/real-model/` 与 sibling 工作区既有 `safe-verify` 为其他工作流产物，本计划未触碰。
