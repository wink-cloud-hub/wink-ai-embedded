# ADR-0056：跨 Profile 量纲策略——A/B 两分类与定标整数，否决全局弱 typedef 别名

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳）** |
| 日期 | 2026-08-03 |
| 触发 | DAL 三份跨 Profile / 量纲文档重叠且存在张力（主规范 §9.3 "Full=float/Micro=int" 二元模型、`dal-micro-profile-spec.md`、损坏的 `dal-cross-profile-contract-spec.md` v1.2.0）；融合计划 [`2026-08-03-dal-cross-profile-spec-merge-plan.md`](../../implementation-plans/core/2026-08-03-dal-cross-profile-spec-merge-plan.md) |
| 影响范围 | `wink-micro-os/dal/` 公开 API 形态（新增驱动）、`codegen/drivers/*.yaml`（`quantity` / `quantity_class` 元数据）、DAL 开发指南三份文档的融合与归档；不改动现有 9 个 32 位驱动代码 |
| 决策者 | 项目 Owner（确认执行融合计划） |
| 关联 ADR | [ADR-0004](0004-static-dispatch-vs-runtime-ops.md)（POD + 静态命名分发，无 vtable）；[ADR-0046](0046-dal-driver-registry-ssot.md)（DAL 驱动全集 SSOT = codegen YAML）；[ADR-0048](0048-actuator-control-semantic-naming.md)（执行器按控制语义命名与 safe_off 绑定） |
| 关联计划 | [implementation-plans/2026-08-03-dal-cross-profile-spec-merge-plan.md](../../implementation-plans/core/2026-08-03-dal-cross-profile-spec-merge-plan.md) |

---

## 背景（Context）

`wink-micro-os` 要同时在两类硬件上同源运行：Full Profile（32 位，ESP32-S3 / STM32 / WASM，硬件 FPU、KB~MB RAM、统一地址空间）与 Micro Profile（8 位，8051 / STC8 / AVR，无 FPU、128~256B RAM、Harvard 存储区与 Overlay 分析）。两者的硬件能力差异是物理事实，无法在 C 函数签名层面抹掉。

在融合三份文档的过程中浮现两个相互竞争的方案：

1. **弱 typedef 别名 + 转换宏方案**（早期 `dal-cross-profile-quantity-spec.md` v1.0.0 提案）：用 `typedef float/int16_t dal_speed_t;` + `DAL_SPEED(v)` 宏，让 DAL 的 C 签名在两端"看起来一样"，宣称"App 源码 100% 同源、零运行时开销"，并以 `-DWINK_PROFILE_MICRO` 在 WASM 里"精准孪生"8 位。
2. **量纲两分类方案**（`dal-cross-profile-contract-spec.md` v1.1.0 引入）：按数据流向与硬件终态把物理量分 A/B 两类，A 类全 Profile 统一定标整数，B 类按 Profile 分化，同源落在 codegen binding 层。

融合评审的核心共识是：

> **"32 位和 8 位的共同规范"应只包含"现在写错、将来要破坏式改公开 API"的规则；纯 8 位机制留在 8 位子规范里。**

## 方案比选（Options）

### 方案 ①：全局弱 typedef 别名 + 转换宏（否决）

定义 `dal_speed_t` 等跨平台 typedef 与 `DAL_SPEED(v)` 宏。**否决理由（7 条）：**

