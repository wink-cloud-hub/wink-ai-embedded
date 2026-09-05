# DAL 同族变体字段统一为 `variant` — 技术设计

| 项 | 内容 |
|----|------|
| 创建日期 | 2026-07-30 |
| 状态 | **Accepted（Wave A）** — 2026-07-30 |
| 关联实施计划 | [2026-07-30-dal-variant-unified-field-plan.md](../../implementation-plans/core/2026-07-30-dal-variant-unified-field-plan.md) |
| 关联 ADR | [ADR-0004](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md)（静态分发）；[ADR-0034](../../decisions/core/0034-dal-progressive-config-disclosure.md)（Zero-as-Default）；[ADR-0046](../../decisions/core/0046-dal-driver-registry-ssot.md) / [ADR-0051](../../decisions/tools/0051-scannable-codegen-extension-roots.md)（YAML SSOT）；[ADR-0048](../../decisions/core/0048-actuator-control-semantic-naming.md) |
| 关联活规范 | [01-dal-device-abstraction.md](../../design/02-wink-micro-os/01-dal-device-abstraction.md) § 机制一（语义不变 + 拓扑枚举） |
| 关联手册 | [`dal-best-practices.md`](../../../wink-micro-os/docs/dal-development-guide/dal-best-practices.md) §3.0；[`wink-app-json-guide.md`](../../../wink-micro-os/docs/wink-app-json-guide.md) |
| 关联评审 | [2026-07-30-dal-type-semantic-and-function-sufficiency-review.md](../../reviews/core/2026-07-30-dal-type-semantic-and-function-sufficiency-review.md)；UniSim [dal-type-unified-refactoring-design-review](../../../../wink-ai/packages/unisim/docs/review/2026-07-30-dal-type-unified-refactoring-design-review.md) P0-1 |
| 范围 | 跨 type 统一「同族变体」JSON/C 字段名为 `variant`；枚举定义位置；codegen 直写；迁移与文档心法回写 |
| 非范围 | 实现新拓扑分支（如 `phase_enable` 真接线）；SPI OLED；跨 type 共用单一枚举类型；YAML 生成 C 枚举头；运行时动态换驱动 `.c` |

---

## 1. 背景与问题

### 1.1 现状

同 `type` 内消化硬件/解码/面板差异的字段，按品类各起一名：

| `type` | JSON / C 字段 | C 枚举类型 | 默认值（=0） |
|--------|---------------|------------|--------------|
| `dc_motor` | `drive_mode` | `dal_dc_motor_drive_mode_t` | `IN_IN` |
| `encoder` | `decode_mode` | `dal_encoder_decode_mode_t` | `X1_RISING` |
| `mono_oled` | `panel_variant`（仅 C；YAML 未暴露） | `dal_mono_oled_panel_variant_t`（config 存 `uint8_t`） | `SSD1306` |

活规范与 best-practices 用「`drive_mode` = 拓扑避风港」描述**模式**，但字面量被 UniSim 设计误读为「C 侧全员统一 `drive_mode`」，触发事实性 P0（命名与 SSOT 不符）。

### 1.2 目标

1. **模式统一**：同族变体一律用同一字段名进入 `wink-app.json` 与 `dal_*_config_t`。  
2. **命名中立**：字段名不绑定「驱动 / 拓扑 / 解码」任一窄语义。  
3. **零字段名映射**：JSON 键 = C 成员名 = `variant`；codegen 按 type schema 校验取值并直写枚举常量。  
4. **取值仍按 type 封闭**：禁止跨 type 共用一套枚举值空间。  
5. **选择机制不变**：config 烘焙 + DAL 内 `switch`（可选 `HAS_*` 按 App 并集裁 `.text`），不生成多套驱动再动态加载。

### 1.3 命名裁决（Owner 已口头确认方向）

| 候选 | 结论 |
|------|------|
| `drive_mode` | ❌ 执行器中心，不适合 encoder / OLED |
| `topology_mode` | ❌ 「拓扑」对解码倍率 / 屏驱芯片拉伸过度 |
| `hw_variant` | 可用，偏长 |
| **`variant`** | ✅ **采纳** — 短、跨域、与 UniSim「变体表」用语对齐 |

中文文档表述：**同族变体（`variant`）**。

---

## 2. 架构心法（三维抽象，字段改名后）

