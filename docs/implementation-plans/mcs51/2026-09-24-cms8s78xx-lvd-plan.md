# CMS8S78xx 低压检测 (LVD) 系统高保真仿真适配实施计划

> 📋 **本文档是 CMS8S78xx 低压检测系统（LVD）高保真仿真适配的正式实施计划（Layer-③）**。
> 遵循 [00-IMPLEMENTATION-PLAN-TEMPLATE.md](../../00-IMPLEMENTATION-PLAN-TEMPLATE.md) 模板规范，针对 [CMS8S78XX_EXAMPLE_CHECKLIST.md](../../vendors/Cmsemicon/CMS8S78XX_EXAMPLE_CHECKLIST.md) 第 7 章节官方子示例（编号 36: `LVD/code`，⚡ Level 3，P2）进行高保真攻坚与闭环。

---

## 1. 元数据表（🔴 必选）

| 字段 | 内容 |
|---|---|
| **计划编号** | `PLAN-20260924-CMS8S78XX-LVD-FIDELITY` |
| **创建日期** | `2026-09-24` |
| **目标平台/SoC** | `host` (GCC/MSVC C++17), `wasm` (Emscripten ASYNCIFY) 基于 `frameworks/mcs51` |
| **工具链/SDK版本**| GCC 11+, MSVC 19+, Emscripten 3.1+, 原厂 `CMS8S78xx_DemoCode_V2.0.2` |
| **计划状态** | `Done / 已完成（2026-09-24，一次执行闭环）` |
| **优先级** | P2（Checklist §7 编号 36，与清单口径一致） |
| **计划版本** | `v1.1`（相对 v1.0：纠正 INPUT_POWER 装载阻塞 → 虚拟 VDD sense；EIP3/IRQ_SET_PRIORITY 已存在勘误；XSFR W0C 改为 ACMP 式 poll 消解；独立 `cms8s_lvd` 外设注册；默认 VDD/迟滞诚实标注；验证命令统一 `winkcli`） |
| **关联技术设计** | 原厂 `CMS8S78xx` 参考手册 Ch.4 (System Control & LVD)；保真度基线 `2026-09-08-mcs51-simulation-vs-silicon-fidelity-and-test-limits.md` |
| **关联设计规范** | [`docs/zh/design/02-wink-micro-os/07-mcs51-simulation-interception.md`](../../zh/design/02-wink-micro-os/07-mcs51-simulation-interception.md) |
| **关联合格清单** | [`docs/vendors/Cmsemicon/CMS8S78XX_EXAMPLE_CHECKLIST.md`](../../vendors/Cmsemicon/CMS8S78XX_EXAMPLE_CHECKLIST.md) §7（编号 36） |
| **关联总纲规范** | UniSim 嵌入式韧性与合成故障注入体系计划总纲（2026-09-24，`unisim` 私有仓，本地 internals 通道） §4.1 PVD/LVD 早期欠压预警投毒子系统（跨仓参考，本计划不依赖） |
| **关联 ADR** | [ADR-0004](../../design/decisions/0004-static-dispatch-vs-runtime-ops.md)（静态分发与无虚表）、[ADR-0012](../../decisions/core/0012-fail-loud-contract-discipline.md)（契约诚实与强报错）、[ADR-0043](../../design/decisions/0043-arch-lint-rules.md)（分层门禁）、[ADR-0070](../../decisions/core/0070-mcs51-zero-code-simulation-interception-layer.md)（C++ 零侵入拦截）、[ADR-0071](../../decisions/core/0071-sfr-proxy-rmw-edge-data-plane.md)（XSFR 代理数据面）、[ADR-0072](../../decisions/core/0072-dual-clock-domain-and-quota-catchup.md)（双时钟域与微步调度）、[ADR-0078](../../decisions/core/0078-mcs51-two-phase-irq-and-in-service-masking.md)（中断两阶段挂起与嵌套深度管理） |
| **前置依赖计划** | `PLAN-20260915-MCS51-RESET-FIDELITY`（复位与看门狗模型已就绪） |
| **跨仓依赖** | **无**（v1.1 选定虚拟 VDD sense + `INPUT_ANALOG`，不引入 `INPUT_POWER` 执行器与新 wasm ABI） |
| **目标里程碑** | 1. 建立 `cms8s_lvd` 物理边沿锁存与迟滞比较模型，根除低压持续期间的中断风暴假死；<br>2. 补齐 `REG_CMS8S78XX.H` 中的 `LVDCON (0xF690)` SFR 声明及 StdDriver inline shims（`EIP3`/`IRQ_SET_PRIORITY` 已存在，仅需核对）；<br>3. 重新生成并放行 `cms8s_xsfr_allowlist.h`（清除 GAP-23 绊线）；<br>4. 在 `wink_mcs51_isr.h` 登记 `IRQ_SOURCE_LVD`（Vector 26）并在 `cms8s_sys.cpp` 建立扩展映射；<br>5. 交付原厂源码零改动的微应用 `vendor_cms8s78xx_lvd` 与三件套编译资产；<br>6. 编写 Host CTest 单元测试与 UniSim Headless 场景用例，100% 绿灯验收并摘牌 Checklist 36。 |
| **所需技能** | `embedded-best-practice` |

### 1.1 变更记录

