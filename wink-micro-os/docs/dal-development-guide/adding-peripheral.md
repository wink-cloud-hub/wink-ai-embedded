# 嵌入式端新增外设指南（ADR-0046 / ADR-0051）

> **路径**：原 `wink-micro-os/docs/adding-peripheral-guide.md` 已迁入本手册目录。旧路径保留跳转 stub。

与仿真侧 [ADDING_PERIPHERAL.md](file:///d:/workspaces/ai-coding/wink-ai/wink-ai/packages/unisim/docs/ADDING_PERIPHERAL.md) 配套。设备 `type` 字符串**两侧必须完全一致**。

设计依据：[peripheral-onboarding tech-design](../../../docs/design/tech-designs/2026-07-27-peripheral-onboarding-optimization-design.md)、[ADR-0046](../../../docs/design/decisions/0046-dal-driver-registry-ssot.md)、[ADR-0051](../../../docs/design/decisions/0051-scannable-codegen-extension-roots.md)（**Accepted**）、[扩展根 tech-design](../../../docs/design/tech-designs/2026-07-28-scannable-codegen-extension-roots-design.md)。

手册索引：[README.md](./README.md) · 用现有驱动请先读 [dal-quickstart.md](./dal-quickstart.md)。

> **主路径（ADR-0051）**  
> 描述写在 `wink-micro-os/codegen/drivers/<type>.yaml`（+ 可选 `roles/` 与 `templates/`）；**不改** `wink-tools` 源码。引擎扫描扩展根。  
> 复杂驱动暂留 tools 内置插件 — 见 [codegen/README.md § Builtin Python exceptions](../../codegen/README.md#builtin-python-exceptions)。

> **DAL vs Role（分清两步）**  
> 本指南覆盖 **DAL + codegen 描述/插件注册**（`type`、引脚、CMake、`WINK_USE_*`）。  
> 把驱动包装成 App 的 `{name}_{verb}` 属于 **可选的 Role Interface（codegen）**，见专文 [role-interface-codegen.md](./role-interface-codegen.md)。  
> `new-dal` 会创建骨架，但**不要求**立刻挂 Role；无 role 的驱动完全合法。

---

## 1. 架构与 SSOT

```text
wink.py new-dal → dal_*.{h,c} + codegen/drivers/<type>.yaml [+ templates/]
                         ↓
         resolve_codegen_roots（内置 → OS → env → App）
                         ↓
         list_drivers.py --cmake --mode=source|defs
                         ↓
         CMake 三入口 foreach（零手改驱动表；YAML ∈ CONFIGURE_DEPENDS）
```

- **机制 SSOT（ADR-0046）**：单一 registry + `list_drivers` 数据型 CMake；不要手改 `dal/CMakeLists.txt` / `wink_dal_drivers.cmake` / Binary SDK / `ALL_WINK_USE_OPTIONS` 中的驱动枚举。  
- **路径 SSOT（ADR-0051）**：默认描述根 = `wink-micro-os/codegen/`；`wink-tools` = 只读引擎。  
- **内置例外**：`button`、`dc_motor`、`encoder`、`rc_servo`、`gps`、`eeprom` 等仍由 `wink-tools/tools/codegen/drivers/*.py` 提供（见 [codegen/README.md](../../codegen/README.md)）；**不要**为这些 type 再写 competing OS YAML。

---

## 2. `wink.py new-dal`

```text
python wink-tools/wink.py new-dal <type> \
  --category <input|output|actuator|sensor|display|comm|storage> \
  [--actuator] [--role <name>] [--pin-field <name>]... [--force]
```

产出：

| 文件 | 位置 |
|------|------|
| DAL 头 | `wink-micro-os/dal/include/<category>/dal_<type>.h` |
| DAL 源 | `wink-micro-os/dal/src/<category>/dal_<type>.c` |
| Codegen 描述 | `wink-micro-os/codegen/drivers/<type>.yaml` |
| Init 模板（默认） | `wink-micro-os/codegen/drivers/templates/<type>_init.c.j2` |
| Role 契约骨架（`--role`） | `wink-micro-os/codegen/roles/<role>.yaml` |

脚手架**不再**写入 `wink-tools/tools/codegen/drivers/<type>.py`。填完 DAL 实现与 YAML 后重新 configure CMake，`WINK_KNOWN_DRIVERS` 会自动包含新驱动。

实现前请对照 [dal-best-practices.md](./dal-best-practices.md)：是否应合并进已有 `dal_*`（同拓扑），还是新开类型（总线/语义不同）。

**可选下一步**：在同一 driver YAML 中补 `role_bindings` → [role-interface-codegen.md](./role-interface-codegen.md)。

---

## 3. 手写 YAML（不用脚手架时）

1. 实现 DAL C 源/头（见上表路径）。  
2. 新建 `codegen/drivers/<type>.yaml`，`type:` 字段**必须**与文件名 stem 一致；含 `codegen_schema: "1.1"`、`experimental`、`fields:`（**禁止**旧四表 `required_fields` / `stable_fields` / `advanced_fields` / `constraints`）、可选 `role_bindings`。  
3. stub（`experimental: true`）须**手写** `category`；复杂 init 放 `codegen/drivers/templates/<type>_init.c.j2`，YAML 里 `config.init_template_file` 引用相对路径。  
4. 参考现网样例：`codegen/drivers/ultrasonic.yaml`、`ssd1306.yaml`；字段心智模型见 [codegen/README.md § Schema 1.1](../../codegen/README.md#schema-11)。

---

## 4. 验收清单

```text
python wink-tools/wink.py lint --pack drivers --pack layering --pack api
python wink-tools/tools/codegen/list_drivers.py --check
python wink-tools/tools/codegen/list_drivers.py --json   # 可选跨仓对照
python wink-tools/wink.py build host --app <your_app>
```

`list_drivers --check` 应打印 `your_type <- …/codegen/drivers/your_type.yaml (os)` 并以 `list_drivers --check: OK` 结束。新 type 仅改 micro-os 树即可被发现，无需 touch tools。

Host 构建至少一次带 `wink-app.json` 裁剪、一次不带 JSON（全量驱动）以覆盖 stub 路径。

---

## 5. Unisim 仿真侧对齐（[Wasm 仿真 3.0 SSOT](../../../docs/design/04-wasm-simulation-3.0/00-README.md)）

1. **`type` 字符串强制逐字一致**：仿真侧 Manifest / `peripheral-definition.json` 的 `type`（或 `id`）字符串必须与 codegen YAML 的 `type:` **逐字一致**。嵌入式侧以 `list_drivers.py --json` 作为对照源。
2. **四层配置源职责（[07-peripheral-registry.md](../../../docs/design/04-wasm-simulation-3.0/02-mechanisms/07-peripheral-registry.md#L18-L32)）**：
   - `wink-app.json`：固件设备树 SSOT（声明 App 用到的外设实例与引脚）。
   - `sim-project.json`：仿真电路画布拓扑（导线连接、元件坐标）。
   - `peripheral-definition.json`：Unisim / Wokwi UI 元数据（管脚定义、SchemaForm 属性面板）。
   - `device_tree.h`：固件编译产物。
3. **仿真 Bypass 方式**：若外设需要在 WASM 仿真中跳过底层 PAL 驱动直接与前端交互，可通过 WASM Bridge 实现 Channel 4 语义旁路（`dal_<type>_*` Direct Bridge，详见 [08-channel-routing.md](../../../docs/design/04-wasm-simulation-3.0/02-mechanisms/08-channel-routing.md)）。

---

## 6. 路径锚点（拆仓后）

| 变量 / 路径 | 含义 |
|-------------|------|
| `WINK_TOOLS_ROOT` | `…/wink-tools`（CLI / codegen / lint） |
| `WINK_MICRO_OS_ROOT` | `…/wink-micro-os`（C 运行时 + codegen 扩展根） |
| 入口 | `python wink-tools/wink.py …` |
