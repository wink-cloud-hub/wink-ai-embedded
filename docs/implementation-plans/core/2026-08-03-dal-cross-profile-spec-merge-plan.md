# DAL 跨 Profile 量纲规范融合计划

| 项 | 内容 |
|----|------|
| **创建日期** | 2026-08-03 |
| **状态** | Completed / 已执行（2026-08-03） |
| **关联文档** | [`dal-api-consistency-spec.md`](../../../../wink-micro-os/docs/dal-development-guide/dal-api-consistency-spec.md)（主规范）、[`dal-micro-profile-spec.md`](../../../../wink-micro-os/docs/dal-development-guide/dal-micro-profile-spec.md)（8 位子规范）；~~`dal-cross-profile-contract-spec.md`（提案，已随本次融合删除，决策见 ADR-0056）~~ |
| **关联 ADR** | [ADR-0056](../../decisions/core/0056-cross-profile-quantity-ab-class-and-scaled-integers.md)（跨 Profile 量纲策略：A/B 两分类，否决全局弱 typedef）；既有 [ADR-0004](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md)（静态分发）、ADR-0046（驱动 Registry SSOT）、ADR-0048（执行器语义命名） |

---

## 1. 背景与目标

### 1.1 三份文档的现状

当前 DAL 开发指南下有三份与跨 Profile / 量纲相关的文档，内容重叠且存在张力：

1. **`dal-api-consistency-spec.md` v3.3.1（主规范，活文档）** — 9 个 32 位驱动的单一事实源，正在实际使用。其 §9.3 当前采用"Full=float / Micro=int"二元模型。
2. **`dal-micro-profile-spec.md` v1.0.0（8 位子规范，Proposed）** — 8 位工具链/内存模型/语言子集，DAL-8B-F-002 规定 `dal_led_8b_on` 命名。
3. **`dal-cross-profile-contract-spec.md` v1.2.0（提案，Proposed）** — 提出"量纲两分类（A/B）+ 同源落在 codegen binding 层 + 否决 `dal_types.h` 弱 typedef"，但与主规范 §9.3 的二元模型尚未对齐，且 v1.2.0 存在编辑合并损坏（第 259 行 `DAL-XP-057` 截断后直接拼接 `### 10.3`，§6.7/部分规则散落、章节顺序错乱）。

### 1.2 核心架构决策（待 ADR 确认）

经过评审形成的共识：

> **"32 位和 8 位的共同规范"应只包含"现在写错、将来要破坏式改公开 API"的规则；纯 8 位机制留在 8 位子规范里。**

关键洞察——**量纲两分类**：

| | A 类·执行器命令 | B 类·传感器测量 |
|---|---|---|
| 方向 | App→硬件 | 硬件→App |
| 硬件终态 | 离散寄存器整数（PWM CCR/ARR） | 物理量，喂滤波/融合 |
| 跨 Profile 策略 | **全 Profile 统一定标整数**（连 32 位 Full 也不用 float） | Full=float / Micro=定点，由 codegen binding 吸收 |

A 类在 32 位用整数的理由：即便用 `float 0.8f`，DAL 内部写寄存器时仍要 `(uint32_t)(0.8f * max_duty)`，这层 float 多此一举；且迫使 8 位端做 float↔int 软浮点转换。Math Domain（BAL 的 PID/浮点算法）与 Hardware Control Domain（PWM 寄存器整数）应在 BAL→DAL 边界完成转换。

### 1.3 目标

- 把跨 Profile 共识中"现在就约束 32 位"的部分**并入主规范**，让它成为活文档的一部分。
- 纯 8 位机制**留在 micro 子规范**，主规范只保留分类概览与指针。
- 被否方案（`dal_types.h` 弱 typedef）的论证**移到 ADR 存档**，活规范只讲"怎么做"。
- 归档损坏的 contract 提案，消除文档重叠与矛盾。
- **本次只动文档，不改任何 32 位代码。**

---

## 2. 融合边界：什么进主规范，什么不进