| 版本 | 日期 | 说明 |
|:---:|:---:|:---|
| v1.0 | 2026-09-24 | 初稿：位映射 SSOT、16 档阈值表、防风暴状态机、5 任务链 |
| v1.1 | 2026-09-24 | 评审闭环：P0 刺激路径改 `INPUT_ANALOG` 虚拟 sense；勘误 EIP3 已存在；XSFR W0C 改 poll 消解；独立外设注册；默认 VDD=5V；迟滞标注为行为级近似；`winkcli` 统一命令；回写锚点补全 |
| v1.2 | 2026-09-24 | 执行闭环（Done）：① sense pin 纠偏 `94→62`——注入 rail 仅 64 项（key 0~63，`adcChannel` 1:1 直映，ACMP 场景 `adcChannel:9=pin 9` 为证），`94` 越界恒读 0；`62` 为板级通道空间空闲键，与物理 0~31 / ADC0832 板键 / AN63 均无冲突；② `test_mcs51_context_budget`：MSVC x64 实测 78432B（含 LVD +8B），参考天花板系 i686 口径，MSVC 侧增条件天花板 `78432+1024`（GCC 侧 `75672+1024+8`），`irq_map==128` 断言同步；③ 预存 MSVC 失败 4 项（`low_power` C7555 / `timer_ext_clk` C4310 / `cms8s_spi`·`sfr_spin_guard` C2220）与本计划无关，未动。证据：Host `test_mcs51_cms8s_lvd` 6/6 + mcs51 74/78（4 预存 Not Run）+ headless 8/8（Virtual 1s / Wall 201ms）+ `lint` 0 findings + 许可门禁 OK + `shim_audit` 0 hard mismatch |

---

## 2. 背景与核心架构断层剖析

### 2.1 问题陈述
中微 CMS8S78xx 芯片内置低电压检测模块（LVD），用于实时监控微控制器的主供电电压 $V_{DD}$。当系统供电跌破设定的安全阈值（支持 16 档，2.0V ~ 4.6V，如官方示例选取的 4.0V）时，硬件置位中断标志 `LVDINTF` 并向 CPU 申请中断向量 26（`LVD_VECTOR`），通知固件紧急保存现场、保存非易失参数或有序关闭大功率负载。

当前 `wink-micro-os/frameworks/mcs51` 核心中存在以下 **6 项关键架构断层**（v1.1 勘误后）：

1. **寻址空间与 GAP-23 绊线违规**：
   - 官方头文件定义 `LVDCON` 位于 **XDATA 空间 0xF690**（`*(volatile unsigned char xdata *) 0xF690`），属于 XSFR 而非普通 8051 SFR；
   - 当前框架的 `REG_CMS8S78XX.H` 尚未声明 `LVDCON`，且 `cms8s_xsfr_allowlist.h` 亦未登记 `0xF690`；
   - 一旦原厂代码访问 `LVDCON`，在 Wasm 运行期将直接触发 `GAP-23 unmodeled XSFR` 异常终止。

2. **连续电平比较引发的“中断风暴（Interrupt Storm）”**：
   - 原厂中断服务函数实现为：
     ```c
     void LVD_IRQHandler(void) interrupt LVD_VECTOR {
         if (SYS_GetLVDIntFlag()) {
             P32 = ~P32;
             SYS_ClearLVDIntFlag();
         }
     }
     ```
   - 若仿真器将 LVD 粗暴建模为“瞬时电平比较”（只要 $V_{DD} < V_{LVD}$ 就置 1），则软件在 ISR 中写 0 清除标志后，仿真器下个微步又立即强制置 1；
   - 结果：CPU 刚执行 `RETI` 就再度触发中断，**每秒触发数万次中断陷入死循环**，Wasm 协程配额耗尽假死。必须按真实硅片引入**高向低跳变单向边沿锁存与迟滞（Hysteresis）模型**。

3. **中断向量与扩展优先级链路缺失**：
   - 物理中断向量为 `LVD_VECTOR = 26`（入口地址 `0x00D3`）；
   - 原厂中断优先级模块声明为 `IRQ_LVD = 27`，按照 CMS8S78xx 计算规则 $\text{bit} = \text{Module} - 24 = 3$，其优先级位位于 **`EIP3`（SFR `0xBB`）的 bit 3**；
   - 8051 标准 `IE` / `EIE1` / `EIE2` 均无 LVD 中断使能位，其局部使能位直接位于 `LVDCON.LVDINTE`（bit 1）；
   - 当前 `wink_mcs51_isr.h` 未定义 `IRQ_SOURCE_LVD`，`cms8s_sys.cpp` 亦未登记 Vector 26 的映射关系。

4. **XSFR W0C（Write-0-to-Clear）语义未在模型侧闭环**：
   - `LVDCON.LVDINTF`（bit 0）属于只读状态置位、软件写 0 清除位（W0C）；
   - `mcs51_trap_register_sfr_write` **仅覆盖 SFR 空间（0x80–0xFF）**，对 `0xF690` XSFR/MOVX 写路径无效——固件 `SYS_ClearLVDIntFlag()` 只改 `xdata_shadow[0xF690]`；
   - 必须仿 ACMP `CNIF@0xF509`：在 `cms8s_lvd_poll` 开头做 `lvdintf &= xdata_shadow[XSFR_LVDCON]` 消解，再发布有效标志回 shadow；**禁止**虚构 `on_lvdcon_write` SFR 写钩。