```text
type     → 驱动护城河：协议 / 控制物理量 / DAL 实现变了 → 新 type
variant  → 同族避风港：机制不变，仅接线 / 解码倍率 / 面板命令集等差异 → config 枚举
role     → 应用隔离：App 用 {name}_{verb}，不绑 dal_* 签名
```

边界钉死：

- `variant` **不替代** `type`，也 **不替代** `role`。  
- **不是**全外设必填字段：仅「该 type 存在已登记同族变体」时出现在 YAML schema / `config_t`。  
- 芯片商品名优先走 catalog / `modelId` → 映射到 `(type, variant, pins…)`；可选糖衣 `driver_ic` 若保留，必须能推导出唯一 `variant`，冲突则 codegen 报错。
- **细分变体优先原则与 `variant_fields` 强制契约（Subdivided Variant First & Mandatory Variant Fields Invariant）**：描述硬件型号/接口协议/接线拓扑/算法机制的字符串选型，统一收拢为确切的**细分变体 `variant`**（如 `ssd1306_i2c` / `ssd1306_spi`；`in_in` / `phase_enable`；`matrix_4x4` / `adc_resistor_ladder`；`standard` / `logarithmic`）。每个 `variant` 枚举值对应的物理引脚映射（Pin Map）与参数表必须**绝对唯一且确定**。在多变体外设 Schema YAML 中**必须声明 `fields.variant.variant_fields` 映射**（漏填直接硬报错）。Codegen 据此自动在生成 C 代码时将未用引脚裁剪为 `-1` 哨兵值。`affects_pins: true` 专门用于向前端与 UniSim 标注该变体在同 `type` 内是否跨变体改变了物理脚位拓扑。



### 2.1 明确不并入 `variant` 的字段

| 字段族 | 原因 |
|--------|------|
| `pull` / `active_low` / `enable_pin` / 各类 `*_pin` | 脚级电气或资源绑定，不是「实现路径变体」 |
| `rc_servo.clock_requirement` / `max_angle` | 控制/时钟策略或行程参数，非同族硬件变体键 |
| `mono_oled` YAML `build_variants.font` | **编译期**字体 TU 裁剪，与运行时面板变体正交 |
| `ultrasonic.use_rmt` 等后端开关 | 若未来升格为同族变体，可迁入 `variant`；本设计不强制本波次改名 |

---

## 3. 方案比选

| 方案 | 内容 | 结论 |
|------|------|------|
| A. JSON 统一键、C 仍旧名，codegen 映射 | 实现快，长期双名 levelling | ❌ 违背「不映射」目标 |
| B. 公共头共享一个 `dal_variant_t` | 假统一，取值空间无法共享 | ❌ |
| C. YAML 生成全部 C 枚举头 | 理想 SSOT，但改 codegen 面过大 | ❌ 本波次不做；可记 follow-up |
| **D. JSON 键 = C 成员 = `variant`；枚举类型按 type 分定义在各 `dal_*.h`** | 零字段名映射；符合静态分发 | ✅ **采纳** |

---

## 4. 详细设计

### 4.1 枚举定义位置（每 type 自有）

枚举**只**定义在对应公开头文件 `dal/include/<category>/dal_<type>.h`，与今日 `drive_mode` / `decode_mode` 同一模式。

命名约定：

| 层 | 约定 | 例（`dc_motor`） |
|----|------|------------------|
| C 枚举类型 | `dal_<type>_variant_t` | `dal_dc_motor_variant_t` |
| C 枚举常量 | `DAL_<TYPE>_VARIANT_<VALUE>` | `DAL_DC_MOTOR_VARIANT_IN_IN` |
| JSON 字符串 | snake_case，与今日取值对齐 | `"in_in"` |
| `config_t` 成员 | **`variant`** | `.variant` |
| 默认 | `= 0`，且 0 必须等于「今日已实现路径」（ADR-0034） | `IN_IN = 0` |

`encoder` / `mono_oled` 同理：

```c
/* dal_encoder.h */
typedef enum {
    DAL_ENCODER_VARIANT_X1_RISING = 0,
    DAL_ENCODER_VARIANT_X2 = 1,        /* reserved → init UNSUPPORTED */
    DAL_ENCODER_VARIANT_X4 = 2,        /* reserved → init UNSUPPORTED */
} dal_encoder_variant_t;

/* dal_mono_oled.h — 建议 config 用强类型，不再裸 uint8_t */
typedef enum {
    DAL_MONO_OLED_VARIANT_SSD1306 = 0,
    DAL_MONO_OLED_VARIANT_SH1106  = 1,
} dal_mono_oled_variant_t;
```

