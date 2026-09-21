# CMS8S78xx 增强型 PWM (EPWM) 与硬件刹车保护系统高保真仿真适配实施计划

> 📋 **本文档是 CMS8S78xx EPWM 体系高保真仿真适配的正式实施计划（Layer-③）**。
> 遵循 [00-IMPLEMENTATION-PLAN-TEMPLATE.md](../../00-IMPLEMENTATION-PLAN-TEMPLATE.md) 模板规范，针对 [CMS8S78XX_EXAMPLE_CHECKLIST.md](../../vendors/Cmsemicon/CMS8S78XX_EXAMPLE_CHECKLIST.md) 第 6 章节官方子示例（编号 24 ~ 31，共 8 个用例）进行分批攻坚。

---

## 1. 元数据表（🔴 必选）

| 字段 | 内容 |
|------|------|
| **计划编号** | `PLAN-20260916-CMS8S78XX-EPWM-FIDELITY` |
| **创建日期** | `2026-09-16` |
| **目标平台/SoC** | `host` (GCC/MSVC C++17), `wasm` (Emscripten ASYNCIFY) 基于 `frameworks/mcs51` |
| **工具链/SDK版本**| GCC 11+, MSVC 19+, Emscripten 3.1+, 原厂 `CMS8S78xx_DemoCode_V2.0.2` |
| **计划状态** | 🟡 阶段一代码/单测/微应用就绪，待阶段二/三真实构建与 Headless 实证 |
| **优先级** | 🔴 P1（直接攻坚 Checklist §6 编号 24 ~ 31 全部 8 个核心子示例） |
| **计划版本** | `v1.1` |
| **关联技术设计** | 原厂 `CMS8S78xx` 参考手册 Ch.18 (EPWM)；保真度基线 `2026-09-08-mcs51-simulation-vs-silicon-fidelity-and-test-limits.md` |
| **关联设计规范** | [`docs/zh/design/02-wink-micro-os/07-mcs51-simulation-interception.md`](../../zh/design/02-wink-micro-os/07-mcs51-simulation-interception.md) |
| **关联合格清单** | [`docs/vendors/Cmsemicon/CMS8S78XX_EXAMPLE_CHECKLIST.md`](../../vendors/Cmsemicon/CMS8S78XX_EXAMPLE_CHECKLIST.md) §6（编号 24~31） |
| **关联 ADR** | [ADR-0004](../../design/decisions/0004-static-dispatch-vs-runtime-ops.md)（静态分发与无虚表）、[ADR-0070](../../decisions/core/0070-mcs51-zero-code-simulation-interception-layer.md)（C++ 零侵入拦截）、[ADR-0071](../../decisions/core/0071-sfr-proxy-rmw-edge-data-plane.md)（XSFR 代理数据面）、[ADR-0072](../../decisions/core/0072-dual-clock-domain-and-quota-catchup.md)（双时钟域与微步调度）、[ADR-0078](../../decisions/core/0078-mcs51-two-phase-irq-and-in-service-masking.md)（中断两阶段挂起）、[ADR-0043](../../design/decisions/0043-arch-lint-rules.md)（分层门禁） |
| **前置依赖计划** | `PLAN-20260911-STAGE3-MODEL-FIDELITY`（已闭环 Timer0~Timer4 全矩阵与 ACMP 基础模型） |
| **目标里程碑** | 1. 建立 `cms8s_epwm` 物理行为级外设模型与时间步进推进调度；<br>2. 补齐 `REG_CMS8S78XX.H` 中的 48 个 EPWM XSFR 寄存器（`0xF120~0xF16F`）与常用 StdDriver inline shims；<br>3. 交付 8 个未修改的原厂微应用镜像与真实编译三件套（Wasm/HTML）；<br>4. 编写全量 CTest 单元测试与 Headless 场景用例（`ASSERT_WAVEFORM` 与 `ASSERT_POINT`），达成 100% 绿灯。 |
| **所需技能** | `embedded-best-practice` |

---

## 2. 背景与技术目标