5. **缺少 StdDriver Inline Shims（部分 SFR 已存在，v1.1 勘误）**：
   - `REG_CMS8S78XX.H` **已有** `sfr EIP3 = 0xBB;`（第 68 行）与 `IRQ_SET_PRIORITY` no-op 宏（第 974 行）——**不再列为缺失项**；
   - 真正缺失的是：`xsfr LVDCON(0xF690)`、LVD 位掩码宏、`SYS_LVD_*` 16 档常量、`SYS_EnableLVD` 等 7 个 inline shim。

6. **双轨刺激源输入契约（Host CTest 与 Wasm Headless）**：
   - **v1.1 关键决策 D1**：放弃 `INPUT_POWER`（统一 ScenarioKernel 中为 `UNEXECUTED`，装载期 fail-loud，且电源轨未桥接到 wasm）；
   - 改为**虚拟 VDD sense 通道 + `INPUT_ANALOG`**：headless 用场景步骤注入，Host 用 `adc_inject_flag`/专用 `wink_mcs51_set_vdd_mv` 注入——零跨仓、零 ABI 变更，与 ACMP 场景同构。

### 2.2 设计决策记录（v1.1 评审吸收）

| 编号 | 决策 | 结论 | 理由 |
|:---:|------|------|------|
| **D1** | VDD 刺激注入路径 | **虚拟 VDD sense + `INPUT_ANALOG`（否决 `INPUT_POWER`）** | `step-table.ts:48` 将 `INPUT_POWER` 标为 `UNEXECUTED('action', 'no executor in this topology')`，ScenarioSession 装载期拒绝；电源轨状态不进 wasm。方案 B 单仓闭环，仿 ACMP `get_pin_norm` |
| **D2** | 中断触发语义 | **单向下降沿锁存 + 100mV 行为级迟滞** | 手册仅写「低于阈值产生中断」，未定义滞回；电平模型必致中断风暴。100mV 为**建模选择而非 datasheet 真值**（见 §3.3 注记） |
| **D3** | irq_map 行形态 | `{26, 0xFF, 0, 0xFF, 0, 0xBB, 3, SW_CLEAR}` | flag/en 均在 XSFR，raise 门控在模型；`flag_sfr==0xFF` 时 predicate 短路，另在 LVD 分支显式三重校验 |
| **D4** | W0C 实现位置 | **poll 消解，非 SFR 写钩** | XSFR 无 per-addr 写陷阱；对齐 ACMP `CNIF` 模式 |
| **D5** | 外设代码组织 | **独立 `cms8s_lvd.h/cpp` + `kCms8sDescs` 注册** | 对齐 ACMP/EPWM 静态分发模式，避免塞进已 569 行的 `cms8s_sys.cpp` |
| **D6** | VDD 与 ADC vrail | **解耦，不联动 `mcs51_adc_set_vrail_mv`** | 硅片上 LVD 监测 VDD、ADC Vref 独立；联动会污染 `test_mcs51_adc_refchain` 等既有用例 |
| **D7** | 复位默认 VDD | **`vdd_norm = 1.0f`（5.0V）** | 场景 10ms 才首次注入；默认 0 会导致 boot 即触发边沿，`50ms` 断言 `P32==0` 失败 |

---

## 3. 技术设计规范与 SSOT

### 3.1 LVDCON 寄存器位映射与写语义 SSOT

```text
LVDCON (XSFR 0xF690, 无 TA 保护，复位默认值: 0x00):
┌───────────┬───────────┬───────────┬───────────┬───────────┬───────────┬───────────┬───────────┐
│   bit 7   │   bit 6   │   bit 5   │   bit 4   │   bit 3   │   bit 2   │   bit 1   │   bit 0   │
│                 LVDSEL[3:0]                   │   LVDEN   │     -     │  LVDINTE  │  LVDINTF  │
└───────────┴───────────┴───────────┴───────────┴───────────┴───────────┴───────────┴───────────┘
```

| 位段 | 名称 | 访问特性 | 硬件与仿真行为定义 |
|:---|:---|:---:|:---|
| **7:4** | `LVDSEL[3:0]` | R/W | **LVD 电压检测点选择**（共 16 档，见下表）。读写直通 `xdata_shadow`。 |
| **3** | `LVDEN` | R/W | **LVD 模块总使能**。<br>• `0`: 关闭 LVD 监测，比较器冻结，中断压制；<br>• `1`: 启动 LVD 硬件分压网络与比较器。 |
| **2** | Reserved | R | 保留位，硬件恒读为 0，写忽略。 |
| **1** | `LVDINTE` | R/W | **LVD 中断使能**。<br>• `0`: 禁止 LVD 产生 CPU 中断；<br>• `1`: 允许 `LVDINTF` 置位时申请 Vector 26 中断（受 `EA` 门控）。 |
| **0** | `LVDINTF` | R/W (W0C) | **LVD 中断请求标志**。<br>• 硬件置位：$V_{DD}$ 发生由正常向欠压的跳变时由模型强制置 1；<br>• **软件写 0 清除，软件写 1 无效**（禁止伪造中断）。<br>• **实现注意（D4）**：W0C 在 `cms8s_lvd_poll` 内用 `priv->lvdintf &= xdata_shadow[0xF690]` 消解后写回 shadow，**不注册 SFR 写钩**。 |

**TA 保护边界**：`LVDCON` **不受 TA 保护**（原厂手册与驱动证实只有 `CLKDIV` 与 `WDCON` 走 TA 机制），禁止错误引入 TA 锁判定。