*注：在 32 位 C 编译器中，为防止 enum 扩展为 32-bit 对齐导致 sizeof(dal_*_config_t) 及 Flash NVS 序列化布局改变，建议在 config_t 内做类型保护或 8-bit 限定。*

**禁止**在 `pal/`、公共 `dal_common.h`、或跨 type 头中堆积全部 `VARIANT_*`。

### 4.2 `config_t` 与 DAL `.c` 行为

1. 成员改名：`drive_mode` / `decode_mode` / `panel_variant` → **`variant`**。  
2. `.c` 内所有读写改为 `cfg->variant` / `dev->config.variant`。  
3. **语义与控制流不变**：  
   - 未实现变体：`init`（或首调）返回 `WINK_ERR_UNSUPPORTED`（fail-closed）。  
   - 已实现变体：`switch (dev->config.variant)` 选路径。  
4. **体积裁剪与死代码消除（DCE）**：codegen 扫 App 内该 type 的 `variant` **并集**，写 `WINK_<TYPE>_HAS_<VALUE>`；未编入分支落入 `default` → `UNSUPPORTED`。在编译期剪掉未引用的物理驱动逻辑。

```c
/* dal_dc_motor.c 物理驱动初始化分支骨架示例 */
dal_err_t dal_dc_motor_init(dal_dc_motor_t *dev, const dal_dc_motor_config_t *cfg) {
    if (!dev || !cfg) return WINK_ERR_INVALID_ARG;

    switch (cfg->variant) {
#if defined(WINK_DC_MOTOR_HAS_IN_IN) || !defined(CONFIG_WINK_BUILD_MINIMAL)
        case DAL_DC_MOTOR_VARIANT_IN_IN:
            return dc_motor_init_in_in(dev, cfg);
#endif

#if defined(WINK_DC_MOTOR_HAS_PHASE_ENABLE)
        case DAL_DC_MOTOR_VARIANT_PHASE_ENABLE:
            return dc_motor_init_phase_enable(dev, cfg);
#endif

        default:
            /* 未编译或未实现的 variant 统一 Fail-Closed 拒绝 */
            return WINK_ERR_UNSUPPORTED;
    }
}
```

不改变：对外 `dal_*` API 动词、Role 包装、POD 其余字段布局规则（除本字段 rename 带来的 ABI 变化，见 §6）。

### 4.3 `wink-app.json` 与 codegen YAML

SSOT：`wink-micro-os/codegen/drivers/<type>.yaml`（ADR-0051）。

字段模板：

```yaml
variant:
  tier: advanced
  type: enum
  emit_when: present   # 省略 → C 零值默认，不发射 designated init 行亦可
  enum: [in_in, phase_enable, pwm_on_in]   # 按 type 替换
  map:
    in_in: DAL_DC_MOTOR_VARIANT_IN_IN
    phase_enable: DAL_DC_MOTOR_VARIANT_PHASE_ENABLE
    pwm_on_in: DAL_DC_MOTOR_VARIANT_PWM_ON_IN
```

规则：

- JSON 键名固定为 **`variant`**（与 C 成员同名）。  
- `map` 的 RHS 必须与对应头文件枚举宏一致。  
- Codegen **直写** `.variant = DAL_…`；**不**再维护 `drive_mode`→其它名的别名层（破坏窗口内硬切；见 §6.2）。  
- **Schema 严格保护**：JSON 中 `variant` 值必须严格为 enum 字符串（如 `"in_in"`），拒绝裸数字硬编码（如 `0`），保证低代码 JSON 的完备可读性。
- **与 `driver_ic` 互斥断言**：显式配置 `driver_ic` 时，由 YAML 中 `ic_to_variant_map` 自动推导；若用户同时填写的 `variant` 与 `driver_ic` 发生冲突，Codegen 编译期直接报错拦截。
- **画布拓扑重新排布标识 (`affects_pins`)**：若变体切换涉及物理管脚增加或减少（如 `dc_motor` 的 `in_in` 3脚 vs `phase_enable` 2脚），可在 YAML 标注 `affects_pins: true`，引导 UniSim 和前端重新排布物理 Overlay 引脚。
- `led` / `button` / `rc_servo` 等无同族变体登记者：**不**在 YAML 增加空的 `variant` 字段。

