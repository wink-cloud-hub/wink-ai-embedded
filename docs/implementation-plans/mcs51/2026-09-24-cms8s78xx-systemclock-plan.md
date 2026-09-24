# CMS8S78xx 系统时钟输出（CLO）高保真仿真适配实施计划

> 📋 **本文档是 CMS8S78xx 系统时钟示例（SystemClock）高保真仿真适配的正式实施计划（Layer-③）**。
> 遵循 [00-IMPLEMENTATION-PLAN-TEMPLATE.md](../../00-IMPLEMENTATION-PLAN-TEMPLATE.md) 模板规范，针对 [CMS8S78XX_EXAMPLE_CHECKLIST.md](../../vendors/Cmsemicon/CMS8S78XX_EXAMPLE_CHECKLIST.md) 第 9 章节官方子示例（编号 40: `SystemClock/code`，⚡ Level 3，P2）进行攻坚与闭环。

---

## 1. 元数据表（🔴 必选）

| 字段 | 内容 |
|---|---|
| **计划编号** | `PLAN-20260924-CMS8S78XX-SYSCLOCK-FIDELITY` |
| **创建日期** | `2026-09-24` |
| **目标平台/SoC** | `host` (GCC/MSVC C++17), `wasm` (Emscripten ASYNCIFY) 基于 `frameworks/mcs51` |
| **工具链/SDK版本**| GCC 11+, MSVC 19+, Emscripten 3.1+, 原厂 `CMS8S78xx_DemoCode_V2.0.2` |
| **计划状态** | `Done / 执行闭环（2026-09-24，5 任务全绿，偏离见 v1.3）` |
| **优先级** | P2（Checklist §9 编号 40，与清单口径一致） |
| **计划版本** | `v1.3`（评审吸收、架构融合与执行偏离见 §1.1） |
| **关联技术设计** | 原厂 `CMS8S78xx` 参考手册 Ch.4（系统时钟结构 §4.1，CLO 输出）；保真度基线 `2026-09-08-mcs51-simulation-vs-silicon-fidelity-and-test-limits.md` |
| **关联设计规范** | [`docs/zh/design/02-wink-micro-os/07-mcs51-simulation-interception.md`](../../zh/design/02-wink-micro-os/07-mcs51-simulation-interception.md) |
| **关联合格清单** | [`docs/vendors/Cmsemicon/CMS8S78XX_EXAMPLE_CHECKLIST.md`](../../vendors/Cmsemicon/CMS8S78XX_EXAMPLE_CHECKLIST.md) §9（编号 40） |
| **关联 ADR** | [ADR-0004](../../design/decisions/0004-static-dispatch-vs-runtime-ops.md)（静态分发与无虚表）、[ADR-0012](../../decisions/core/0012-fail-loud-contract-discipline.md)（契约诚实与强报错）、[ADR-0043](../../design/decisions/0043-arch-lint-rules.md)（分层门禁）、[ADR-0070](../../decisions/core/0070-mcs51-zero-code-simulation-interception-layer.md)（C++ 零侵入拦截）、[ADR-0071](../../decisions/core/0071-sfr-proxy-rmw-edge-data-plane.md)（XSFR 代理数据面）、[ADR-0072](../../decisions/core/0072-dual-clock-domain-and-quota-catchup.md)（双时钟域与微步调度） |
| **前置依赖计划** | `PLAN-20260924-CMS8S78XX-LVD-FIDELITY`（CLO 模型复用其验证过的微步/Headless 链路；`MCS51_MAX_PERIPHERALS` 在 LVD 落地后已满 12 槽） |
| **跨仓依赖** | **无**（CLO 走标准 `js_pal_gpio_write` ch1 数字写通道，`ASSERT_WAVEFORM` 为已有契约） |
| **目标里程碑** | 1. 建立 `cms8s_clo` 高保真时钟输出模型（P1.3 复用 CLO，Fsys/64 方波，buzzer 同构 + Bresenham 相位累加）；<br>2. 补齐 `REG_CMS8S78XX.H` 缺失的 `GPIO_P13_MUX_CLO (0x05)`；<br>3. 外设注册表扩容 `MCS51_MAX_PERIPHERALS 12→16`（LVD 已占满）；<br>4. 交付原厂源码零改动的微应用 `vendor_cms8s78xx_systemclock` 与三件套编译资产；<br>5. Host CTest（7 用例矩阵）+ Headless 波形断言（P13 CLO 375kHz + P32 闪烁，4s 长跑宽窗，v1.3 偏离替代 20ms 短窗）100% 绿灯并摘牌 Checklist 40。 |
| **所需技能** | `embedded-best-practice` |