### 3.2 16 档电压检测阈值（逐档数值对照）

根据原厂 `system.h`（`SYS_LVD_2_0V` ~ `SYS_LVD_4_6V`）标定标准毫伏阈值（已与 `Libary/StdDriver/inc/system.h:66-81` 逐档核对一致）：

| LVDSEL | 官方常量宏 | 标称电压 | 仿真毫伏阈值 ($V_{th}$) | 归一化比例 ($V_{DD}=5.0\text{V}$) |
|:---:|:---|:---:|:---:|:---:|
| `0x0` | `SYS_LVD_2_0V` | 2.00V | 2000 mV | 0.4000 |
| `0x1` | `SYS_LVD_2_16V` | 2.16V | 2160 mV | 0.4320 |
| `0x2` | `SYS_LVD_2_31V` | 2.31V | 2310 mV | 0.4620 |
| `0x3` | `SYS_LVD_2_45V` | 2.45V | 2450 mV | 0.4900 |
| `0x4` | `SYS_LVD_2_60V` | 2.60V | 2600 mV | 0.5200 |
| `0x5` | `SYS_LVD_2_73V` | 2.73V | 2730 mV | 0.5460 |
| `0x6` | `SYS_LVD_2_88V` | 2.88V | 2880 mV | 0.5760 |
| `0x7` | `SYS_LVD_2_98V` | 2.98V | 2980 mV | 0.5960 |
| `0x8` | `SYS_LVD_3_21V` | 3.21V | 3210 mV | 0.6420 |
| `0x9` | `SYS_LVD_3_42V` | 3.42V | 3420 mV | 0.6840 |
| `0xA` | `SYS_LVD_3_62V` | 3.62V | 3620 mV | 0.7240 |
| `0xB` | `SYS_LVD_3_81V` | 3.81V | 3810 mV | 0.7620 |
| **`0xC`** | **`SYS_LVD_4_0V` (官方Demo)** | **4.00V** | **4000 mV** | **0.8000** |
| `0xD` | `SYS_LVD_4_2V` | 4.20V | 4200 mV | 0.8400 |
| `0xE` | `SYS_LVD_4_43V` | 4.43V | 4430 mV | 0.8860 |
| `0xF` | `SYS_LVD_4_6V` | 4.60V | 4600 mV | 0.9200 |

### 3.3 虚拟 VDD Sense 与边沿检测状态机（防中断风暴）

#### 3.3.1 虚拟 VDD sense（D1/D7）

```text
read_vdd_norm(ctx):
  1) 若 ctx->adc_inject_flag[LVD_VDD_SENSE_PIN] → 返回 adc_injected/4095   // Host CTest 路径
  2) 否则 js_pal_adc_read_norm(LVD_VDD_SENSE_PIN)                          // headless INPUT_ANALOG 路径
  3) 若读值 ≤ ε (未注入默认轨) → 回退 1.0f（5.0V）                          // D7 防 boot 竞态
```

- **虚拟 pin 选型**：`LVD_VDD_SENSE_PIN = 94`（=`32+62`，板级 AN 通道空间高位，**开工前 grep 确认与物理 pin 0–31、现有 AN0–AN63 映射及 `temperture_sensor` AN63 无冲突**；若冲突则改用邻近空闲 `32+N` 并同步场景 `adcChannel`）。
- 语义诚实性：此通道是**行为级 VDD 监测注入缝**，不是硅片真实 sense 引脚；在 Layer-① 拦截文档 §证据口径中记一句即可。
- **D6**：不调用 `mcs51_adc_set_vrail_mv`，不与 ADC 基准联动。

#### 3.3.2 边沿状态机

```text
                ┌────────────────────────────────────────────────────────┐
                │                LVD 硬件仿真边沿状态机                  │
                └────────────────────────────────────────────────────────┘
                                             │
                     ┌───────────────────────┴───────────────────────┐
                     ▼                                               ▼
          ┌─────────────────────┐                         ┌─────────────────────┐
          │  STATE_NORMAL (0)   │                         │  STATE_UNDERVOLT (1)│
          │ (Vdd >= Vth 正常态) │                         │ (Vdd < Vth 欠压态)  │
          └──────────┬──────────┘                         └──────────┬──────────┘
                     │                                               │
                     │ 供电跌落: Vdd < Vth                           │ 供电回升: Vdd >= (Vth + Vhys)
                     │ 动作:                                         │ 动作:
                     │ 1. LVDINTF 置 1                               │ 1. 恢复 STATE_NORMAL
                     │ 2. mcs51_raise_irq(LVD)                       │ 2. 重新就绪下一次跌落检测
                     │ 3. 切换到 STATE_UNDERVOLT                     │    (期间不清除已有 LVDINTF)
                     ▼                                               ▼
                     └───────────────────────────────────────────────┘
```

1. **单向边沿检测**：
   - 仅当从 `STATE_NORMAL` 进入 `STATE_UNDERVOLT` 时，产生**唯一次** `LVDINTF = 1` 置位与中断请求；
   - 若电压持续维持在欠压区间（$V_{DD} < V_{th}$），即使软件在 ISR 中调用 `SYS_ClearLVDIntFlag()` 将 `LVDINTF` 清零，**内核绝不再度置位**，从根源上杜绝中断风暴。