### 2.1 问题陈述
中微 CMS8S78xx 芯片内置高性能的 6 通道 16 位增强型 PWM 发生器（EPWM），具备互补对称输出、硬件死区插入、边沿/中心对称计数模式、多种刹车保护（Stop/Suspend/Recover/DelayRecover）以及与外部故障引脚 FB 和片内模拟比较器 ACMP 硬件联动的能力。
当前 `frameworks/mcs51` 核心中：
1. 尚无 `cms8s_epwm.cpp` 硬件模型，XSFR 空间 `0xF120 ~ 0xF16F` 仅被当做无模型接管的 Tripwire 寄存器；
2. 中断向量 18（`EPWM_VECTOR`）尚未打通与微步推进调度；
3. Checklist 中第 6 模块除蜂鸣器外，剩余 8 个官方示例（编号 24 ~ 31）全部处于待适配状态，阻碍了该芯片高级电机控制特性的仿真闭环。

### 2.2 技术目标
1. **零改动原厂源码编译**：8 个示例的 `main.c`, `demo_epwm.c`, `demo_epwm.h`, `demo_acmp.c`, `isr.c` 原样编译运行，通过头文件垫片遮蔽 Keil 专有方言。
2. **状态池与内存无动态分配**：遵循 Scheme A 范式，在 `Cms8sPriv` 中定义 `Cms8sEpwmState` POD 结构，并在 `s_cms8s_priv_pool` 中分配，严禁 `malloc`/`new` 或文件级全局可变状态。
3. **高保真时间片计数与中断派发**：
   - 递减模式（Down-Count）：从 `PERIOD` 减至 0 触发周期中断（`PWMUIF`）并翻转 P32；
   - 增减模式（Up-Down Count）：从 0 计至 `PERIOD` 再减至 0，过零点触发中断（`PWMZIF`）并翻转 P32；
   - 载波频率严格匹配硬件时钟公式，通过 `ASSERT_WAVEFORM` 验证方波频率。
4. **片内外设事件深度联动**：
   - 支持外部故障引脚 FB 边沿刹车；
   - 支持片内模拟比较器 ACMP0/ACMP1 正向穿越电平直接触发 EPWM 硬件封锁。

---

## 3. 官方示例矩阵与分批实施路径

8 个子示例按外设复杂度和依赖拓扑拆解为三大批次：

```text
┌────────────────────────────────────────────────────────┐
│ 第一批次：计数模式与周期/过零中断 (P0/P1，编号 24 & 25)    │
│ • #24: EPWM/CoutMode/DownCountMode                     │
│ • #25: EPWM/CoutMode/UpDownCountMode                   │
│ 关键产出: cms8s_epwm 基础模型、XSFR 寄存器、Vector 18 中断调度 │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│ 第二批次：基础硬件刹车与恢复时序 (P2，编号 26 ~ 29)        │
│ • #26: EPWM/BrakeMode/Brake_Recover_Mode (恢复模式)     │
│ • #27: EPWM/BrakeMode/Brake_Delay_Recover_Mode (延时) │
│ • #28: EPWM/BrakeMode/Brake_Stop_Mode (急停模式)       │
│ • #29: EPWM/BrakeMode/Brake_Supend_Mode (悬挂模式)     │
│ 关键产出: 刹车状态机、输出强制高阻/无效电平、延时计数器    │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│ 第三批次：片内外设深度联动刹车 (P3，编号 30 & 31)         │
│ • #30: EPWM/BrakeMode/FBBrakeMode (外部故障引脚 FB)     │
│ • #31: EPWM/BrakeMode/ACMPBrakeMode (模拟比较器联动)   │
│ 关键产出: ACMP -> EPWM 片内硬件直通线、联合场景断言    │
└────────────────────────────────────────────────────────┘
```

### 示例详细对应表

