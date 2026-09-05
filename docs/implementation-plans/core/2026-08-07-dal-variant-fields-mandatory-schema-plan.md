# DAL 变体全量字段契约 (`variant_fields`) 强制 Schema 校验与自动裁剪实施计划

| 项 | 内容 |
|---|---|
| **计划名称** | DAL 变体全量字段契约 (`variant_fields`) 强制 Schema 校验与代码/参数自动裁剪落地计划 |
| **创建日期** | 2026-08-07 |
| **目标版本** | Wink OS Codegen Schema v1.2 / DAL Driver Spec v2.0 |
| **关联技术设计** | [`dal-variant-unified-field-design.md`](../../tech-designs/core/2026-07-30-dal-variant-unified-field-design.md)、[`dal-best-practices.md`](../../../wink-micro-os/docs/dal-development-guide/dal-best-practices.md) §3.0.1 |
| **关联规则/规范** | [`00-master-execution-plan.md`](../unisim/00-master-execution-plan.md) §4.2 |
| **核心机制** | 强制 `variant_fields` 字段契约声明、Fail-Closed 校验断言、C 初始化代码参数/引脚自动裁剪 |

---

## 1. 背景与设计目标

### 1.1 现状痛点
1. **拓扑引脚与参数模糊**：过去 DAL 驱动 Schema (`codegen/drivers/<type>.yaml`) 的 `fields` 声明了外设的所有配置字段（超集）。当某个 `type` 包含多个 `variant` 时（如 `dc_motor` 的 `in_in` 3脚 vs `phase_enable` 2脚），Schema 层面无法感知每个变体究竟需要哪些字段。
2. **缺乏必填断言**：用户在 `wink-app.json` 中配置了 `phase_enable` 变体，但如果多填或少填了 `dir_pin_b`，Codegen 与静态 Lint 无法精准给出“该变体不支持或缺少某字段”的断言。
3. **C 代码生成无法自动裁剪**：在生成 `device_tree.c` 的初始化结构体 `dal_<type>_config_t` 时，Codegen 无法自动将当前变体未使用的引脚裁剪为哨兵值 `-1`，也无法对无关软参数进行自动静默或告警。

### 1.2 目标与核心不变式 (Invariants)
1. **升维为 `variant_fields` 契约**：将 `variant_pins` 泛化为包含引脚、接口、参数、阈值的全量 `variant_fields` 契约字典。
2. **强制必填断言 (Mandatory Fail-Closed Guard)**：
   - 规则：**只要一个外设 `type` 定义了 2 个及以上的 `variant` 枚举值，YAML 的 `fields.variant` 下必须包含 `variant_fields` 属性，且必须覆盖每个变体**。
   - 效果：如果不填或漏填变体 Key，`wink.py lint --pack drivers` 与 `yaml_schema.py` **直接抛出 `YamlSchemaError` 拦截构建**。
3. **引脚与参数自动裁剪 (Automatic Trimming & Precision Emission)**：
   - Codegen 在读取 `wink-app.json` 的激活变体时，依据 `variant_fields[active_variant]`：
     - **引脚自动裁剪**：未被当前变体使用的引脚字段自动发射为 `-1` 哨兵值。
     - **参数自动校验**：误填了当前变体不需要的独立参数时，发射编译期裁剪警告。
4. **单仓闭环与三仓零 Drift**：C 侧单仓即可完成 100% 精准的变体字段校验，并通过 `gen-frontend-binders --check` 保证与前端/UniSim 物理引脚切片零漂移。

---

## 2. YAML Schema 规范定义 (Schema 1.2)

在 `wink-micro-os/codegen/drivers/<type>.yaml` 中，`variant` 字段必须按以下格式声明 `variant_fields`：

```yaml
type: dc_motor
fields:
  pwm_channel: { type: int }
  dir_pin_a:   { type: int }
  dir_pin_b:   { type: int, default: -1 }
  enable_pin:  { type: int, default: -1 }
  variant:
    tier: advanced
    type: enum
    affects_pins: true
    enum: [in_in, phase_enable]
    
    # 强制必填：变体全量字段契约 (variant_fields)
    variant_fields:
      in_in:        [pwm_channel, dir_pin_a, dir_pin_b, enable_pin]
      phase_enable: [pwm_channel, dir_pin_a, enable_pin]
```