2. **迟滞恢复机制 ($\Delta V_{hys} = 100\text{mV}$)**：
   - 只有当供电电压回升至 $V_{th} + \Delta V_{hys}$（如 4.0V 档需回升至 4.10V）以上，状态机才切回 `STATE_NORMAL`，重新激活下一次跌落检测的触发锁存。

> ⚠️ **诚实标注（v1.1，对应评审 G2）**：数据手册 §4.3.3 仅声明「VDD 低于阈值产生中断请求」，**未给出滞回电压值与边沿极性**。`ΔVhys = 100mV` 是本仿真的**行为级建模选择**（防中断风暴 + 可重臂），**不是 silicon 标定真值**。落地时在 `cms8s_lvd.cpp` 文件头注释与 Layer-① `07-mcs51-simulation-interception.md` 证据口径各记一句；若日后拿到 silicon 实测再开 ADR 修订。

3. **复位/初始态（D7）**：
   - `Cms8sLvdState` 复位：`current_state = STATE_NORMAL`，`vdd_norm = 1.0f`，`lvdintf = 0`；
   - 保证 `LVD_Config()` 执行时 VDD 已是 5.0V，首次 `INPUT_ANALOG`（10ms）之前不会误触发下降沿。

4. **W0C 消解时序（每 poll）**：
   ```text
   // 1) 消解固件写 0（shadow 低位为 0 表示软件清了）
   priv->lvdintf &= (ctx->xdata_shadow[XSFR_LVDCON] & 0x01u);
   // 2) 读配置 + 读 VDD + 边沿状态机（可能置 priv->lvdintf / raise）
   // 3) 发布有效 LVDINTF 回 shadow bit0（硬件置位对固件可见）
   ```

### 3.4 扩展中断映射与优先级集成（`cms8s_sys.cpp`）

在 `kCms8sIrqExtensions` 表中新增 LVD 条目（D3）：
```cpp
// LVD: Vector 26, EIP3 (0xBB) bit 3, SW_CLEAR (软件写 LVDCON.0 清除)
{ IRQ_SOURCE_LVD, { 26u, 0xFFu, 0u, 0xFFu, 0u, 0xBBu, 3u, MCS51_IRQ_SW_CLEAR } },
```

并在 `cms8s_irq_flag_predicate` 中实现联合门控断言（**必须放在 `flag_sfr==0xFF` 短路之前**）：
```cpp
if (src == IRQ_SOURCE_LVD) {
    constexpr uint16_t XSFR_LVDCON = 0xF690u;
    const uint8_t lvdcon = ctx->xdata_shadow[XSFR_LVDCON];
    // 必须满足: 模块使能 (bit 3) + 中断使能 (bit 1) + 硬件标志置位 (bit 0)
    return ((lvdcon & 0x08u) != 0u) &&
           ((lvdcon & 0x02u) != 0u) &&
           ((lvdcon & 0x01u) != 0u);
}
```

**家族白名单**：`kIrqVectorsCms8s` 已含 `26u`（`mcs51_family.cpp:17-21`），无需改；`LVD_VECTOR=26` 已在 `transpile_app_keil_c51.py:82`。

**预算**：`IRQ_SOURCE__COUNT` 15→16，`irq_map` +8B；`kBudgetBytes = 75672+1024`，余量充足——Task 3 跑 `test_mcs51_context_budget` 验证。

---

## 4. 实施任务分解与执行路径

```text
┌────────────────────────────────────────────────────────────────────────┐
│ Task 1: 头文件方言垫片、StdDriver 与 XSFR Allowlist 重新生成            │
│ • 目标: REG_CMS8S78XX.H 补齐 LVDCON/常量/API；allowlist 放行 0xF690 │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│ Task 2: 仿真内核外设建模与边沿中断调度 (cms8s_lvd + sys 扩展)          │
│ • 目标: 独立外设 Cms8sLvdState、poll W0C 消解、Vector 26 映射注册      │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│ Task 3: Host 原生 CTest 单元测试开发与全绿门禁 (test_mcs51_cms8s_lvd)  │
│ • 目标: 覆盖 W0C、正常不高报、跌落精准触发、低压保持无风暴、回升再跌落 │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│ Task 4: 原厂微应用接入、三件套真实编译与 Headless 场景断言实证          │
│ • 目标: vendor_cms8s78xx_lvd 七件镜像；lvd.scenario.json 8 步 100% 绿  │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│ Task 5: 分层门禁自检、Layer-① 设计回写与 Checklist 36 摘牌             │
│ • 目标: winkcli lint 0 告警；Checklist 36 打勾；文档库同步             │
└────────────────────────────────────────────────────────────────────────┘
```

### Task 1: 头文件方言垫片与 XSFR Allowlist 补齐
- **目标文件**：
  - `wink-micro-os/frameworks/mcs51/chips/cms8s78xx/include/REG_CMS8S78XX.H`
  - `wink-micro-os/frameworks/mcs51/chips/cms8s78xx/include/cms8s_xsfr_allowlist.h`