| 批次 | 编号 | 官方示例路径 | 模式特征 | 验收指标与断言 |
|:---:|:---:|:---|:---|:---|
| **Batch 1** | 24 | `EPWM/CoutMode/DownCountMode/code` | 递减计数，周期溢出中断 | 触发 Vector 18，ISR 翻转 P32，`ASSERT_WAVEFORM` 校验方波频率 |
| **Batch 1** | 25 | `EPWM/CoutMode/UpDownCountMode/code` | 增减中心对称，过零点中断 | 触发 Vector 18，ISR 翻转 P32，`ASSERT_WAVEFORM` 校验中心对称频率 |
| **Batch 2** | 26 | `EPWM/BrakeMode/Brake_Recover_Mode/code` | 刹车信号撤销后自动恢复 PWM | 刹车触发时输出封锁，撤除后下一周期自动恢复驱动 |
| **Batch 2** | 27 | `EPWM/BrakeMode/Brake_Delay_Recover_Mode/code` | 刹车撤销后延时 N 周期恢复 | 延时计数溢出前维持封锁，计数满后安全重载输出 |
| **Batch 2** | 28 | `EPWM/BrakeMode/Brake_Stop_Mode/code` | 刹车急停，需软件显式清除标志 | 刹车一旦触发永久封锁，直到软件清零 `PWMBRKC` |
| **Batch 2** | 29 | `EPWM/BrakeMode/Brake_Supend_Mode/code` | 刹车悬挂模式 | 刹车有效期间时钟/计数器冻结，撤除后原位继续 |
| **Batch 3** | 30 | `EPWM/BrakeMode/FBBrakeMode/code` | 外部引脚 `PS_FB0/1` 故障触发 | `INPUT_PIN` 注入外部故障脉冲，验证刹车封锁 |
| **Batch 3** | 31 | `EPWM/BrakeMode/ACMPBrakeMode/code` | 片内比较器 ACMP 触发刹车 | 注入模拟电压使 ACMP 翻转，联动触发 EPWM 硬件刹车 |

---

## 4. 详细技术实现方案

### 4.1 寄存器数据面布局（`REG_CMS8S78XX.H`）
在 `0xF120 ~ 0xF16F` 地址段声明 `WinkXsfr` 代理，并在 `cms8s_xsfr_allowlist.h` 中放行：

```cpp
// 基础控制与计数器模式
xsfr PWMCON(0xF120);     // PWM 控制 (使能、互补、对齐模式)
xsfr PWMOE(0xF121);      // PWM 通道输出使能 (PWM0~PWM5)
xsfr PWMPINV(0xF122);    // PWM 输出极性翻转
xsfr PWM01PSC(0xF123);   // PWM0/1 时钟预分频
xsfr PWM23PSC(0xF124);   // PWM2/3 时钟预分频
xsfr PWMCNTE(0xF126);    // 计数器使能
xsfr PWMCNTM(0xF127);    // 计数器模式 (0: 递增, 1: 递减, 2: 增减)
xsfr PWMCNTCLR(0xF128);  // 计数器清零
xsfr PWMLOADEN(0xF129);  // 影子寄存器重载使能

// 周期与占空比 (16位，高低字节)
xsfr PWMP0L(0xF130); xsfr PWMP0H(0xF131); // 周期寄存器 0
xsfr PWMD0L(0xF140); xsfr PWMD0H(0xF141); // 占空比寄存器 0
// ... 扩展至 PWMP1~3, PWMD1~3, PWMDD0~3

// 刹车控制与死区
xsfr PWMBRKC(0xF15C);    // 刹车控制 (刹车源、恢复模式、使能)
xsfr PWMBRKRDTL(0xF15D); // 刹车恢复延时低字节
xsfr PWMBRKRDTH(0xF15E); // 刹车恢复延时高字节
xsfr PWMDTE(0xF160);     // 死区使能
xsfr PWM01DT(0xF161);    // PWM0/1 死区时间
xsfr PWM23DT(0xF162);    // PWM2/3 死区时间

// 中断使能与中断标志 (W0C 清除)
xsfr PWMPIE(0xF168);     // 周期匹配中断使能
xsfr PWMZIE(0xF169);     // 过零中断使能
xsfr PWMUIE(0xF16A);     // 递增匹配中断使能
xsfr PWMDIE(0xF16B);     // 递减匹配中断使能
xsfr PWMPIF(0xF16C);     // 周期匹配中断标志
xsfr PWMZIF(0xF16D);     // 过零中断标志
xsfr PWMUIF(0xF16E);     // 递增匹配中断标志
xsfr PWMDIF(0xF16F);     // 递减匹配中断标志
```

### 4.2 硬件模型内部状态机（`cms8s_priv.h`）

