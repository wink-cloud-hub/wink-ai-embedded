# DAL API 一致性规范 (DAL API Consistency Specification)

| 项 | 内容 |
|----|------|
| **规范版本** | v3.4.3 (Active) |
| **状态** | 现行规范 / 已采纳 |
| **适用范围** | `wink-micro-os` 器件抽象层 (`dal/`) 驱动开发与代码生成器 (`codegen`) |
| **关联活规范** | [`01-dal-device-abstraction.md`](../../../docs/design/02-wink-micro-os/01-dal-device-abstraction.md) |
| **关联 ADR** | [ADR-0001](../../../docs/design/decisions/0001-error-code-sign-convention.md) (错误码符号约定), [ADR-0004](../../../docs/design/decisions/0004-static-dispatch-vs-runtime-ops.md) (静态分发), [ADR-0017](../../../docs/design/decisions/0017-blocking-api-hard-isolation.md) (阻塞隔离), [ADR-0024](../../../docs/design/decisions/0024-fault-three-phase-model-and-dal-deinit-contract.md) (Deinit 清场), [ADR-0043](../../../docs/design/decisions/0043-yaml-driven-layer-lint.md) (Lint 规约), [ADR-0046](../../../docs/design/decisions/0046-dal-driver-registry-ssot.md) (驱动 Registry SSOT), [ADR-0048](../../../docs/design/decisions/0048-actuator-control-semantic-naming.md) (执行器语义命名), [ADR-0056](../../../docs/design/decisions/0056-cross-profile-quantity-ab-class-and-scaled-integers.md) (跨 Profile 量纲 A/B 两分类与定标整数) |
| **变更历史** | v3.4.3 (2026-08-04) **补充架构评审 P0 契约与升格**：基于架构级深度评审 [2026-08-04-dal-api-consistency-spec-v342-architect-review.md](../../../docs/design/reviews/2026-08-04-dal-api-consistency-spec-v342-architect-review.md)，新增 DAL-L-030/031（`safe_off` ↔ `deinit` 顺序与幂等契约）、DAL-B-026（异步三段式状态机 `ERROR` 态恢复迁移契约）；将 DAL-U-030（Setter/Getter/句柄缓存同表示）升格为 `[LINT-ENFORCED]` (dal_quantity.py) 强管控；v3.4.2 (2026-08-04) **新增 Lint 自动化分级标注与索引矩阵**：为全规范所有 `DAL-x-xxx` 规则引入 `[LINT-ENFORCED]` (100% 静态 Lint 强管控)、`[LINT-PARTIAL]` (部分结构校验) 与 `[MANUAL-REVIEW]` (架构/语义契约) 三级状态标注；在 §1.4 新增「Lint 自动化覆盖索引矩阵」；支持 AI 助手按行读取规范时自动跳过已被 100% Lint 化规则以降低上下文 Token 消耗; v3.4.1 (2026-08-03) 新增 DAL-S-006：引脚字段类型选择约定（必填引脚用 `uint16_t`，可选引脚用 `wink_pin_t`+`-1` 哨兵），将 led/button/ultrasonic 与 dc_motor/encoder 既有实践固化为 SHOULD 规则；源自 led 驱动 v3.4.0 合规整改中"必填 uint16_t 引脚做 `<0` 检查是恒假死代码"的实证; v3.4.0 (2026-08-03) **跨 Profile 量纲两分类融合（[ADR-0056](../../../docs/design/decisions/0056-cross-profile-quantity-ab-class-and-scaled-integers.md)）**：重构 §9——§9.1 升级为封闭单位后缀枚举表；§9.2 强化越界钳位饱和无 UB（DAL-U-011）；新增 §9.3 量纲两分类原则（A 执行器命令 / B 传感器测量）、§9.4 A 类全 Profile 整数定标（两种定标形态、符号规范、运算溢出防护、钳位饱和、同表示、禁弱 typedef，DAL-U-020~028）、§9.5 B 类跨 Profile 映射、§9.6 Micro Profile 量纲瘦身概览（纯 8 位机制移交子规范）；§1.3 补 codegen binding 同源边界；§16 新增 YAML `quantity`/`quantity_class` 字段并标注 per-profile 模板为待实现；§17.1 说明 dc_motor float 为迁移前现状；本次纯文档变更，9 个 32 位驱动零代码改动; v1.0.0 (2026-08-01) 初稿; v2.0.0 基于评审重写; v2.1.0 整合 review notes; v3.0.0 整合 8 位 Profile 体系; v3.1.0 (2026-08-02) 补充 8 位动态内存禁令; v3.2.0 (2026-08-03) 纠正 offsetof 位宽误导与断言逻辑、更正 Golden Ref 样板为 dc_motor、新增 DMA 缓冲区归属契约 (§7.3)、修订忙等禁令微秒豁免与 ERR_NO_DATA 状态码、将 8 位 Micro Profile 解耦独立出子规范 [`dal-micro-profile-spec.md`](dal-micro-profile-spec.md); v3.3.0 (2026-08-03) 新增 init 资源回滚契约 (DAL-L-008)、deinit best-effort 语义 (DAL-L-015)、config 不可变性 (DAL-S-015)、poll 返回值语义 (DAL-B-025)、错误码分段 (DAL-EC-004)；补充 Watchdog/总线恢复/init 时间预算/was_* 原子性/self_test API 等中优先级规则；补充电流功率单位后缀与 Contract Side-effects 字段；修复 5 处文字/格式问题; v3.3.1 (2026-08-03) **勘误 §2.3 ABI 范例**：更正 Golden Ref 偏移常量（原 dc_motor 范例将 `offsetof(initialized)` 误写为 24/32，实测为 28/36——`initialized` 在 `float current_speed` 之后，并非紧跟 config 末尾），补全整句柄 `sizeof` 断言并加注"实测勿臆测"说明；补充 DAL-L-022「safe_off 未初始化返回 WINK_OK」的设计理由（基于 actuator_registry safe_off_all 故障消费链路分析） |

---

## 术语与约定

本文使用 RFC 2119 关键字：

- **MUST / MUST NOT** — 强制规则，CI lint 以 error 报告违反（新增驱动指示平台立即适用）
- **SHOULD / SHOULD NOT** — 推荐规则，CI lint 以 warning 报告违反（存量可豁免至迁移期结束）
- **MAY** — 可选，不纳入 lint

### Lint 自动化分级与 AI 阅读策略 (Lint Automation Tags)

本规范中的所有规则均标注了 Lint 静态校验状态，分为三档：

- `[LINT-ENFORCED]` — **100% 完全 Lint 强管控**：由 `wink-tools/tools/lint/packs/` 自动静态拦截。**AI 助手按行读取本规范时可直接跳过此类规则的细节描述**，代码生成/修改完成后直接运行 `python wink-tools/wink.py lint dal` 校验即可。
- `[LINT-PARTIAL]` — **部分 Lint 覆盖**：具备基础结构性静态检查，但复杂边界需配合人工/AI 验证。
- `[MANUAL-REVIEW]` — **纯语义/架构契约**：无法靠静态语法提取拦截（如硬件清场、死锁防护、资源回滚）。**AI 助手与 Reviewer 必须深度阅读与理解**。