- **改动细节**：
  1. 在 `REG_CMS8S78XX.H` 声明 `xsfr LVDCON (0xF690);`（**不重复声明 `EIP3`/`IRQ_SET_PRIORITY`——已存在**，仅核对位号 0xBB / bit 3 与本计划一致）；
  2. 声明位掩码与位置宏：`LVD_LVDCON_LVDSEL_Pos/Msk`, `LVDEN`, `LVDINTE`, `LVDINTF`；
  3. 声明 16 档阈值常量宏：`SYS_LVD_2_0V` ~ `SYS_LVD_4_6V`（逐字镜像原厂 `system.h:66-81`）；
  4. 声明中断常量：`LVD_VECTOR 26`, `IRQ_LVD 27`（若 REG 内尚无同名宏则补；`IRQ_SET_PRIORITY` 已有则复用）；
  5. 镜像原厂 `system.c` 实现 7 个 `static inline` StdDriver shims：
     - `SYS_EnableLVD()` / `SYS_DisableLVD()`
     - `SYS_ConfigLVD(uint8_t LVDValue)`
     - `SYS_EnableLVDInt()` / `SYS_DisableLVDInt()`
     - `SYS_GetLVDIntFlag()` / `SYS_ClearLVDIntFlag()`
  6. 执行门禁脚本自动更新放行表：
     ```powershell
     python wink-micro-os/frameworks/mcs51/tools/mcs51_shim_audit.py --emit-xsfr-allowlist
     ```
     验证 `cms8s_xsfr_allowlist.h` 包含 `0xF690u`。

### Task 2: 仿真内核外设建模与中断注册
- **目标文件**：
  - `wink-micro-os/frameworks/mcs51/include/wink_mcs51_isr.h`
  - `wink-micro-os/frameworks/mcs51/chips/cms8s78xx/include/cms8s_priv.h`
  - `wink-micro-os/frameworks/mcs51/chips/cms8s78xx/include/cms8s_lvd.h`（**新建**）
  - `wink-micro-os/frameworks/mcs51/chips/cms8s78xx/src/cms8s_lvd.cpp`（**新建**，D5）
  - `wink-micro-os/frameworks/mcs51/chips/cms8s78xx/src/cms8s_register.cpp`（**注册 desc**）
  - `wink-micro-os/frameworks/mcs51/chips/cms8s78xx/src/cms8s_sys.cpp`（仅 irq_map + predicate）
- **改动细节**：
  1. `wink_mcs51_isr.h` 中 `mcs51_irq_source_t` 枚举在 `IRQ_SOURCE_WDT` 后新增 `IRQ_SOURCE_LVD`（在 `__COUNT` 前）；
  2. `cms8s_priv.h` 的 `Cms8sPriv` 新增 `Cms8sLvdState`：
     ```cpp
     typedef struct {
         uint8_t  state;          // 0=NORMAL, 1=UNDERVOLT
         uint8_t  lvdintf;        // 模型私有有效标志（W0C 消解后的真相源）
         float    vdd_norm;       // 复位默认 1.0f (D7)
         uint64_t last_poll_us;
     } Cms8sLvdState;
     ```
  3. 新建 `cms8s_lvd.h`：`cms8s_lvd_init/reset/poll/next_event_us` 声明（仿 `cms8s_acmp.h`）；
  4. 新建 `cms8s_lvd.cpp`：
     - `constexpr uint16_t XSFR_LVDCON = 0xF690u;`
     - `constexpr uint16_t LVD_VDD_SENSE_PIN = 94u;`（开工前冲突检查，见 §3.3.1）
     - `read_vdd_norm()`：inject → `js_pal_adc_read_norm` → ε 回退 1.0（D1/D7）
     - threshold：`(LVDCON>>4)&0xF` → 查 §3.2 表得 mV
     - `poll`：W0C 消解 → `LVDEN` 门控（关断时 `state=UNDERVOLT` 若 VDD 已低则重置为「保持不边沿」策略：`LVDEN=0` 时 `state` 按当前 VDD 与阈值**静默对齐、不 raise**）→ 边沿检测 → 置 `lvdintf` + `mcs51_raise_irq(IRQ_SOURCE_LVD)` → 发布 bit0 回 shadow
     - **不注册任何 `on_lvdcon_write`**（D4）；
     - 文件头注释：迟滞 100mV 为行为级近似（G2）；
     - `SPDX-License-Identifier: LGPL-3.0-only`
  5. `cms8s_register.cpp`：前向声明 + `kCms8sDescs[]` 增加 `"cms8s_lvd"` 行（`init/reset/poll/next_event_us`；`next_event_us` 返回常量/0——无独立定时事件，随外设轮询评估）；
  6. `cms8s_sys.cpp`：
     - `kCms8sIrqExtensions` 增加 LVD 行（§3.4）；
     - `cms8s_irq_flag_predicate` 增加 LVD 分支（置于 `flag_sfr==0xFF` 短路**之前**）；
  7. Host 注入 API（**仅 host 测试缝，D6 不碰 vrail**）：
     - 在 `cms8s_lvd.h` 提供 `cms8s_lvd_set_vdd_mv(uint16_t mv)`（写 `priv->vdd_norm`）或直接约定测试写 `priv`；**不**新增/联动 `mcs51_adc_set_vrail_mv`；
  8. 同步更新 `cms8s_extint.cpp:131` 注释：`LVD have no model` → LVD 已由 `cms8s_lvd` 建模（STOP 唤醒是否放行见 Task 5 G6-descope）。

### Task 3: Host 原生 CTest 单元测试
- **目标文件**：
  - `wink-micro-os/frameworks/mcs51/test/cms8s78xx/test_mcs51_cms8s_lvd.cpp`（**新建**，SPDX `GPL-3.0-only`，仿 `test_mcs51_cms8s_acmp.cpp` harness）
  - `wink-micro-os/frameworks/mcs51/CMakeLists.txt`（注册 suite）