1. **"100% 同源"在句柄层破产。** Micro 句柄是 `const WINK_CODE *cfg` Flash 指针，Full 是 POD 深拷贝；config 的 `WINK_CODE` 限定符与类型两端不兼容。仅统一标量参数无法让 `static dal_dc_motor_t m; ...` 同源，差异被推后到设备声明处，仍要 `#ifdef` 或 codegen。
2. **"零开销"只对字面量成立，范例却用运行时变量。** `app_motor_set_target(float speed_norm)` 中 `DAL_SPEED(speed_norm)` 对运行时 float 入参在 8 位上必发软浮点乘法——恰好链接它声称要消灭的软浮点库。宏伪装零开销，比显式转换更危险。
3. **弱 typedef 提供虚假类型安全。** `typedef float dal_speed_t;` 与 `typedef float dal_angle_t;` 在 C 中可互相赋值传参不告警；编译器不做单位检查，却把单位从可读的后缀（`angle_deg`）藏进类型名，调试时看到 `900` 反要查刻度。
4. **跨端量纲静默分叉 + 溢出 UB。** `dal_distance_t` Full=cm、Micro=mm，使同一份 `if (dist < 10)` 跨端语义不同；`DAL_SPEED(1.5f)` 在 Micro 得 `(int16_t)1500` 越界，是 C 未定义行为。方案既无饱和也无物理不变量约束，"能编译但运行错"比函数名割裂更危险。
5. **与已定稿规范冲突且未走 ADR。** 方案要废止 8 位子规范的 MUST 条款、改动 9 个已冻结驱动的公开原型，却无 ADR、无迁移窗口、无回滚计划，违反主规范向后兼容红线与项目文档治理。
6. **WASM-Micro 是伪孪生。** 在 32 位有 FPU 的 WASM 里用 int16 模拟，无法仿真 8051 的存储区 / Overlay / 栈 / 软浮点时序，只验证了定点缩放，却号称"100% 数字孪生"。
7. **YAGNI。** 仓库无一行 8 位实现、无 `dal_types.h`、无 8051 port。为不存在的目标做波及全部冻结 API 的全局抽象层，是过早抽象。

### 方案 ②：A/B 两分类 + codegen binding（采纳）

见决策结论。

## 决策结论（Decision）

### 1. 量纲两分类原则（一等设计原则）

按**数据流向与硬件终态**把 DAL 物理量分两类：

| | **A 类：执行器命令（Actuator Command）** | **B 类：传感器测量（Sensor Measurement）** |
|---|---|---|
| 方向 | App → 硬件（输出） | 硬件 → App（输入） |
| 硬件终态 | 离散寄存器整数（PWM CCR/ARR、比较值、分频） | 物理量，喂滤波 / 融合 / 显示 |
| 典型量 | 速度、占空比、舵机角度、亮度、频率、直线/角度位置、超时/延时、引脚、计数 | 温度、距离、电压、电流、加速度、角速度、经纬度 |
| 跨 Profile 策略 | **全 Profile 统一定标整数**（同类型、同刻度、同字面量），32 位 Full 也不用 float | Full=`float`+后缀；Micro=定点整型+后缀，由 codegen binding 吸收 |
| App 字面量 | 直接写整数（`-500`、`900`、`2500`），无需转换宏 | 用 `_LITERAL` 宏（字面量）或具名转换函数（运行时变量） |
| 软浮点成本 | 零 | 低频可接受，成本显式声明 |

**设计理由——Math Domain 与 Hardware Control Domain 的边界**：控制量在硬件侧终态本就是离散整数。即便 32 位 Full 用 `float duty_norm = 0.8f`，DAL 内部写寄存器时仍要 `(uint32_t)(0.8f * max_duty)`——这层 float 多此一举，还迫使 Micro 端做 float↔int 转换。浮点应留在 BAL 的 PID / 滤波等**数学域**，在 BAL→DAL 边界做一次 `(int16_t)(u * 1000)` 是正确分层。

### 2. A 类整数定标的两种形态

A 类"用定标整数"不等于只能用千分比（‰）：

| 形态 | 适用 | 定标示例 | 后缀 |
|------|------|---------|------|
| **归一化比例量** | 无量纲相对命令（速度、占空比、亮度） | 千分比 ‰（`[-1000,1000]`/`[0,1000]`，约 10 位）；需更细用 per-10k（0.01%） | `_promille` / `_per10k` |
| **绝对物理量** | 有 SI 单位的绝对命令（位置、脉宽、电流、转矩） | 直接选物理 LSB：µm、0.01mm(cmm)、µs、mA、0.1°(ddeg) | `_um` / `_cmm` / `_us` / `_ma` / `_ddeg` |

刻度选择三原则：①LSB 满足器件全量程有效精度，不为"统一 ‰"牺牲绝对量直观性；②量程装得进所选整数位宽，8 位优先 `uint16_t`/`int16_t`；③一旦选定，Full 与 Micro 必须用**完全相同的类型、后缀与倍率**（刻度是全 Profile 契约）。