### 1.1 变更记录

| 版本 | 日期 | 说明 |
|:---:|:---:|:---|
| v1.0 | 2026-09-24 | 初稿：CLO 模型 SSOT、µs 量化诚实标注、5 任务链 |
| v1.1 | 2026-09-24 | 深度评审补丁：Host notify 128 饱和断言纪律；Headless 虚拟时间预算 ≤200ms；Test 5 改直写 `clock_hz`；波形断言定为 `$near 250000±5000`。评审确认：TRIS 方向门控不拦截 bridge 直写（buzzer/ACMP 先例）、空 ISR 转译零风险、P13 无复用冲突、CLO 不进 `Mcu51Context` 故预算零影响 |
| v1.2 | 2026-09-24 | 专家评审融合闭环（Accepted）：① 纠正半周期物理公式（分子 64→32），引入纯整数 Bresenham 相位累加器实现 24MHz 下真 375kHz（消除 33.3% 频偏）；② 攻克 PinTracer 10000 槽环形缓冲溢出风险，Headless 虚拟时间由 ≤200ms 紧缩至 20ms，断言窗口前置至 `[2ms, 6ms]`；③ 扩充 Host CTest 至 7 用例矩阵（补齐 Classic 跨 Family 隔离防护与运行态 Reset 飞态清零安全）；④ 规范 CTest 路径为 `../frameworks/mcs51/test/cms8s78xx/test_cms8s_clo.cpp` 对齐既有规范。 |
| v1.3 | 2026-09-24 | 执行偏离记录（Done，证据见 Checklist #40）：① v1.2 的 20ms 紧凑预算 + `[2ms,6ms]` 前置短窗实证不可行——headless 引擎按 ~10ms 主节拍批量执行固件、同节拍内全部边沿共享一个时间戳，375kHz 在毫秒级窗口内恒读 0（CLO 模型经 wasm 内打点证实以真 375kHz 翻转，固件/P32 亦正常，纯属观测层量化问题）；改用 4s 长跑 + `[100ms,3900ms]` 宽窗（多秒平均稀释批量量化噪声至 <0.2%，`$near 375000±2000` 实测 ±500Hz 稳定通过，Virtual 4s / Wall ~3.6s）；② `getPinEdges` 反馈不分引脚，CLO 与 P32 须拆分为 `clo.scenario.json` / `p32.scenario.json` 双文件隔离（混窗互染计数）；③ P32 取活性带 `$between [1000,300000]`（实测 sim 节奏 50kHz，非硅片真值，符合原计划“不断言绝对周期”）。附带发现（非本计划引入，干净树可复现）：`timer0_timming_mode` 现读 10000（期望 5000），系引擎侧预存漂移，已知会相关方。 |

---

## 2. 背景与核心架构断层剖析

### 2.1 问题陈述

原厂 `SystemClock/code` 示例（仅 `main.c` + `isr.c`，ISR 全空）做两件事：① `SYS_SET_SYSTEM_CLK(SYS_CLK_DIV_1)` 配 Fsys=24MHz 后 `GPIO_SET_MUX_MODE(P13CFG, GPIO_P13_MUX_CLO)`，令 **P1.3 输出系统时钟 64 分频（CLO）方波**；② 主循环软件延时 `for(i=3000;i>0;i--)` 翻转 **P3.2 闪烁指示**。

当前框架存在 **3 项断层**：

1. **CLO 复用码缺失**：原厂 `gpio.h` 定义 `GPIO_P13_MUX_CLO (0x05)`，`REG_CMS8S78XX.H` 仅有 `P13_MUX_ADET/RXD`，无 CLO——原厂 `main.c` 直接编译不过（`shim_audit` GPIO 比对亦会报 vendor-not-in-shim 新增项，放行前为覆盖缺口）。
2. **无 CLO 输出模型**：即使 mux 写进影子，无任何模型驱动 P1.3（pin 11），P13 在仿真中恒为输入 HiZ——Headless 对 `gpio:11` 的任何断言恒失败。
3. **外设注册表已满**：LVD 落地后 `kCms8sDescs` 用满 `MCS51_MAX_PERIPHERALS (12)` 槽位，新 `cms8s_clo` 无处注册（强行注册触发 build 期 abort 熔断）。

