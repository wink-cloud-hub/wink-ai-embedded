# DAL `variant` 同族变体字段统一 — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `subagent-driven-development` (recommended) or `executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.  
> Domain skill: `embedded-best-practice`（静态分发 / 负数错误码 / 双 target）；文档对照 `.claude/rules/docs-adr.md`。

**Goal:** 将 `dc_motor` / `encoder` / `mono_oled` 的同族变体 JSON 键与 C `config_t` 成员统一为 `variant`；codegen 直写；旧键软迁移一个 minor；活规范回写；不实现新拓扑、不做 `HAS_*` DCE 重构。

**Architecture:** JSON 键 = C 成员 = `variant`；枚举类型仍按 type 定义在各自 `dal_*.h`；`app_codegen` 在约束校验前把旧键归一到 `variant` 并告警；YAML SSOT 更新 `map` 宏名；DAL `.c` 仅改成员/枚举名，fail-closed 行为保持。

**Tech Stack:** C99（host Unity）、`wink-micro-os/codegen/drivers/*.yaml`、`wink-tools` codegen / lint / `wink.py test`、Markdown 活规范。

## Global Constraints

- SSOT 设计：[2026-07-30-dal-variant-unified-field-design.md](../../tech-designs/core/2026-07-30-dal-variant-unified-field-design.md)（执行前状态须为 **Accepted**）
- **本波次 = Wave A only**（见 §2）；Wave B 禁止混入同一组提交
- ADR-0004：禁止为抽象引入 vtable / 公共 `dal_variant.h`
- ADR-0034：省略 `variant` → C 零值 = 今日已实现路径（`in_in` / `x1_rising` / `ssd1306`）
- 枚举**数值**不得改变（0/1/2 与今日一致）
- JSON：`variant` 必须是 **enum 字符串**；裸数字已由 `_coerce_type(..., "enum")` 拒绝——本计划补测试锁死
- 旧键软迁移（Q1）：`drive_mode` / `decode_mode` / `panel_variant` → 归一为 `variant` + stderr `@deprecated` 警告；与 `variant` **同时出现 → error**
- **无跨字段永久映射**：软迁移仅迁移期别名，不是 `drive_mode` 长期双写
- Commit：英文、按 Task 原子提交；**未经用户明确要求不要 `git commit`**
- 验收：`python wink-tools/wink.py test`；`python wink-tools/wink.py lint --pack layering --pack api --pack user_surface`

---

## 1. 元数据

| 字段 | 内容 |
|------|------|
| **计划编号** | `PLAN-20260730-DAL-VARIANT` |
| **创建日期** | 2026-07-30 |
| **计划状态** | ✅ Wave A 已完成（2026-07-30） |
| **优先级** | 🔴 P0（命名 SSOT / 防 UniSim 再误读） |
| **计划版本** | `v1.0` |
| **关联技术设计** | [2026-07-30-dal-variant-unified-field-design.md](../../tech-designs/core/2026-07-30-dal-variant-unified-field-design.md) |
| **关联活规范** | `01-dal-device-abstraction.md`；`dal-best-practices.md`；`wink-app-json-guide.md` |
| **关联 ADR** | ADR-0004、ADR-0034、ADR-0046、ADR-0051；**不新开 ADR**（设计 Q3） |
| **前置** | 设计 §9 已拍板；破坏窗口仍开放 |

---

## 2. Wave 边界（必须守住）

### 2.1 Wave A — 本计划必做

| # | 交付物 |
|---|--------|
| A0 | 设计文档标 Accepted + §10 与 Q1 措辞对齐 |
| A1 | codegen 旧键软迁移 + 双写报错 + 单测 |
| A2 | YAML：`drive_mode`/`decode_mode` → `variant`；`mono_oled` 补 `variant`；可选 `affects_pins` 元数据 |
| A3 | `dal_dc_motor` rename + host 单测 |
| A4 | `dal_encoder` rename + host 单测 |
| A5 | `dal_mono_oled` rename + 强类型存储约定 + SH1106 fail-closed + YAML + 单测 |
| A6 | 遗留 `wink-tools/.../drivers/{dc_motor,encoder}.py` baseline 同步 |
| A7 | 可选 `driver_ic` + `ic_to_variant_map`（仅 `dc_motor` 最小落地） |
| A8 | 活规范 / 手册回写 |
| A9 | 出口闸：全量 test + lint |

### 2.2 Wave B — 本计划明确不做（另立计划）