`mono_oled.yaml`：本波次**补登记** `variant`（`ssd1306` / `sh1106`），与头文件对齐；默认省略 = SSD1306。

### 4.4 端到端数据流

```text
wink-app.json
  devices.motor_l: { "type": "dc_motor", "variant": "in_in", ... }
        │
        ▼  wink-tools codegen（读 dc_motor.yaml）
device_tree.c
  static const dal_dc_motor_config_t motor_l_cfg = {
      ...
      .variant = DAL_DC_MOTOR_VARIANT_IN_IN,  /* 或省略，依赖零值 */
  };
  WINK_TRY(dal_dc_motor_init(&motor_l, &motor_l_cfg));
        │
        ▼
dal_dc_motor.c
  switch (dev->config.variant) { case IN_IN: ...; default: UNSUPPORTED; }
```

**不是**：按 `variant` 生成/链接不同 `.c` 文件再 `dlopen` 式选择。

### 4.5 三仓对齐（嵌入式为本设计主范围）

| 仓 | 要求 |
|----|------|
| **wink-ai-embedded** | C ABI、YAML、文档、golden 测试以本设计为准（SSOT） |
| **UniSim** | 变体表键使用 `variant`（或 props 字段与 JSON 同名）；禁止再写「C 统一 drive_mode」 |
| **前端 workbench** | catalog / binder 生成物与 `variant` 对齐；旧 `driveMode` camelCase 若存在，迁移期映射到 JSON `variant`（仅 UI 层，不进入 C） |

嵌入式仓落地不阻塞于 UniSim PR，但文档交叉引用须同步，避免二次事实错误。

---

## 5. 迁移范围与阶段

### 5.1 本波次必改（P0）

| 组件 | 变更 |
|------|------|
| `dal_dc_motor.h/.c` | 枚举/成员/注释 rename；逻辑不变 |
| `dal_encoder.h/.c` | 同上 |
| `dal_mono_oled.h/.c` | `panel_variant` → `variant`；建议 `uint8_t` → `dal_mono_oled_variant_t` |
| `codegen/drivers/{dc_motor,encoder,mono_oled}.yaml` | 字段改名 + map 宏名更新；oled 补字段 |
| 遗留 `wink-tools/.../drivers/dc_motor.py` 等 baseline | 若仍被 golden 引用则同步（`register=False` 类） |
| golden / 单测 / 样本 `wink-app.json` | 键名与期望字符串更新 |
| `dal-best-practices.md`、`wink-app-json-guide.md`、`01-dal-device-abstraction.md` | 心法与字段表回写为 `variant` |
| 评审文档三维表述 | 活规范回写后，旧评审可保留历史用词或加脚注指向本设计 |

### 5.2 明确不做（本波次）

- 实现 `phase_enable` / `pwm_on_in` / encoder `x2`/`x4` 真路径（仍 reserved + fail-closed）。  
- 新增 SPI `mono_oled` 变体（无 C 能力则仿真也不得冒充，见 UniSim P0-2）。  
- 公共 `dal_variant.h`。  
- 从 YAML 自动生成 C 枚举头。  
- Flash override wire：若某 type 的 wire 已序列化旧字段名/布局，须单独评估 bump `wire_version`（`dc_motor`/`encoder` 注释现状多为尚无 override；`mono_oled` 按实况检查）。本设计默认：**无 wire 的 type 只动源 ABI；有 wire 则单列任务**。

### 5.3 建议实施顺序

1. 文档心法回写（可与代码同 PR 或先文档后代码）。  
2. `dc_motor` + `encoder` rename + 测试。  
3. `mono_oled` rename + YAML 补齐 + 测试。  
4. UniSim / 前端跟进（独立 PR，契约以嵌入式头文件 + YAML 为准）。

---

## 6. 兼容性与风险

### 6.1 ABI / 源兼容

- POD 成员改名：对 designated initializer **源码**破坏（`.drive_mode` 不再编译）。  
- 若 Binary SDK 已发布含旧成员名的头 + `.a`：按 ADR-0028 bump；若仍在破坏窗口且无外部消费者，可硬切并在发布说明写明。  
- 枚举**数值**保持与今日一致（0=当前实现路径），避免静默行为变化。

### 6.2 JSON 兼容

**默认：破坏窗口内硬切**，只接受 `variant`。  

若 Owner 要求软迁移：codegen 可短暂接受旧键 `drive_mode` / `decode_mode` / `panel_variant`，**与 `variant` 同时出现则报错**，并打印 deprecated 警告；一个版本周期后删除。本设计推荐硬切，除非已有外部 App JSON 依赖。