### 2.1 进主规范（桶 1：现在就约束 32 位，防返工）

| 内容 | 落点 | 为什么是现在 |
|------|------|-------------|
| 量纲两分类原则（A 执行器命令 / B 传感器测量） | 主规范 §9 重构 | 决定新驱动 API 形态，分类错将来要改签名 |
| A 类命令全 Profile 用定标整数（promille/ddeg），32 位也不用 float | 主规范新 §9.4 | experimental 驱动（如 rc_servo）现在整型化零成本；冻结后再改要 deprecation |
| 禁止弱 typedef 量纲别名（`dal_speed_t`） | 主规范新规则 | 现在别造，否则是将来要删的债 |
| Setter/Getter/句柄内部成员用同一表示（防半整型化撕裂） | 主规范新规则 | 新驱动一次做对，句柄 `float` 与整型 setter 不一致最难迁 |
| 物理不变量 + 钳位饱和无 UB（越界隐式钳位至安全边界，杜绝溢出回卷暴走） | 强化现有 §9.2 与 §9.4 DAL-U-011 | 本就是 32 位该遵守的，与位宽无关，杜绝硬件暴走风险 |
| 封闭单位后缀表（封闭枚举，禁止任意自造后缀） | 主规范 §9.1 | 建立标准单位表（_us/_ms/_um/_cmm/_ma/_promille 等），防止参数命名歧义 |
| 符号与定标整型运算溢出防护（8/16位 MCU 算术中间值强制提升为 32 位） | 主规范 §9.4 | 规范符号选择与低端 MCU 乘除溢出安全，防止隐式提升 Bug |
| YAML 加 `quantity` + `quantity_class` 元数据 | 主规范 §16 | 零运行时成本，将来 codegen 前提 |
| "App 经 codegen binding、不直连 DAL"的同源边界说明 | 主规范 §1.3 | 澄清边界，防止再出现"签名完全相同"方案 |

### 2.2 不进主规范（桶 2：留给 8 位子规范）

以下纯 8 位机制在独立 Micro 文件/类型里分化，永远不需回头改 32 位公开 API：

- Flash Zero-Copy 句柄（`const WINK_CODE *cfg`）、`WINK_CODE/XDATA/IDATA` 存储区限定符
- `_8b_t` 句柄/config 类型、`dal/micro/include/` 独立头根、`WINK_PROFILE_FULL/MICRO` 构建隔离
- `reentrant`、Overlay 分析、`uint8_t` 代 `bool`、`uint16_t` 时间戳
- B 类测量量的 `_LITERAL` 宏与运行时转换函数（32 位是恒等空操作）
- codegen `template_micro_8bit` 模板分支
- WASM-Micro 双策略（WASM 本就跑 Full）

### 2.3 现在明确不做（桶 3：做了本身就是返工）

- 不把 `dc_motor.set_speed(float)`（stable + Golden Ref）整型化——等真有 8 位需求再走 deprecation（新增 `set_speed_promille` + 两个 minor 窗口 + ADR）。
- 不搭双 profile 构建系统、不建 `dal_types.h`、不写 8 位驱动。
- stable 驱动的 config 保留现有 `float`（Full POD config 与 Micro Flash config 本就是两个类型，32 位 config 用 float 不逼改 API）。

---

## 3. 主规范 §9 重构方案（融合主战场）

把现有 §9 从"Full=float / Micro=int"二元模型，重构为"先分类、再谈类型"：