| 项 | 原因 |
|----|------|
| `WINK_<TYPE>_HAS_<VALUE>` 并集裁剪 + init 大 `switch` 重构 | 设计 §4.2 示意；现网仍是 `!= default → UNSUPPORTED`，改动面大 |
| UniSim / 前端消费 `affects_pins` / props.`variant` | 嵌仓先发 SSOT（设计 Q5） |
| 实现 `phase_enable` / `pwm_on_in` / encoder x2/x4 / SH1106 真路径 | 仍 reserved |
| 公共 `dal_variant.h` / YAML 生成 C 枚举头 | 设计 Non-goals |
| Flash wire bump | 三 type 现状**无** `apply_override` 序列化实现（已核 `dal_dc_motor.h`/`dal_encoder.h` 注释：`No apply_override wire yet. Future serialization follows config member order.`）；未来序列化按**成员顺序**而非字段名，改名安全。Task 9 Step 1 显式 grep `apply_override` 留档 |

### 2.3 成功指标

| 指标 | 通过标准 |
|------|----------|
| 命名 | 三 type 头文件成员均为 `variant`；无公开 `drive_mode`/`decode_mode`/`panel_variant` 成员 |
| Codegen | 新键发射 `.variant = DAL_*_VARIANT_*`；默认省略不发射；旧键有警告且仍能生成 |
| 行为 | 默认 init 成功；reserved 变体 `WINK_ERR_UNSUPPORTED` |
| oled | `sizeof` 中 `variant` 仍为 1 字节（`uint8_t` 存储）；非 0 变体 init fail-closed |
| 文档 | 三维心法写 `variant`；无「C 全员 drive_mode」 |
| 回归 | `wink.py test` 全绿；相关 lint packs 无新增 error |

---

## 3. 文件清单（Wave A）

| 路径 | 变更 |
|------|------|
| `docs/tech-designs/core/2026-07-30-dal-variant-unified-field-design.md` | 状态 Accepted；§10 措辞 |
| `wink-tools/tools/codegen/variant_normalize.py` | **新建**：旧键归一 + `driver_ic` 推导 |
| `wink-tools/tools/codegen/app_codegen.py` | 在约束校验前调用归一 |
| `wink-tools/tools/codegen/tests/test_variant_normalize.py` | **新建** |
| `wink-tools/tools/codegen/tests/test_motor_encoder.py` | 断言改名 + 旧键用例 |
| `wink-micro-os/codegen/drivers/dc_motor.yaml` | `variant` + map + `affects_pins` + `ic_to_variant_map` |
| `wink-micro-os/codegen/drivers/encoder.yaml` | `variant` + map |
| `wink-micro-os/codegen/drivers/mono_oled.yaml` | 新增 `variant` |
| `wink-micro-os/dal/include\|src/actuator/dal_dc_motor.*` | 枚举/成员 rename |
| `wink-micro-os/dal/include\|src/sensor/dal_encoder.*` | 同上 |
| `wink-micro-os/dal/include\|src/display/dal_mono_oled.*` | rename + fail-closed |
| `wink-micro-os/test/unit/dal/test_dal_dc_motor.c` | 符号更新 |
| `wink-micro-os/test/unit/dal/test_dal_encoder.c` | 符号更新 |
| `wink-micro-os/test/unit/dal/test_dal_mono_oled.c`（若无则新建最小用例） | variant fail-closed |
| `wink-tools/tools/codegen/drivers/dc_motor.py` | baseline 同步（`register=False`） |
| `wink-tools/tools/codegen/drivers/encoder.py` | 同上（若存在旧字段） |
| `wink-micro-os/docs/dal-development-guide/dal-best-practices.md` | §3.0/§3.3 |
| `wink-micro-os/docs/wink-app-json-guide.md` | 字段表 |
| `docs/design/02-wink-micro-os/01-dal-device-abstraction.md` | 机制一 + 摘要表 |
| `wink-tools/tools/codegen/drivers/base.py` | 注释中的旧字段名 |

---

## 4. 任务详情

### Task 0: 设计 Accepted + 措辞对齐

**Files:**
- Modify: `docs/tech-designs/core/2026-07-30-dal-variant-unified-field-design.md`

**Interfaces:** 无代码接口；后续 Task 以 Accepted 设计为准。

- [ ] **Step 1:** 将表头「状态」改为：`**Accepted（Wave A）** — 2026-07-30`；关联本计划路径。

- [ ] **Step 2:** 改写 §10 小结，消除与 Q1 的矛盾。目标文案：

```markdown
**同族变体在 JSON 与 DAL `config_t` 统一叫 `variant`；枚举类型与取值仍按 type
定义在各自 `dal_*.h`；codegen 直写、DAL 内按 `variant` 选择路径；不共用全局枚举、
不按变体动态换驱动文件。迁移期内允许旧 JSON 键别名归一到 `variant`（非永久双字段映射）。**
```

- [ ] **Step 3:** 在设计 §8 第 4 点改为已指向本计划：  
  `implementation-plans/2026-07-30-dal-variant-unified-field-plan.md`

- [ ] **Step 4:**（可选，用户要求时再 commit）  
  `docs(design): accept dal variant unified field design`

---

### Task 1: Codegen 旧键软迁移（先写测试）