P32 软件延时闪烁**无需外设模型**（纯 CPU 指令流，fiber 自然执行），但其虚拟时间周期未知——场景断言阈值必须**先跑一次实测再落值**（本计划 Task 4 显式安排 measure-then-assert）。

### 2.2 设计决策记录

| 编号 | 决策 | 结论 | 理由 |
|:---:|------|------|------|
| **D1** | CLO 模型形态 | **独立 `cms8s_clo.h/cpp` 外设，buzzer 同构**（`running/pin_level/next_toggle_us/toggle_count` + `MAX_TOGGLES_PER_POLL` 封顶） | buzzer 是已验证的"mux 门控 + 分频 pin toggle"同构解；`cms8s_sys.cpp` 已 569+ 行，不再塞入 |
| **D2** | 运行门控 | **`P13CFG == 0x05` 即运行，无使能寄存器** | 原厂示例无任何 CLO 使能调用，硅片即纯复用选择；频率恒 `Fsys/64`，`Fsys` 取 `ctx->clock_hz` 活值（CLKDIV 钩子维护，复位种子 24MHz） |
| **D3** | µs 量化与保真度 | **纯整数 Bresenham 相位累加器（真 375kHz 纳秒守恒）** | 纠正 v1.1 误将整周期分子 64 当半周期的物理失误（$T_{half}=\frac{32\times 10^6}{F_{sys}}$）；通过整型余数累加（24MHz 下翻转间隔序列 `[1, 1, 2, 1, 1, 2...]µs`，平均半周期 1.333µs），宏观波形频率 100% 保持在硅片真实的 375,000 Hz（0% 频偏），告别 250kHz 虚假参数 |
| **D4** | P32 与观测层批量量化 | **4s 长跑 + `[100ms, 3900ms]` 宽窗（替代 v1.2 的 20ms 前置短窗，见 v1.3）** | headless 引擎按 ~10ms 主节拍批量执行固件、同节拍边沿共享一时间戳，375kHz 在毫秒窗内恒读 0；headless 边沿采集为无界数组（非 PinTracer 10k 环形），多秒平均稀释批量量化噪声至 <0.2%；`getPinEdges` 不分引脚，CLO/P32 须分文件隔离 |
| **D5** | shim 范围 | **仅加 `GPIO_P13_MUX_CLO`**（P13CFG XSFR、`SYS_SET_SYSTEM_CLK`、GPIO 宏均已存在） | 最小差分；`shim_audit` 须保持 0 hard mismatch |
| **D6** | 注册表扩容 | **`MCS51_MAX_PERIPHERALS 12→16`** | LVD 已占满 12 槽；16 预留 LCD 类模型与新 family 空间，注释同步修订 |

---

## 3. 技术设计规范与 SSOT

### 3.1 CLO 寄存器与复用 SSOT

```text
P13CFG (XSFR 0xF013, 已存在): 0x05 = CLO (系统时钟 64 分频输出至 P1.3, pin 11)
CLKDIV (SFR 0x8F, TA 保护, 已建模): Fsys = Fosc (div=0) else Fosc/(2*div)
CLO 频率: Fclo = Fsys / 64 (示例 DIV_1 下 Fsys=24MHz → 理想 375kHz，Bresenham 相位累加器实现宏观零误差)
```

### 3.2 `cms8s_clo` 状态机与数据结构（Bresenham 高保真）

#### 3.2.1 状态结构体定义（纳入 `cms8s_priv.h`）
```c
typedef struct {
    bool     running;
    uint8_t  pin_level;
    uint32_t step_us;        // 整数商：(32 * 1e6) / clock_hz (24MHz 时为 1)
    uint32_t rem_step;       // 整数余数：(32 * 1e6) % clock_hz (24MHz 时为 8,000,000)
    uint32_t rem_accum;      // 相位累加器：累积并溢出进位微秒
    uint32_t clock_hz_last;  // 记录最后一次配置的 Fsys，用于动态变频跟随
    uint64_t next_toggle_us; // 下次引脚跳变虚拟时间戳
    uint32_t toggle_count;   // 累计翻转计数器（供 Host 单测精确核验）
} Cms8sCloState;
```