> **0.01mm 位置精度用例**：用绝对物理量整数定标——`int32_t position_um`（1µm=0.001mm，量程 ±2147m，长行程首选）或 `uint16_t position_cmm`（0.01mm，量程 0~655.35mm，短行程 8 位最省）。全 Profile 同类型同字面量，零软浮点、真同源。

### 3. 关键安全与类型约束

- **符号规范**：无物理反向的控制量（PWM 占空比、LED 亮度、单向脉冲）强制无符号（`uint16_t`/`uint32_t`）；允许反向/双向（双向电机转速、相对偏角）强制有符号。
- **运算溢出防护**：DAL 底层寄存器换算（如 `CCR = (ARR * duty_promille) / 1000`）的乘法中间值，即使操作数是 `uint16_t`，在 8/16 位 MCU 上也易溢出，**必须显式强转为 `uint32_t`/`int32_t` 运算再做除法**，codegen / 静态检查须自动化校验。
- **钳位饱和（Clamp Saturation）无 UB**：A 类 Setter 越界参数（如 `set_duty_promille(1200)`）必须**隐式钳位饱和**到 `[min, max]`（如 1000），**严禁溢出回卷**引致硬件暴走；Debug 构建可 Warn/Assert，Release 必须安全钳位运行。这与位宽无关，32 位同样遵守。
- **setter/getter/句柄内部缓存同一表示**：新增 A 类驱动的 Setter 类型、Getter 出参类型、句柄内部缓存成员必须同步整型化，禁止"Setter 用定点而句柄残留 float"的半整型化撕裂。
- **封闭单位后缀表**：建立封闭枚举后缀表（`_us/_ms/_um/_cmm/_ma/_mv/_promille/_per10k/_ddeg` 等），所有 DAL API 参数及 YAML 声明必须限定在标准后缀中，严禁任意自造拼写。
- **禁止弱 typedef 量纲别名**：MUST NOT 引入 `dal_speed_t` 之类的跨 Profile 弱 typedef（否决方案①）。
- **物理不变量**：B 类量在两端折算到标准物理单位后必须一致；Micro 超位宽量程由 codegen 在生成期报错，不静默截断。

### 4. 同源边界 = codegen binding 层

- **逻辑契约**（动词集、生命周期、错误码、状态机、量纲语义、safe_off 绑定、A 类刻度）全 Profile 一致。
- **C 符号名**全 Profile 一致（都叫 `dal_<type>_<verb>`，函数名去 `_8b_`；Micro **类型名**保留 `_8b_t`/`_8b_config_t` 因其内存模型不同构）。
- **C 签名/句柄/实现**允许按 Profile 分化；一个固件只链接一个 Profile，同名不冲突。
- **App MUST NOT 直接调用 DAL**，MUST 通过 codegen 生成的 role binding 访问器件；手写 `dal_xxx_*` 仅允许出现在 DAL 自身、BAL、单测中。同源承诺由 codegen binding 层兑现，而非硬压进 DAL 签名。

### 5. 桶 1 / 2 / 3 落地策略

| 桶 | 内容 | 处理 |
|----|------|------|
| **桶 1：现在就约束 32 位（防返工）** | A/B 分类、A 类全 Profile 定标整数（32 位也不用 float）、禁弱 typedef、setter/getter/句柄同表示、物理不变量+钳位饱和无 UB、封闭单位后缀表、符号与运算溢出防护、YAML `quantity`+`quantity_class`、codegen binding 同源边界 | **并入主规范活文档** |
| **桶 2：纯 8 位机制（留子规范）** | Flash Zero-Copy 句柄、`WINK_CODE/XDATA/IDATA`、`_8b_t` 类型与独立 include 根、`reentrant`/Overlay、`uint8_t` 代 bool、`uint16_t` 时间戳、B 类 `_LITERAL` 宏与运行时转换、codegen micro 模板分支 | **留 `dal-micro-profile-spec.md`**，主规范只留概览+指针 |
| **桶 3：现在明确不做** | `dc_motor.set_speed(float)` 整型化（等真有 8 位需求走 deprecation）、双 profile 构建系统、`dal_types.h`、8 位驱动代码 | **不做** |