**Files:**
- Create: `wink-tools/tools/codegen/variant_normalize.py`
- Create: `wink-tools/tools/codegen/tests/test_variant_normalize.py`
- Modify: `wink-tools/tools/codegen/app_codegen.py`（在 `evaluate_device_constraints` 之前调用）

**Interfaces:**
- Produces: `normalize_device_variant_fields(dev_name: str, type_name: str, spec: dict) -> tuple[dict, list[str]]`  
  返回 `(new_spec, warning_messages)`；错误用抛 `YamlSchemaError` 或返回由 caller `_die`。
- Consumes: device `type` 字符串与 raw device spec dict。

- [ ] **Step 1: 写失败测试**

```python
# wink-tools/tools/codegen/tests/test_variant_normalize.py
from tools.codegen.variant_normalize import normalize_device_variant_fields
from tools.codegen.yaml_schema import YamlSchemaError
import pytest

def test_legacy_drive_mode_maps_to_variant():
    spec, warns = normalize_device_variant_fields(
        "m1", "dc_motor", {"type": "dc_motor", "drive_mode": "phase_enable", "pwm_channel": 1}
    )
    assert spec["variant"] == "phase_enable"
    assert "drive_mode" not in spec
    assert any("@deprecated" in w and "drive_mode" in w for w in warns)

def test_variant_and_legacy_conflict():
    with pytest.raises(YamlSchemaError, match="variant"):
        normalize_device_variant_fields(
            "m1", "dc_motor",
            {"variant": "in_in", "drive_mode": "phase_enable"},
        )

def test_variant_rejects_bare_int_via_constraints_path():
    # 集成：build_context 或 evaluate_device_constraints 对 variant: 0 报错
    ...
```

- [ ] **Step 2: 跑测试确认失败**

```bash
cd wink-tools && python -m pytest tools/codegen/tests/test_variant_normalize.py -v
```

Expected: FAIL（模块不存在）

- [ ] **Step 3: 实现 `variant_normalize.py`**

```python
"""Normalize legacy same-family variant JSON keys to ``variant`` (Wave A)."""
from __future__ import annotations

from typing import Any

from .yaml_schema import YamlSchemaError

# type → legacy JSON keys that mean the same as ``variant``
_LEGACY_VARIANT_KEYS: dict[str, tuple[str, ...]] = {
    "dc_motor": ("drive_mode",),
    "encoder": ("decode_mode",),
    "mono_oled": ("panel_variant",),
}

def normalize_device_variant_fields(
    dev_name: str,
    type_name: str,
    spec: dict[str, Any],
) -> tuple[dict[str, Any], list[str]]:
    legacy_keys = _LEGACY_VARIANT_KEYS.get(type_name, ())
    present_legacy = [k for k in legacy_keys if k in spec]
    warnings: list[str] = []
    out = dict(spec)

    if present_legacy and "variant" in out:
        raise YamlSchemaError(
            f"device '{dev_name}': cannot set both 'variant' and legacy "
            f"key(s) {present_legacy}; use only 'variant'"
        )
    if len(present_legacy) > 1:
        raise YamlSchemaError(
            f"device '{dev_name}': multiple legacy variant keys {present_legacy}"
        )
    if present_legacy:
        old = present_legacy[0]
        out["variant"] = out.pop(old)
        warnings.append(
            f"device '{dev_name}': @deprecated JSON key '{old}' — "
            f"use 'variant' instead (will be removed next minor)"
        )
    return out, warnings
```

- [ ] **Step 4: 接入 `app_codegen.build_context`**

在「Resolve drivers and validate fields/roles」循环内，于 `evaluate_device_constraints` **之前**：

```python
from .variant_normalize import normalize_device_variant_fields
# ...
spec, var_warns = normalize_device_variant_fields(name, driver.type, spec)
for w in var_warns:
    print(f"[codegen] warning: {w}", file=sys.stderr)
    warnings.warn(w, UserWarning, stacklevel=2)
# then evaluate_device_constraints(...)
```

注意：`driver.type` 须在 `_resolve_driver` 之后已知；若 type 来自 spec，用 `spec["type"]` 亦可，但须与 registry 一致。

- [ ] **Step 5: 跑测试通过**

```bash
cd wink-tools && python -m pytest tools/codegen/tests/test_variant_normalize.py tools/codegen/tests/test_motor_encoder.py -v
```

（此时 motor_encoder 旧断言可能仍红——Task 2 再改；本 Task 至少 normalize 单测全绿。）

- [ ] **Step 6:**（可选 commit）`feat(codegen): normalize legacy variant JSON keys to variant`

---

### Task 2: YAML SSOT 字段改名（dc_motor / encoder）+ codegen 断言更新

**Files:**
- Modify: `wink-micro-os/codegen/drivers/dc_motor.yaml`
- Modify: `wink-micro-os/codegen/drivers/encoder.yaml`
- Modify: `wink-tools/tools/codegen/tests/test_motor_encoder.py`