#### 3.2.2 调度与状态机逻辑（buzzer 同构 + Bresenham 累加）
```text
update_clo_state(ctx):
  p13cfg = xdata_shadow[0xF013]
  should_run = (p13cfg == 0x05)
  若 should_run:
      clk_hz = ctx->clock_hz ? ctx->clock_hz : 24000000u
      若 clk_hz != clock_hz_last: // 首次启动或运行时 CLKDIV 改变
          clock_hz_last = clk_hz
          step_us = 32000000ull / clk_hz
          rem_step = 32000000ull % clk_hz
          rem_accum = 0
          若 step_us == 0 && rem_step == 0: step_us = 1
      若 !running:
          running = true, pin_level = 1, gpio_write(11, 1, MCS51_DRIVE_SUPPLY)
          delta = step_us
          rem_accum += rem_step
          若 rem_accum >= clock_hz_last: rem_accum -= clock_hz_last, delta += 1
          next_toggle_us = now + max(1, delta)
  否则若 running:
      running = false, next_toggle_us = UINT64_MAX, pin_level = 0, gpio_write(11, 0, MCS51_DRIVE_SUPPLY)

poll 每微步:
  update_clo_state(ctx)
  若 !running || next_toggle_us == UINT64_MAX: return
  while (now >= next_toggle_us):
      pin_level ^= 1
      gpio_write(11, pin_level != 0, MCS51_DRIVE_SUPPLY)
      toggle_count++
      delta = step_us
      rem_accum += rem_step
      若 rem_accum >= clock_hz_last: rem_accum -= clock_hz_last, delta += 1
      next_toggle_us += max(1, delta)
      if (++toggles >= MAX_TOGGLES_PER_POLL):
          若 (now >= next_toggle_us): next_toggle_us = now + max(1, delta)
          break

next_event_us: (running && next_toggle_us != UINT64_MAX) ? next_toggle_us : UINT64_MAX
```

- 复位：`memset(&priv->clo, 0, sizeof(Cms8sCloState)); priv->clo.next_toggle_us = UINT64_MAX;`；`cms8s_soc_bind` 幂等绑定。
- `SPDX-License-Identifier: LGPL-3.0-only`；文件头注释量化诚实注记（D3）。
- Host 可观测接口：`cms8s_clo_is_running()`, `cms8s_clo_toggle_count()`, `cms8s_clo_half_period_us()`（buzzer 同款主动 context 读数）。

### 3.3 P1.3 引脚号

P1.3 = `(1<<3)|3 = 11`；P3.2 = 26（沿用全系列场景约定）。

---

## 4. 实施任务分解与执行路径

```text
Task 1: REG 补 GPIO_P13_MUX_CLO + shim_audit 0 漂移
        ▼
Task 2: cms8s_clo 建模 + priv + 注册 + MAX_PERIPHERALS 16 + sources
        ▼
Task 3: Host CTest (test_mcs51_cms8s_clo, 7 用例矩阵) + 全绿门禁
        ▼
Task 4: 微应用 + 三件套 + 长跑宽窗 Headless 实证 (375kHz, v1.3)
        ▼
Task 5: lint/许可/回写 + Checklist 40 摘牌
```

### Task 1: 方言垫片补齐（无 Allowlist 变更）
- **目标**：`REG_CMS8S78XX.H`（P13 mux 区，`GPIO_P13_MUX_RXD` 行旁）
- 新增：`#define GPIO_P13_MUX_CLO (0x05)  // P1.3 as CLO (system clock /64 output)`（verbatim 原厂 `gpio.h`）
- 验证：`python wink-micro-os/frameworks/mcs51/tools/mcs51_shim_audit.py`（0 hard mismatch；Allowlist 无新 XSFR，不重生成，仅 `--check-xsfr-allowlist` 过新鲜度）

### Task 2: 仿真内核 CLO 建模与注册
- **新建**：`chips/cms8s78xx/include/cms8s_clo.h`（`init/reset/poll/next_event_us` + 3 项可观测函数 + 头注释 D3）
- **新建**：`chips/cms8s78xx/src/cms8s_clo.cpp`（§3.2，仿 `cms8s_buzzer.cpp`，Bresenham 相位累加）
- **修改**：`cms8s_priv.h`（增加 `Cms8sCloState` + `Cms8sPriv::clo` 成员）、`cms8s_register.cpp`（注册 `"cms8s_clo"`，挂载在 `MCS51_PHASE_CLOCK`）、`mcs51_sources.cmake`（添加 `cms8s_clo.cpp`）、`mcs51_peripheral.h`（`12u→16u` + 容量注记修订）

