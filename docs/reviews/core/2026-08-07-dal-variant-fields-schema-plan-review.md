# 2026-08-07 DAL 变体字段契约 (`variant_fields`) 强制 Schema 计划架构评审

| 项 | 内容 |
|----|------|
| **评审对象** | [`2026-08-07-dal-variant-fields-mandatory-schema-plan.md`](../../implementation-plans/core/2026-08-07-dal-variant-fields-mandatory-schema-plan.md) |
| **评审日期** | 2026-08-07 |
| **评审角色** | 资深嵌入式架构师 / 编译器与工具链工程师视角 |
| **评审方法** | ① 审阅实施计划全文；② 核对 `wink-tools/tools/codegen/schema/yaml_schema.py` 与 `drivers.py` 解析器；③ 追踪 `wink-app.json` $\rightarrow$ `device_tree.c` 自动裁剪数据流；④ 校验单变体与多变体驱动向下兼容性；⑤ 对比 UniSim / 前端 Binder `PIN_VARIANTS` 的跨仓契约 |
| **严重度** | **中/低**（方向 100% 正确，架构极其通透，仅有 3 处边界与向下兼容性需要补充落地细节） |
| **结论** | **通过（Approved with Minor Enhancements）**。计划设计非常优雅，完全解决了多变体引脚与参数裁剪的无歧义定义问题。建议按本评审 §3 补充单变体兼容逻辑、字段引用断言与旧 JSON 裁剪警告机制后即刻动工实施。 |

---

## 1. 总体评价

### 1.1 值得肯定的架构亮点

- **闭环与强契约（Mandatory Fail-Closed）**：
  将 `variant_fields` 设为多变体外设 Schema YAML 的强制必填项（当 `len(enum) > 1` 时），彻底消除了“变体选型与引脚/参数依赖不透明”的历史隐患。
- **错误左移（Shift-Left Error Detection）**：
  将校验主防线放在 C 侧 `wink-tools/tools/codegen/schema/yaml_schema.py` 和 `wink.py lint --pack drivers` 中，在 C 代码提交和编译的 0.1 秒内就能机械拦截漏填和误填，绝不将问题拖延至 WebAssembly 仿真或前端运行期。
- **设备树 C 代码自动精准裁剪（Zero-Noise Emission）**：
  在生成 `device_tree.c` 时，通过 `variant_fields[active_variant]` 实现未用引脚自动赋予 `-1` 哨兵值、无关软参数编译期告警与裁剪，保证了生成的初始化常数 `static const dal_<type>_config_t` 极致干净。
- **性能与内存开销保持 0 冗余**：
  完美延续了 C 驱动 1 字节 `uint8_t` 枚举与 Codegen DCE（死代码消除）机制，CPU 运行期零额外跳转开销，Flash 固件零冗余。

---

## 2. 边界细节与防错断言核对 (Edge Cases Check)

针对实施计划，以下 3 个边界细节需要明确并补充到规则库中：

### F-1: 单变体外设 (`len(enum) <= 1`) 的向下兼容性
* **问题描述**：对于 `button`、`led`、`ultrasonic` 等只有一个 `default` 变体或没有变体枚举的外设，强制要求填写 `variant_fields` 会增加不必要的 YAML 冗余。
* **裁决与补充**：
  - 当 `enum` 列表不存在或长度 $\le 1$ 时，`variant_fields` **允许省略**。
  - 当 `variant_fields` 省略时，Schema 解析器默认推导为 `variant_fields: { default: [fields 中的全量字段] }`。

### F-2: `variant_fields` 字段名拼写检查（Field Reference Validation）
* **问题描述**：如果开发者在 `variant_fields` 列表中写错了一个字段名（例如写成 `dir_pina` 漏了下划线），如果仅仅校验它是不是字符串列表，解析器可能会放过该错误。
* **裁决与补充**：
  - 在 `yaml_schema.py` 中解析 `variant_fields` 时，必须强制校验：`variant_fields` 中出现的**每一个字段名必须存在于当前 YAML 的 `fields` 属性字典中**。
  - 找不到则抛出 `YamlSchemaError("variant_fields lists unknown field 'dir_pina' in fields.variant")`。

### F-3: 迁移期旧 `wink-app.json` 的自动裁剪与 Warning 级别
* **问题描述**：在 `wink-app.json` 刚升级的过渡期，若低代码配置中仍然残留了旧变体下不需要的参数（例如 `phase_enable` 模式下多填了 `dir_pin_b: 6`）。
* **裁决与补充**：
  - Codegen 不应该在读到该 JSON 时直接抛异常崩掉，而是应该 **“自动裁剪为 `-1` 并打印 `@deprecated` / `@trimmed` 告警”**。
  - 这保证了旧工程 JSON 的向后兼容性与平滑迁移。

---

## 3. 具体改进与补充建议 (Actionable Recommendations)

建议在实施计划中补充以下 3 点具体的落地细节：

### 建议 1：在 `yaml_schema.py` 中补充单变体默认推导与字段名匹配断言
在 `yaml_schema.py` 解析 `variant_fields` 时，实现如下严谨的校验伪代码：

```python
# 校验伪代码补强
if len(enum_values) > 1:
    if not variant_fields:
        raise YamlSchemaError(f"fields.variant.variant_fields is mandatory for multi-variant driver in {path}")
    
    # 1. 校验 Key 是否与 enum 完全覆盖匹配
    if set(variant_fields.keys()) != set(enum_values):
        raise YamlSchemaError(f"fields.variant.variant_fields keys must match variant.enum values exactly in {path}")
        
    # 2. 校验 Value 里的字段名是否在 fields 中定义
    all_field_names = set(fields_dict.keys())
    for v_key, v_fnames in variant_fields.items():
        unknown_fields = set(v_fnames) - all_field_names
        if unknown_fields:
            raise YamlSchemaError(f"fields.variant.variant_fields[{v_key}] references unknown field(s) {unknown_fields} in {path}")
```

### 建议 2：`gen-frontend-binders --check` 增加变体切片跨仓自动校验
在外置插件/三仓绑定工具（`packages/unisim/tools/gen-frontend-binders.ts`）执行 `--check` 时：
* 读取 C 侧 `variant_fields` 契约，自动比对 UniSim 仿真器导出的 `PIN_VARIANTS` 表。
* 若 C 侧 YAML 声明 `phase_enable` 变体只用 2 个脚，而 TS 侧 `PIN_VARIANTS.phase_enable` 声明了 3 个脚，CLI `--check` 自动抛错，从根源拦截 C 侧与 TS 侧的跨仓 Drift。

### 建议 3：更新驱动脚手架工具 (`wink.py new-dal`)
更新 `wink-tools/tools/cli/commands/new_dal.py` 脚手架生成代码：
* 当使用 `wink.py new-dal <type>` 创建带变体的新 DAL 外设时，自动在生成的 `codegen/drivers/<type>.yaml` 中渲染包含 `variant_fields` 的模板注释，引导开发者从第一天起就编写合规的 YAML。

---

## 4. 结论与下一步

评审结论：**通过（Approved with Minor Enhancements）**。

本设计将 DAL 外设变体规范彻底推进到了 **100% 精准契约与机械化自动裁剪** 的新高度。请按本评审建议补全 `F-1`、`F-2`、`F-3` 的 3 处边界处理后，即刻按照实施计划的 4 个 Phase 推进动工！