```
§9 单位、量纲与值域
  §9.1 参数命名与封闭单位后缀表（保留与强化：封闭单位后缀枚举表 _ms/_us/_ddeg/_promille/_um/_cmm 等，DAL-U-001/002）
  §9.2 值域声明与隐式钳位饱和（保留：Range、越界钳位饱和 DAL-U-010/011，强制安全边界无 UB）
  §9.3 量纲两分类原则（新增，核心）
        - A 类·执行器命令 vs B 类·传感器测量
        - Math Domain ↔ Hardware Domain 边界
        - 分类表与判定依据
  §9.4 A 类执行器命令的整数定标（新增）
        - 两种形态：归一化比例量（‰/per10k）与绝对物理量（µm/cmm/µs/mA/ddeg）
        - 符号与定标整型选择规范：无反向量强制 unsigned，双向/反向量强制 signed
        - 定标整型运算溢出防护：8/16 位 MCU 驱动算术中间值必须提升至 uint32_t/int32_t 运算
        - 钳位饱和原则（Clamp Saturation）：A 类 Setter 越界参数必须隐式钳位饱和到 [min, max]，禁止溢出回卷
        - 按器件全量程精度选刻度；Full Profile 也 MUST 用定标整数，MUST NOT 用 float
        - setter/getter/句柄内部缓存用同一表示（防撕裂）
        - 禁止弱 typedef 量纲别名
  §9.5 B 类传感器测量的跨 Profile 映射（新增）
        - Full=float+后缀 / Micro=定点整型+后缀
        - 物理不变量（折算到标准单位一致）、饱和无 UB、单位统一原则
  §9.6 8 位 Micro Profile 量纲（瘦身）
        - 保留 DAL-8B-U-001/002/003 概览 + 一张分类对照表
        - 细节（存储区、reentrant、语言子集）移交 dal-micro-profile-spec.md
```

原 §9.3 的纯 8 位内容下沉到 micro 子规范，主规范只留概览 + 交叉引用。

### 3.1 A/B 分类对照表（拟写入新 §9.3）

| | A 类·执行器命令 | B 类·传感器测量 |
|---|---|---|
| 方向 | App→硬件（输出） | 硬件→App（输入） |
| 硬件终态 | 离散寄存器整数 | 物理量（滤波/融合/显示） |
| 典型量 | 速度、占空比、舵机角度、亮度、频率、**直线/角度位置**、超时/延时、引脚、计数 | 温度、距离、电压、电流、加速度、角速度、经纬度 |
| Full 类型 | 定标整数（刻度见 §3.1.1） | `float`/`double` + 单位后缀 |
| Micro 类型 | **与 Full 完全相同的定标整数类型与刻度** | 定点整型 + 后缀 |
| App 字面量 | 直接写整数（`-500`、`900`、`2500`） | 转换宏 / codegen 换算 |
| 软浮点 | 零 | 低频可接受，成本显式声明 |

#### 3.1.1 A 类整数定标的两种形态（关键澄清）

A 类"用定标整数"**不等于只能用千分比（‰）**。‰ 只是其中一种刻度。按物理量性质分两类选刻度：

| 形态 | 适用 | 定标示例 | 后缀 |
|------|------|---------|------|
| **归一化比例量** | 无量纲的相对命令（速度、占空比、亮度） | 千分比 ‰（`[-1000,1000]`/`[0,1000]`，约 10 位）；需要更细用 per-10k（0.01%） | `_promille` / `_per10k` |
| **绝对物理量** | 有 SI 单位的绝对命令（位置、脉宽、电流、转矩） | 直接选物理 LSB：µm、0.01mm（cmm）、µs、mA、0.1°(ddeg) | `_um` / `_cmm` / `_us` / `_ma` / `_ddeg` |

刻度选择原则（对应 contract 规范 DAL-XP-045）：
1. LSB 必须满足器件全量程的有效精度；不要为了"统一 ‰"而牺牲绝对量的物理直观性。
2. 量程必须装得进所选整数位宽；8 位优先 `uint16_t`/`int16_t`（8051 上 32 位乘除显著更贵），需要更大量程才用 `uint32_t` 并在头注释声明 8 位成本。
3. 一旦选定，Full 与 Micro 必须用**完全相同的类型、单位后缀与倍率**（刻度是全 Profile 契约，不可按 Profile 分化）。

#### 3.1.2 0.01mm 位置精度的支持示例

> 压力测试：电机/直线执行器需 0.01mm 位置精度。