### 6.3 风险表

| 风险 | 等级 | 缓解 |
|------|------|------|
| AI/文档仍写 `drive_mode` | 中 | 回写活规范 + lint/schema 只认 `variant` |
| 把 `pull` 等误并进 `variant` | 中 | §2.1 Non-merge 表 + 评审把关 |
| UniSim 继续用统一假名 | 高 | 嵌仓 SSOT + 明确交叉评审关闭 P0-1 |
| `variant` 单词过泛 | 低 | 文档限定「同 type 已登记变体」；YAML 无登记则无该键 |

---

## 7. 验收标准

- [x] 三个目标 type 的公开头文件中，同族变体成员均为 `variant`，枚举类型符合 `dal_<type>_variant_t`。  
- [x] 对应 YAML 字段名为 `variant`，`map` 与头文件宏一致；`mono_oled` 已暴露。  
- [x] codegen golden：默认省略不强制发射；显式非默认值发射 `.variant = …`。  
- [x] 现有行为不变：默认路径成功；reserved 变体 `WINK_ERR_UNSUPPORTED`。  
- [x] `dal-best-practices` / `wink-app-json-guide` / `01-dal-device-abstraction` 三维心法已改为 `variant`，无「全员 drive_mode」表述。  
- [x] `wink lint --pack layering --pack api`（及既有 drivers/user_surface 相关包）通过。  
- [x] 无新增「公共 variant 枚举头」或跨 type 映射表。

---

## 8. 文档与决策流转

本设计 **Accepted 后**：

1. 回写活规范 `01-dal-device-abstraction.md`（机制一示例字段改为 `variant`）。  
2. 回写 `dal-best-practices.md` §3.0 / §3.3 与 `wink-app-json-guide.md`。  
3. 若 Owner 认为「字段名统一」构成长期约束，可补短 ADR（建议标题：`dal-config-variant-field-convention`）；否则本 tech-design + 活规范回写即可，避免 ADR 通胀。  
4. 拆实施计划 → 已写：[2026-07-30-dal-variant-unified-field-plan.md](../../implementation-plans/core/2026-07-30-dal-variant-unified-field-plan.md)（Wave A 可执行；Wave B DCE/UniSim 另列）。  
5. 通知 UniSim 侧关闭/修订「统一 drive_mode」类表述，改对齐本 SSOT。

---

## 9. 开放问题拍板结论（专家评审已拍板）

| # | 问题 | 专家评审拍板裁决与落地指示 |
|---|------|----------------|
| Q1 | JSON 旧键是否保留一个版本兼容？ | **建议保留 1 个 minor 版本的软迁移警告**：Codegen 读到旧键（`drive_mode` / `decode_mode` / `panel_variant`）时自动映射到 `variant` 并打印 `@deprecated` 警告；若与 `variant` 同时出现则报错。下一个版本彻底硬切。 |
| Q2 | `mono_oled` config 是否从 `uint8_t panel_variant` 改为强类型 `dal_mono_oled_variant_t variant`？ | **是**：提升 C 编译期强类型安全检查。*（注：在 config_t 内做类型保护或 8-bit 限定，防止 32 位编译器展开 enum 导致 sizeof 变化）。* |
| Q3 | 是否本波次写独立 ADR？ | **否**：回写活规范 `01-dal-device-abstraction.md` 即可，避免 ADR 通胀。 |
| Q4 | `driver_ic` 糖衣是否保留？ | **保留为可选**：YAML 显式配置 `ic_to_variant_map`；若用户填写的 `driver_ic` 与 `variant` 冲突，Codegen 编译期直接报错。 |
| Q5 | UniSim/前端是否同一里程碑强制对齐？ | **嵌仓先落地 SSOT**：C 侧头文件与 Codegen YAML 率先发布；UniSim 仿真仓与前端随后跟版对齐，禁止仿真仓超前暴露出 C 侧未实现的变体。 |

---

## 10. 小结（评审一句话）

**同族变体在 JSON 与 DAL `config_t` 统一叫 `variant`；枚举类型与取值仍按 type
定义在各自 `dal_*.h`；codegen 直写、DAL 内按 `variant` 选择路径；不共用全局枚举、
不按变体动态换驱动文件。迁移期内允许旧 JSON 键别名归一到 `variant`（非永久双字段映射）。**