### Task 3: Host 原生 CTest 单元测试
- **新建**：`frameworks/mcs51/test/cms8s78xx/test_cms8s_clo.cpp`（SPDX `GPL-3.0-only`，仿 buzzer harness：`wink_mcs51_host_gpio_notify_*` + `ctx->virtual_us` 步进）
- **注册**：`wink-micro-os/test/CMakeLists.txt`（注册 `test_mcs51_cms8s_clo`，源码路径 `../frameworks/mcs51/test/cms8s78xx/test_cms8s_clo.cpp`）
- **用例矩阵（7 项完备覆盖）**：
  - `Test 1` 初始 idle 检验（`!running`，`next==UINT64_MAX`）；
  - `Test 2` 使能配置：配 DIV_1 + mux CLO → `running==true`，pin11 首驱高（`wink_mcs51_host_gpio_notify` 取首沿）；
  - `Test 3` Bresenham 步进推进：步进 3 次翻转，验证步进间隔为 `1µs, 1µs, 2µs`（累计 4µs 恰好 1.5 周期），`toggle_count==3` 且 `next_event` 推进；
  - `Test 4` mux 切回 GPIO：切为非 0x05 模式 → 停止运行，引脚驱动恢复电平 0，`next==UINT64_MAX`；
  - `Test 5` 动态变频跟随：直写 `ctx->clock_hz = 6000000`（6MHz）→ 重新计算 $step=5, rem=2000000$ → 追踪真实 93.75kHz 物理频率；
  - `Test 6` 跨 Family 隔离门禁（ADR-0004）：切换上下文至 `MCS51_FAMILY_CLASSIC`，调用 `poll` 与 `next_event_us` 必须安全 no-op，返回 `UINT64_MAX`，不越界篡改；
  - `Test 7` 飞态复位（Reset-in-flight）：运行过程中直接调用 `cms8s_clo_reset(ctx)`，状态干净清零，引脚恢复安全电平。
- **Host 断言纪律**：`wink_mcs51_host_gpio_notify` 日志仅 128 项且饱和截断——首沿电平可用 notify 取，批量翻转一律用 `toggle_count`，禁止步进数百次后读 notify
- **连带门禁**：`test_mcs51_context_budget` 仍绿（CLO 状态进 `Cms8sPriv` 芯片池，不增加 `Mcu51Context` 大小）

### Task 4: 原厂微应用接入与 Headless 实证
- **路径**：`wink-micro-app/vendor/cms8s78xx/systemclock/`；原厂仅 `main.c` + `isr.c`（全空 ISR），**一行不改**镜像
- **配套**：`CMakeLists.txt`（仿 acmp0/reset_software，`_SRC_FILES main.c isr.c`）、`wink-app.json`（`app_name: systemclock`，`templateId` 对齐 `vendor_cms8s78xx_systemclock`，`upstream.source_dir` 指 `.../Example/SystemClock/code`，devices 绑 P32 LED）
- **场景设计**（`unisim-scenarios/clo.scenario.json` + `p32.scenario.json`，双文件隔离——headless `getPinEdges` 反馈不分引脚，混窗互染计数）：
  - **CLO 频率高保真断言**（`clo.scenario.json`）：`ASSERT_WAVEFORM pin: 11`，观测窗口 `windowUs: ["100000", "3900000"]` @ `timeUs: "3950000"`（4s 长跑；引擎 ~10ms 批量时间戳量化经多秒平均稀释至 <0.2%），断言值 `$near: {target: 375000, tolerance: 2000}`（100% 吻合硅片真实 375kHz；±500Hz 重跑稳定）；
  - **P32 闪烁指示断言**（`p32.scenario.json`）：对 `pin: 26`（P3.2）执行 `ASSERT_WAVEFORM`，断言活性 `$between: [1000, 300000]`（实测 sim 节奏 50kHz；不断言绝对周期，只断言翻转活性，符合“先实测后落值”）；
  - **虚拟时间预算（v1.3 修订）**：场景 `timeoutUs` 为 `"4000000"`（4s，Wall-clock ~3.6s），替代 v1.2 的 20ms 紧凑预算（实证不可行，见 v1.3 偏离记录）。
- **命令**：`winkcli build sim --app vendor_cms8s78xx_systemclock`；`winkcli sim run --app vendor_cms8s78xx_systemclock --mode headless --scenarios ...`