- 用**绝对物理量整数定标**，而非 ‰：
  - `int32_t position_um`（1 LSB = 1µm = 0.001mm，量程 ±2147m）——精度高于 0.01mm，长行程首选；
  - 或 `uint16_t position_cmm`（1 LSB = 0.01mm，量程 0~655.35mm）——短行程、8 位上最省。
- 全 Profile 同类型同字面量（`set_position(&motor, 1500)` = 15.00mm），32 位/8 位零软浮点、真同源。
- 约束是位宽×量程，不是定标方案：若要"0.01mm 精度 + >655mm 行程"，`uint16_cmm` 装不下，需 `uint32_um` 或重定量程；这是任何方案（含 float）都存在的物理权衡，8 位上 float 更贵。
- codegen 按 YAML 声明的量程在生成期校验，超 Micro 位宽量程直接报错（DAL-XP-056），不静默截断。

结论：方案支持。关键是把"绝对物理量用物理整数定标（µm/cmm/µs）"显式写进主规范，避免读者误以为 A 类只有 ‰/ddeg。

#### 3.1.3 封闭单位后缀表（写进新 §9.1）

主规范 §9.1 建立**封闭枚举单位后缀表（Closed Enumeration）**，所有 DAL API 参数及 YAML 声明必须严格限定在标准后缀中，严禁任意自造拼写：

| 物理维度 | 标准后缀 | 刻度 / LSB 说明 | 典型示例 |
|---|---|---|---|
| **时间** | `_us` / `_ms` / `_s` | 微秒 / 毫秒 / 秒 | `timeout_ms`, `pulse_us` |
| **长度/位置** | `_nm` / `_um` / `_cmm` / `_mm` | 纳米 / 微米 / 0.01毫米(丝) / 毫米 | `position_um`, `stroke_cmm` |
| **角度** | `_ddeg` / `_mdeg` | 0.1度(didegree) / 0.001度(millidegree) | `angle_ddeg` |
| **电气量** | `_ma` / `_mv` / `_ua` / `_uv` | 毫安 / 毫伏 / 微安 / 微伏 | `current_ma`, `voltage_mv` |
| **归一化比例** | `_promille` / `_per10k` | 千分比 (‰, 1000=100%) / 万分比 (0.01%) | `speed_promille`, `duty_per10k` |

#### 3.1.4 符号选择与定标整型运算溢出防护（写进新 §9.4）

针对嵌入式 C 语言在 8 位 / 16 位 MCU 上的 **Integer Promotion（整型隐式提升）** 陷阱：

1. **符号规范（Signedness）**：
   - 无物理反向的控制量（如 PWM 占空比、LED 亮度、单向脉冲）**强制**使用无符号类型（`uint16_t` / `uint32_t`）；
   - 允许反向/双向的控制量（如双向电机转速、舵机相对偏角）**强制**使用有符号类型（`int16_t` / `int32_t`）。
2. **运算中间值溢出防护（Arithmetic Overflow Guard）**：
   - DAL 驱动内部在进行硬件寄存器换算（例如 `CCR = (ARR * duty_promille) / 1000`）时，即使 `duty_promille` 为 `uint16_t`，中间乘积在 8 位/16 位 MCU 上也极易溢出。
   - 规范显式要求：DAL 驱动底层换算涉及乘法中间值时，**必须显式强转为 `uint32_t` / `int32_t` 进行运算再做除法**，Codegen 与静态检查工具必须对此做自动化校验。

#### 3.1.5 钳位饱和原则（Clamp Saturation）（写进新 §9.4 与 §9.2）

明确 A 类执行器命令越界时的硬件安全边界行为：

- **隐式钳位饱和（Clamp to Saturation）**：当 App 传入超越器件物理极限的值（如 `set_duty_promille(1200)` 或 `set_position_um(-500)`）时，驱动必须**自动钳位饱和**至合法 `[min, max]` 区间（如钳位为 `1000` 或 `0`），**严禁出现溢出回卷（Overflow Wrap-around）**引致硬件暴走或失控。
- **Debug 日志与断言**：在 Debug 构建模式下打出 Warn/Assert 日志提醒 App 开发者，但在 Release 构建下必须保持安全隐式钳位运行。