**Interfaces:**
- Consumes: Task 1 归一化后的 `spec["variant"]`
- Produces: convention emit `.variant = DAL_*_VARIANT_*`

- [ ] **Step 1: 改 `dc_motor.yaml` 字段块**（替换原 `drive_mode:`）

```yaml
  variant:
    tier: advanced
    type: enum
    emit_when: present
    affects_pins: true   # UniSim/前端消费；codegen `_parse_field_entry` 仅 .get() 已知键，未知键静默忽略——Wave B 消费前需在 schema 加白名单，否则 typo 不报错
    enum: [in_in, phase_enable, pwm_on_in]
    map:
      in_in: DAL_DC_MOTOR_VARIANT_IN_IN
      phase_enable: DAL_DC_MOTOR_VARIANT_PHASE_ENABLE
      pwm_on_in: DAL_DC_MOTOR_VARIANT_PWM_ON_IN
```

- [ ] **Step 2: 改 `encoder.yaml`**

```yaml
  variant:
    tier: advanced
    type: enum
    emit_when: present
    enum: [x1_rising, x2, x4]
    map:
      x1_rising: DAL_ENCODER_VARIANT_X1_RISING
      x2: DAL_ENCODER_VARIANT_X2
      x4: DAL_ENCODER_VARIANT_X4
```

- [ ] **Step 3: 更新 `test_motor_encoder.py`**

| 旧断言 | 新断言 |
|--------|--------|
| `assertNotIn(".drive_mode", init)` | `assertNotIn(".variant", init)`（默认省略） |
| `"drive_mode": "phase_enable"` 输入 | 主路径改 `"variant": "phase_enable"` |
| `.drive_mode = DAL_DC_MOTOR_MODE_PHASE_ENABLE` | `.variant = DAL_DC_MOTOR_VARIANT_PHASE_ENABLE` |
| encoder `decode_mode` 同理 | `variant` / `DAL_ENCODER_VARIANT_X2` |

另增用例：

```python
def test_motor_legacy_drive_mode_emits_variant_with_warning(self) -> None:
    cfg = {..., "devices": {"left_motor": {
        "type": "dc_motor", "pwm_channel": 1, "dir_pin_a": 4,
        "drive_mode": "phase_enable",
    }}}
    with self.assertWarns(UserWarning):
        ctx = app_codegen.build_context(cfg, "test.json")
    init = ctx["devices"][0]["config_init"]
    self.assertIn(".variant = DAL_DC_MOTOR_VARIANT_PHASE_ENABLE", init)
    self.assertNotIn(".drive_mode", init)
```

- [ ] **Step 4: 跑测试**

```bash
cd wink-tools && python -m pytest tools/codegen/tests/test_motor_encoder.py -v
```

Expected: PASS（若 C 头尚未 rename，emit 字符串已是新宏名——头文件 Task 3/4 必须紧随，否则 host 编译会挂；**建议 Task 2 与 Task 3 同会话连续完成后再跑 wink.py test**）

- [ ] **Step 5:**（可选 commit）与 Task 3 合并提交亦可：`refactor(dal+codegen): rename dc_motor drive_mode to variant`

---

### Task 3: `dal_dc_motor` C 侧 rename

**Files:**
- Modify: `wink-micro-os/dal/include/actuator/dal_dc_motor.h`
- Modify: `wink-micro-os/dal/src/actuator/dal_dc_motor.c`
- Modify: `wink-micro-os/test/unit/dal/test_dal_dc_motor.c`

**Interfaces:**
- Produces: `dal_dc_motor_variant_t`；成员 `variant`；常量 `DAL_DC_MOTOR_VARIANT_*`
- 行为：`cfg->variant != DAL_DC_MOTOR_VARIANT_IN_IN` → `WINK_ERR_UNSUPPORTED`（保持今日）

- [ ] **Step 1: 改头文件枚举与成员**

```c
typedef enum {
    DAL_DC_MOTOR_VARIANT_IN_IN = 0,        /* default — today's PWM + IN_A + IN_B */
    DAL_DC_MOTOR_VARIANT_PHASE_ENABLE = 1, /* reserved */
    DAL_DC_MOTOR_VARIANT_PWM_ON_IN = 2,    /* reserved */
} dal_dc_motor_variant_t;

/* in dal_dc_motor_config_t: */
dal_dc_motor_variant_t variant; /* 0 = IN_IN (today's path) */
```

删除 `dal_dc_motor_drive_mode_t` / `drive_mode` / `DAL_DC_MOTOR_MODE_*`。  
全文件注释中的 `drive_mode` 改为 `variant`。

- [ ] **Step 2: 改 `.c`**

```c
if (cfg->variant != DAL_DC_MOTOR_VARIANT_IN_IN) {
    return WINK_ERR_UNSUPPORTED;
}
```