### 6. 现存 Full 驱动的迁移纪律

- **stable 驱动**（`dc_motor`，Golden Ref，`experimental: false`）：`set_speed(float)`/`get_speed`/句柄 `float current_speed` **本次保留**；将来整型化是破坏性变更，MUST 新增 `set_speed_promille(int16_t)` + 旧 API 标 `WINK_DEPRECATED_MSG` 内部转调 + 两个 minor 版本窗口 + 单独 ADR，MUST NOT flag-day 改签名。
- **experimental 驱动**（`rc_servo`，`experimental: true`）：将来可直接把 `set_angle(float)` 改为 `set_angle(uint16_t angle_ddeg)`、句柄 `float current_angle` 改 `uint16_t current_angle_ddeg`，无需 deprecation 窗口，但 MUST 在 changelog/PR 记录签名变更。**本 ADR 不改其代码。**
- 其余 A 类量（`pwm_freq_hz`、引脚、计数）本就是整数，无需迁移。

## 后果与约束（Consequences & Constraints）

| 正面 | 负面 / 缓解 |
|------|-------------|
| 新增 A 类驱动一次做对，32 位即用定标整数，冻结后无需破坏性整型化 | stable `dc_motor` 暂留 float，主规范须明确"迁移前现状"，避免自相矛盾（已在 §17.1 说明） |
| 最热控制路径全 Profile 零软浮点、零转换宏、真同源 | 刻度选择需作者判断（归一化比例 vs 绝对物理量），靠封闭后缀表 + 评审与 codegen 校验约束 |
| 弱 typedef 被明确否决，避免引入将来要删的债 | 8 位落地仍待真实 port（8051 PAL + SDCC CI + `led_8b` 端到端验证），本 ADR 不排期代码 |
| 三份重叠文档融合为一主规范 + 一子规范 + 一 ADR，消除矛盾与损坏文件 | 归档三个提案文件（contract/quantity/cross-profile 随笔），须确认无残留链接 |
| YAML `quantity`/`quantity_class` 为将来 codegen 校验提供零运行时成本的 SSOT 前提 | `quantity_class` 缺失须在 codegen 报错拦截，属后续 codegen 工作（本次只在规范中定义字段） |

## 遵循与后续（Compliance & Follow-up）

Accepted 后必须：

- [x] 回写主规范 `dal-api-consistency-spec.md`：重构 §9（新增 §9.3 两分类 / §9.4 A 类整数定标 / §9.5 B 类映射 / §9.6 Micro 概览，强化 §9.1 封闭后缀表与 §9.2 钳位饱和），小补 §1.3 同源边界、§16 YAML `quantity`/`quantity_class`、§17.1 迁移现状；升 v3.4.0 — 2026-08-03
- [x] 修订 `dal-micro-profile-spec.md`：DAL-8B-F-002 函数名去 `_8b_`，量纲表述对齐 A/B 分类 — 2026-08-03
- [x] 归档/删除 `dal-cross-profile-contract-spec.md`（损坏 v1.2.0，内容已吸收）、`dal-cross-profile-quantity-spec.md`（被否方案①原始提案，否决理由见本 ADR）、`dal-cross-profile.md`（分类思路随笔草稿）— 2026-08-03
- [ ] rc_servo 整型化（experimental，可直接改）单开任务，不在本计划范围
- [ ] 8 位真实 port 落地时，先 8051 PAL + SDCC CI + `led_8b` 端到端验证，再据实证校准 Micro 子规范规则
- [ ] codegen 实现 `quantity`/`quantity_class` 校验与 `template_full`/`template_micro_8bit` per-profile 模板（当前为单 `template`，属待落地能力）

> **本次纯文档变更，现有 9 个 32 位驱动零代码改动，`wink lint` 不受影响。**

---

*本 ADR 状态变更请在此记录：*
- 2026-08-03：Proposed（配合 DAL 跨 Profile 量纲规范融合计划起草）
- 2026-08-03：Accepted（Owner 确认执行融合计划；桶 1 并入主规范、桶 2 留子规范、桶 3 不做；否决弱 typedef 方案）