**合规分级**：新增驱动须满足所有 MUST；存量驱动在基线冻结日前以 warning 模式运行，见 [§17 合规矩阵](#17-合规矩阵与迁移策略)。

---

## 1. 背景与架构目标

`wink-micro-os` 的 **DAL（Device Abstraction Layer）** 承载各类传感器、执行器、显示屏与通信模块的逻辑驱动。随着硬件种类增加，缺乏统一 API 规范会导致：

1. **命名碎片化** — 不同开发者对相同动作混用 `turn_on` / `enable` / `start` / `on`，AI 代码生成器正确率下降。
2. **工具链适配成本高** — `app_codegen.py` 和 `wink.py lint` 难以自动提取设备能力。
3. **应用层学习曲线陡峭** — 掌握 `dal_led` 后无法自然推导 `dal_dc_motor` 或 `dal_ultrasonic` 的使用范式。
4. **安全隐患** — 并发与失效安全缺乏统一契约，在双核 SMP (ESP32-S3) 上产生真实 bug。
5. **跨芯片级别兼容物理阻碍** — 8 位超低端 MCU (8051/STC8/AVR) 存在 RAM 极度匮乏 (<256B)、无硬件 FPU、Harvard 存储区隔离与 Keil C51 Overlay 覆盖分析机制，无法直接运行 32 位全量 C 代码。

### 核心设计目标

| 目标 | 含义 |
|------|------|
| **高度一致性** | 统一生命周期范式、领域动词库与注释契约 |
| **安全优先** | 并发/ISR 安全、失效安全是硬性条款 |
| **零破坏性演进** | 新特性不破坏现有 App/BAL 代码 |
| **两端同源** | Wasm 仿真、ESP32/STM32 硬件与 8051 极简 MCU 共享同一套 YAML SSOT |
| **静态化与零开销** | POD 句柄模式，无 vtable / 无堆分配（ADR-0004），8 位下实现零开销 Flash 引用 |
| **AI 可分析** | codegen 能从 DAL 静态提取能力并生成 role verb |

### 1.3 Profile 分级设计原则 (Profile Tiering)

为实现“**同源 YAML SSOT，两端精准生成**”，`wink-micro-os` 引入 Profile 区分机制：

* **Full Profile (32-bit / POSIX / WASM / STM32 / ESP32)**：包含 `owner` 跟踪、支持 32/64 位高精度时间戳与 POD 配置深拷贝句柄（本规范主推标准；WASM 仿真架构与通道定义见 [Wasm 仿真 3.0 SSOT](../../../docs/design/04-wasm-simulation-3.0/00-README.md)）。**A 类执行器命令用定标整数（不用 float），B 类传感器测量用 `float`/`double`**（见 §9）。
* **Micro Profile (8-bit / 8051 / STC8 / AVR)**：整数量纲（A 类与 Full 同刻度定标整数；B 类定点）、Flash Zero-Copy 句柄引用、静态硬编码/直接分发、16 位低开销计数器与 `uint8_t` 状态标志。详细条款已独立解耦出扩展规范 [`dal-micro-profile-spec.md`](dal-micro-profile-spec.md)。

**同源边界 = codegen binding 层（[ADR-0056](../../../docs/design/decisions/0056-cross-profile-quantity-ab-class-and-scaled-integers.md)）**：

- **逻辑契约**（动词集、生命周期、错误码、状态机、量纲语义、safe_off 绑定、A 类刻度）全 Profile 一致；
- **C 符号名**全 Profile 一致（都叫 `dal_<type>_<verb>`，函数名不带 `_8b_`；Micro **类型名**保留 `_8b_t`/`_8b_config_t`，因其内存模型不同构）；
- **C 签名 / 句柄 / 实现**允许按 Profile 分化——一个固件只链接一个 Profile，同名不冲突；
- App MUST NOT 直接调用 DAL，MUST 通过 codegen 从 YAML 生成的 role binding 访问器件；同源承诺由 binding 层兑现，**而非强行让 DAL 的 C 签名两端完全相同**。

| 特性维度 | Full Profile (32-bit Target) | Micro Profile (8-bit Target, 见扩展规范) |
|---------|-----------------------------|---------------------------------------|
| **适用芯片** | ESP32-S3, STM32, WASM 仿真 | 8051, STC8, AVR, PIC |
| **A 类执行器命令** | 定标整数（`int16_t`/`uint16_t`/`uint32_t` + 刻度后缀） | **与 Full 完全相同的定标整数类型与刻度** |
| **B 类传感器测量** | `float` / `double` + 单位后缀 | 定点整型 + 后缀（由 codegen binding 吸收） |
| **句柄内存模式** | POD 深拷贝 `config_t` (RAM 驻留) | Flash 指针引用 `const WINK_CODE *cfg` (2~4 字节 RAM) |
| **资源归因** | 包含 `const char *owner` 指针 | 可通过 `#ifdef WINK_DISABLE_OWNER_TRACKING` 裁减 |
| **调度与分发** | C 函数调用、C99 `bool` | 静态内联/硬编码宏、`uint8_t` 替代 `bool` |
| **临界区实现** | PAL 总线锁 / 互斥锁 / 原子操作 | `EA` 总中断使能保存/恢复宏 (`WINK_8B_CRITICAL_*`) |

### 1.4 Lint 自动化覆盖索引矩阵 (Lint Automation Index)

`wink-tools/tools/lint/packs/` 对本规范规则的实现映射如下。详细清单见 [§17.3.1 规则实施状态](#1731-规则实施状态)。

| 领域模块 | 主要覆盖规则 ID | 负责 Lint Pack (`wink-tools/tools/lint/packs/`) |
|---|---|---|
| **数据结构/句柄** (§2) | `DAL-S-001`, `DAL-S-005`, `DAL-S-006`, `DAL-S-011`, `DAL-S-012`, `DAL-S-014`, `DAL-S-020` | `dal_struct.py` |
| **API 函数范式** (§4) | `DAL-F-001`, `DAL-F-002`, `DAL-F-004`, `DAL-F-010`~`013`, `DAL-V-003` | `dal_api_shape.py` |
| **量纲与定标** (§9) | `DAL-U-001`~`004`, `DAL-U-011`, `DAL-U-022`~`029` | `dal_quantity.py` |
| **生命周期校验** (§3) | `DAL-L-001`, `DAL-L-003`, `DAL-L-004`, `DAL-L-010`, `DAL-L-022` | `dal_lifecycle.py` |
| **并发与 ISR** (§6) | `DAL-C-002`, `DAL-C-020`, `DAL-C-021` | `dal_concurrency.py` |
| **阻塞与延时** (§7) | `DAL-B-001`, `DAL-B-004`, `DAL-B-012` | `dal_blocking.py` |
| **文档与 YAML** | `DAL-B-003`, `DAL-P-014`, `DAL-L-020`, `DAL-U-021` | `dal_contract_doc.py`, `dal_yaml_parity.py` |

---

## 2. 数据结构与句柄规范

### 2.1 配置结构体 `dal_<type>_config_t`

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-S-001 | MUST | `[LINT-ENFORCED]` (dal_struct.py) | 首个成员 MUST 为 `const char *owner;`（供 device_tree/codegen 静态资源冲突检测与运行时日志归因。DAL 层不直接做资源仲裁；底层资源冲突由 PAL resource claim 机制在 `pal_gpio_init` / `pal_pwm_init` 等调用中检测并返回 `WINK_ERR_BUSY` 或 `WINK_ERR_RESOURCE_EXHAUSTED`） |
| DAL-S-002 | MUST | `[MANUAL-REVIEW]` | `owner` MUST 指向**静态存储期**字符串（字符串字面量或 `static const char[]`），MUST NOT 指向栈/堆 |
| DAL-S-003 | SHOULD | `[LINT-PARTIAL]` | 成员按数据类型尺寸降序排列以减少自然对齐填充（如 `uint32_t` → `uint16_t` → `bool`） |
| DAL-S-004 | MUST NOT | `[MANUAL-REVIEW]` | 当成员顺序兼做序列化线格式时（见 `apply_override`），MUST NOT 为了填充优化而重排已有成员。**兼容性 > 填充优化** |
| DAL-S-005 | MUST NOT | `[LINT-ENFORCED]` (dal_struct.py) | MUST NOT 使用位域（`uint8_t flags : 3;`）或 `#pragma pack` |
| DAL-S-006 | SHOULD | `[LINT-ENFORCED]` (dal_struct.py) | **引脚字段类型按"是否可选"选择**：必填引脚（器件工作必须存在，无"无引脚"态，如 LED 的 `pin`、ultrasonic 的 `trig_pin`/`echo_pin`、button 的 `pin`）SHOULD 使用 `uint16_t`；可选引脚（可缺省，需 `-1` 哨兵表示"未使用"，如 dc_motor 的 `dir_pin_b`/`enable_pin`、encoder 的 `pin_b`）SHOULD 使用 `wink_pin_t`（`int16_t`）。判据：合法 GPIO 编号恒 ≥ 0，必填引脚用无符号类型使无效负值在类型层不可能、无需 `< 0` 防御检查（对 `uint16_t` 写 `pin < 0` 是恒假死代码，会触发 `-Werror=type-limits`）；可选引脚才需要 out-of-band 哨兵 `-1`，且 init MUST 校验/规范化该哨兵（参见 dc_motor `enable_pin` 的 `0→-1` 规范化）。现存驱动中 dc_motor 的必填 `dir_pin_a` 用 `wink_pin_t` 属历史宽松，不强制改 |

**范例**（`dal_led_config_t`）：

```c
typedef struct {
    const char *owner;     /* 资源占用 owner 静态字符串 */
    uint16_t pin;          /* 逻辑 GPIO 引脚 */
    bool active_high;      /* true: 高电平点亮 */
} dal_led_config_t;
```

### 2.2 实例句柄 `dal_<type>_t`

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-S-010 | MUST | `[LINT-ENFORCED]` (dal_struct.py) | MUST 为 POD (Plain Old Data) 结构体 |
| DAL-S-011 | MUST | `[LINT-ENFORCED]` (dal_struct.py) | **首个成员** MUST 内嵌 `dal_<type>_config_t config;`（`offsetof(dal_xxx_t, config) == 0`） |
| DAL-S-012 | MUST | `[LINT-ENFORCED]` (dal_struct.py) | MUST 包含 `bool initialized;` 状态标志 |
| DAL-S-013 | MUST | `[LINT-PARTIAL]` | MUST 支持 `{0}` 零初始化（所有成员零值为安全默认态） |
| DAL-S-014 | SHOULD | `[LINT-ENFORCED]` (dal_struct.py) | 新增句柄类型 SHOULD 添加 `_Static_assert(offsetof(dal_xxx_t, config) == 0, ...)` |
| DAL-S-015 | MUST | `[MANUAL-REVIEW]` | `init` 成功后，调用方 MUST NOT 直接修改 `dev->config` 中的任何字段。需要运行时变更配置的操作 MUST 通过专用 API（如 `set_*` / `apply_override`）进行，由驱动实现负责同步硬件状态与配置缓存。直接写 `dev->config.xxx = val` 会导致硬件状态与缓存脱节，是隐蔽 bug 的常见来源 |

### 2.3 ABI 稳定性断言

每个 config_t 和 dal_<type>_t SHOULD 添加编译期断言，使任何布局变化在编译时被捕获。

**位宽相关的 `offsetof` 与 `sizeof` 编译期断言规约**：

1. **绝对首成员断言**：`offsetof(dal_xxx_t, config) == 0` 以及不含指针的纯数据结构体成员偏移跨位宽恒等。
2. **含指针结构体的后续成员断言**：结构体中若包含指针（如 `config_t` 首成员 `const char *owner`），其后成员（如 `initialized`）的偏移在 32 位 Target（指针 4B）与 64 位 Host 测试（指针 8B）上不同。**严禁声明无条件的具体数值 `offsetof` 断言**。
3. **断言分发**：所有含指针的结构体 `offsetof` 与 `sizeof` 断言 MUST 按 `INTPTR_MAX` 分档：

```c
_Static_assert(offsetof(dal_dc_motor_t, config) == 0, "config must be the first member");

#if INTPTR_MAX == INT32_MAX   /* ILP32: ESP32 xtensa, wasm32 */
_Static_assert(sizeof(dal_dc_motor_config_t) == 24, "ABI break: config size changed on 32-bit target");
_Static_assert(offsetof(dal_dc_motor_t, initialized) == 28, "ABI break: initialized offset changed on 32-bit");
_Static_assert(sizeof(dal_dc_motor_t) == 32, "ABI break: handle size changed on 32-bit target");
#else                         /* LP64 / LLP64: 64-bit Host Simulation */
_Static_assert(sizeof(dal_dc_motor_config_t) == 32, "ABI break: config size changed on 64-bit host");
_Static_assert(offsetof(dal_dc_motor_t, initialized) == 36, "ABI break: initialized offset changed on 64-bit host");
_Static_assert(sizeof(dal_dc_motor_t) == 40, "ABI break: handle size changed on 64-bit host");
#endif
```

> **数字来源（实测，勿臆测）**：以上偏移以 Full Profile 的前提为准——`enum` 为 `int`(4B)、`wink_pin_t` 为 `int16_t`(2B)。`initialized` 并不紧跟 `config` 末尾：它在 `float current_speed`(4B) 之后，故 32 位为 28（config=24 + speed 4）、64 位为 36（config=32 + speed 4）。任何 config/handle 布局改动都 MUST 用目标编译器重新核算这些常量，禁止凭"末尾紧挨"直觉填写。优先 `sizeof(整结构体)` 断言：指向中间字段的 `offsetof` 无法捕获该字段之后的追加/重排，而整尺寸断言能捕获除"末尾等尺寸替换"外的几乎所有 ABI 漂移。

> **owner 裁剪与 Wire Format**：若通过 `#ifdef WINK_DISABLE_OWNER_TRACKING` 裁减 `owner`，`config_t` 成员偏移将整体前移。序列化/反序列化（如 `apply_override`）MUST 显式解耦线格式，或在 8 位裁剪模式下保留对应尺寸的 reserved 占位字段。

#### 2.3.1 自动化验证（`wink.py lint --pack abi`）

§2.3 的数字**严禁凭直觉填写**。本仓库提供 `wink.py lint --pack abi` 子命令**自动核对**手填数字是否与目标编译器实测一致：

```bash
# 跑全部 DAL 头文件的 ABI 探针（ILP32 + LP64 两档）
python wink-tools/wink.py lint --pack abi

# 只对单个文件
python wink-tools/wink.py lint --pack abi --paths wink-micro-os/dal/include/output/dal_led.h
```

**工作原理**：pack 对每个 `dal/include/**/dal_<type>.h` 临时生成一个**探针 TU**（probe translation unit）：

```c
#include <stddef.h>
#include <stdint.h>
#include "<header>"
#include "wink_status.h"

#define EMIT(name, expr) \
    const unsigned long long v_##name = (unsigned long long)(expr)

EMIT(handle_sizeof,     sizeof(dal_<type>_t));
EMIT(config_sizeof,     sizeof(dal_<type>_config_t));
EMIT(handle_config_off, offsetof(dal_<type>_t, config));
EMIT(<field1>_off,      offsetof(dal_<type>_t, <field1>));
EMIT(<field2>_off,      offsetof(dal_<type>_t, <field2>));
/* ... */
```

接着用 `gcc -m32 -S`（ILP32）和 `gcc -S`（LP64）各编一次，从 `.s` 产物中解析出每个 `v_<name>` 标号的 `.long`/`.quad` 初值（即 sizeof/offsetof 的实际数字）。然后将**探针数字**与头文件中**已声明的 `_Static_assert` 右值**逐行对照：

| Rule ID | 级别 | 触发条件 | 含义 |
|---------|------|----------|------|
| `abi.abi_assert_value_mismatch` | **error** | 探针数字 ≠ `_Static_assert` RHS | 手填数字与目标编译器实测不符；该断言会在 real target 编译挂掉，或在 host 静默失效。**修法**：用探针给出的真实数字替换 RHS（参考 `wink.py lint --pack abi` 的 `measured` 字段） |
| `abi.abi_assert_missing` | warning | 缺 `offsetof(handle, config) == 0` 断言 | spec §2.3 的最小护栏（DAL-S-011 首成员保证）未声明 |
| `abi.probe_compile_failed` | warning | gcc -m32 / gcc -S 编译失败 | 多为 `gcc-multilib` 未装、include 路径不全、或头文件本身有编译错误。**关键副作用**：若 LP64 probe 失败的原因是头文件内 `_Static_assert(... == 32-bit-number)` 在 64 位编译时直接挂掉，**说明该 DAL 已有真实 ABI bug**，必须先修 |
| `abi.probe_compile_failed`（找不到 gcc） | warning | PATH 上无 gcc / cc / clang | 安装 `build-essential` 后重跑 |

**典型修法**（以 `dal_led.h` 为例）：

```bash
$ python wink-tools/wink.py lint --pack abi
error[abi.abi_assert_value_mismatch]: sizeof(dal_led_config_t) declared == 8
                                   but probe measured 16 on LP64
  --> dal/include/output/dal_led.h:50
error[abi.abi_assert_value_mismatch]: sizeof(dal_led_t) declared == 12
                                   but probe measured 24 on LP64
  --> dal/include/output/dal_led.h:52
```

→ 头文件 `dal_led.h:50-52` 的 `_Static_assert` 右值 8 / 12 是**手填时凭"末尾紧挨"直觉**得到的，**与 `owner` 指针在 64 位上 8B 翻倍的事实不符**。修法是改为探针给出的真实数字：

```c
#if INTPTR_MAX == INT32_MAX   /* ILP32: ESP32 xtensa, wasm32 */
_Static_assert(sizeof(dal_led_config_t) == 8,  "...");
_Static_assert(sizeof(dal_led_t)         == 12, "...");
#else                         /* LP64 / LLP64 */
_Static_assert(sizeof(dal_led_config_t) == 16, "...");
_Static_assert(sizeof(dal_led_t)         == 24, "...");
#endif
```

> **ILP32 数字可能仍然**靠手填**（若开发机无 32-bit multilib）**。此时 pack 只能验证 LP64。要拿 ILP64 数字请装 `gcc-multilib`（Debian/Ubuntu：`apt install gcc-multilib`；MSYS2：`pacman -S mingw-w64-i686-gcc`）后重跑。

**CI 集成建议**：在仓库根的 `wink.py test`（或等效的 CI 步骤）中加入：

```yaml
- name: ABI lint
  run: python wink-tools/wink.py lint --pack abi --strict
```

`--strict` 把 warning 也升级为 error，可在迁移期逐步推广（先 `--pack abi` 单跑无 strict，仅在 PR 模板中说明"必须 0 个 abi.abi_assert_value_mismatch"）。

### 2.4 动态内存与句柄分配规约

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-S-020 | SHOULD | `[LINT-ENFORCED]` (dal_struct.py) | 在 Full Profile 下，DAL 驱动 `init` 阶段 SHOULD NOT 进行堆内存动态分配（`malloc`）。所有驱动句柄与配置数据由调用方提供静态/栈存储空间，满足零开销与 POD 属性（ADR-0004） |
| DAL-S-021 | MUST | `[LINT-PARTIAL]` | 若底层硬件驱动（如 ESP32 RMT 传输通道）确需初始化堆内存，MUST 在头文件 API Contract 的 `@note` 处显式声明 `Eager-init Memory: Heap (X bytes)`，并在 `init` 阶段一次性分配完成（Eager Allocation），严禁在运行期 `read/write` 路径做隐式 `malloc` |
| DAL-S-022 | MUST | `[LINT-PARTIAL]` | 8 位 Micro Profile 的 Flash 常量 Zero-Copy 配置模式与严格 No-Malloc 禁令见扩展规范 [`dal-micro-profile-spec.md`](dal-micro-profile-spec.md) |

---

## 3. 生命周期 API

生命周期 API 按适用范围分为三档：**无条件必需**、**按 category 必需**、**推荐**。

### 3.1 无条件必需 (MUST — 所有驱动)

#### `dal_<type>_init`

```c
WINK_WARN_UNUSED_RESULT
wink_status_t dal_<type>_init(dal_<type>_t *dev, const dal_<type>_config_t *cfg);
```

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-L-001 | MUST | `[LINT-ENFORCED]` (dal_lifecycle.py) | MUST 校验 `dev` 和 `cfg` 非 NULL |
| DAL-L-002 | MUST | `[MANUAL-REVIEW]` | Full Profile 下 MUST 将 `cfg` 深拷贝到 `dev->config` |
| DAL-L-003 | MUST | `[LINT-ENFORCED]` (dal_lifecycle.py) | 成功时 MUST 置 `dev->initialized = true` |
| DAL-L-004` / `DAL-EC-010 | MUST | `[LINT-ENFORCED]` (dal_lifecycle.py) | 对已 `initialized` 的设备重复调用 MUST 返回 `WINK_ERR_ALREADY_INITIALIZED`（fail-fast，不隐式 deinit） |
| DAL-L-005 | MUST | `[LINT-PARTIAL]` | MUST 对关键配置参数做最小化防御校验（NULL 检查、引脚范围等），即使 codegen 已校验 |
| DAL-L-006 | MUST | `[MANUAL-REVIEW]` | **执行器**的 init MUST 使输出处于零能量状态（duty=0 / enable 引脚 inactive），严禁 init 即通电 |
| DAL-L-007 | MUST | `[MANUAL-REVIEW]` | `init` 失败时（如参数校验失败或硬件 claim 失败），MUST 将句柄清理回 `dev->initialized = false` 的可 safe-deinit 状态，严禁留下半初始化隐患 |
| DAL-L-008 | MUST | `[MANUAL-REVIEW]` | `init` 内部若在某一步失败，MUST 回滚释放本次 init 已成功 claim 的所有 PAL 资源（GPIO/PWM/Timer/I2C client 等），保证不产生资源泄漏。推荐实现模式为 goto-cleanup 链式回滚（见下方范例） |

**DAL-L-008 goto-cleanup 回滚范例**：

```c
wink_status_t dal_dc_motor_init(dal_dc_motor_t *dev, const dal_dc_motor_config_t *cfg) {
    // ... 参数校验 (DAL-L-001/005) ...
    wink_status_t rc = pal_pwm_init(cfg->pwm_a, ...);
    if (rc != WINK_OK) return rc;  // 无需回滚，尚未 claim 任何资源

    rc = pal_pwm_init(cfg->pwm_b, ...);
    if (rc != WINK_OK) goto cleanup_pwm_a;  // 回滚 pwm_a

    rc = pal_gpio_init(cfg->enable_pin, ...);
    if (rc != WINK_OK) goto cleanup_pwm_b;  // 回滚 pwm_b + pwm_a

    // ... 深拷贝 config (DAL-L-002) ...
    dev->initialized = true;  // DAL-L-003
    return WINK_OK;

cleanup_pwm_b:
    pal_pwm_deinit(cfg->pwm_b);
cleanup_pwm_a:
    pal_pwm_deinit(cfg->pwm_a);
    // dev->initialized 保持 false (DAL-L-007)
    return rc;
}
```

#### `dal_<type>_deinit`

```c
WINK_WARN_UNUSED_RESULT
wink_status_t dal_<type>_deinit(dal_<type>_t *dev);
```

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-L-010 | MUST | `[LINT-ENFORCED]` (dal_lifecycle.py) | MUST 幂等：未 init 时返回 `WINK_OK`，不 crash |
| DAL-L-011 | MUST | `[MANUAL-REVIEW]` | MUST 按 ADR-0024 清场顺序：禁用中断/ISR → 等待 in-flight 回调结束（见 DAL-L-012）→ 释放硬件资源 → 清零句柄（`memset` 清零，含 `initialized=false`） |
| DAL-L-012 | MUST | `[MANUAL-REVIEW]` | 若驱动使用 ISR，MUST 先禁中断 → 等待 in-flight 回调结束 → 再释放资源（防 use-after-deinit） |
| DAL-L-013 | MUST | `[MANUAL-REVIEW]` | 共享总线（I2C/SPI）的驱动 deinit MUST 仅释放自身 client claim，MUST NOT 销毁总线（bus-owner 管理） |
| DAL-L-014 | MUST | `[MANUAL-REVIEW]` | `deinit` 内部若底层 PAL/硬件清场失败，MUST 在函数内部输出 `LOGW` 日志痕迹，确保在调用方忽略返回值时故障依然可追溯 |
| DAL-L-015 | SHOULD | `[LINT-ENFORCED]` (dal_lifecycle.py) | `deinit` 返回非 `WINK_OK` 时，句柄 MUST 仍已完成 **best-effort 清场**（`initialized = false`，硬件资源尽力释放）。调用方 MUST NOT 对失败的 deinit 进行重试，也 MUST NOT 尝试继续使用该句柄。`deinit` 的返回值仅供诊断日志使用，不驱动恢复逻辑 |

### 3.2 按 category 必需 (MUST — 仅当 `is_actuator: true`)

#### `dal_<type>_safe_off`

```c
wink_status_t dal_<type>_safe_off(dal_<type>_t *dev);
```

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-L-020 | MUST | `[LINT-ENFORCED]` (dal_yaml_parity.py) | 仅当 YAML `is_actuator: true` 时 MUST 实现；`is_actuator: false` 的器件（button, encoder, eeprom, gps, ultrasonic）MUST NOT 实现空壳 `safe_off`（YAML `safe_off_fn: ""` 是正确表达） |
| DAL-L-021 | MUST | `[LINT-ENFORCED]` (dal_api_shape.py) | MUST 不标 `WINK_WARN_UNUSED_RESULT`（应急路径不强制检查返回值）；但返回 `wink_status_t` 以报告成功/失败 |
| DAL-L-022` / `DAL-E-001 | MUST | `[LINT-ENFORCED]` (dal_lifecycle.py) | MUST 幂等 + 未初始化时安全返回 `WINK_OK`（设计理由见下方「safe_off 未初始化返回值」） |
| DAL-L-023 | MUST NOT | `[MANUAL-REVIEW]` | MUST NOT 依赖调度器与堆 |
| DAL-L-024 | SHOULD | `[MANUAL-REVIEW]` | SHOULD 满足 ISR-safe（见 [§6](#6-并发isr-与线程安全)） |
| DAL-L-025 | MUST | `[MANUAL-REVIEW]` | `safe_off` 绑定的具体关断原语由 ADR-0048 逐器件裁决（如 dc_motor 绑定 brake，rc_servo 绑定 duty=0），MUST 在头注释声明具体行为 |
| DAL-L-030 | MUST | `[LINT-ENFORCED]` (dal_lifecycle.py) | `deinit` 执行完毕后调用 `safe_off` MUST 为 no-op（句柄已清零状态）且返回 `WINK_OK`，严禁产生空指针解引用或二次报错 |
| DAL-L-031 | MUST | `[MANUAL-REVIEW]` | `safe_off` 调用后继续调用 `deinit` MUST 仍能完成完整 deinit 资源清理语义（即使硬件输出已物理切断，底层资源与句柄状态 MUST 正常释放） |

**设计理由：为什么 `safe_off` 未初始化必须返回 `WINK_OK`（DAL-L-022）**

这条规则不是洁癖，而是由真实故障消费链路决定的：

1. **主消费方是系统级批量关断。** `wink_actuator_safe_off_all()`（`runtime/src/wink_actuator_registry.c`）在 watchdog / panic / assert 失败 / 异常回滚路径被调用，遍历注册表逐个调 `safe_off`；单个返回错误码时会 `wink_trace_fault(7000u + i)` 记一条故障码，**然后继续遍历其余执行器**。
2. **未初始化是该路径下的合法态。** 故障可能发生在启动早期（执行器尚未 init 完），或执行器已 deinit 但其 registry 条目因生命周期竞态尚未摘除。此时该执行器本就无能量输出——「没东西可关」在应急关断语义里**就是成功**，不是错误。
3. **返回错误码只会制造假阳性。** 若此时返回 `WINK_ERR_NOT_INITIALIZED`，系统在已经处理一个真实故障的同时，会凭空刷出一条 7000 段 actuator fault，干扰根因诊断、可能掩盖真正的故障。应急路径应当「尽力关、少报噪」。
4. **与 teardown 语义族保持一致。** `deinit` 对未初始化句柄同样返回 `WINK_OK`（DAL-L-010 幂等）；`safe_off` 与 `deinit` 同属关断语义，应对齐。
5. **它不承担「检测忘记 init」的职责。** `safe_off` 依 DAL-L-021 不标 `WINK_WARN_UNUSED_RESULT`，返回值本就不驱动任何恢复逻辑，因此返回 `NOT_INITIALIZED` 对帮助程序员发现忘记 init 毫无作用——忘记 init 的真实症状是「器件从不响应控制」，应由带 `WARN_UNUSED_RESULT` 的 `init` 返回值与开发期 assert 来捕获，而不是在 best-effort 的应急关断里兜底。

> 反过来说：若想在开发期暴露「对未初始化句柄调 safe_off」这类编程错误，正确手段是 `WINK_ASSERT` 之类的**调试期检查**，而非改变运行期返回码。生产路径仍返回 `WINK_OK`。

### 3.3 推荐 (SHOULD)

| API | 级别 | 说明 |
|-----|------|------|
| `dal_<type>_reset(dev)` | SHOULD | 软件复位，重置器件内部状态机与缓存。语义因器件而异（encoder 的 reset 含义是计数清零，不是硬件复位）。如实现，MUST 在头注释明确语义 |
| `dal_<type>_get_state(dev, *out_state)` | SHOULD | 返回器件统一状态枚举。如实现，签名 MUST 为 `wink_status_t dal_xxx_get_state(const dal_xxx_t *dev, dal_xxx_state_t *out_state)`，不得直接返回枚举值 |
| `dal_<type>_self_test(dev, *out_result)` | MAY | 触发硬件自检（如 IMU 内建自检模式、EEPROM 读写校验、电机编码器回读）。如实现，签名 MUST 为 `wink_status_t dal_xxx_self_test(dal_xxx_t *dev, dal_xxx_self_test_result_t *out_result)`，MUST 标 `WINK_BLOCKING`，MUST 在头注释声明自检内容与预期耗时 |

---

## 4. 函数签名与返回值契约

### 4.1 返回值

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-F-001 | MUST | `[LINT-ENFORCED]` (dal_api_shape.py) | 所有公开 DAL API MUST 返回 `wink_status_t`（`WINK_OK` 为 0，负数为错误），ADR-0001 |
| DAL-F-002 | MUST | `[LINT-ENFORCED]` (dal_api_shape.py) | MUST NOT 直接返回 `bool` 作为公开 DAL API 的返回值（lint: `STATUS-NOT-BOOL-PUBLIC`） |
| DAL-F-003 | MUST | `[LINT-PARTIAL]` | 布尔谓词查询 MUST 通过出参传递：`wink_status_t dal_xxx_is_pressed(const dal_xxx_t *dev, bool *out_pressed)` |
| DAL-F-004 | MUST | `[LINT-ENFORCED]` (dal_api_shape.py) | 除以下豁免白名单外，所有返回 `wink_status_t` 的公开 API MUST 标注 `WINK_WARN_UNUSED_RESULT` |

**DAL-F-004 豁免白名单**：

| 豁免 API | 理由 |
|---------|------|
| `safe_off` | 应急路径不强制检查返回值 |
| `poll` | 每 tick 调用的状态机推进函数，大部分调用点有意忽略返回值（"推进一下，失败了下次再推"）；错误通过 `get_status` 查询。强制检查会在事件循环中制造告警噪音 |
| `deinit` | best-effort 清场，返回值仅供诊断日志（见 DAL-L-015） |

> **注意**：`toggle` 等操作类 API 不在豁免名单中——`toggle` 失败（如 `ERR_NOT_INITIALIZED`）应被检查。`poll` 的返回值对单测和故障诊断仍有价值（`ERR_NOT_INITIALIZED`, `ERR_DISCONNECTED`），白名单仅豁免 `WINK_WARN_UNUSED_RESULT` 属性，不改变返回类型。

### 4.2 参数约定

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-F-010 | MUST | `[LINT-ENFORCED]` (dal_api_shape.py) | 第一参数 MUST 为实例句柄指针 |
| DAL-F-011 | MUST | `[LINT-ENFORCED]` (dal_api_shape.py) | **不修改设备状态**的查询类 API（getter / 谓词 / get_state）MUST 使用 `const dal_<type>_t *dev` |
| DAL-F-012 | MUST | `[LINT-ENFORCED]` (dal_api_shape.py) | **修改设备状态**的操作类 API MUST 使用 `dal_<type>_t *dev`（非 const） |
| DAL-F-013 | MUST | `[LINT-ENFORCED]` (dal_api_shape.py) | 出参指针 MUST 以 `out_` 前缀命名（如 `bool *out_pressed`, `float *out_speed`） |
| DAL-F-014 | SHOULD | `[LINT-PARTIAL]` | `init` 的第二参数 SHOULD 为 `const dal_<type>_config_t *cfg` |

### 4.3 错误返回时出参状态契约

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-F-020 | MUST | `[MANUAL-REVIEW]` | 公开 API 返回非 `WINK_OK` 时，所有 `out_*` 出参 MUST 保持调用前的值不变（MUST NOT 清零或写入半成品数据）。实现方式：出参只在函数末尾、确定返回 `WINK_OK` 前一次性写入 |
| DAL-F-021 | MUST | `[MANUAL-REVIEW]` | 调用方 MUST NOT 在错误返回路径读取 out 参数的值 |
| DAL-F-022 | SHOULD | `[MANUAL-REVIEW]` | 调用方 SHOULD 在调用前将结构体出参清零（`= {0}`），避免错误路径读到未初始化的栈垃圾 |

### 4.4 `apply_override` 技术债声明

`dal_rc_servo_apply_override(void *dev, ...)` 和 `dal_ultrasonic_apply_override(void *dev, ...)` 的 `void *dev` 是为适配统一函数指针表 `wink_dev_override_fn` 的已知技术债（违反 DAL-F-010），列入合规矩阵例外。收敛计划：未来 `wink_dev_override_fn` 类型参数化后消除 `void *`。

### 4.5 Micro Profile (8-bit) 函数分发与重入契约

在 8051 / Keil C51 环境下，编译器默认使用静态覆盖分析（Overlay Analysis）分配局部变量内存。函数指针与泛型 `void *` 指针会导致分析失效，强迫参数压入极其狭小的 Hardware Stack。

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-8B-F-001 | MUST | `[LINT-ENFORCED]` (dal_api_shape.py) | 8 位 Profile 下 **MUST NOT** 使用 `void *dev` 虚分发与函数指针表 (Function Pointer Tables) |
| DAL-8B-F-002 | MUST | `[LINT-PARTIAL]` | 驱动方法 MUST 为具名静态函数或内联函数（如 `dal_led_on(dal_led_8b_t *dev)`——**函数名去 `_8b_`，仅句柄类型保留 `_8b_t`**，见 [ADR-0056](../../../docs/design/decisions/0056-cross-profile-quantity-ab-class-and-scaled-integers.md) §4），允许编译器进行完整的 Overlay 覆盖分析 |
| DAL-8B-C-001 | MUST | `[LINT-PARTIAL]` | 8 位 Profile 下不使用原子指令，临界区保护统一使用 `EA` 保存/恢复宏：<br/>`#define WINK_8B_CRITICAL_ENTER() do { uint8_t _ea_save = EA; EA = 0;`<br/>`#define WINK_8B_CRITICAL_EXIT() EA = _ea_save; } while(0)` |
| DAL-8B-C-002 | MUST | `[MANUAL-REVIEW]` | 临界区内部 **MUST NOT** 调用含有耗时 busy-wait 或复杂状态机推进的代码 |
| DAL-8B-C-010 | MUST | `[MANUAL-REVIEW]` | 若某个 DAL API 既可能在 ISR 中被调用，又可能在主循环 (Task) 中被调用，在 Keil C51 环境下该 API **MUST** 加上 `reentrant` 关键字声明，或设计为完全无局部变量的内联宏 |

---

## 5. 动词语义模型与命名规范

### 5.1 函数命名格式

```
dal_<type>_<verb>[_<object>](dal_<type>_t *dev, ...)
```

- `<type>`：器件类型名，与 YAML `type` 字段和目录名一致（`led`, `dc_motor`, `rc_servo`, `button`, `encoder`, `ultrasonic`, `mono_oled`, `eeprom`, `gps`）
- `<verb>`：来自本规范允许的动词集
- `<object>`：可选的操作对象，用于细化语义（如 `set_speed`, `get_count`, `draw_text`）

### 5.2 动词语义三元模型

本仓库刻意区分三种数据访问语义，新增驱动 MUST 遵循：

| 前缀 | 语义 | 是否碰硬件 | 是否可能阻塞 | 示例 |
|------|------|-----------|-------------|------|
| `read_*` | 触发一次物理采样并返回结果 | ✅ | 可能 | `dal_ultrasonic_read(dev, &cm)` |
| `get_*` | 读取已缓存的值或最后设定值，不触发硬件 | ❌ | ❌ | `dal_ultrasonic_get_cached_distance(dev, &cm)`, `dal_dc_motor_get_speed(dev, &speed)` |
| `get_state` / `get_status` | 查询驱动/器件状态机 | ❌ | ❌ | `dal_eeprom_get_status(dev, &state)` |

> **关键契约**: `read_*` 调用后硬件状态改变（已触发测量/采样），`get_*` 调用后硬件状态不变。这是 API 使用者的核心假设，破坏它是安全隐患。

### 5.3 按 category 分组的标准动词库

分组直接复用 codegen YAML 的 `category` 字段（7 类：`actuator`, `output`, `input`, `sensor`, `display`, `storage`, `comm`），不引入新的分类词。

#### 5.3.1 执行器与输出类 (category: actuator / output)

涵盖器件：LED, Relay, DC Motor, RC Servo, Stepper Motor, Buzzer.

| 动词 | 签名范式 | 适用场景 | 现存示例 |
|------|---------|---------|---------|
| `on` | `(dev)` | 开启/点亮 | `dal_led_on(dev)` |
| `off` | `(dev)` | 关闭/熄灭 | `dal_led_off(dev)` |
| `toggle` | `(dev)` | 翻转开关状态 | `dal_led_toggle(dev)` |
| `set` | `(dev, bool on)` | 显式设置开关 | `dal_led_set(dev, true)` |
| `set_<property>` | `(dev, <type> val)` | 设置物理量。新增 A 类执行器命令用定标整数（§9.4，如 `set_speed_promille(dev, -500)`）；下表 float 范例为 stable 驱动迁移前现状 | `dal_dc_motor_set_speed(dev, 0.8f)`（stable，迁移前），`dal_rc_servo_set_angle(dev, 90.0f)`（experimental，待整型化） |
| `get_<property>` | `(const dev, <type> *out_val)` | 读回最后设定值（不碰硬件） | `dal_dc_motor_get_speed(dev, &speed)` |
| `is_<pred>` | `(const dev, bool *out_pred)` | 布尔状态查询 | — |
| `brake` | `(dev)` | 执行器制动（电机专用，ADR-0048） | `dal_dc_motor_brake(dev)` |
| `coast` | `(dev)` | 执行器滑行/自由转动 | `dal_dc_motor_coast(dev)` |
| `safe_off` | `(dev)` | 应急关断（§8） | `dal_dc_motor_safe_off(dev)` |

**黑名单**（禁用词）：❌ `turn_on`, `enable_output`, `run_motor`, `spin`, `start_pwm`

#### 5.3.2 传感器与输入类 (category: sensor / input)

涵盖器件：Ultrasonic, Button, Encoder, Temp/Humidity, IMU, Photoelectric.

| 动词 | 签名范式 | 适用场景 | 现存示例 |
|------|---------|---------|---------|
| `read` | `(dev, *out)` | 同步阻塞读取（MUST 标 `WINK_BLOCKING`） | `dal_ultrasonic_read(dev, &cm)` |
| `read_<metric>` | `(dev, *out)` | 读取具体物理量 | `dal_ultrasonic_read_cm(dev, &cm)` |
| `request_<op>` | `(dev)` | 非阻塞请求（三段式首步） | `dal_ultrasonic_request_measurement(dev)` |
| `poll` | `(dev)` | 推进状态机 | `dal_button_poll(dev)`, `dal_gps_poll(dev)` |
| `get_cached_<metric>` | `(const dev, *out)` | 读取缓存结果（不碰硬件） | `dal_ultrasonic_get_cached_distance(dev, &cm)` |
| `get_<property>` | `(const dev, *out)` | 读取缓存值 | `dal_encoder_get_count(dev, &count)`, `dal_button_get_edge_count(dev, &count)` |
| `is_<condition>` | `(const dev, bool *out)` | 快速条件判断 | `dal_button_is_pressed(dev, &pressed)` |
| `was_<condition>` | `(dev, bool *out)` | 边沿事件检测（读后清） | `dal_button_was_pressed(dev, &pressed)` |
| `calibrate` / `zero` | `(dev)` | 校准/清零 | `dal_encoder_reset(dev)` |
| `reset_<counter>` | `(dev)` | 清零计数器 | `dal_button_reset_edge_count(dev)` |

**黑名单**：❌ `fetch_data`, `sample_now`, `get_dist`（缩写不清晰）

> **注意**：`get_value` 不再列入黑名单。`get_*` 系列在本仓库有明确语义（读缓存/读设定值），是合法动词。

> **`was_*` 原子性要求 (DAL-V-010)**：`was_*` 类读后清 API 的内部 "读取+清除" 操作 MUST 是原子的（使用 PAL 临界区或原子 compare-and-swap），防止 SMP 双核或 ISR 并发场景下事件丢失。在 8 位 Micro Profile 下使用 `WINK_8B_CRITICAL_ENTER/EXIT` 宏保护。

#### 5.3.3 显示屏类 (category: display)

涵盖器件：Mono OLED, TFT LCD, Segment LED, E-Paper.

| 动词 | 签名范式 | 适用场景 | 现存示例 |
|------|---------|---------|---------|
| `clear` | `(dev)` | 清空显存 | `dal_mono_oled_clear(dev)` |
| `flush` | `(dev)` | 将显存刷入物理屏幕 | `dal_mono_oled_flush(dev)` |
| `draw_<shape>` | `(dev, ...)` | 绘制基础图形 | `dal_mono_oled_draw_text(dev, col, page, str)` |
| `set_brightness` | `(dev, level)` | 设置背光/亮度 | — |

**黑名单**：❌ `clean_screen`, `refresh_display`, `light_on`

#### 5.3.4 通信与存储类 (category: storage / comm)

涵盖器件：EEPROM, SPI Flash, GPS, LoRa, CAN Node.

| 动词 | 签名范式 | 适用场景 | 现存示例 |
|------|---------|---------|---------|
| `read` / `read_blocking` | `(dev, addr, buf, len)` | 阻塞读取数据块 | `dal_eeprom_read_blocking(dev, addr, buf, len)` |
| `write` / `write_blocking` | `(dev, addr, buf, len)` | 阻塞写入数据块 | `dal_eeprom_write_blocking(dev, addr, buf, len)` |
| `request_read` / `request_write` | `(dev, addr, len)` | 非阻塞请求（三段式） | `dal_eeprom_request_read(dev, addr, len)` |
| `poll` | `(dev)` | 推进状态机 | `dal_eeprom_poll(dev)`, `dal_gps_poll(dev)` |
| `get_<result>` | `(dev, *buf, len)` | 获取异步操作结果 | `dal_eeprom_get_read_result(dev, buf, len)` |
| `get_status` | `(const dev, *out)` | 查询操作状态 | `dal_eeprom_get_status(dev, &state)` |
| `get_position` | `(const dev, *out)` | GPS 定位结果 | `dal_gps_get_position(dev, &pos)` |
| `erase` | `(dev, addr, len)` | 擦除存储扇区 | — |

### 5.4 器件特有 API

当器件有不属于上述标准动词的特有能力时（如 CAN 过滤器配置、IMU 滤波深度）：

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-V-001 | MUST | `[LINT-PARTIAL]` | 使用具名 typed API：`dal_<type>_<specific_verb>(dev, const dal_<type>_<arg>_t *arg)` |
| DAL-V-002 | MUST | `[LINT-ENFORCED]` (dal_yaml_parity.py) | 在 YAML 中标记 `device_specific: true`，使其不进入通用 role verb 平面 |
| DAL-V-003 | MUST NOT | `[LINT-ENFORCED]` (dal_api_shape.py) | MUST NOT 使用 `control(cmd, void *arg)` 形式的 IOCTL 窗口 — 它摧毁类型安全与 codegen 可分析性（违背 ADR-0004 静态分发精神） |

---

## 6. 并发、ISR 与线程安全

本章是安全相关的硬性条款。ESP32-S3 是双核 SMP，`volatile` 不产生 acquire/release 内存屏障。

### 6.0 Task-to-Task 并发默认契约

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-C-040 | MUST | `[MANUAL-REVIEW]` | DAL 实例默认**非线程安全**：同一 `dal_xxx_t *dev` 的方法调用、以及 init/deinit 与其他方法之间，MUST 由调用方在外部串行化（同一 mutex / 同一 task / 消息队列） |
| DAL-C-041 | MUST | `[MANUAL-REVIEW]` | 仅当驱动在头注释显式声明 `Thread-safe: Yes` 并说明锁/无锁机制时，才允许对同一实例并发调用 |
| DAL-C-042 | MUST | `[LINT-ENFORCED]` (dal_contract_doc.py) | `Thread-safe` Contract 字段缺失时默认按 `No` 解释，lint SHOULD 对公开 API 缺该字段报 warning |
| DAL-C-043 | MAY | `[MANUAL-REVIEW]` | **不同** `dal_xxx_t` 实例（不同 dev 指针）之间 MAY 并发调用，前提是它们不共享底层资源（如同一 I2C 总线的两个设备仍需外部串行化——由 PAL 总线锁保证） |

### 6.1 volatile 使用约束

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-C-001 | MUST | `[MANUAL-REVIEW]` | 跨 ISR/跨核共享字段仅允许 **"单字宽 + 单写者 + 读者容忍旧值"** 模式 |
| DAL-C-002 | MUST | `[LINT-ENFORCED]` (dal_concurrency.py) | 任何对 volatile 字段的 read-modify-write（如 `count += delta`）MUST 使用 PAL 原子操作或临界区（`PAL_CRITICAL_SECTION`） |
| DAL-C-003 | MUST NOT | `[MANUAL-REVIEW]` | MUST NOT 仅凭 `volatile` 声明就在注释中写"无需临界区"，除非满足 DAL-C-001 的三个条件 |

### 6.2 多字段快照一致性

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-C-010 | MUST | `[MANUAL-REVIEW]` | 若驱动句柄含多个跨 ISR/跨核共享的 volatile 字段（如 `ultrasonic` 的 `last_distance` + `last_pulse_us` + `last_status` + `state`），MUST 在头注释中声明读取顺序契约（先读 payload 后读 state，或反之），或使用以下方案之一确保快照一致性 |

推荐方案（按复杂度排列）：

1. **读序契约 + 状态版本号**（最简单）：在头注释声明"先读 state，若 == READY 再读 payload；payload 有效性以读到 state==READY 为准"，并在 ISR 写端保持"先写 payload 后写 state"的顺序。
2. **单原子快照结构 + 版本号**：将相关字段打包为一个结构体，用 `_Atomic` 或临界区做整体交换。
3. **seqlock**：读端循环检查序列号，写端 ISR 在修改前后递增序列号。

### 6.3 ISR 安全

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-C-020 | MUST | `[LINT-ENFORCED]` (dal_concurrency.py) | ISR 上下文 MUST NOT：分配/释放内存、取互斥锁、调用日志 API、执行阻塞操作 |
| DAL-C-021 | SHOULD | `[LINT-ENFORCED]` (dal_concurrency.py) | 可在 ISR 上下文安全调用的 API 必须在其内部调用的 PAL/HAL 原语全量落在 **PAL ISR 安全白名单** 内（如 `pal_gpio_write` 允许；`pal_pwm` / `pal_i2c` / `pal_os_mutex` 严禁）。若执行器 `safe_off` 内部涉及非 ISR 安全调用（如 ESP-IDF `ledc_stop`），该 API **MUST NOT** 标注为 ISR-safe |
| DAL-C-022 | MUST | `[LINT-ENFORCED]` (dal_contract_doc.py) | API Contract 注释的 `ISR-safe` 字段 MUST 如实声明（`No` / `Yes`） |

### 6.4 回调上下文归属

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-C-030 | MUST | `[MANUAL-REVIEW]` | 驱动的事件回调在哪个上下文（ISR / task / poll 循环）中被调用 MUST 在头注释中明确声明 |
| DAL-C-031 | MUST | `[MANUAL-REVIEW]` | 声明回调内允许调用哪些类别的 API（如"允许 WINK_BLOCKING" 或 "仅允许 ISR-safe API"） |

**现存范例**：`dal_button_event_cb` 的注释明确声明"在 `dal_button_poll()` 的 task 上下文同步调用，非 ISR；允许调用 `WINK_BLOCKING` API 但建议保持短小"。新增驱动的回调 MUST 达到相同声明精度。

### 6.5 deinit 与 ISR 竞态

见 [§3.1 DAL-L-011/012](#31-无条件必需-must--所有驱动)：若驱动使用 ISR，deinit MUST 按顺序 **先禁中断 → 等待 in-flight 回调结束 → 释放硬件资源 → 清零句柄（含 initialized=false）**。

---

## 7. 阻塞、超时与异步模式

### 7.1 阻塞 API 标注

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-B-001 | MUST | `[LINT-ENFORCED]` (dal_blocking.py) | 任何可能阻塞调用者的 API MUST 同时满足：(a) 函数名带 `_blocking` 后缀或名字本身暗示阻塞（如 `read`）；(b) 标注 `WINK_BLOCKING` 属性宏 |
| DAL-B-001a | MUST | `[LINT-PARTIAL]` | **命名选择规则**：当同一器件**同时**提供阻塞与非阻塞两种形态时，阻塞变体 MUST 使用 `_blocking` 后缀以明确区分（如 `dal_eeprom_read_blocking` vs `dal_eeprom_request_read`）；当该操作**仅存在阻塞形态**时，允许使用裸动词（如 `read`）。历史遗留：`dal_ultrasonic_read` 同时存在阻塞 `read` 与非阻塞 `request_measurement`，按新规应命名为 `read_blocking`；因其属公开 API，不做破坏性改名，计划在迁移期为其补 `@deprecated`/`WINK_DEPRECATED` 别名并纳入退役轨道（跟踪号待定） |
| DAL-B-002 | MUST | `[LINT-ENFORCED]` (dal_blocking.py) | 有 `_blocking` 后缀但未标 `WINK_BLOCKING`，或标了 `WINK_BLOCKING` 但无后缀，均为 lint error |
| DAL-B-003 | MUST | `[LINT-ENFORCED]` (dal_contract_doc.py) | API Contract 注释的 `Blocking` 字段 MUST 给出最坏阻塞时间的**数值上界**（如 "Yes, worst-case ≈ 38ms"），不得仅写 "Yes(ms)" 占位 |
| DAL-B-004 | MUST | `[LINT-ENFORCED]` (dal_blocking.py) | 阻塞 API MUST 在 `#ifndef WINK_STRICT_NONBLOCKING` 守卫内声明（ADR-0017 层 2 隔离） |

**现存范例**：

```c
WINK_BLOCKING WINK_WARN_UNUSED_RESULT
wink_status_t dal_ultrasonic_read(dal_ultrasonic_t *dev, float *distance_cm);
// Blocking: Yes. Worst-case ≈ 60ms+.

WINK_BLOCKING WINK_WARN_UNUSED_RESULT
wink_status_t dal_eeprom_read_blocking(dal_eeprom_t *dev, uint32_t addr, uint8_t *buf, uint32_t len);

WINK_BLOCKING WINK_WARN_UNUSED_RESULT
wink_status_t dal_gps_init_blocking(dal_gps_t *dev, const dal_gps_config_t *cfg);
```

### 7.2 超时来源与延时规约

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-B-010 | MUST | `[LINT-PARTIAL]` | 超时值 MUST 来自 config 字段或编译期常量宏，MUST NOT 硬编码在函数体内 |
| DAL-B-011 | MUST | `[LINT-PARTIAL]` | 非阻塞 API MUST NOT 内部 busy-wait 超过 100μs，否则必须改为 request/poll 三段式 |
| DAL-B-012 | MUST | `[LINT-ENFORCED]` (dal_blocking.py) | 微秒/毫秒级长等待 **MUST NOT** 使用与 CPU 主频相关的裸空循环 busy-wait（如 `for(int i=0; i<N; i++)`），必须统一调用 PAL 时钟/延时原语（`pal_delay_us()` / `pal_os_get_ms()`），防止跨芯片主频产生耗时漂移。<br/>**豁免条款**：10μs 以下的亚微秒/微秒级确定性硬件脉冲（如 1-Wire 1μs 复位脉冲、HC-SR04 10μs 触发脉冲、SPI CS 建立时间），允许使用 PAL 封装的 `pal_nop_n(n)` 或编译期 barrier 原语，避免 `pal_delay_us(1)` 函数开销引入大的时序抖动 |
| DAL-B-013 | SHOULD | `[MANUAL-REVIEW]` | 阻塞 API 的 worst-case 超时 SHOULD 显著小于系统 Task Watchdog Timer (TWDT) 窗口（建议 ≤ TWDT × 50%）。API Contract 注释的 `Blocking` 字段 SHOULD 同时声明与 TWDT 的关系（如 "worst-case 60ms, TWDT-safe at default 5s window"）。若阻塞时间可能接近或超过 TWDT 窗口，MUST 在文档中明确警告调用方需调整 TWDT 配置或使用异步三段式替代 |
| DAL-B-014 | SHOULD | `[LINT-PARTIAL]` | `init` 函数耗时超过 **100ms** 的（如 GPS 初始化序列、OLED 复位、EEPROM 自检），SHOULD 标注 `WINK_BLOCKING` 并在函数名使用 `_blocking` 后缀（如 `dal_gps_init_blocking`）。API Contract 注释 SHOULD 声明 `Init-time budget: ≤ Xms`，使系统启动时间可预测 |

### 7.3 DMA 与共享 Buffer 归属契约 (Buffer Ownership & DMA Constraints)

在包含 DMA 背景传输（如 SPI Flash、RMT 脉冲捕获/发送、Display 刷新）的驱动中，内存与 Buffer 的生命周期管理是安全隐患的高发区。

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-BUF-001 | MUST | `[MANUAL-REVIEW]` | **Buffer 持有生命周期**：在异步三段式（`request_*` / `get_result`）或异步传输中，调用方传入的 Buffer MUST 在 `get_result` 返回或回调完成前由调用方保持有效。驱动内部 MUST NOT 隐式做异步深拷贝 |
| DAL-BUF-002 | MUST | `[MANUAL-REVIEW]` | **DMA 内存域与对齐**：支持 DMA 传输的 DAL 驱动（如 SPI Flash、Mono OLED），其 Buffer MUST 满足硬件平台对齐与内存域限制。在 ESP32-S3 等芯片上，DMA Buffer MUST 分配在 Internal SRAM（`DRAM_ATTR` 或 `MALLOC_CAP_DMA`），**严禁直接对 PSRAM 或栈上 Buffer 启动 DMA 硬件传输** |
| DAL-BUF-003 | MUST | `[MANUAL-REVIEW]` | 驱动在启动 DMA 传输前后，MUST 显式调用 PAL Cache 原语（如 `pal_cache_msync` / Cache Line Flush/Invalidate），确保 CPU 缓存与物理 RAM 之间的数据一致性 |

### 7.4 异步三段式 (request / poll / get_result)

本仓库的主流异步模式是三段式状态机，优先于回调模式，契合协作式调度与双 target 同源。

#### 命名契约

```
dal_<type>_request_<op>(dev, ...)   → 提交请求，立即返回
dal_<type>_poll(dev)                → 每 tick 调用，推进状态机
dal_<type>_get_<op>_result(dev, *out) / dal_<type>_get_cached_<metric>(dev, *out) → 获取结果
dal_<type>_get_status(dev, *out_state) → 查询状态机
```

#### 状态机基线

```
IDLE ──request──▶ BUSY ──[完成]──▶ DONE/READY ──get_result──▶ IDLE
                    │                                            ▲
                    ├──[失败]──▶ ERROR ──get_result──────────────┘
                    │
                    └──request──▶ return WINK_ERR_BUSY (不改变状态)
```

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-B-020 | MUST | `[MANUAL-REVIEW]` | 状态值域 MUST 包含：`IDLE` / `BUSY` / `DONE` (或 `READY`) / `ERROR` |
| DAL-B-021 | MUST | `[MANUAL-REVIEW]` | BUSY 时重复 `request_*` MUST 返回 `WINK_ERR_BUSY`，不改变状态 |
| DAL-B-022 | MUST | `[MANUAL-REVIEW]` | `poll` 在 IDLE / DONE / ERROR 时 MUST 为 no-op |
| DAL-B-023 | MUST | `[MANUAL-REVIEW]` | `get_*_result` 成功读取后 MUST 将状态机重置为 IDLE |
| DAL-B-024 | MUST | `[MANUAL-REVIEW]` | 三段式的 `get_cached_*` / `get_*_result` 在从未执行过 `request_*` 时（state == IDLE），MUST 返回 `WINK_ERR_NO_DATA` 或 `WINK_ERR_EMPTY`，MUST NOT 返回 `WINK_OK` 或 `WINK_ERR_BUSY`（`BUSY` 仅保留给传输中）。这确保调用方不会误读初始化时的零值为有效测量结果 |
| DAL-B-026 | MUST | `[LINT-PARTIAL]` | 当状态机处于 `ERROR` 崩溃态时，再次调用 `request_*` MUST 触发恢复迁移（`ERROR → IDLE → BUSY`），尝试重新建立通信与硬件发请求；若重新发起通信再次失败，MUST 将状态重新迁回 `ERROR`。严禁在进入 `ERROR` 后无限期锁死在 `ERROR` 导致系统无法自愈 |

**现存范例**：`dal_eeprom` (request_read/request_write → poll → get_status/get_read_result) 和 `dal_ultrasonic` (request_measurement → get_cached_distance) 是已验证的参考实现。

#### `poll` 返回值语义 (DAL-B-025)

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-B-025 | MUST | `[MANUAL-REVIEW]` | `poll` 返回值语义 MUST 遵循以下约定：|

| 返回值 | 含义 |
|--------|------|
| `WINK_OK` | 状态机正常推进（含 no-op 空转，如 IDLE/DONE/ERROR 状态下的合法调用） |
| `WINK_ERR_NOT_INITIALIZED` | 句柄未初始化 |
| `WINK_ERR_NO_DATA` | 当前无新数据或状态无变化（可选，供诊断使用；调用方 MAY 忽略） |
| 其他负数错误码 | 状态机内部遇到不可恢复错误；驱动 MUST 同时将内部 state 迁移到 ERROR，后续可通过 `get_status` 查询 |

> **注意**：`poll` 的返回值已被 DAL-F-004 豁免 `WINK_WARN_UNUSED_RESULT`，但返回值语义依然有意义——单测和故障诊断依赖此契约。

### 7.5 与仿真 Asyncify 的关系

阻塞 API 在 Wasm target 下通过 Asyncify 挂起/恢复实现。DAL 作者约束：

- 阻塞函数体内 MUST NOT 使用裸汇编或平台特定的等待原语
- MUST 通过 PAL 层的 `pal_delay_ms` / `pal_os_sleep_ms` / semaphore 实现等待
- 详见 `reviews/2026-06-24-phase1-asyncify-deep-dive`

---

## 8. 失效安全与应急路径

### 8.1 safe_off 语义裁决

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-E-001 | MUST | `[LINT-ENFORCED]` (dal_yaml_parity.py) | `safe_off` 不是独立的新语义，它是"绑定到某个具体关断原语"。头注释 MUST 声明具体行为 |
| DAL-E-002 | MUST | `[MANUAL-REVIEW]` | `safe_off` 会在 watchdog / panic / assert 失败 / 异常回滚路径被调用，因此 MUST 尽量简单、确定性好 |

**safe_off 的两种代码形态**：

| 形态 | 说明 | 示例 |
|------|------|------|
| **具名 safe_off API** | 驱动暴露独立的 `dal_xxx_safe_off` 函数符号 | `dal_dc_motor_safe_off`, `dal_rc_servo_safe_off` |
| **YAML 绑定的关断函数** | 驱动无独立 safe_off 符号，由 `config.safe_off_fn` 指向某个已有原语，codegen/actuator_registry 消费 | led → `safe_off_fn: dal_led_off`（头文件中不存在 `dal_led_safe_off` 符号） |

两者都是 DAL-E-001 意义上的"绑定到具体关断原语"，但代码形态不同。

**现存语义**（ADR-0048 裁决）：

| 器件 | safe_off 绑定 | 形态 | 物理后果 |
|------|-------------|------|--------|
| dc_motor | brake (H 桥短接制动) | 具名 API | 电机急停 |
| rc_servo | duty=0 (失去保持力) | 具名 API | 舵机 limp = 安全 |
| led | `dal_led_off` | YAML 绑定 | LED 熄灭 |

### 8.2 系统级 safe-all

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-E-010 | SHOULD | `[MANUAL-REVIEW]` | 执行器 SHOULD 注册到 `wink_actuator_registry`，使系统能在故障时一次性遍历所有执行器 `safe_off` |

### 8.3 Init 零能量

见 [DAL-L-006](#31-无条件必需-must--所有驱动)：执行器 init 成功后输出 MUST 处于零能量状态。严禁 init 即通电或保持上一次 duty。

**零能量与 Init-to-Ready 的关系**：执行器 init 成功后即**立即接受控制指令**（Init-to-Ready），零能量只是默认输出值而非额外的使能闸门。MUST NOT 出于安全考虑引入 `enable()` / `arm()` 前置调用——否则破坏 Init-to-Ready 契约（DAL-BC-001）。安全关断由 `safe_off` 承担，而非 init 后的待使能态。

---

## 9. 单位、量纲与值域

> **本章核心（[ADR-0056](../../../docs/design/decisions/0056-cross-profile-quantity-ab-class-and-scaled-integers.md)）**：DAL 物理量按数据流向与硬件终态分 **A 类（执行器命令）/ B 类（传感器测量）** 两类。**A 类在所有 Profile（含 32 位 Full）MUST 用定标整数，MUST NOT 用 float**；B 类 Full 用 float、Micro 用定点，差异由 codegen binding 吸收。浮点属于 BAL 的数学域（PID / 滤波），在 BAL→DAL 边界完成 float→定标整数转换。

### 9.1 参数命名与封闭单位后缀表

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-U-001 | MUST | `[LINT-ENFORCED]` (dal_quantity.py) | 所有物理量参数与出参名 MUST 带单位后缀 |
| DAL-U-002 | MUST | `[LINT-PARTIAL]` | 无量纲归一化参数 MUST 在参数名中体现（`_norm` / `_ratio`），并在注释声明值域 |
| DAL-U-003 | MUST | `[LINT-ENFORCED]` (dal_quantity.py) | 单位后缀 MUST 取自下方**封闭枚举表**，严禁任意自造拼写或同义后缀（如自造 `_msec` / `_millis`）。新增后缀 MUST 走规范评审并补入本表 |
| DAL-U-004 | MUST | `[LINT-ENFORCED]` (dal_quantity.py) | A 类量的后缀 MUST 同时编码其刻度（`_promille` / `_ddeg` / `_um` / `_cmm` ...），使读到 `900` 即知是 90.0% 还是 90.0°；MUST NOT 用不编码刻度的裸后缀（如 `_angle`）配合隐含缩放 |

**封闭单位后缀枚举表（Closed Enumeration）**：

| 物理维度 | 标准后缀 | 刻度 / LSB | 现存 / 典型示例 |
|----------|----------|-----------|----------------|
| 时间 | `_us` / `_ms` / `_s` | 微秒 / 毫秒 / 秒 | `timeout_ms`, `pulse_us`, `last_pulse_us` |
| 长度 / 位置 | `_nm` / `_um` / `_cmm` / `_mm` / `_cm` | 纳米 / 微米 / 0.01 mm（丝）/ 毫米 / 厘米 | `position_um`, `stroke_cmm`, `distance_cm`, `alt_mm` |
| 角度 | `_ddeg` / `_mdeg` / `_udeg` / `_deg` | 0.1° / 0.001° / 10⁻⁶° / 1° | `angle_ddeg`, `course_deg`, `lat_udeg` |
| 电气量 | `_ma` / `_mv` / `_ua` / `_uv` | 毫安 / 毫伏 / 微安 / 微伏 | `current_ma`, `voltage_mv` |
| 归一化比例 | `_promille` / `_per10k` | 千分比 ‰（1000=100%）/ 万分比（0.01%） | `speed_promille`, `duty_per10k` |
| 频率 | `_hz` | 赫兹 | `pwm_freq_hz` |
| 速度 | `_kmh` | 公里/小时 | `speed_kmh` |
| 百分比 | `_pct` | 百分比 | — |
| 角速度 | `_dps` | 度/秒 | （IMU 预留） |
| 加速度 | `_mps2` | m/s² | （IMU 预留） |
| 原始计数 | `_raw` | 原始 ADC/传感器计数 | （ADC 预留） |
| 温度 | `_degc` | 摄氏度（禁用易撞名的 `_c`） | （温湿度预留） |
| 功耗 / 容量 | `_mw` / `_mah` | 毫瓦 / 毫安时 | （电池管理预留） |

> **关于 `_norm`**：`_norm` 后缀不编码正负号区间。有符号归一化（如电机转速 `[-1.0, 1.0]`）与无符号归一化（如占空比 `[0, 1.0]`）的值域区分以 API Contract 注释的 `Range` 字段为权威，不在后缀中引入 `_snorm` / `_unorm`。注意：A 类控制量在新驱动中应优先用 `_promille`（带符号）等定标整数后缀，而非 float `_norm`（见 §9.4）。

### 9.2 值域声明与钳位饱和

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-U-010 | MUST | `[LINT-ENFORCED]` (dal_contract_doc.py) | API Contract 注释 MUST 在 `Range` 字段声明参数合法值域（含单位与正负号区间） |
| DAL-U-011 | MUST | `[LINT-ENFORCED]` (dal_quantity.py) | A 类执行器控制量越界 MUST 采用**隐式钳位饱和（saturate to [min, max]）**，**MUST NOT 溢出回卷（overflow wrap-around）**。Debug 构建 MAY 输出 Warn/Assert 提醒开发者，Release 构建 MUST 保持安全钳位运行。只有非控制量（如配置索引、枚举）才 MAY 选择返回 `WINK_ERR_INVALID_ARG` |
| DAL-U-012 | MUST | `[MANUAL-REVIEW]` | B 类传感器测量的内部换算 MUST 饱和无 UB：超量程值钳至该整数类型可表示的物理极值（或标记为无效/饱和状态），MUST NOT 产生回卷或未定义行为 |

**钳位饱和的安全意义**：执行器越界值若直接写入寄存器（如 `(ARR * duty_promille) / 1000` 中 `duty_promille=1200`），在无饱和保护下可能因整数回卷产生一个极小占空比或反向输出，导致电机/舵机暴走。钳位至硬件安全边界是与位宽无关的硬性安全约束。

**现状范例**（迁移前的 stable 驱动，仍为 float；新 A 类驱动应按 §9.4 用整数）：

```c
/**
 * @param speed -1.0 (full reverse) … 1.0 (full forward).
 *        0.0 = coast. Out of range → saturate to [-1.0, 1.0].
 *        When config.invert == true, direction sense is swapped.
 */
wink_status_t dal_dc_motor_set_speed(dal_dc_motor_t *dev, float speed);

/**
 * @param angle 0.0 ~ effective_max_angle (度). 超出范围自动钳位 (saturate).
 */
wink_status_t dal_rc_servo_set_angle(dal_rc_servo_t *dev, float angle);
```

### 9.3 量纲两分类原则（A / B）

DAL 物理量按**数据流向与硬件终态**分两类，采用截然不同的跨 Profile 策略。这是本章主干原则。

| | **A 类：执行器命令（Actuator Command）** | **B 类：传感器测量（Sensor Measurement）** |
|---|---|---|
| 方向 | App → 硬件（输出） | 硬件 → App（输入） |
| 硬件终态 | 离散寄存器整数（PWM CCR/ARR、比较值、分频） | 物理量，喂给滤波 / 融合 / 显示 |
| 典型量 | 速度、占空比、舵机角度、亮度、频率、**直线/角度位置**、超时/延时、引脚、计数 | 温度、距离、电压、电流、加速度、角速度、经纬度 |
| Full 类型 | **定标整数**（刻度见 §9.4），MUST NOT 用 float | `float` / `double` + 单位后缀 |
| Micro 类型 | **与 Full 完全相同的定标整数类型与刻度** | 定点整型 + 后缀 |
| App 字面量 | 直接写整数（`-500`、`900`、`2500`），无需转换宏 | `_LITERAL` 宏（字面量）或具名转换函数（运行时变量） |
| 软浮点成本 | 零 | 低频可接受，成本显式声明 |

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-U-020 | MUST | `[LINT-PARTIAL]` | 每个 DAL 物理量 MUST 归类为 A（执行器命令）或 B（传感器测量）。归类依据是**数据方向与硬件终态**，而非"是否有单位"。舵机角度是 A 类（终态=PWM 脉宽整数），不是 B 类 |
| DAL-U-021 | MUST | `[LINT-ENFORCED]` (dal_yaml_parity.py) | 新驱动的新增物理量 MUST 在 YAML 中以 `quantity_class: actuator_command \| sensor_measurement` 声明其分类（见 §16）；codegen MUST 对缺失分类报错拦截，MUST NOT 提供隐式默认 |
| DAL-U-022 | MUST NOT | `[LINT-ENFORCED]` (dal_quantity.py) | MUST NOT 引入跨 Profile 弱 typedef 量纲别名（如 `typedef float/int16_t dal_speed_t;`）"统一"类型。C typedef 不做单位检查、隐藏真实刻度、诱导 App 直连 DAL，是虚假的类型安全（[ADR-0056](../../../docs/design/decisions/0056-cross-profile-quantity-ab-class-and-scaled-integers.md) 方案①否决理由 3） |

**Math Domain ↔ Hardware Control Domain 边界**：控制量在硬件侧终态本就是离散整数——即便 32 位用 `float 0.8f`，DAL 内部写寄存器仍要 `(uint32_t)(0.8f * max_duty)`。这层 float 多此一举，还迫使 8 位端做 float↔int 软浮点转换。把 A 类量在**所有 Profile（含 32 位 Full）统一为定标整数**，可在最热控制路径消除全部浮点与转换宏；PID / 滤波等数学域留在 BAL，在 BAL→DAL 边界做一次 `(int16_t)(u * 1000)`。

### 9.4 A 类执行器命令的整数定标

#### 9.4.1 两种定标形态

A 类"用定标整数"不等于只能用千分比（‰）。按物理量性质选刻度：

| 形态 | 适用 | 定标示例 | 后缀 |
|------|------|---------|------|
| **归一化比例量** | 无量纲相对命令（速度、占空比、亮度） | 千分比 ‰（`[-1000,1000]` / `[0,1000]`，约 10 位）；需更细用 per-10k（0.01%） | `_promille` / `_per10k` |
| **绝对物理量** | 有 SI 单位的绝对命令（位置、脉宽、电流、转矩） | 直接选物理 LSB：µm、0.01 mm(cmm)、µs、mA、0.1°(ddeg) | `_um` / `_cmm` / `_us` / `_ma` / `_ddeg` |

**0.01 mm 位置精度用例**：用绝对物理量整数定标而非 ‰——`int32_t position_um`（1 LSB=1µm=0.001mm，量程 ±2147 m，精度高于 0.01mm，长行程首选）或 `uint16_t position_cmm`（1 LSB=0.01mm，量程 0~655.35mm，短行程 8 位最省）。全 Profile 同类型同字面量（`set_position(&m, 1500)` = 15.00mm），32/8 位零软浮点、真同源。约束是位宽×量程（要"0.01mm 精度 + >655mm 行程"则 `uint16_cmm` 装不下，需 `uint32_um`），这是任何方案（含 float）都存在的物理权衡。

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-U-023 | MUST | `[LINT-ENFORCED]` (dal_quantity.py) | A 类执行器命令在 Full 与 Micro 下 MUST 使用**完全相同的定标整型类型、单位后缀与倍率**，MUST NOT 在 Full 下退化为 `float`。两端原型仅句柄指针类型不同。刻度是全 Profile 契约，不可按 Profile 分化 |
| DAL-U-024 | MUST | `[MANUAL-REVIEW]` | 定标刻度 MUST 满足器件全量程有效精度，MUST NOT 为"统一 ‰"而牺牲绝对物理量的直观性。8 位优先 `uint16_t`/`int16_t`（8051 上 32 位乘除显著更贵）；需更大量程才用 `uint32_t` 并在头注释声明 8 位成本 |
| DAL-U-025 | MUST | `[LINT-ENFORCED]` (dal_quantity.py) | A 类量的 App 字面量 MUST 直接写成目标刻度整数（`-500`、`800`），MUST NOT 用 `DAL_*()` 转换宏包装——这是 A 类相对 B 类的核心简洁性优势 |
| DAL-U-026 | MUST | `[LINT-ENFORCED]` (dal_quantity.py) | A 类定标整型 API 的头注释 MUST 含至少 3 个具名字面量示例（如 `1000=全速正转, 0=停车/coast, -500=半速反转`）及刻度换算说明，MUST NOT 假设调用者熟悉 ‰ / ddeg 隐藏缩放 |

#### 9.4.2 符号规范（Signedness）

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-U-027 | MUST | `[LINT-ENFORCED]` (dal_quantity.py) | 无物理反向的控制量（PWM 占空比、LED 亮度、单向脉冲、超时）**MUST** 使用无符号类型（`uint16_t` / `uint32_t`） |
| DAL-U-028 | MUST | `[LINT-ENFORCED]` (dal_quantity.py) | 允许反向/双向的控制量（双向电机转速、舵机相对偏角）**MUST** 使用有符号类型（`int16_t` / `int32_t`） |

#### 9.4.3 定标整型运算溢出防护（Arithmetic Overflow Guard）

8 位 / 16 位 MCU 存在 C 语言 **Integer Promotion（整型隐式提升）** 陷阱：即使操作数是 `uint16_t`，`(ARR * duty_promille)` 的中间乘积也可能在 16 位运算中溢出。

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-U-029 | MUST | `[LINT-ENFORCED]` (dal_quantity.py) | DAL 驱动底层硬件寄存器换算涉及乘法中间值时（如 `CCR = (ARR * duty_promille) / 1000`），MUST 显式强转为 `uint32_t` / `int32_t` 完成乘法再做除法，MUST NOT 依赖 16 位隐式提升。codegen 与静态检查工具 MUST 对此自动化校验 |

```c
/* ✅ 正确：中间乘积提升至 32 位，避免 16 位溢出 */
uint32_t ccr = ((uint32_t)arr * (uint32_t)duty_promille) / 1000u;

/* ❌ 错误：arr 与 duty_promille 均 uint16_t 时，乘积可能在 16 位回卷 */
uint16_t ccr = (arr * duty_promille) / 1000u;
```

#### 9.4.4 钳位饱和（Clamp Saturation）

A 类 Setter 越界参数 MUST 按 DAL-U-011 隐式钳位饱和至 `[min, max]`，严禁溢出回卷引致硬件暴走：

```c
/* set_duty_promille(dev, 1200) → 钳位为 1000（100%），而非回卷 */
if (duty_promille > 1000) duty_promille = 1000;
```

#### 9.4.5 Setter / Getter / 句柄同表示（防半整型化撕裂）

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-U-030 | MUST | `[LINT-ENFORCED]` (dal_quantity.py) | 新增 A 类驱动的 Setter 参数类型、Getter 出参类型、句柄内部缓存成员 MUST 使用**同一定标整型表示**，MUST NOT 出现"Setter 用定点而 Getter 或句柄成员留存 float"的半整型化撕裂——这种不一致是最难迁移的隐蔽债 |

#### 9.4.6 A 类标准量纲映射

| 逻辑量 | 全 Profile 类型 / 刻度 | 说明 |
|--------|----------------------|------|
| 有符号归一化速度/控制量 | `int16_t speed_promille` `[-1000,1000]` ‰ | 0.1% 精度，0=coast/stop |
| 无符号占空比/亮度 | `uint16_t duty_promille` `[0,1000]` ‰ | 0.1% 精度 |
| 舵机/执行器角度 | `uint16_t angle_ddeg` 0.1° | 0~1800 = 0~180.0°；精度远高于机械公差 |
| 直线位置 | `int32_t position_um` / `uint16_t position_cmm` | 按行程与精度选 |
| PWM 频率 | `uint32_t pwm_freq_hz` | 全 Profile 同宽 |
| 超时/延时/周期 | 见 §9.4.7 | 时间是 A 类但宽度需特殊处理 |

> **精度论证**：‰（约 10 位）对电机、舵机、LED 背光、蜂鸣器音量等 99.9% 场景足够——8 位 PWM 仅 256 级，舵机机械公差通常 ±1° 以上。若个别器件需更细（如 12 位以上调光），按 DAL-U-024 提升至 per-10k 或物理定标，MUST NOT 为此回退 float。

#### 9.4.7 时间量的特殊处理

时间/超时/延时属 A 类（终态是定时器计数），但宽度跨 Profile 不同，是已知例外：

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-U-031 | MUST | `[MANUAL-REVIEW]` | Full Profile 时间量用 `uint32_t`（ms）；Micro Profile 默认 `uint16_t`（ms，上限 65535 ≈ 65.5s）。两端**宽度不同但刻度相同（毫秒）**，字面量 `100` 语义一致 |
| DAL-U-032 | MUST | `[LINT-PARTIAL]` | 超过 Micro `uint16_t` 上限的时间值 MUST 在 codegen 阶段报错，MUST NOT 静默截断/回绕。需要更长超时的 Micro 器件可显式选用 `uint32_t`（8051 运算更贵，需在头注释声明成本） |

### 9.5 B 类传感器测量的跨 Profile 映射

B 类量（温度、距离、电压、IMU、GPS 等）在 32 位上常需浮点做滤波/融合，在 8 位上退化为定点整型。此类**不追求全 Profile 同类型**，差异由 codegen binding 吸收。

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-U-040 | MUST | `[MANUAL-REVIEW]` | B 类量在 Full Profile MUST 使用带单位后缀的 `float`/`double`（`temp_degc`、`distance_cm`、`voltage_mv`）；在 Micro Profile 使用带后缀的定标整型（`temp_ddegc`、`distance_mm`、`voltage_mv`） |
| DAL-U-041 | SHOULD | `[MANUAL-REVIEW]` | B 类量在两端 SHOULD 优先统一物理单位（如 Full 内部也用 mm 而非 cm），以减少 App 阈值换算；单位选择以"物理不变量最直观、App 换算最少"为准，但不强制（cm 在 32 位浮点更直观时可保留） |
| DAL-U-042 | MUST | `[LINT-PARTIAL]` | **物理不变量**：同一 B 类量折算到标准物理单位后在两端 MUST 相等；Micro 超位宽量程由 codegen 生成期报错或饱和标记，MUST NOT 静默截断导致"同一读数跨端语义不同" |
| DAL-U-043 | SHOULD | `[LINT-PARTIAL]` | B 类 Micro binding SHOULD 区分两种策略：①`float_bridge`（默认，App 接口保持 float，内部转换，同源优先）；②`native_int`（暴露原生定点整型，零软浮点优先）。器件作者按采样频率与资源预算在 YAML 显式选择 |

**标准 B 类量纲映射**：

| 逻辑量 | Full 类型/刻度 | Micro 类型/刻度 | 不变量 |
|--------|---------------|----------------|--------|
| 温度 | `float temp_degc` °C | `int16_t temp_ddegc` 0.1°C | 同一温度 |
| 距离 | `float distance_cm` cm | `uint16_t distance_mm` mm | 同一距离 |
| 电压 | `float voltage_mv` mV | `uint16_t voltage_mv` mV | 同一电压 |
| 电流/加速度/角速度 | `float`（A / m·s⁻² / °·s⁻¹） | 定标整型（`_ma` / `_mps2` / `_mdps`） | 同一物理量 |
| 经纬度 | `double`（µ°） | 仅 Full（`supported_profiles: [full]`） | — |

> B 类的字面量转换宏（`_LITERAL` 后缀，零开销，仅实参为编译期常量时成立）与运行时具名转换函数（Micro 上软浮点成本由头注释显式声明）属纯 8 位机制，细节见 [`dal-micro-profile-spec.md`](dal-micro-profile-spec.md)。MUST 严格区分"编译期字面量换算"与"运行时变量换算"，不得用同一宏掩盖运行时软浮点成本。

### 9.6 8 位 Micro Profile 量纲（概览）

8 位 Micro Profile 下 DAL 公开 API MUST NOT 使用 `float`/`double`（无 FPU，软浮点库膨胀且缓慢）。A 类量纲直接适用 §9.4 的全 Profile 定标整数表；B 类量按 §9.5 退化为定点整型。纯 8 位机制——Flash Zero-Copy 句柄、`WINK_CODE/XDATA/IDATA` 存储区、`reentrant` 与 Overlay 分析、`uint8_t` 代 `bool`、`uint16_t` 时间戳、`_LITERAL` 宏与运行时转换、codegen micro 模板分支——见独立子规范 [`dal-micro-profile-spec.md`](dal-micro-profile-spec.md)。

**Micro Profile 量纲对照（概览，详见子规范）**：

| 物理量 | Full (32-bit) | Micro (8-bit) | 单位 / 范围 |
|--------|--------------|---------------|------------|
| 速度（A 类） | `int16_t speed_promille` | `int16_t speed_promille` | 千分比，`[-1000,1000]` |
| 占空比（A 类） | `uint16_t duty_promille` | `uint16_t duty_promille` | 千分比，`[0,1000]` |
| 舵机角度（A 类） | `uint16_t angle_ddeg` | `uint16_t angle_ddeg` | 0.1°，`0~1800` |
| 距离（B 类） | `float distance_cm` | `uint16_t distance_mm` | cm / mm |
| 毫秒超时（A 类） | `uint32_t timeout_ms` | `uint16_t timeout_ms` | 毫秒（宽度差异见 DAL-U-031） |

---

## 10. 事件回调规范

### 10.1 注册 API

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-CB-001 | MUST | `[LINT-PARTIAL]` | 回调注册动词统一使用 `on_event`（与现存 `dal_button_on_event` 一致） |
| DAL-CB-002 | MUST | `[LINT-PARTIAL]` | 上下文参数命名统一使用 `ctx`（不用 `user_data`） |
| DAL-CB-003 | MUST | `[MANUAL-REVIEW]` | 传入 `cb = NULL` MUST 注销回调（无需额外 unregister API） |

**签名范式**：

```c
/* 回调类型 */
typedef void (*dal_<type>_event_cb)(dal_<type>_event_t evt, void *ctx);

/* 注册 API */
WINK_WARN_UNUSED_RESULT
wink_status_t dal_<type>_on_event(dal_<type>_t *dev, dal_<type>_event_cb cb, void *ctx);
```

### 10.2 回调上下文声明

见 [§6.4 DAL-C-030/031](#64-回调上下文归属)。每个回调注册 API 的头注释 MUST 声明：

1. 回调在哪个上下文被调用（ISR / task / poll 循环）
2. 回调内允许调用的 API 类别
3. 回调是否允许阻塞

### 10.3 全局钩子

如有进程级（非 per-device）的钩子（如 `dal_button_set_irq_hook`），MUST 与 per-device 回调分开声明，并放入 BAL 内部头文件（如 `dal_button_bal.h`），不暴露在公开冻结 API 面。

---

## 11. 裁剪、Stub 与禁用态

### 11.1 编译期裁剪 (WINK_USE_xxx)

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-P-001 | MUST | `[LINT-PARTIAL]` | 每个驱动 MUST 支持 `WINK_USE_<TYPE>` CMake 开关裁剪 |
| DAL-P-002 | MUST | `[LINT-PARTIAL]` | 裁剪禁用时，所有公开 API 声明 MUST 带 `WINK_UNAVAILABLE_MSG(WINK_<TYPE>_DISABLED_MSG)` 标注，使调用方得到友好的编译错误而非链接失败 |
| DAL-P-003 | MUST | `[LINT-PARTIAL]` | `WINK_<TYPE>_DISABLED_MSG` 的文本 MUST 指引用户如何启用（"add a \"xxx\" device to wink-app.json"） |
| DAL-P-004 | MUST | `[LINT-PARTIAL]` | 阻塞 API 的裁剪 stub MUST 同时在 `#ifndef WINK_STRICT_NONBLOCKING` 守卫内（守卫对称性） |

### 11.2 Stub 实现（功能未完成态）

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-P-010 | MUST | `[MANUAL-REVIEW]` | Stub 实现的 API MUST 返回 `WINK_ERR_UNSUPPORTED` |
| DAL-P-011 | MUST | `[MANUAL-REVIEW]` | Stub MUST NOT claim 任何硬件资源 |
| DAL-P-012 | MUST | `[MANUAL-REVIEW]` | Stub MUST NOT 置 `initialized = true`（避免上层误判设备可用） |
| DAL-P-013 | MUST | `[LINT-PARTIAL]` | Stub 的头注释 MUST 标注 `@experimental Stub: returns WINK_ERR_UNSUPPORTED until ... lands` |
| DAL-P-014 | SHOULD | `[LINT-ENFORCED]` (dal_yaml_parity.py) | YAML SHOULD 使用 `experimental: true` 标记实现未完成的驱动 |

**现存范例**：`dal_gps` 和 `dal_eeprom` 的 init 均为 stub，注释明确声明"当前 stub 实现将 *dev 清零，dev->initialized=false，不 claim 资源"。

### 11.3 实现成熟度

YAML 中 `experimental: true` 表示"接口可能变动 + 实现可能不完整"。建议未来扩展为：

- `experimental: true` → 接口不稳定，codegen MAY 不生成 role binding
- `implementation_status: stub | partial | complete`（待 ADR 裁决后引入）

---

## 12. 双 Target 一致性

### 12.1 源码层约束

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-T-001 | MUST | `[LINT-ENFORCED]` (layering.yaml) | `dal/` 层源码 MUST NOT 出现 `#ifdef SIMULATION` / `#ifdef ESP_PLATFORM` / `#ifdef __EMSCRIPTEN__` 等平台宏（下沉到 PAL/targets） |
| DAL-T-002 | MUST | `[MANUAL-REVIEW]` | 同一 `.c` 源文件 MUST 同时进入 ESP32 和 Wasm 两个 target 构建，禁止 per-target 分叉源文件 |
| DAL-T-003 | MUST | `[LINT-PARTIAL]` | 时间相关 API MUST 通过 PAL 时钟（`pal_os_get_ms`, `pal_delay_ms`），MUST NOT 直接调用平台 API |

### 12.2 行为差异声明

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-T-010 | SHOULD | `[MANUAL-REVIEW]` | 两端行为有差异时（如仿真不模拟 I2C NAK），SHOULD 在头注释新增 `Simulation-parity` 字段声明 |

**范例**（`dal_ultrasonic.h`）：

```
 * Sim 分支：跳过物理 GPIO 配置（旁路最低物理信号层，ADR-0003 决策2），仅置结构状态。
 * ESP32：自动初始化 RMT 硬件脉冲捕获；RMT 失败自动降级到 busy-wait。
```

---

## 13. 向后兼容与演进规则

### 13.1 红线 (MUST)

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-BC-001 | MUST | `[MANUAL-REVIEW]` | **Init-to-Ready**：`init()` 成功后器件默认进入可用状态。MUST NOT 要求旧调用方显式调用新增的 `start()` 才能工作。**例外**：执行器 init 后 MUST 处于零能量态（可用但不输出） |
| DAL-BC-002 | MUST | `[MANUAL-REVIEW]` | **句柄末尾追加**：`dal_<type>_t` 新增字段 MUST 追加在结构体末尾，保证 `{0}` 清零初始化不受影响 |
| DAL-BC-003 | MUST | `[MANUAL-REVIEW]` | **Config 只追加不重排**：`dal_<type>_config_t` 新增成员 MUST 追加在末尾，MUST NOT 重排已有成员（因 `apply_override` 按成员顺序反序列化） |
| DAL-BC-004 | MUST | `[MANUAL-REVIEW]` | **保持同步 API 存续**：增加异步/回调 API 时，原有的同步 API MUST 完整保留 |
| DAL-BC-005 | MUST | `[MANUAL-REVIEW]` | **兼容性 > 填充优化**：DAL-BC-003 与"按尺寸降序排列"可能冲突，此时兼容性优先 |

### 13.2 函数级 Deprecation 与退役策略

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-BC-020 | MUST | `[LINT-PARTIAL]` | 废弃公开 DAL 函数 MUST 用 `WINK_DEPRECATED_MSG("use dal_xxx_new() instead; will be removed in vN+2")` 标注旧函数 |
| DAL-BC-021 | MUST | `[MANUAL-REVIEW]` | 废弃的同版本 MUST 提供替代 API，并在头注释 `@deprecated` 指向新 API |
| DAL-BC-022 | MUST | `[MANUAL-REVIEW]` | 废弃函数 MUST 至少保留**两个 minor 版本**窗口后才可删除（与 button BAL rename 先例一致） |
| DAL-BC-023 | MUST | `[MANUAL-REVIEW]` | 删除已废弃函数 MUST 走 ADR 并在 changelog 列出 |

**现存先例**：`WINK_DEPRECATED_MSG` 已在 `wink_status.h` 中定义；`wink_button_events.h` 已使用 `WINK_DEPRECATED("use wink_button_enable_events (ADR-0032 B-class)")` 标注旧 API。

### 13.3 结构体尺寸断言

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-BC-010 | SHOULD | `[LINT-ENFORCED]` (abi.py) | config_t 和 dal_xxx_t SHOULD 添加编译期 ABI 断言。首选 `offsetof(...) == N`（位宽无关，详见 [§2.3](#23-abi-稳定性断言)）；含指针的结构体若用整体 `sizeof` 断言 MUST 按 `INTPTR_MAX` 分 32/64 位两档，避免在 64 位 host 测试上误报 |
| DAL-BC-011 | SHOULD | `[MANUAL-REVIEW]` | `apply_override` 的 params 反序列化 SHOULD 有显式长度校验（不仅依赖 `len` 参数隐式判版本） |
| DAL-BC-012 | MUST | `[MANUAL-REVIEW]` | `apply_override` 的 wire payload MUST 携带 `schema_version` 字段（至少 1 字节），接收方 MUST 校验 version + length 后再反序列化。版本不匹配时 MUST 返回 `WINK_ERR_VERSION_MISMATCH`（待定义）或 `WINK_ERR_INVALID_ARG` |

### 13.4 API 版本标识 (MAY)

驱动头文件 MAY 定义编译期 API 版本宏，供 OTA 升级、Wasm 模块热加载等场景的版本兼容性检查使用：

```c
/* 版本格式: 0xMMmmPP (Major.Minor.Patch) */
#define DAL_DC_MOTOR_API_VERSION  0x030300  /* v3.3.0 */
```

运行时如需查询，可通过全局宏或包裹函数（待设计）暂不作强制规定。

---

## 14. 错误码与可观测性

### 14.1 错误码层级

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-EC-001 | MUST | `[LINT-ENFORCED]` (dal_api_shape.py) | 通用错误码（`WINK_OK`, `WINK_ERR_INVALID_ARG`, `WINK_ERR_NOT_INITIALIZED`, `WINK_ERR_TIMEOUT`, `WINK_ERR_BUSY`, `WINK_ERR_UNSUPPORTED`, `WINK_ERR_IO`）从 `wink_status.h` 全局定义 |
| DAL-EC-002 | MUST | `[LINT-PARTIAL]` | DAL API MUST 优先使用通用错误码 |
| DAL-EC-003 | SHOULD | `[MANUAL-REVIEW]` | 如需器件特有错误码（如未来可为 GPS 定义 `WINK_ERR_GPS_NO_FIX`，当前 `wink_status.h` 尚未定义），SHOULD 通过头文件中的 `#define` 宏定义并在 `wink_status.h` 预留数值范围 |
| DAL-EC-004 | MUST | `[MANUAL-REVIEW]` | 器件特有错误码 MUST 落在 `wink_status.h` 预留的分段范围内，严禁随意选取数值导致集成期冲突 |

**错误码分段预留方案**（待 `wink_status.h` 正式落地）：

| 数值范围 | 所属层 | 说明 |
|-----------|--------|------|
| 0 | 全局 | `WINK_OK` |
| -1 ~ -99 | 全局通用 | `WINK_ERR_INVALID_ARG`, `WINK_ERR_TIMEOUT` 等 |
| -100 ~ -199 | DAL 保留 | DAL 层共用错误码（如未来的 `WINK_ERR_VERSION_MISMATCH`） |
| -200 ~ -299 | 器件特有 | 按器件类型分配子段（由 driver registry 管理） |
| -300 ~ -399 | PAL | PAL 层平台相关错误码 |
| -400 ~ -499 | App | 应用层错误码 |

### 14.2 init 幂等性

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-EC-010 | MUST | `[LINT-ENFORCED]` (dal_lifecycle.py) | 对已 `initialized` 的设备调用 `init` MUST 返回 `WINK_ERR_ALREADY_INITIALIZED`，不做任何操作（fail-fast） |
| DAL-EC-011 | MUST | `[MANUAL-REVIEW]` | MUST NOT 隐式先 deinit 再 init（有竞态风险） |

### 14.3 日志约定

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-EC-020 | SHOULD | `[MANUAL-REVIEW]` | `init` 成功 SHOULD 输出 INFO 级别日志（含 owner 和关键配置） |
| DAL-EC-021 | SHOULD | `[MANUAL-REVIEW]` | `init` 失败 SHOULD 输出 WARN 级别日志（含错误码和失败原因） |
| DAL-EC-022 | MUST | `[LINT-ENFORCED]` (dal_concurrency.py) | ISR 上下文 MUST NOT 调用日志 API |
| DAL-EC-023 | SHOULD | `[LINT-PARTIAL]` | 日志 tag SHOULD 使用 `"dal_<type>"` 格式 |

### 14.4 配置验证职责

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-EC-030 | MUST | `[LINT-PARTIAL]` | `init` 函数 MUST 做最小化防御校验（NULL 检查、关键字段范围），即使 codegen 已在生成时校验 |
| DAL-EC-031 | SHOULD | `[LINT-PARTIAL]` | codegen 层 SHOULD 做完整语义校验（字段约束、值域、枚举合法性）。两层防御 |

### 14.5 总线错误恢复策略 (Bus Error Recovery)

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-EC-040 | SHOULD | `[MANUAL-REVIEW]` | 使用 I2C/SPI 总线的 DAL 驱动，在检测到连续 N 次通信超时/NAK 后，SHOULD 通过 PAL 层的 bus recovery 原语（如 `pal_i2c_bus_recover`）尝试恢复总线，并在 API Contract 注释声明恢复策略。重试次数 N SHOULD 可通过 config 字段配置（默认值建议 3 次） |
| DAL-EC-041 | SHOULD | `[MANUAL-REVIEW]` | 总线恢复失败后 SHOULD 将驱动状态设为 ERROR，并通过事件回调或 `get_status` 通知上层。在三段式驱动中，总线故障 SHOULD 将状态机迁移到 ERROR 状态 |

> **I2C 总线死锁背景**：I2C 总线因 slave 设备异常保持 SDA 低电平导致总线挂死（bus stuck）是嵌入式产品中极其常见的故障场景。PAL 层 bus recovery 通常通过发送 9 个时钟脉冲释放 SDA 来恢复。

---

## 15. API Contract 注释模板

所有公开头文件函数 MUST 标注以下 Contract 元数据。带 ★ 为必填字段：

```c
/**
 * @brief 一句话描述 API 功能。
 *
 * 详细说明（可选）。
 *
 * @param dev  器件实例句柄
 * @param ...  其他参数（含单位、值域）
 * @return wink_status_t
 *
 * @note API Contract:
 *   - Preconditions: ★ dev 非 NULL；init 已成功。
 *   - Postconditions: ★ 成功时的状态变化。
 *   - Range: ★ 参数值域与单位（A 类例 speed_promille: [-1000, 1000]；B 类例 distance_cm: [0.0, 500.0]）。
 *   - Blocking: ★ No / Yes (worst-case Xms)。
 *   - Thread-safe: ★ No（默认值；缺失时按 No 解释，见 DAL-C-042）。
 *   - ISR-safe: ★ No / Yes。
 *   - Reentrancy: No / Yes。
 *   - Simulation-parity: 两端行为差异说明（如有）。
 *   - Side-effects: 除返回值外的所有可观测副作用（如 "Modifies dev->last_speed; triggers PWM duty update"）。
 *   - Error-codes: ★ WINK_OK / WINK_ERR_xxx / ...
 */
```

> **Thread-safe 默认值**：`Thread-safe` 字段缺失时默认按 `No` 解释（见 [§6.0 DAL-C-040/042](#60-task-to-task-并发默认契约)）。lint SHOULD 对公开 API 缺该字段报 warning。

**现存最佳范例**：`dal_dc_motor_set_speed`、`dal_ultrasonic_request_measurement`、`dal_button_init` 已覆盖大部分字段。`led` / `encoder` 部分函数缺少完整 Contract 块，属迁移项。

---

## 16. Codegen YAML 集成

### 16.1 SSOT 原则

驱动的全集 SSOT 是 `wink-micro-os/codegen/drivers/*.yaml`（ADR-0046）。本规范不复述 YAML schema，仅说明 API 规范与 YAML 字段的对应关系。

**YAML schema 版本**：当前锁定 `codegen_schema: "1.1"`。

### 16.2 规范与 YAML 字段映射

| 本规范条款 | YAML 字段 | 说明 |
|-----------|----------|------|
| §1.3 Profile 分级支持 | `profiles` | 声明驱动支持的 Profile（如 `[full, micro_8bit]`） |
| §2.4 / §9.6 8位类型重映射 | `profile_overrides.micro_8bit` | 定义 8 位 Micro Profile 专用的 `config_type`、`handle_type` 与整数量纲类型 |
| §9.3 量纲两分类 | `quantity_class: actuator_command \| sensor_measurement` | 每个物理量字段声明 A/B 分类；缺失 MUST 在 codegen 生成期报错（DAL-U-021） |
| §9 量纲与定标 | `quantity` + `unit`/`scale`/`range` | 声明逻辑量纲（speed/angle/distance...）、刻度后缀与量程；codegen 据此生成类型、后缀、全 Profile 同刻度校验与 Micro 超量程报错（DAL-U-023/032/042） |
| §3.2 safe_off 按 category | `is_actuator: true/false` + `config.safe_off_fn` | `safe_off_fn: ""` = 该器件无安全关断语义 |
| §5.3 按 category 分组 | `category: actuator/output/input/sensor/display/storage/comm` | 直接复用，不引入新分类词 |
| §5.4 器件特有 API | 未来 `device_specific: true` | 待 codegen 支持 |
| §11.2 Stub | `experimental: true` | 标记实现未完成 |
| §13 兼容性 | `codegen_schema: "1.1"` | schema 版本变更需评审 |
| §3.1 / §14.5 总线依赖 (建议) | `dependencies` (待引入) | 声明驱动对总线/其他器件的依赖关系（如 I2C bus, encoder feedback） |

### 16.3 真实 YAML Profile 多 Target 支持参考

> **现状说明（待 codegen 落地）**：当前 `codegen/drivers/*.yaml` 的 role verb 模板使用**单一 `template` 字段**（见 `rc_servo.yaml`），per-profile 模板（`template_full` / `template_micro_8bit`）、`quantity`/`quantity_class` 量纲元数据、`profile_overrides` 类型重映射均为**待 codegen 实现的目标形态**，尚未在生成器中生效。下列示例为规范目标（[ADR-0056](../../../docs/design/decisions/0056-cross-profile-quantity-ab-class-and-scaled-integers.md)），不代表当前生成器能力。

```yaml
codegen_schema: "1.1"     # schema 版本号
type: rc_servo             # 器件类型名
category: actuator         # 分类
is_actuator: true          # 执行器标记
experimental: true
default_role: angular_actuator

# 声明支持的 Profile 列表
profiles:
  - full
  - micro_8bit

# 8 位 Profile 专用重映射 (Zero-Copy Flash 配置与定标整数)
profile_overrides:
  micro_8bit:
    config_type: dal_rc_servo_8b_config_t
    handle_type: dal_rc_servo_8b_t

quantities:                # 量纲 SSOT（§9）
  angle:
    quantity_class: actuator_command   # A 类：全 Profile 同定标整数
    type: uint16_t
    unit: ddeg             # 0.1°
    range: [0, 1800]       # 0~180.0°

config:                    # Full Profile 默认 C 类型映射
  c_type: dal_rc_servo_t
  config_type: dal_rc_servo_config_t
  headers: [dal_rc_servo.h]
  deinit_fn: dal_rc_servo_deinit
  safe_off_fn: dal_rc_servo_safe_off

role_bindings:             # Role verb → Jinja 模板 (per-profile 待实现)
  angular_actuator:
    verbs:
      # A 类：angle 全 Profile 都是 uint16_t angle_ddeg，模板体完全一致（无转换、无软浮点）
      set_angle:
        template_full:       "dal_rc_servo_set_angle(&{{dev}}, {{angle_ddeg}});"
        template_micro_8bit: "dal_rc_servo_set_angle(&{{dev}}, {{angle_ddeg}});"
```

> **B 类（传感器测量）模板差异**：B 类量在 Full 是 float、Micro 是定点整型，Micro 模板中用 `_LITERAL` 宏包装字面量或具名函数转换运行时变量（见 §9.5 与子规范）；其软浮点成本须显式声明。A 类无此问题——这是分类治理带来的核心简洁性。

---

## 17. 合规矩阵与迁移策略

### 17.1 现状合规矩阵 (v3.2.0 基线)

图例：✅ 合规 / ❌ 不合规 / — 不适用 / ⚠ 部分合规

> 本基线冻结于 v3.3.0，覆盖本轮新增的 MUST 条款（DAL-L-008, DAL-L-015, DAL-S-015, DAL-B-025, DAL-EC-004, DAL-BUF-003, DAL-BC-012 等）。下方矩阵聚焦生命周期与注释形态；完整逐条合规状态见 [§17.3.1](#1731-规则实施状态)。
>
> 🌟 **Golden Reference (黄金参考驱动样板)**：正式标定 `dc_motor` 驱动 (`dal_dc_motor.h/c`) 为 Full Profile 黄金参考实现。新增驱动开发与 Code Review 必须以 `dc_motor` 驱动的接口范式、ABI 断言、Contract 注释与剪枝守卫格式为标准样板。待 `dal_led_8b` 在 8051 CI 上成功构建后，双 Profile 标杆将迁移至 `led` 驱动。
>
> ⚠️ **dc_motor 的 `float set_speed` 是迁移前现状，不是新 A 类驱动的量纲范例**：dc_motor 为 stable（Golden Ref、已冻结），其 `set_speed(dev, float speed_norm)` 与句柄 `float current_speed` 保留至真正的 8 位需求出现，再经 deprecation（新增 `set_speed_promille(int16_t)` + 两个 minor 窗口 + ADR，见 [ADR-0056](../../../docs/design/decisions/0056-cross-profile-quantity-ab-class-and-scaled-integers.md)）整型化。**新增 A 类执行器驱动的 Golden Ref 行为应以 §9.4 定标整数为准**（如 rc_servo 整型化后可考虑更新量纲标杆）；stable 现状不构成对新驱动使用 float 的背书。

| 驱动 | init | deinit | safe_off | const getter | Contract 注释 | WINK_BLOCKING 标注 |
|------|------|--------|----------|-------------|---------------|-------------------|
| dc_motor (⭐Golden Ref) | ✅ | ✅ | ✅ (brake) | ✅ | ✅ | — |
| led | ✅ | ✅ | ⚠ (别名 off) | — | ⚠ 部分缺 | — |
| rc_servo | ✅ | ✅ | ✅ (duty=0) | — | ✅ | — |
| button | ✅ | ✅ | — | ✅ | ⚠ 部分缺 | — |
| encoder | ✅ | ✅ | — | ✅ | ⚠ 缺 Contract | — |
| ultrasonic | ✅ | ✅ | — | ✅ | ✅ | ✅ |
| mono_oled | ✅ | ✅ | — | — | ✅ | — |
| eeprom | ✅ | ✅ | — | ✅ | ✅ | ✅ |
| gps | ✅ | ✅ | — | ✅ | ✅ | ✅ |

### 17.2 迁移策略

| 阶段 | 范围 | 规则模式 | 截止 |
|------|------|---------|------|
| v3.2 发布 | 冻结基线矩阵（见 §17.1） | — | 本文档合并时 |
| 新增驱动 | 新增 YAML + 头文件 | 所有 MUST 规则以 **error** 模式执行 | 立即生效 |
| 存量驱动 | 已有 9 个驱动 | MUST 规则以 **warning** 模式执行 | v3.3 迁移完成 |
| v3.3 | 存量提级 | warning → error | 待定（建议 2 个迭代内） |

### 17.3 规则 ID 与 lint 集成

| 要素 | 约定 |
|------|------|
| 规则 ID 格式 | `DAL-<category>-<number>`（如 `DAL-S-001`, `DAL-BUF-001`） |
| Lint 引擎 ID | `dal.<snake_case_rule>`（如 `dal.config_owner_first`），与 `wink-tools/tools/lint/` 的 `<pack>.<rule>` 命名法对齐 |
| 所属 pack | DAL 规则使用独立 pack `dal`（`wink lint --pack dal`），不塞进现有 `api` 或 `layering` pack |
| 例外标注 | 代码内：`// lint-allow: DAL-S-001 (reason)`；YAML 内：`lint_exceptions` 字段（待引擎支持） |

### 17.3.1 规则实施状态

每条 MUST/SHOULD 规则按以下级别标注实施状态：

| 级别 | 含义 |
|------|------|
| `lint-enforced` | CI 已有 lint 规则拦截 |
| `review-enforced` | 纯语义约束，靠代码评审保障（如"越界饱和还是报错"） |
| `pending` | 待实现（必须指定 Issue 跟踪号，如 `#WINK-DAL-001`，避免永不落地） |

**核心规则实施状态**：

| 规则 ID | 条款摘要 | 实施状态 | 跟踪 |
|---------|---------|----------|------|
| DAL-F-001 | 返回 wink_status_t | `lint-enforced` (api pack: `STATUS-NOT-BOOL-PUBLIC`) | — |
| DAL-F-002 | 禁止 bool 返回值 | `lint-enforced` (api pack: `STATUS-NOT-BOOL-PUBLIC`) | — |
| DAL-T-001 | dal/ 禁 #ifdef 平台宏 | `lint-enforced` (layering pack) | — |
| DAL-S-001 | config 首成员 owner | `lint-enforced` (dal pack: `dal.config_owner_first`) | issue `#WINK-DAL-001` |
| DAL-S-005 | config 禁位域 / pragma pack | `lint-enforced` (dal pack: `dal.config_no_bitfield`, `dal.config_no_pragma_pack`) | — |
| DAL-S-006 | 引脚字段类型约定 | `lint-enforced` (dal pack: `dal.pin_required_uint16`, `dal.pin_optional_wink_pin_t`) | — |
| DAL-S-011 | handle 首成员 config | `lint-enforced` (dal pack: `dal.handle_config_first`) | — |
| DAL-S-012 | handle 必含 initialized 标志 | `lint-enforced` (dal pack: `dal.handle_has_initialized`) | — |
| DAL-S-014 | handle 必含 ABI size/offset 断言 | `lint-enforced` (dal pack: `dal.handle_has_static_assert`) | — |
| DAL-S-020 | handle 禁用动态内存分配 | `lint-enforced` (dal pack: `dal.handle_no_dynamic_alloc`) | — |
| DAL-BUF-001 | DMA/Buffer 持有契约 | `review-enforced` | issue `#WINK-DAL-002` |
| DAL-C-040 | 默认非线程安全 | `review-enforced` | — |
| DAL-U-010 | Range 值域声明 | `lint-enforced` (dal pack: `dal.contract.required_fields`) | — |
| DAL-U-011 | A 类越界钳位饱和无回卷 | `lint-enforced` (dal pack: `dal.quantity.a_class_saturate_not_reject`) | — |
| DAL-U-023 | A 类全 Profile 定标整数（Full 禁用 float） | `lint-enforced` (dal pack: `dal.quantity.a_class_no_float`) | issue `#WINK-DAL-030` |
| DAL-U-029 | 定标乘法中间值提升 32 位 | `lint-enforced` (dal pack: `dal.quantity.a_class_overflow_guard`) | issue `#WINK-DAL-031` |
| DAL-U-022 | 禁弱 typedef 量纲别名 | `lint-enforced` (dal pack: `dal.quantity.no_weak_typedef`) | — |
| DAL-E-001 | safe_off 声明具体行为 | `lint-enforced` (dal pack: `dal.yaml.actuator_safe_off_present`, `dal.lc.safe_off_idempotent`) | — |
| DAL-F-020 | 错误返回时出参不变 | `review-enforced` | — |
| DAL-B-012 | 严禁空循环忙等 | `lint-enforced` (dal pack: `dal.blk.no_busy_wait_loop`) | — |
| DAL-L-008 | init 失败资源回滚 | `review-enforced` | issue `#WINK-DAL-020` |
| DAL-L-015 | deinit best-effort 语义 | `lint-enforced` (dal pack: `dal.lc.deinit_idempotent`) | — |
| DAL-L-030 | deinit 后 safe_off 为 no-op | `lint-enforced` (dal pack: `dal.lc.deinit_idempotent`) | — |
| DAL-L-031 | safe_off 后 deinit 正常清场 | `review-enforced` | — |
| DAL-S-015 | config 不可变性 | `review-enforced` | — |
| DAL-B-025 | poll 返回值语义 | `review-enforced` | issue `#WINK-DAL-021` |
| DAL-B-026 | ERROR 态 request 自动重置恢复 | `lint-enforced` (dal pack: `dal.blk.error_recovery`) | — |
| DAL-EC-004 | 错误码分段预留 | `pending` | issue `#WINK-DAL-022` |
| DAL-V-010 | was_* 读后清原子性 | `review-enforced` | — |
| DAL-BUF-003 | DMA Cache 同步 | `review-enforced` | — |
| DAL-BC-012 | override wire 版本校验 | `pending` | issue `#WINK-DAL-023` |
| DAL-8B-* | 8位 Micro Profile 规则全集 | `pending` | 见 [`dal-micro-profile-spec.md`](dal-micro-profile-spec.md) (issue `#WINK-DAL-010`) |

### 17.4 已知例外

| 规则 | 违规点 | 原因 | 收敛计划 |
|------|--------|------|---------|
| DAL-F-010 | `dal_rc_servo_apply_override(void *dev, ...)` | 适配 `wink_dev_override_fn` 统一函数指针 | 待 override 类型参数化后消除 |
| DAL-F-010 | `dal_ultrasonic_apply_override(void *dev, ...)` | 同上 | 同上 |
| DAL-S-003 | `dal_dc_motor_config_t` 成员顺序 | 按 config member order 序列化（DAL-S-004 优先） | 不修复，兼容性优先 |

---

## 附录 A. 功耗模式 (Reserved — 暂不实现)

> **状态**：Reserved (Phase N, 待 PM 框架 ADR)

当前无系统级 PM (Power Management) 框架，0 个驱动实现 `suspend` / `resume`。

```c
/* ⚠ RESERVED — 待 PM ADR 裁决后再实现，当前不得提前实现空壳 */
wink_status_t dal_<type>_suspend(dal_<type>_t *dev);
wink_status_t dal_<type>_resume(dal_<type>_t *dev);
```

**为什么不能现在实现**：空壳 `suspend` 比没有 `suspend` 更危险——上层以为已省电/已停止输出，实际器件仍在运行。

**开放设计问题**（待 PM ADR 回答）：

1. `suspend` 期间执行器是否保持输出？
2. `resume` 后是否恢复 suspend 前的 duty/角度？
3. 未 `suspend` 时调 `resume` 返回什么？
4. 与系统级 PM 框架的集成方式？

---

## 附录 B. 源文件编码

| 规则 ID | 级别 | Lint 状态 | 条款 |
|---------|------|---|------|
| DAL-ENC-001 | MUST | `[LINT-PARTIAL]` | 所有 DAL 相关源文件（`.h`, `.c`, `.yaml`）MUST 为 **UTF-8 无 BOM** 编码 |

---

## 附录 C. 文档定位与变更治理

本文件定位为 **DAL 开发编码规范**（位于 `dal-development-guide/`），面向驱动开发者和 AI 代码生成器提供可查阅的实操规范。

- **架构真相** → [`docs/design/02-wink-micro-os/01-dal-device-abstraction.md`](../../../docs/design/02-wink-micro-os/01-dal-device-abstraction.md)
- **Wasm 仿真 SSOT** → [`docs/design/04-wasm-simulation-3.0/00-README.md`](../../../docs/design/04-wasm-simulation-3.0/00-README.md)
- **设计决策** → 各 ADR
- **Codegen SSOT** → `codegen/drivers/*.yaml` + `codegen/roles/*.yaml`

### 变更治理

| 变更类型 | 流程 |
|---------|------|
| MUST 条款的新增、删除或语义变更 | MUST 走 ADR |
| SHOULD / MAY 条款变更 | 可由维护者评审合入 |
| 涉及 DAL 架构契约（生命周期、并发模型、分发机制）的 Accepted 变更 | MUST 同步回写 `01-dal-device-abstraction.md` 活规范 |
| 纯实操约定（命名后缀、lint ID 格式） | 不需要回写活规范 |

所有变更 MUST 在本文件头部变更历史中记录版本号与摘要。