- **测试分层（G5）**——模型层为主，ISR 派发为可选增强：
  - `Test 1 (Register W0C)`: 写 1 无法凭空置位 `LVDINTF`，写 0（经 shadow）后下一 poll 清除；
  - `Test 2 (Normal High Voltage)`: $V_{DD} = 5000\text{mV} > 4000\text{mV}$，无 raise，标志恒 0；
  - `Test 3 (Falling Edge Trigger)`: `cms8s_lvd_set_vdd_mv(3500)` + poll → `LVDINTF==1` + irq pending；**可选**：注册 `WINK_ISR(26)` 后 `wink_mcs51_dispatch_vector(26)` 断言 `isr_dispatch_count(26)==1` 与 P32 翻转（若固件桩成本高，可仅断言 flag+raise，P32 翻转由 headless 承担——二选一在实现时定，验收以「模型层 5 用例全绿 + headless 含 ISR 翻转」为准）；
  - `Test 4 (Anti-Storm Latch)`: 3500mV 下 SW 清标志后再 poll/run 100ms，**不再**二次 raise；
  - `Test 5 (Recovery & Re-arming)`: 回升 5000mV（≥4100 迟滞线）再跌 3500mV → 第二次 raise；
  - `Test 6 (Enable Gates)`: `LVDEN=0` 或 `LVDINTE=0` 时同激励不 raise（建议补，堵 predicate/模型双门控回归）。
- **连带门禁**：`test_mcs51_context_budget` 必须仍绿（§3.4）。

### Task 4: 原厂微应用接入与 UniSim Headless 实证
- **微应用路径**：`wink-micro-app/vendor/cms8s78xx/lvd/`
- **交付七件（v1.1 补全 N4）**：
  | # | 文件 | 来源 |
  |---|------|------|
  | 1 | `main.c` | 原样镜像 [LVD/code/main.c](../../vendors/Cmsemicon/CMS8S78xx_DemoCode_V2.0.2/CMS8S78xx_Example/Example/LVD/code/main.c) |
  | 2 | `demo_lvd.c` | 原样镜像 |
  | 3 | `demo_lvd.h` | 原样镜像 |
  | 4 | `isr.c` | 原样镜像（`LVD_IRQHandler` vector 26 翻转 P32） |
  | 5 | `CMakeLists.txt` | 仿 `vendor/cms8s78xx/wdt/CMakeLists.txt` |
  | 6 | `wink-app.json` | `templateId`/`app_name` 对齐 `vendor_cms8s78xx_lvd`，`upstream.source_dir` 指向 `.../Example/LVD/code`，仿 wdt |
  | 7 | `unisim-scenarios/lvd.scenario.json` | 见下 |

  原厂四源文件**一行不改**（Playbook 硬门禁）。

- **场景设计 (`lvd.scenario.json`)**——8 步（3×`INPUT_ANALOG` + 5×`ASSERT_POINT`；**禁用 `INPUT_POWER`**，D1）：
  ```json
  {
    "header": {
      "version": "1.0.0",
      "name": "mcs51 CMS8S78xx LVD 4.0V threshold detection headless proof",
      "templateId": "vendor_cms8s78xx_lvd",
      "accuracyMode": "behavioral",
      "timeoutUs": "1000000",
      "failurePolicy": "fail-fast",
      "determinism": { "prngSeed": 42 }
    },
    "steps": [
      {
        "type": "INPUT_ANALOG",
        "timeUs": "10ms",
        "adcChannel": 62,
        "valueNorm": 1.0,
        "description": "虚拟 VDD sense (pin 94=32+62) 注入 5.0V，高于 4.0V 门限"
      },
      {
        "type": "ASSERT_POINT",
        "timeUs": "50ms",
        "target": "gpio:26",
        "matcher": 0,
        "description": "[上电基准] P3.2 保持初始低电平 0，无 LVD 中断"
      },
      {
        "type": "INPUT_ANALOG",
        "timeUs": "100ms",
        "adcChannel": 62,
        "valueNorm": 0.7,
        "description": "VDD 跌落至 3.5V (< 4.0V 阈值)"
      },
      {
        "type": "ASSERT_POINT",
        "timeUs": "150ms",
        "target": "gpio:26",
        "matcher": 1,
        "description": "[首次触发] LVD 中断 (Vector 26)，ISR 执行 P32=~P32 翻转为 1"
      },
      {
        "type": "ASSERT_POINT",
        "timeUs": "250ms",
        "target": "gpio:26",
        "matcher": 1,
        "description": "[抗风暴保持] 持续 3.5V 欠压，已清标志不重复触发，P3.2 稳定 1"
      },
      {
        "type": "INPUT_ANALOG",
        "timeUs": "300ms",
        "adcChannel": 62,
        "valueNorm": 1.0,
        "description": "VDD 恢复 5.0V (≥4.1V 迟滞线，重新就绪)"
      },
      {
        "type": "INPUT_ANALOG",
        "timeUs": "350ms",
        "adcChannel": 62,
        "valueNorm": 0.7,
        "description": "VDD 二次跌落至 3.5V"
      },
      {
        "type": "ASSERT_POINT",
        "timeUs": "400ms",
        "target": "gpio:26",
        "matcher": 0,
        "description": "[二次触发] 重新就绪后再次检测到跌落，ISR 再次翻转 P3.2 回落至 0"
      }
    ]
  }
  ```
  > `adcChannel: 62` 与 §3.3.1 `LVD_VDD_SENSE_PIN=94=32+62` 的对应关系以开工冲突检查后的最终常量为准，二者必须同步修改。