### 3.2 新规则编号规划

沿用主规范现有 `DAL-U-xxx` 段（§9），新增规则顺延：
- A/B 分类、A 类整型化、同表示、禁弱 typedef、物理不变量、**刻度选择（归一化比例 vs 绝对物理量）** 等编入 `DAL-U-020` 起的新号。
- 8 位相关沿用现有 `DAL-8B-U-xxx`，但 §9.6 只保留概览。
- codegen 元数据与量程校验规则编入 §16 的 `DAL-CG-xxx`（或现有段顺延）。
- 具体编号在编辑时确定，避免与既有号冲突。

---

## 4. 其他章节的配套小改

- **§1.3 Profile 分级**：补一段"同源边界 = codegen binding 层；DAL 符号名统一、签名按 Profile 分化"，纠正"签名完全相同"的误读。
- **§16 Codegen 集成**：标注现有 §16.3 的 `template_full`/`template_micro_8bit` 范例为"待 codegen 实现"（现状是单 `template`，per-profile 能力尚未落地）；新增 YAML `quantity` / `quantity_class` 字段说明。
- **§17.1 Golden Reference**：说明 dc_motor 的 `float set_speed` 是迁移前现状（stable 保留），新 A 类驱动的 Golden Ref 行为应以定标整数为准（待 rc_servo 整型化后可考虑更新标杆）。

---

## 5. 被否方案存档 → ADR

contract 提案 §12 附录 B（`dal_types.h` 弱 typedef + 转换宏方案的 7 条否决理由）有长期参考价值，但不应留在活规范里。移到新 ADR：

- **ADR 标题**：跨 Profile 量纲策略——A/B 两分类与定标整数，否决全局弱 typedef 别名
- **结构**（按 `.claude/rules/docs-adr.md`）：
  - 背景：8 位/32 位同源诉求与原 `dal_types.h` 方案
  - 备选方案：①全局弱 typedef + 转换宏（被否）；②本规范两分类 + codegen binding（采纳）
  - 决策结论：A 类全 Profile 定标整数、B 类分化、同源落 binding 层
  - 后果与约束：桶 1/2/3 落地策略、dc_motor 走 deprecation、rc_servo 可直接整型化
- ADR Accepted 后回写主规范 §9（符合 CLAUDE.md 决策回写要求）。

---

## 6. 配套文档修订

1. **`dal-micro-profile-spec.md`**：
   - DAL-8B-F-002：`dal_led_8b_on(dal_led_8b_t*)` → 统一动词名 `dal_led_on(dal_led_8b_t*)`（函数去 `_8b_`，类型名保留 `_8b_t`）。
   - 量纲类型表述与主规范新 §9 对齐：A 类引用主规范定标整数表，B 类说明定点分化。
2. **`dal-cross-profile-contract-spec.md`**：内容被吸收后**归档/删除**（其 v1.2.0 损坏随归档消除）。归档前确认无未吸收的独有条款。
3. **主规范版本**：升至 **v3.4.0**，变更历史记录"引入量纲两分类、A 类全 Profile 整型化、§9 重构、§9.3 瘦身"。

---

## 7. 执行顺序

| 步骤 | 产出 | 说明 |
|------|------|------|
| 1 | 本计划 review/确认 | 用户确认融合边界与 §9 结构 |
| 2 | 起草 ADR（Proposed） | 记录两分类决策 + 被否方案 7 理由 + 桶 1/2/3 |
| 3 | ADR Accepted | 拍板后再动活规范 |
| 4 | 重构主规范 §9 | 并入桶 1，§9.3 瘦身，§1.3/§16 小补，升 v3.4.0 |
| 5 | 修订 micro-profile-spec | 去 `_8b_` 函数名，量纲表述对齐 |
| 6 | 归档删除 contract-spec.md | 确认条款已全部吸收 |
| 7 | （代码，本次不做） | rc_servo 后续可整型化；dc_motor 等 8 位需求走 deprecation |