```cpp
struct Cms8sEpwmChannel {
    uint16_t counter;       // 当前计数值
    uint16_t period;        // 周期重载值
    uint16_t duty;          // 占空比匹配值
    uint8_t  direction;     // 0: 递增, 1: 递减
    bool     output_level;  // 当前输出引脚电平
};

struct Cms8sEpwmState {
    Cms8sEpwmChannel ch[4]; // 4 组独立/互补时基单元
    uint64_t last_poll_us;  // 上次时间推进微秒
    bool     brake_active;  // 硬件刹车锁存标志
    uint16_t brake_delay_cnt;// 刹车延时计数器
    uint8_t  active_flags;  // 活跃的中断标志影子
};
```

### 4.3 计数与中断触发时序算法

1. **时钟周期换算**：
   - 基础时钟频率 $F_{sys} = 24\text{MHz}$；
   - 预分频系数 $PSC \in \{1, 2, 4, 8, 16, 32, 64, 128\}$；
   - 每 tick 纳秒数 $T_{tick} = \frac{1000 \times PSC}{24}\text{ns}$。
2. **事件时间推进（`next_event_us` 算法）**：
   - 计算距离下一次比较匹配、过零或溢出的最小 tick 差量 $\Delta tick$；
   - 转换为微秒并调度 Fiber 切入，避免每微秒轮询造成无谓的 CPU 消耗。
3. **中断两阶段派发（遵循 ADR-0078）**：
   - 匹配产生时置位影子标志（如 `PWMZIF` 或 `PWMPIF`）；
   - 检查使能位（`PWMZIE` 等）及全系统中断总使能 `EA`；
   - 调用 `mcs51_raise_irq(IRQ_SOURCE_EPWM)` 派发中断向量 18。

---

## 5. 阶段任务拆解与甘特图

### Phase 0: 基础底座与数据面放行（耗时估算：半天）
- [x] **Task 0.1**：在 `chips/cms8s78xx/include/cms8s_priv.h` 中扩展 `Cms8sEpwmState`；
- [x] **Task 0.2**：在 `chips/cms8s78xx/include/REG_CMS8S78XX.H` 声明全部 48 个 EPWM XSFR 寄存器及常用位掩码；
- [x] **Task 0.3**：在 `chips/cms8s78xx/include/cms8s_xsfr_allowlist.h` 登记 `0xF120 ~ 0xF16F` 合法地址段与 `0xF0CD/CE`；
- [x] **Task 0.4**：在 `REG_CMS8S78XX.H` 中补齐原厂 `StdDriver/inc/epwm.h` 的基础宏与 inline API 桩。

### Phase 1: 第一批次 —— 计数与中断模型落地（编号 24 & 25）（耗时估算：1天）
- [x] **Task 1.1**：创建 `chips/cms8s78xx/include/cms8s_epwm.h` 与 `src/cms8s_epwm.cpp`，实现递减与增减中心对称计数推进；
- [x] **Task 1.2**：实现过零点（Zero）与周期（Period）中断标志置位及 Vector 18 派发逻辑；
- [x] **Task 1.3**：在 `chips/cms8s78xx/src/cms8s_register.cpp` 绑定 EPWM 的 init / reset / poll / next_event_us 钩子；
- [x] **Task 1.4**：编写 CTest 单元测试 `test_mcs51_cms8s_epwm.cpp`（覆盖 DownCount 与 UpDownCount 模式）；
- [ ] **Task 1.5**：构建微应用 `vendor_cms8s78xx_epwm_down_count` 与 `vendor_cms8s78xx_epwm_updown_count`（代码与场景已镜像，待执行 `winkcli build/test` 实证）。

### Phase 2: 第二批次 —— 基础硬件刹车与保护恢复（编号 26 ~ 29）（耗时估算：1天）
- [x] **Task 2.1**：在 `cms8s_epwm.cpp` 中实现 `PWMBRKC` 刹车控制逻辑及 4 种刹车模式状态机；
- [x] **Task 2.2**：实现延时恢复计数器（`PWMBRKRDTL/H`）；
- [ ] **Task 2.3**：微应用构建与场景实证（代码与场景已镜像，待执行 `winkcli build/test` 实证）：
  - `vendor_cms8s78xx_epwm_brake_recover`
  - `vendor_cms8s78xx_epwm_brake_delay_recover`
  - `vendor_cms8s78xx_epwm_brake_stop`
  - `vendor_cms8s78xx_epwm_brake_suspend`