### 校验约束断言 (Validation Rules)
1. `variant_fields` 必须是一个映射（Mapping）。
2. `variant_fields` 的 Key 集合必须与 `variant.enum` 的字符串列表 **100% 完全匹配**（不能多也不能少）。
3. `variant_fields` 的 Value 必须是一个字符串列表，列表中的每个字段名必须存在于 `fields` 块中。
4. **硬门禁**：若 `len(variant.enum) > 1` 且未声明 `variant_fields`，解析器直接抛出 `YamlSchemaError: 'variant_fields' is mandatory for multi-variant drivers`。

---

## 3. 详细实现组件与改动范围

### 3.1 组件 1：`wink-tools` Codegen Schema 解析器更新

#### 修改文件：
* `wink-tools/tools/codegen/schema/yaml_schema.py`
* `wink-tools/tools/codegen/schema/driver_record.py`

#### 改动要点：
1. 在 `_FIELD_ENTRY_KNOWN_KEYS` 白名单集合中增加 `"variant_fields"`。
2. 在 `FieldSpec` 数据类中增加 `variant_fields: dict[str, list[str]] | None = None` 成员。
3. 在 `_parse_field_entry()` 中解析并强类型校验 `variant_fields`：
   ```python
   variant_fields = raw.get("variant_fields")
   if variant_fields is not None:
       if not isinstance(variant_fields, dict):
           raise YamlSchemaError(f"fields.{name}.variant_fields must be a mapping in {path}")
       # 校验 key 匹配 enum，val 匹配 fields
   ```
4. 在 `_parse_fields_block()` 结尾增加硬断言：
   ```python
   if variant_spec and len(variant_spec.enum_values or []) > 1 and not variant_spec.variant_fields:
       raise YamlSchemaError(
           f"fields.variant.variant_fields is mandatory when multiple variants exist in {path}"
       )
   ```

---

### 3.2 组件 2：`wink-tools` 静态 Lint 规则扩展

#### 修改文件：
* `wink-tools/tools/lint/packs/drivers.py`
* `wink-tools/tools/lint/packs/dal_yaml_parity.py`

#### 新增 Lint 规则：
1. **`drivers.variant_fields_mandatory` (error 级别)**：
   扫描全量 `codegen/drivers/*.yaml`，断言多变体外设必须包含 `variant_fields`。
2. **`drivers.variant_fields_valid_references` (error 级别)**：
   检查 `variant_fields` 中列出的字段名是否确实定义在 `fields` 中。
3. **`drivers.variant_fields_affects_pins_consistency` (warn 级别)**：
   比对各 variant 列表中的 `*_pin` 字段：若不同 variant 的 pin 字段集合不一致，校验 `affects_pins` 是否为 `true`。

---

### 3.3 组件 3：Codegen C 初始化代码自动裁剪与精准生成

#### 修改文件：
* `wink-tools/tools/codegen/generators/c_init.py` (及相应的 Jinja2 初始化模板)
* `wink-tools/tools/codegen/schema/variant_normalize.py`

#### 自动裁剪逻辑：
1. 读取 `wink-app.json` 中该设备实例的选定变体 `active_variant`（若省写则用 `default_variant`）。
2. 获取当前变体允许的字段列表 `allowed_fields = variant_spec.variant_fields[active_variant]`。
3. 在生成 `static const dal_<type>_config_t cfg = { ... };` 结构体赋值时：
   - 对于 `fields` 中声明的所有脚位字段（如 `dir_pin_b`）：如果 `fname not in allowed_fields`，自动裁剪并赋值为 **`-1` 哨兵值**。
   - 对于 `fields` 中的非引脚参数（如 `deadzone_promille`）：如果 `fname not in allowed_fields` 且用户在 JSON 中填写了该值，Codegen 打印警告并自动裁剪抛弃。
   - 针对 `allowed_fields` 中标记为 `required: true` 的字段：如果用户 JSON 未填写，Codegen **直接报错终止**。

---

### 3.4 组件 4：存量 9 个 DAL 驱动 YAML 全量迁移更新

#### 修改文件：
* `wink-micro-os/codegen/drivers/dc_motor.yaml`
* `wink-micro-os/codegen/drivers/relay.yaml`
* `wink-micro-os/codegen/drivers/keypad.yaml`
* `wink-micro-os/codegen/drivers/analog_knob.yaml`
* `wink-micro-os/codegen/drivers/mono_oled.yaml`
* `wink-micro-os/codegen/drivers/encoder.yaml`
* `wink-micro-os/codegen/drivers/buzzer.yaml`
* `wink-micro-os/codegen/drivers/button.yaml`
* `wink-micro-os/codegen/drivers/led.yaml`

