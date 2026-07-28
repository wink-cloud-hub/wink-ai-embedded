# Role Interface Codegen 指南

面向：**已有（或新建）DAL 驱动**，在 codegen 侧挂上 App 可用的 `{instance}_{verb}` 封装。

| 项 | 说明 |
|----|------|
| **本篇职责** | Role 是什么 / 不是什么、实现位置、如何挂到驱动插件、验收、扩展约定 |
| **不是本篇** | DAL 驱动本体实现 → [adding-peripheral.md](./adding-peripheral.md)；字段分层摘要 → [dal-best-practices.md §3.0](./dal-best-practices.md) |
| **动词表 SSOT** | [`01-app-business-logic.md` § Role Interface](../../../docs/design/03-app-codegen/01-app-business-logic.md)（标准角色与错误层级以活规范为准；本篇不复制整表以免漂移） |
| **未来扩展模型** | [ADR-0051](../../../docs/design/decisions/0051-scannable-codegen-extension-roots.md)（Proposed）+ [tech-design](../../../docs/design/tech-designs/2026-07-28-scannable-codegen-extension-roots-design.md)：描述迁出 tools、可扫描扩展根；**Accepted 前本篇仍描述现状（插件在 wink-tools）** |

---

## 1. 和「加外设」的关系（两件不同的事）

| | 加外设（DAL） | 包装成 Role |
|--|---------------|-------------|
| **回答的问题** | 这块硬件按什么**控制语义**驱动？ | App 把它**当什么能力**来调？ |
| **代码包** | `wink-micro-os/dal/`（运行时 C） | `wink-tools/tools/codegen/`（生成期 Python → C 头） |
| **关键产物** | `dal_<type>.{h,c}`、`WINK_USE_*` | `device_tree.h` 内 `static inline {name}_{verb}` |
| **JSON** | 必填 `type` + 引脚/通道等 | 可选 `role`（缺省 `default_role`） |
| **是否必须** | 新 `type` **必须**有 DAL | **可选**；无 role 时 App 仍可直接调 `dal_*` |

同仓 monorepo 里两者常一起演进（`wink.py new-dal` 会建插件骨架），但：

- 插件里可以**先只实现** `render_config_init` / DAL 绑定，**晚些再**补 `role_verbs`；
- 现网例如 `dc_motor`：**有 DAL，无 Role** —— 完全合法。

```text
wink-app.json
  type  ──────────►  DAL 实例 + dal_* API          （wink-micro-os）
  role  ──────────►  {name}_{verb} inline 封装       （wink-tools codegen）
                     └─ 内部通常调用 dal_*；偶发调 BAL helper（如事件 enable）
                        但 role 定义本身 ≠ BAL 组件
```

---

## 2. 概念边界（写文档 / 评审时用）

定稿句：

> **`type`**：控制语义族与 DAL 绑定（驱动平面）。  
> **`role`**：面向 App 的 Role Interface（能力平面；codegen 门面）。  
> **`role` 不是 BAL**；也不是未来「左轮 / 云台 / 运动意图」用户平面。

| 层 | 是什么 | 典型符号 |
|----|--------|----------|
| DAL | 控制语义 API | `dal_led_on`、`dal_dc_motor_set_speed` |
| Role Interface | 按实例生成的 App 门面 | `status_led_activate`、`front_radar_read_distance` |
| BAL | 可复用算法 / 闭环 / 编排 | `wink_closed_loop_*`、底盘等 |
| App | 业务状态机 | 推荐 role 动词；复杂场景 Escape Hatch → `&instance` + `dal_*` / BAL |

短口诀：**`type` = 是什么驱动；`role` = 当什么用（App 封装，非 BAL）。**

字段与 `drive_mode` 等关系见 [dal-best-practices §3.0](./dal-best-practices.md)。  
未来意图平面：[role/意图演进计划](../../../docs/design/implementation-plans/2026-07-28-wink-app-role-intent-evolution-plan.md)（⏸️）。

---

## 3. 实现位置与数据流

### 3.1 文件地图

| 环节 | 路径 |
|------|------|
| 钩子基类 | `wink-tools/tools/codegen/drivers/base.py` — `default_role`、`role_verbs`、`get_role_headers`、`render_role_wrapper` |
| 编排与校验 | `wink-tools/tools/codegen/app_codegen.py` — 读 JSON `role`、校验、收集 wrappers |
| 注入模板 | `wink-tools/tools/codegen/templates/device_tree.h.j2` — `role_headers` / `role_wrappers` |
| 各驱动插件 | `wink-tools/tools/codegen/drivers/<type>.py`（如 `led.py`、`ultrasonic.py`、`button.py`、`ssd1306.py`、`rc_servo.py`） |

### 3.2 生成流水线

```text
devices[].role（可缺省 → driver.default_role）
        │
        ▼
若 JSON 显式写了 role：必须 ∈ role_verbs.keys()，否则 codegen 退出码失败
        │
        ▼
for verb in role_verbs[role]:
    render_role_wrapper(dev_name, role, verb, spec)
        │
        ▼
device_tree.h
  #include role_headers…
  /* Role-based instance APIs */
  static inline … {name}_{verb}(…) { … dal_* / wink_* … }
```

无 `default_role` 且 JSON 未写 `role` → **不生成**任何 role wrapper（仅 DAL 句柄 `extern`）。

### 3.3 现网已挂 Role 的驱动（截至文档编写时）

| `type` | `default_role` | 插件 |
|--------|----------------|------|
| `led` | `binary_indicator` | `drivers/led.py` |
| `button` | `binary_sensor` | `drivers/button.py` |
| `ultrasonic` | `distance_sensor` | `drivers/ultrasonic.py` |
| `ssd1306` | `text_display` | `drivers/ssd1306.py` |
| `rc_servo` | `angular_actuator` | `drivers/rc_servo.py` |
| `dc_motor` 等 | （空） | **无** role；App 调 `dal_*` |