- **验证命令**（统一 `winkcli`，在 embedded 仓根执行；N7）：
  ```powershell
  winkcli build sim --app vendor_cms8s78xx_lvd
  winkcli sim run --app vendor_cms8s78xx_lvd --mode headless --scenarios wink-micro-app/vendor/cms8s78xx/lvd/unisim-scenarios/lvd.scenario.json
  ```
  （若环境仅有 `python <wink-tools>/wink.py` 入口，以 `wink-tools/README.md` 为准等价替换，并在执行记录注明实际命令。）

### Task 5: 治理回写与 Checklist 摘牌
1. **Checklist**：`CMS8S78XX_EXAMPLE_CHECKLIST.md` 第 36 项 `[ ]` → `[x]`，附真实资产与 Headless 证据（场景 8 步、virtual 1s、wasm 大小、lint 0）；
2. **静态门禁**：
   ```powershell
   winkcli lint --pack layering --pack api --pack wasm
   python .github/scripts/check_license_map.py
   ```
   确保 0 findings；
3. **回写锚点补全（G6）**：
   - `cms8s_extint.cpp` LVD 注释已在 Task 2 更新；
   - `docs/todolist/2026-09-10-mcs51-sim-vs-silicon-gap-todolist.md` **GAP-17'**：更新「WUT/LSE/SWE/LVD 标不支持」中 LVD 分句——LVD 模型已落地；**STOP(PD) 唤醒是否放行 LVD 单独裁定**：本计划默认 **descope**（仅建模运行态中断，不扩 `mcs51_pcon` 唤醒表），在 GAP 行注明「LVD 模型已有，PD 唤醒接入另项」；
   - 红线手册 §4.7 / 保真度文档：若有「LVD 无模型」表述则同步修订；
   - Layer-① `07-mcs51-simulation-interception.md`：补虚拟 VDD sense 证据口径 + 迟滞行为级近似声明。

---

## 5. 验收准则与黄金门禁

所有工作完成后，必须同时满足以下硬性条件方可宣告收官：

1. **真实编译产物完整**：`wink-micro-app/vendor/cms8s78xx/lvd/unisim-assets/` 下具备 `device-tree.json`、`wink_simulator.js`、`wink_simulator.wasm` 三件套；
2. **Headless 断言 100% 绿灯**：**8 步**全部通过，退出码 0，虚拟时间 1s 内完成（**不设**无依据的 wall-clock 300ms 硬指标，N2）；
3. **Host CTest 全绿**：新增 `test_mcs51_cms8s_lvd` 全部用例 PASS（≥5，建议 6），且既有 **165 个 mcs51 CTest suite 无回退**（口径对齐 I2C/SPI 计划「CTest 165/165 → 166/166」，N3）；`test_mcs51_context_budget` 仍绿；
4. **代码纯净与零反噬**：无动态内存分配（零 `malloc`）、无未受管全局变量、无未经注册的 XSFR 访问、`mcs51_shim_audit` 0 漂移、符合 LGPL-3.0-only（runtime）/ GPL-3.0-only（test）许可地图；
5. **原厂零改动**：四源文件与 `docs/vendors/.../LVD/code/` SHA256 或 diff 为空。

---

## 6. 风险与缓解（v1.1）

| 风险 | 等级 | 缓解 |
|------|:---:|------|
| 虚拟 pin 94 与现有 AN/板级映射冲突 | 中 | Task 2 开工前 grep `32u + 62` / pin 94 / `adcChannel: 62`；冲突则换空闲 `32+N` 并同步场景 |
| 默认 VDD 回退被 scenario 绕过/误注入 | 低 | D7 默认 1.0 + Test 2/6 + 场景 t=10ms 显式注入 |
| XSFR W0C 用错 SFR 写钩导致清不掉/风暴 | 高 | D4 强制 poll 消解；Test 1/4 专测 |
| `IRQ_SOURCE_LVD` 扩容触发 context budget | 低 | +8B ≪ 1KB slack；Task 3 跑预算测试 |
| 迟滞 100mV 被误当手册真值 | 低 | §3.3.2 + 代码头 + Layer-① 三处诚实标注 |
| `INPUT_POWER` 回潮（后续贡献者再写） | 中 | 场景文件头注释 + 本计划 D1 作为评审检查项 |
| STOP 唤醒范围蔓延 | 低 | Task 5 明确 descope，仅更新 GAP 文案 |

---

## 7. 交付物清单

1. 计划文档：本文件（v1.1）
2. 框架：`REG_CMS8S78XX.H` 扩展、`cms8s_xsfr_allowlist.h` 再生成、`wink_mcs51_isr.h`、`cms8s_priv.h`、**新建** `cms8s_lvd.h/cpp`、`cms8s_register.cpp`、`cms8s_sys.cpp`、`cms8s_extint.cpp` 注释
3. 测试：**新建** `test_mcs51_cms8s_lvd.cpp` + CMake 注册
4. App：`wink-micro-app/vendor/cms8s78xx/lvd/` 七件 + `unisim-assets/` 三件套
5. 文档：Checklist #36、GAP-17'、红线/保真度、Layer-① 拦截文档回写