#### 迁移要点：
为所有包含多变体（或未来有预留变体）的 YAML 显式补全 `variant_fields` 契约，例如：

```yaml
# 1. dc_motor.yaml
variant:
  tier: advanced
  type: enum
  affects_pins: true
  enum: [in_in, phase_enable]
  variant_fields:
    in_in: [pwm_channel, dir_pin_a, dir_pin_b, enable_pin]
    phase_enable: [pwm_channel, dir_pin_a, enable_pin]

# 2. relay.yaml
variant:
  tier: advanced
  type: enum
  affects_pins: true
  enum: [direct_gpio, ssr, latching_dual_pin]
  variant_fields:
    direct_gpio: [gpio_pin, active_low]
    ssr: [gpio_pin, active_low]
    latching_dual_pin: [gpio_pin, reset_pin, active_low]

# 3. analog_knob.yaml
variant:
  tier: advanced
  type: enum
  affects_pins: false
  enum: [standard, logarithmic, center_detent]
  variant_fields:
    standard: [gpio_pin, enable_pin, min_mv, max_mv]
    logarithmic: [gpio_pin, enable_pin, min_mv, max_mv]
    center_detent: [gpio_pin, enable_pin, min_mv, max_mv, deadzone_promille]
```

---

## 4. 实施阶段与步骤计划 (Phase Breakdown)

```text
┌──────────────────────────────────────────────────────────────────────────────┐
│ Phase 1: Schema 解析器与 Python 单测 (wink-tools)                            │
│ 1.1 修改 yaml_schema.py 增加 variant_fields 解析与必填强断言                      │
│ 1.2 编写 test_fields_schema.py 测试用例                                       │
└──────────────────────────────────────────────────────────────────────────────┘
                                       │
                                       ▼
┌──────────────────────────────────────────────────────────────────────────────┐
│ Phase 2: Lint 规则增加与存量 YAML 全量迁移 (wink-micro-os)                      │
│ 2.1 在 drivers.py 中添加 drivers.variant_fields_mandatory 门禁                 │
│ 2.2 迁移全量 9 个现有外设 YAML，补齐 variant_fields 契约                        │
│ 2.3 运行 python wink-tools/wink.py lint --pack drivers 验证 0 Error           │
└──────────────────────────────────────────────────────────────────────────────┘
                                       │
                                       ▼
┌──────────────────────────────────────────────────────────────────────────────┐
│ Phase 3: Codegen 初始化代码自动裁剪与生成逻辑重构                             │
│ 3.1 重构 c_init.py，按 active_variant 的 variant_fields 进行引脚/参数自动裁剪 │
│ 3.2 增加单元测试，验证 phase_enable 变体自动将 dir_pin_b 发射为 -1             │
└──────────────────────────────────────────────────────────────────────────────┘
                                       │
                                       ▼
┌──────────────────────────────────────────────────────────────────────────────┐
│ Phase 4: 文档回写与模板 SSOT 固化                                             │
│ 4.1 回写 dal-best-practices.md §3.0.1 增加 variant_fields 必填说明           │
│ 4.2 回写 00-master-execution-plan.md §4.2 规范模版                           │
└──────────────────────────────────────────────────────────────────────────────┘
```

---

## 5. 验证与测试计划 (Verification Plan)

### 5.1 自动化单元测试
1. **Schema 必填解析测试**：
   运行 `pytest tools/codegen/tests/test_fields_schema.py`，验证多变体未填写 `variant_fields` 时精准抛出 `YamlSchemaError`。
2. **Codegen 自动裁剪测试**：
   运行 `pytest tools/codegen/tests/test_motor_encoder.py`，验证 `dc_motor` 的 `phase_enable` 变体生成的 `device_tree.c` 中 `.dir_pin_b = -1` 被自动正确裁剪。

### 5.2 静态 Lint 门禁全检测
在 `wink-micro-os` 根目录下运行：
```bash
python wink-tools/wink.py lint --pack layering --pack api --pack drivers --pack dal --pack abi --pack user_surface
```
确保全包扫描无新增 error。

### 5.3 三仓闭环对比校验
在 `packages/unisim` 下运行：
```bash
bun tools/gen-frontend-binders.ts --check
```
确保由 C 侧 YAML `variant_fields` 驱动导出的前端 Binder 变体切片与 UniSim 仿真器 `PIN_VARIANTS` 保持 100% 无缝一致！