- [ ] **Step 3: 改 Unity 测试**

- 函数名可改为 `test_dc_motor_unimplemented_variant_init_unsupported`
- `.drive_mode = DAL_DC_MOTOR_MODE_PHASE_ENABLE` → `.variant = DAL_DC_MOTOR_VARIANT_PHASE_ENABLE`
- `RUN_TEST(...)` 同步

- [ ] **Step 4: 跑 DAL 电机单测**

```bash
python wink-tools/wink.py test --filter dal_dc_motor
```

（若 CLI 无 filter，则跑全量 `python wink-tools/wink.py test`，至少确认 `test_dal_dc_motor` 相关输出 PASS）

Expected: PASS

- [ ] **Step 5:**（可选 commit）`refactor(dal): rename dc_motor drive_mode to variant`

---

### Task 4: `dal_encoder` C 侧 rename

**Files:**
- Modify: `wink-micro-os/dal/include/sensor/dal_encoder.h`
- Modify: `wink-micro-os/dal/src/sensor/dal_encoder.c`
- Modify: `wink-micro-os/test/unit/dal/test_dal_encoder.c`

**Interfaces:**
- Produces: `dal_encoder_variant_t`；`DAL_ENCODER_VARIANT_X1_RISING` / `_X2` / `_X4`；成员 `variant`

- [ ] **Step 1: 头文件**

```c
typedef enum {
    DAL_ENCODER_VARIANT_X1_RISING = 0,
    DAL_ENCODER_VARIANT_X2 = 1,
    DAL_ENCODER_VARIANT_X4 = 2,
} dal_encoder_variant_t;

/* config: */
dal_encoder_variant_t variant; /* 0 = x1_rising */
```

- [ ] **Step 2: `.c`**

```c
if (cfg->variant != DAL_ENCODER_VARIANT_X1_RISING) {
    return WINK_ERR_UNSUPPORTED;
}
```

- [ ] **Step 3: 测试** — 所有 `.decode_mode` / `DAL_ENCODER_DECODE_*` / 测试函数名中的 `decode_mode` 改为 `variant` / `DAL_ENCODER_VARIANT_*`。

- [ ] **Step 4: 跑测**

```bash
python wink-tools/wink.py test
```

Expected: encoder 相关 PASS；与 Task 2/3 一起后 codegen+host 全绿。

- [ ] **Step 5:**（可选 commit）`refactor(dal): rename encoder decode_mode to variant`

---

### Task 5: `dal_mono_oled` rename + 强类型存储 + YAML + fail-closed

**Files:**
- Modify: `wink-micro-os/dal/include/display/dal_mono_oled.h`
- Modify: `wink-micro-os/dal/src/display/dal_mono_oled.c`
- Modify: `wink-micro-os/codegen/drivers/mono_oled.yaml`
- Modify or Create: `wink-micro-os/test/unit/dal/test_dal_mono_oled.c`（先 `Glob`/`rg` 确认是否已有）

**背景:** 今日 `dal_mono_oled_config_t.panel_variant` **已是裸 `uint8_t`**（非 enum 成员）；旧枚举 `dal_mono_oled_panel_variant_t` 定义了却**未绑定为成员类型**。且今日 `.c` **未读取** 该字段，SH1106 未实现。本 Task：①把裸 `uint8_t` 升级为 `typedef uint8_t dal_mono_oled_variant_t` 强类型别名并绑定枚举常量（sizeof 天然不变，1 字节）；②删除未使用旧 enum；③init 诚实 fail-closed，禁止静默当 SSD1306。

**Interfaces:**
- Produces: 存储宽度保持 **1 字节**：

```c
typedef uint8_t dal_mono_oled_variant_t;

enum {
    DAL_MONO_OLED_VARIANT_SSD1306 = 0,
    DAL_MONO_OLED_VARIANT_SH1106  = 1,
};

typedef struct {
    ...
    dal_mono_oled_variant_t variant; /* 0=SSD1306; values from enum above */
} dal_mono_oled_config_t;

/* 编译期守卫：wink_app.h 提供 static_assert 兼容宏（pre-C11） */
static_assert(sizeof(dal_mono_oled_variant_t) == 1,
              "variant must stay 1 byte to keep dal_mono_oled_config_t layout stable");
```

删除 `dal_mono_oled_panel_variant_t` / `panel_variant` / `DAL_MONO_OLED_PANEL_*`。

> **dc_motor / encoder 同办**：其 `variant` 是 enum 成员（编译器可能取 4 字节）。在各自 `.h` 加
> `static_assert(sizeof(dal_dc_motor_config_t) == <改前值>, "config_t layout regression");`
> 锁死改名过程不误动底层布局（改前先 `printf sizeof` 或看现网 `_Static_assert` 取基线）。