### Task 5: 治理回写与 Checklist 摘牌
1. `winkcli lint --pack layering --pack api --pack wasm` + `python .github/scripts/check_license_map.py` → 0 问题
2. Checklist #40 `[ ]`→`[x]`（资产 + Headless 证据 + 单测 + wasm 大小 + lint）
3. Layer-① `07-mcs51-simulation-interception.md` 表①增 CLO 行（含 Bresenham 零频偏口径）；`MCS51_MAX_PERIPHERALS` 注释即回写（头文件内）

---

## 5. 验收准则与黄金门禁

1. **真实编译产物完整**：`unisim-assets/` 下 `device-tree.json`、`wink_simulator.js`、`wink_simulator.wasm` 三件套；
2. **Headless 断言 100% 绿灯**：退出码 0，虚拟时间 4s（v1.3 修订：20ms 短窗实证不可行，见 §1.1），Wall-clock 耗时 ~3.6s，CLO 波形断言精确命中 375kHz（`$near 375000±2000`，±500Hz 重跑稳定）；
3. **Host CTest 全绿**：新增 `test_mcs51_cms8s_clo` 全 PASS（7/7 用例），`test_mcs51_context_budget` 仍绿，既有可构建 suite 无回退（4 项预存 MSVC 失败除外，见 LVD 计划 v1.2）；
4. **代码纯净**：零 `malloc`、CLO 状态零进 `Mcu51Context`（chip 池）、`mcs51_shim_audit` 0 漂移、许可地图（runtime LGPL / test GPL）；
5. **原厂零改动**：`main.c`/`isr.c` 与 `docs/vendors/.../SystemClock/code/` diff 为空。

---

## 6. 风险与缓解

| 风险 | 等级 | 缓解 |
|------|:---:|------|
| 引擎批量时间戳量化导致毫秒窗恒读 0（v1.3 已证伪短窗假设） | 高 | 4s 长跑 + `[100ms, 3900ms]` 宽窗（多秒平均稀释量化噪声至 <0.2%）；CLO/P32 分文件隔离（`getPinEdges` 不分引脚）；headless 边沿采集为无界数组，10k PinTracer 环形之忧不适用本链路 |
| 超高频 CLO 翻转拖慢 Headless wall-clock | 中 | `MAX_TOGGLES_PER_POLL` 封顶（buzzer 同款）；`next_event_us` 精确调度避免忙轮询；4s 长跑 Wall-clock 实测 ~3.6s（EPWM 刹车类已有 8~18s 先例，可接受） |
| 离散微秒步进导致 375kHz 频偏 | 低 | 引入 Bresenham 相位累加器，微观 `[1, 1, 2]µs` 步进，宏观平均半周期 1.333µs，达成 100% 零频偏 |
| P32 软件延时周期不可预估 | 低 | Task 4 显式先实测后落值；不断言绝对周期，只断言翻转活性或差分点 |
| `MAX_PERIPHERALS` 扩容弱化注册表熔断语义 | 低 | 仅 12→16，注释修订保留 abort 熔断；LCD 仍有 3 槽 headroom |
| CLKDIV 运行时改分频，CLO 未跟随 | 低 | poll 逐次检测 `clock_hz` 变化并动态重算步进；Test 5 专测变频 |

---

## 7. 交付物清单

1. 计划文档：本文件（v1.3，执行闭环 Done）
2. 框架：`REG_CMS8S78XX.H`（1 宏）、**新建** `cms8s_clo.h/cpp`、`cms8s_priv.h`、`cms8s_register.cpp`、`mcs51_sources.cmake`、`mcs51_peripheral.h`
3. 测试：**新建** `frameworks/mcs51/test/cms8s78xx/test_cms8s_clo.cpp` + CMake 注册
4. App：`wink-micro-app/vendor/cms8s78xx/systemclock/`（2 源镜像 + CMake + json + `clo`/`p32` 双 scenario + assets 三件套）
5. 文档：Checklist #40、Layer-① 表① CLO 行

---

## 8. 回滚方案

1. **快速回退**：`cms8s_clo` 的 `poll` 首行恒返（或注册行注释掉）即回到"CLO 无输出"旧行为，应用层零改动；`GPIO_P13_MUX_CLO` 宏保留无害。
2. **版本回退**：`git revert` 本次各原子 commit（shim / 模型 / 测试 / 应用分立提交）。
3. **回滚验证**：回退后 `test_mcs51_cms8s_acmp` + `test_mcs51_cms8s_lvd` + `test_mcs51_context_budget` 仍绿。