以仓库 `drivers/*.py` 为准；本表仅作导航。

---

## 4. 如何给驱动挂上 Role（操作步骤）

前置：该 `type` 的 DAL 已可用（见 [adding-peripheral.md](./adding-peripheral.md)）。

### 4.1 优先复用已有标准角色

先查活规范角色表（`binary_indicator` / `binary_sensor` / `distance_sensor` / `text_display` / …）。  
控制语义对得上 → **复用同名 role**，只在本插件里实现相同动词集合（或子集需与规范对齐，勿擅自删减已对外承诺的动词）。

控制语义对不上 → 再开新 `role` 名，并**回写** `01-app-business-logic.md` 角色表（否则 App/AI 无 SSOT）。

### 4.2 在 `drivers/<type>.py` 声明

```python
default_role = "speed_actuator"  # 示例名；落地前先对齐活规范命名
role_verbs = {
    "speed_actuator": ["set_speed", "coast", "brake"],
}

def get_role_headers(self, role: str) -> list[str]:
    # 仅当 wrapper 需要额外头（如 BAL 事件 API）时返回；多数只需 DAL 头（已在 get_headers）
    return []

def render_role_wrapper(self, dev_name: str, role: str, verb: str, spec: dict) -> str:
    if role != "speed_actuator":
        return ""
    if verb == "set_speed":
        return (
            f"static inline void {dev_name}_set_speed(float speed) {{ "
            f"WINK_IGNORE_RESULT(dal_dc_motor_set_speed(&{dev_name}, speed)); }}"
        )
    if verb == "coast":
        return (
            f"WINK_WARN_UNUSED_RESULT static inline wink_status_t "
            f"{dev_name}_coast(void) {{ return dal_dc_motor_coast(&{dev_name}); }}"
        )
    # …
    return ""
```

最小可抄模板：`drivers/led.py`（短）、`drivers/rc_servo.py`（单动词）、`drivers/ultrasonic.py`（含 `get_role_headers` + Convenience/Normal 成对接口）。

### 4.3 错误层级（生成风格）

与活规范一致，常见三类：

| 层级 | 典型形态 | 生成注意 |
|------|----------|----------|
| Fire-and-forget | `void {name}_activate(void)` | 内部 `WINK_IGNORE_RESULT(dal_…)` |
| Convenience | `bool` / `float` 等 | 失败时约定返回值（如距离 `-1.0f`）；可忽略 status |
| Normal / Fatal | `wink_status_t` + out 指针 | `WINK_WARN_UNUSED_RESULT`；**禁止**吞掉错误 |

同一物理量常成对提供 Convenience + `_status`（见 `binary_sensor` / `distance_sensor`）。

### 4.4 JSON 用法

```json
"left_motor": {
  "type": "dc_motor",
  "role": "speed_actuator",
  "pwm_channel": 0,
  "dir_pin_a": 18,
  "dir_pin_b": 19
}
```

- 省略 `role` → 使用 `default_role`（若插件写了）。
- 显式 `role` 且不在 `role_verbs` → **codegen 失败**（有意 fail-closed）。
- 一个 `type` 可在 `role_verbs` 里声明**多个**角色键（少见）；实例一次只选一个 `role`。

### 4.5 可选：`new-dal --role`

```text
python wink-tools/wink.py new-dal <type> --category … [--role <name>] …
```

仅脚手架提示/预填；**真正生成 wrapper 仍依赖**你在插件里写完 `role_verbs` + `render_role_wrapper`。

### 4.6 验收

```text
python wink-tools/wink.py build host --app <your_app>
# 检查生成的 device_tree.h（或 docs/device_tree_api.md）是否出现 {instance}_{verb}
```

建议：

- [ ] 显式错误 `role` 的 JSON 能令 codegen 失败（防回归可参考 `test_golden` 中 invalid role）
- [ ] App 用 role API 编译通过；Escape Hatch `&dev` + `dal_*` 仍可用
- [ ] 新标准角色已回写 `01-app-business-logic.md`
- [ ] `wink.py lint --pack layering --pack api`（若 wrapper 引入了新 include 路径）

---

## 5. 设计约定与反模式

| 做 | 不做 |
|----|------|
| Role 包 DAL（或已存在的事件 helper） | 在 role 里塞复杂业务状态机（那是 App） |
| 复用标准角色名 | 每个芯片一个 role（`l298n_role`） |
| 新角色先改活规范再铺插件 | 只改 Python、文档表不更新 |
| 无 role 的驱动保持可编译 | 强迫所有 `type` 必须有 `default_role` |
| 需要算法闭环时 App/BAL 显式调用 | 把 BAL 组件伪装成 `devices[].role` |

**Role vs BAL 再强调**：即使某个 verb 内部调用了 `wink_ultrasonic_enable_distance_events`（BAL/域头），**角色声明仍在 codegen 插件**；BAL 源码树不因「写了 role」而自动多一个组件。

---

## 6. 相关阅读

- 手册索引：[README.md](./README.md)
- 新增 DAL：[adding-peripheral.md](./adding-peripheral.md)
- 字段 / type vs role 摘要：[dal-best-practices.md §3.0](./dal-best-practices.md)
- App 快速上手：[dal-quickstart.md](./dal-quickstart.md)
- 角色与动词 SSOT：[01-app-business-logic.md](../../../docs/design/03-app-codegen/01-app-business-logic.md)
- BAL 边界：[06-bal-layer.md](../../../docs/design/02-wink-micro-os/06-bal-layer.md)
- `wink-app.json`：[../wink-app-json-guide.md](../wink-app-json-guide.md)