- [ ] **Step 1: 改头文件**（如上）；注释写明：`typedef uint8_t` 保持 1 字节存储，配 `static_assert` 锁死 `sizeof`；今日成员本已是裸 `uint8_t`，本改动是引入强类型别名 + 绑定枚举常量，布局不变。

- [ ] **Step 2: 在 `dal_mono_oled_init` 早返回处（尺寸校验附近）加入**

```c
if (cfg->variant != DAL_MONO_OLED_VARIANT_SSD1306) {
    return WINK_ERR_UNSUPPORTED;
}
```

- [ ] **Step 3: YAML 增加字段**

```yaml
  variant:
    tier: advanced
    type: enum
    emit_when: present
    enum: [ssd1306, sh1106]
    map:
      ssd1306: DAL_MONO_OLED_VARIANT_SSD1306
      sh1106: DAL_MONO_OLED_VARIANT_SH1106
```

- [ ] **Step 4: 单测**

```c
void test_mono_oled_sh1106_variant_unsupported(void)
{
    dal_mono_oled_t dev;
    dal_mono_oled_config_t cfg = {
        .owner = "t",
        .i2c_addr = 0x3C,
        .width = 128,
        .height = 64,
        .i2c_port = 0,
        .variant = DAL_MONO_OLED_VARIANT_SH1106,
    };
    TEST_ASSERT_EQUAL(WINK_ERR_UNSUPPORTED, dal_mono_oled_init(&dev, &cfg));
}
```

若尚无测试文件：按现有 `test_dal_*.c` + CMake 登记方式新增（参考 `test_dal_dc_motor.c` 的 `CMakeLists`/`unity` 注册；用 `rg test_dal_dc_motor` 定位登记点并复制模式）。

- [ ] **Step 5: codegen 单测（可放 `test_motor_encoder.py` 旁新建 `test_mono_oled_variant.py`）**

```python
def test_mono_oled_variant_emit():
    cfg = {"app_name": "o", "board": "esp32_devkitc", "devices": {
        "oled": {"type": "mono_oled", "i2c_bus": 0, "variant": "sh1106"}
    }}
    init = app_codegen.build_context(cfg, "t.json")["devices"][0]["config_init"]
    assert ".variant = DAL_MONO_OLED_VARIANT_SH1106" in init
```

（`i2c_bus` 等必填以 `mono_oled.yaml` 为准。）

- [ ] **Step 6: 跑测**

```bash
cd wink-tools && python -m pytest tools/codegen/tests/test_mono_oled_variant.py -v
python wink-tools/wink.py test
```

- [ ] **Step 7:**（可选 commit）`refactor(dal): rename mono_oled panel_variant to variant`

---

### Task 6: 遗留 Python baseline 插件同步

**Files:**
- Modify: `wink-tools/tools/codegen/drivers/dc_motor.py`
- Modify: `wink-tools/tools/codegen/drivers/encoder.py`（若含 `decode_mode`）
- Modify: `wink-tools/tools/codegen/drivers/base.py`（注释）

**说明:** 这些类 `register = False`，SSOT 是 YAML；但仍被部分对比/文档引用。必须与 YAML/C 宏名一致，避免后人复制旧名。

- [ ] **Step 1:** `dc_motor.py`：`advanced_fields` 中 `drive_mode` → `variant`；`_DRIVE_MODE_TO_C` → `_VARIANT_TO_C` 且值为 `DAL_DC_MOTOR_VARIANT_*`；`render_config_init` 读写 `variant`。

- [ ] **Step 2:** `encoder.py`：同样改 `decode_mode` → `variant`。

- [ ] **Step 3:** `base.py` 注释改为 ``variant`` / pin knobs。

- [ ] **Step 4:** 确认无测试仍 import 旧符号；跑

```bash
cd wink-tools && python -m pytest tools/codegen/tests/ -v --tb=line
```

- [ ] **Step 5:**（可选 commit）`chore(codegen): sync legacy driver baselines to variant`

---

### Task 7: `driver_ic` + `ic_to_variant_map`（dc_motor 最小落地）

**Files:**
- Modify: `wink-micro-os/codegen/drivers/dc_motor.yaml`
- Modify: `wink-tools/tools/codegen/variant_normalize.py`（或新建 `ic_variant.py` 并由其调用）
- Modify: `wink-tools/tools/codegen/driver_record.py` / `yaml_schema.parse_driver_document` — **仅当**需要把顶层 `ic_to_variant_map` 挂到 `DriverRecord`
- Modify: `wink-tools/tools/codegen/tests/test_variant_normalize.py`

**设计约束（Q4）：** `driver_ic` 可选；与 `variant` 冲突 → error；能推导则写入 `variant`。

- [ ] **Step 1: YAML 追加**（`dc_motor.yaml` 顶层，与 `fields:` 同级）