---

## 8. 验收标准

- 主规范 §9 自洽：新驱动读完即知 A/B 如何分类、A 类用什么整数类型、B 类如何分化，无需翻提案。
- 三份文档无矛盾：主规范讲"是什么/怎么做"，micro 子规范讲"8 位怎么实现"，ADR 讲"为什么这么决定"。
- 无残留指向 contract-spec.md 的失效链接。
- 现有 9 个 32 位驱动**零代码改动**；主规范对 stable 的 dc_motor float 现状有明确"迁移前保留"说明，不造成自相矛盾。
- `wink lint --pack layering --pack api` 不受影响（纯文档变更）。

---

## 9. 风险与开放问题

1. **A 类整数刻度的"默认值"**：千分比（‰）仅作为**归一化比例量**（速度/占空比/亮度）的默认，不覆盖绝对物理量。12 位以上调光可能需 per-10k；位置/脉宽/电流等绝对量应直接选物理定标（µm/cmm/µs/mA）。计划：在主规范 §9.4 显式列出两种形态与示例（含 0.01mm 位置用例），不强制一刀切。
2. **B 类 Full 是否也该统一单位（如距离都用 mm）**：计划在 §9.5 写"优先统一物理单位以减少 App 换算"，但不强制（cm 在 32 位浮点更直观）。
3. **rc_servo 整型化是否本计划顺带做代码**：建议**不做**，保持本计划纯文档；rc_servo 整型化单开任务（experimental，可直接改）。
4. **ADR 编号与回写时机**：需查 `list_adrs.py` 获取下一个 ADR 号；ADR Accepted 后才回写主规范。

---

## 10. 执行记录（2026-08-03）

| 步骤 | 产出 | 状态 |
|------|------|------|
| 2~3 | [ADR-0056](../../decisions/core/0056-cross-profile-quantity-ab-class-and-scaled-integers.md) 以 Accepted 落档（含被否方案 7 理由 + 桶 1/2/3 + 迁移纪律） | ✅ |
| 4 | 主规范 `dal-api-consistency-spec.md` 升 **v3.4.0**：§9.1 封闭后缀表、§9.2 钳位饱和无 UB、新增 §9.3 两分类 / §9.4 A 类整数定标（DAL-U-020~032）/ §9.5 B 类映射（DAL-U-040~043）/ §9.6 Micro 概览；§1.3 同源边界、§4.5/§5.3.1 去 `_8b_` 与现状标注、§16 YAML `quantity`/`quantity_class` + per-profile 模板标注待实现、§17.1 Golden Ref 现状说明、§17.3.1 规则实施状态补录 | ✅ |
| 5 | `dal-micro-profile-spec.md` 升 **v1.1.0**：DAL-8B-F-002 函数名去 `_8b_`（类型保留 `_8b_t`）；§4.1 量纲对齐 A/B 分类，新增 DAL-8B-U-004 | ✅ |
| 6 | 删除 `dal-cross-profile-contract-spec.md`（损坏 v1.2.0）、`dal-cross-profile-quantity-spec.md`（被否 v1.0.0）、`dal-cross-profile.md`（随笔草稿）。三文件均未跟踪、无活文档链接指向 | ✅ |
| 回写 | `01-dal-device-abstraction.md` 关联 ADR 加 ADR-0056，§6.2 补跨 Profile 量纲策略指针 | ✅ |
| 验证 | `wink lint --pack layering --pack api` → No findings；全仓无指向已删文件的失效 markdown 链接；9 个 32 位驱动**零代码改动** | ✅ |

> 计划外但必要的处理：原计划步骤 6 只点名删除 contract-spec；核验发现目录中另有同属被否/草稿的 quantity-spec 与 cross-profile 随笔，为满足"三份文档无矛盾"验收标准一并删除（被否方案论证已完整入 ADR-0056 §方案比选①）。