- [ ] **Task 2.4**：断言刹车信号触发时 PWM 引脚瞬态封锁电平及恢复时序（待 Headless 运行）。

### Phase 3: 第三批次 —— 高级片内外设联动刹车（编号 30 & 31）（耗时估算：1天）
- [x] **Task 3.1**：打通外部引脚 `PS_FB0/1 (0xF0CD/CE)` 故障输入边沿检测与刹车触发；
- [x] **Task 3.2**：打通片内 `cms8s_acmp` 与 `cms8s_epwm` 内部事件直通桥：
  - 当 ACMP 比较器正向翻转且配置了 `EPWM_BRK_ACMP0/1` 时，内部硬件级打入 EPWM 刹车态；
- [ ] **Task 3.3**：微应用构建与场景实证（代码与场景已镜像，待执行 `winkcli build/test` 实证）：
  - `vendor_cms8s78xx_epwm_brake_fb`
  - `vendor_cms8s78xx_epwm_brake_acmp`
- [ ] **Task 3.4**：双外设联动全链路断言（外部模拟电压调变 ➔ ACMP 翻转 ➔ EPWM 刹车 ➔ P32 状态响应，待 Headless 运行）。

### Phase 4: 门禁复核、全量回归与文档闭环（耗时估算：半天）
- [ ] **Task 4.1**：运行全量 Host CTest，确保已有用例（Timer0~4, ACMP, ADC, UART, GPIO）零回归（待终端运行 `ctest`）；
- [ ] **Task 4.2**：运行 `winkcli lint --pack layering --pack api` 确保 0 findings（待终端运行）；
- [x] **Task 4.3**：回写更新 [CMS8S78XX_EXAMPLE_CHECKLIST.md](../../vendors/Cmsemicon/CMS8S78XX_EXAMPLE_CHECKLIST.md) §6 状态；
- [x] **Task 4.4**：更新 Layer-① 活规范 [`07-mcs51-simulation-interception.md`](../../zh/design/02-wink-micro-os/07-mcs51-simulation-interception.md)。

---

## 6. 风险评估与防御策略

| 风险点 | 等级 | 潜在影响 | 防御策略 |
|:---|:---:|:---|:---|
| **高频 PWM 引脚微步事件风暴** | 🟡 中 | 频繁向 UniSim 派发 GPIO 边沿导致仿真吞吐量暴跌 | 针对仅内部翻转 P32 的中断模式，按中断周期而非 PWM 载波高频步进；对引脚波形输出采用阶段式跳跃计算。 |
| **ACMP 与 EPWM 跨外设死锁** | 🔴 高 | ACMP 轮询中直接调用 EPWM 造成状态串扰或重入 | 采用纯数据面单向解耦：EPWM 在自身的 `poll` 阶段读取 ACMP 的输出标志，绝不直接跨函数互相调用修改私有状态。 |
| **刹车 W0C 标志误清除** | 🟡 中 | 软件清除刹车标志时错误写 1 导致误置位 | 严格遵循 W0C（Write-0-to-Clear）位操作契约：`shadow &= write_val`。 |
| **内存预算超标** | 🟢 低 | BSS 空间溢出 | `Cms8sEpwmState` 严格控制在 128 字节以内，总 Context 保持在预算红线内。 |

---

## 7. 验收标准与准出清单

1. **构建要求**：8 个微应用均通过 `winkcli build --target wasm`，生成合规的 `.wasm`、`device-tree.json` 与 `wink_simulator.js` 三件套。
2. **测试要求**：
   - `test_mcs51_cms8s_epwm` 单元测试通过；
   - 8 个微应用对应的 Headless 场景断言 100% 绿灯（`ASSERT_WAVEFORM` 与 `ASSERT_POINT`）；
   - 现存 Host CTest 与 Wasm 测试 100% 通过（零回归）。
3. **规范要求**：
   - 架构分层门禁（Layering & API Lint）0 违规；
   - Checklist §6 全部 8 个子示例打勾并附带实证数据。