```yaml
ic_to_variant_map:
  tb6612: in_in
  l298n: in_in
  # phase_enable 芯片别名待真拓扑落地后再加
```

```yaml
fields:
  driver_ic:
    tier: advanced
    type: string
    emit: none          # 纯推导用，不发射到 C init；`emit: none` 已使 `_iter_config_fields` 跳过，`emit_when` 冗余，故不设
```

- [ ] **Step 2: 解析 `ic_to_variant_map`**

在 `load_driver_yaml` / `parse_driver_document` 路径：若 doc 含 `ic_to_variant_map`，校验为 `dict[str,str]`，存入 `DriverRecord` 新可选字段（默认 `()` / `None`）。  
若改 `DriverRecord` 冻结字段导致大量构造处要补参：给新字段 **默认值** `ic_to_variant_map: dict[str, str] | None = None`。

- [ ] **Step 3: 归一逻辑（在 legacy 键处理之后）**

```python
def apply_driver_ic(
    dev_name: str,
    spec: dict[str, Any],
    ic_map: dict[str, str] | None,
) -> tuple[dict[str, Any], list[str]]:
    if "driver_ic" not in spec:
        return spec, []
    if not ic_map:
        raise YamlSchemaError(
            f"device '{dev_name}': driver_ic set but type has no ic_to_variant_map"
        )
    ic = str(spec["driver_ic"]).lower()
    if ic not in ic_map:
        raise YamlSchemaError(
            f"device '{dev_name}': unknown driver_ic {ic!r}; known: {sorted(ic_map)}"
        )
    derived = ic_map[ic]
    out = dict(spec)
    if "variant" in out and out["variant"] != derived:
        raise YamlSchemaError(
            f"device '{dev_name}': driver_ic={ic!r} implies variant={derived!r} "
            f"but variant={out['variant']!r}"
        )
    if "variant" not in out:
        out["variant"] = derived
    # driver_ic 保留在 spec 或 pop；emit:none 即可保留
    return out, []
```

在 `app_codegen` 中：normalize legacy → apply_driver_ic(record.ic_to_variant_map) → constraints。

- [ ] **Step 4: 测试**

```python
def test_driver_ic_derives_variant():
    # build_context with driver_ic: tb6612, no variant → emit may omit if in_in default
    ...

def test_driver_ic_conflicts_with_variant():
    # driver_ic tb6612 + variant phase_enable → YamlSchemaError / _die
    ...
```

- [ ] **Step 5: 跑测通过**

- [ ] **Step 6:**（可选 commit）`feat(codegen): map driver_ic to variant for dc_motor`

---

### Task 8: 文档回写

**Files:**
- Modify: `wink-micro-os/docs/dal-development-guide/dal-best-practices.md`
- Modify: `wink-micro-os/docs/wink-app-json-guide.md`
- Modify: `docs/design/02-wink-micro-os/01-dal-device-abstraction.md`
- Modify: `wink-micro-os/docs/dal-development-guide/role-interface-codegen.md`（仅交叉引用句）
- Modify: `wink-micro-os/docs/dal-development-guide/dal-quickstart.md`（若仍写 drive_mode 非通用）

**必改内容清单：**

| 位置 | 改法 |
|------|------|
| 三维心法 | `drive_mode` → **`variant`**（同族避风港） |
| §3.0 字段表 | 用 `variant` 行替换 `drive_mode` 行；写明「非全 type 必填」 |
| `decode_mode` 专节 | 改为 encoder 的 `variant` 取值说明 |
| `01-dal-device-abstraction` 机制一 | 示例字段改为 `variant`；`HAS_*` 表述保留为可选 Wave B |
| `wink-app-json-guide` | dc_motor/encoder 字段表键名；注明旧键 deprecated |
| oled | `panel_variant` → `variant`；type 名以现网 `mono_oled` 为准（若文中仍写 ssd1306 type，加注历史） |

- [ ] **Step 1:** `rg -n "drive_mode|decode_mode|panel_variant" wink-micro-os/docs docs/design/02-wink-micro-os` 列出命中，逐条改（**不要**改 `docs/reviews/**` 历史评审正文；可在相关评审顶加一行「字段已迁 variant，见 tech-design」仅当 Owner 要求）。

- [ ] **Step 2:** 人工通读三维心法与 wink-app-json 电机/编码器节。

- [ ] **Step 3:**（可选 commit）`docs(dal): rename drive_mode taxonomy to variant`

---

### Task 9: 出口闸（Wave A Done）

**Files:** 无新文件；可能修样本 `wink-app.json`（若有人写了旧键——软迁移下不必改，但建议样本示范新键）。

- [ ] **Step 1: 全仓残留检查（公开 ABI / SSOT）**

```bash
rg -n "drive_mode|decode_mode|panel_variant|DAL_DC_MOTOR_MODE_|DAL_ENCODER_DECODE_|DAL_MONO_OLED_PANEL_" \
  wink-micro-os/dal wink-micro-os/codegen wink-tools/tools/codegen \
  --glob '!**/reviews/**' --glob '!**/implementation-plans/2026-07-28*'
```

Expected: 命中仅限于：`variant_normalize.py` 旧键表、测试里的 legacy 用例、deprecated 文档句、历史计划。

- [ ] **Step 1b: Flash wire 兼容性留档**

```bash
rg -n "apply_override" wink-micro-os/dal/include/actuator/dal_dc_motor.h \
  wink-micro-os/dal/include/sensor/dal_encoder.h \
  wink-micro-os/dal/include/display/dal_mono_oled.h
```

Expected: 三 type 仅有 "No apply_override wire yet" 注释、无实际序列化函数 → 确认改名不触碰 Flash blob 布局。若任一出现真实 `dal_*_apply_override` 定义 → 停止，单列 wire bump 子任务。

- [ ] **Step 2: 测试与 lint**

```bash
python wink-tools/wink.py test
python wink-tools/wink.py lint --pack layering --pack api --pack user_surface
```

Expected: 全绿 / 无新增 error。

- [ ] **Step 3: 抽查 codegen 默认零 diff 语义**

对任意现有 sample（无写 variant 字段）：

```bash
python wink-tools/wink.py gen wink-micro-app/devkitc_smoke
```

确认生成的 `device_tree` 中电机/编码器 **无**强制 `.variant =` 行（零值默认）；行为与改前一致。

> **建议自动化**：在 `test_motor_encoder.py` 增断言——`variant` 省略时 `config_init` 不含 `.variant =` 且 `build_context` 不抛错；避免仅靠人工抽查。

- [ ] **Step 4: 更新本计划元数据**「计划状态」→ `✅ Wave A 已完成（日期）`；在技术设计 §8 勾验收项。

- [ ] **Step 5:** 开 Wave B 跟踪项（issue 或 `implementation-plans/2026-07-30-dal-variant-wave-b-dce-plan.md`  stub 即可）：`HAS_*` DCE、UniSim `affects_pins`、SH1106 真实现。

---

## 5. 建议执行顺序与合并策略

```text
Task 0
  → Task 1（normalize + 测试）
  → Task 2 + Task 3 + Task 4（YAML + 两电机/编码器 C，同一工作时段完成，避免中间态无法编）
  → Task 5（oled）
  → Task 6（baseline）
  → Task 7（driver_ic，可与 6 并行但须在 2 之后）
  → Task 8（文档）
  → Task 9（出口闸）
```

**PR 建议：**  
- PR1：Task 0–4 + 6（核心 rename）  
- PR2：Task 5（oled）  
- PR3：Task 7（driver_ic）  
- PR4：Task 8–9（文档 + 闸）  

或单 PR 若变更集中且 CI 一次过。

---

## 6. 风险与回滚

| 风险 | 缓解 |
|------|------|
| 中途只改 YAML 未改 C → 生成宏未声明 | Task 2–4 连续落地；出口前全量 test |
| oled 改成 enum 成员撑大 POD | 强制 `typedef uint8_t dal_mono_oled_variant_t` |
| 软迁移被当成永久 API | 警告文案含 `next minor`；Wave B/下一 minor 删旧键表 |
| `DriverRecord` 加字段漏改构造 | 新字段带默认值；跑 `pytest tools/codegen` |
| 历史文档/评审仍写旧名 | 活规范必改；reviews 只读 |

回滚：按 Task 逆序 revert；软迁移模块可单独留存。

---

## 7. Spec 覆盖自检（计划作者已核）

| 设计要求 | 对应 Task |
|----------|-----------|
| 字段名 `variant` JSON+C | 2–5 |
| 每 type 自有枚举 | 3–5 |
| codegen 直写 / 零永久映射 | 1–2 |
| 旧键软迁移 + 双写报错 | 1 |
| 拒绝裸数字 enum | 1（锁测）+ 既有 `_coerce_type` |
| oled 强类型 + 8-bit | 5 |
| `driver_ic` 冲突检测 | 7 |
| `affects_pins` 登记 | 2（元数据；消费 Wave B） |
| 不做 HAS_* DCE | §2.2 |
| 不做公共头 / 新拓扑 | §2.2 |
| 活规范回写、不新开 ADR | 0、8 |
| 嵌仓先于 UniSim | §2.2 / Task 9 Step 5 |

---

## 8. 执行交接

计划已保存至：

`docs/implementation-plans/core/2026-07-30-dal-variant-unified-field-plan.md`

**两种执行方式：**

1. **Subagent-Driven（推荐）** — 每 Task 新开子代理，Task 间人工/父代理复核  
2. **Inline Execution** — 本会话按 Task 连续执行，出口闸前暂停复核  

确认本计划可执行后，回复选 **1** 或 **2**（或先要求修改计划）。

