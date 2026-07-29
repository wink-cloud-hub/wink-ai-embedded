# Role Interface Codegen 指南



面向：**已有（或新建）DAL 驱动**，在 codegen 侧挂上 App 可用的 `{instance}_{verb}` 封装。



| 项 | 说明 |

|----|------|

| **本篇职责** | Role 是什么 / 不是什么、实现位置、如何挂到驱动描述、验收、扩展约定 |

| **不是本篇** | DAL 驱动本体实现 → [adding-peripheral.md](./adding-peripheral.md)；字段分层摘要 → [dal-best-practices.md §3.0](./dal-best-practices.md) |

| **动词表 SSOT** | [`01-app-business-logic.md` § Role Interface](../../../docs/design/03-app-codegen/01-app-business-logic.md)（标准角色与错误层级以活规范为准；本篇不复制整表以免漂移） |

| **用户稳定面 SSOT** | [user-surface-insulation-design.md](../../../docs/design/tech-designs/2026-07-28-user-surface-insulation-design.md) |

| **关联计划 / 评审** | [user-surface-phase1-plan.md](../../../docs/design/implementation-plans/2026-07-28-user-surface-phase1-plan.md)；[completeness-review §10](../../../docs/design/reviews/2026-07-28-dal-control-semantic-completeness-review.md)；[phase1-plan-review.md](../../../docs/design/reviews/2026-07-28-user-surface-phase1-plan-review.md) |

| **扩展模型（ADR-0051 Accepted）** | [ADR-0051](../../../docs/design/decisions/0051-scannable-codegen-extension-roots.md) + [tech-design](../../../docs/design/tech-designs/2026-07-28-scannable-codegen-extension-roots-design.md)：**主路径** = driver YAML 的 `role_bindings`（模板）+ `codegen/roles/*.yaml`（契约-only）；引擎沙箱渲染 Jinja。内置例外见 [codegen/README.md](../../codegen/README.md#builtin-python-exceptions-p3-exit--owner-sign-off-list) |



---



## 1. 和「加外设」的关系（两件不同的事）



| | 加外设（DAL） | 包装成 Role |

|--|---------------|-------------|

| **回答的问题** | 这块硬件按什么**控制语义**驱动？ | App 把它**当什么能力**来调？ |

| **代码包** | `wink-micro-os/dal/`（运行时 C） | `wink-micro-os/codegen/`（driver YAML + 模板；引擎在 `wink-tools`） |

| **关键产物** | `dal_<type>.{h,c}`、`WINK_USE_*` | `device_tree.h` 内 `static inline {name}_{verb}` |

| **JSON** | 必填 `type` + 引脚/通道等 | 可选 `role`（缺省 `default_role`） |

| **是否必须** | 新 `type` **必须**有 DAL | **可选**；无 role 时 App 仍可直接调 `dal_*` |



同仓 monorepo 里两者常一起演进（`wink.py new-dal` 会建 YAML 骨架），但：



- driver YAML 可以**先只实现** init 模板 / DAL 绑定，**晚些再**补 `role_bindings`；

- 现网例如 `gps` / `eeprom`：**有 DAL，无 Role** —— 完全合法。



```text

wink-app.json

  type  ──────────►  DAL 实例 + dal_* API          （wink-micro-os）

  role  ──────────►  {name}_{verb} inline 封装       （codegen YAML → device_tree.h）

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



### 2.1 用户稳定面、Escape Hatch、BAL-backed



| 概念 | Role codegen 侧含义 |

|------|---------------------|

| **用户稳定面** | 发布后的 `{name}_{verb}` 签名与语义；改/删 = 破坏性变更 |

| **驱动面** | `role_bindings` 模板内部如何调 `dal_*`；DAL 改名后只改 YAML + 重 codegen |

| **无板卡模板** | JSON 引脚仍由各 App 填写；Role 不隐藏 `type`/接线 |

| **Escape Hatch** | 无 role 或专家场景仍可用 `extern dal_*_t` + `dal_*`；lint `APP-NO-DAL-CALL` warn |

| **BAL-backed** | 如 button 的 `start_auto_poll` 内部调 `wink_button_enable_events`；动词仍在 codegen 定义，**不是**把 BAL 注册成 `devices[].role` |



**Phase 1 语义（Role 挡不住的行为变化 = 破坏性）：**



- `open_loop_actuator` / `set_speed`：底层 `in_in` 拓扑与 [IN/IN 真值表](../../../docs/design/03-app-codegen/01-app-business-logic.md)；`set_speed` **必须**返回 `wink_status_t`（禁止 `IGNORE_RESULT`）。

- `pulse_counter`：`get_count` 为原始脉冲，**无 CPR**；encoder x1 + `invert` 语义见 [dal-best-practices §3.5](./dal-best-practices.md)。

- `text_display`：底层 `ssd1306` 芯片名仅驱动面；App 不依赖 `type` 字符串。



字段与 `drive_mode` 等关系见 [dal-best-practices §3.0](./dal-best-practices.md)。  

未来意图平面：[role/意图演进计划](../../../docs/design/implementation-plans/2026-07-28-wink-app-role-intent-evolution-plan.md)（⏸️）。



---



## 3. 实现位置与数据流



### 3.1 文件地图



| 环节 | 路径 |

|------|------|

| 引擎 / 编排 | `wink-tools/tools/codegen/app_codegen.py` — 读 JSON `role`、校验、收集 wrappers |

| 根解析 | `wink-tools/tools/codegen/roots.py` — 内置 → OS → env → App |

| YAML 渲染 | `wink-tools/tools/codegen/yaml_render.py` — 沙箱 Jinja 渲染 `role_bindings` / init 模板 |

| 注入模板 | `wink-tools/tools/codegen/templates/device_tree.h.j2` — `role_headers` / `role_wrappers` |

| **OS driver 描述（主路径）** | `wink-micro-os/codegen/drivers/<type>.yaml` — `default_role`、`role_bindings`（**模板只放这里**） |

| **Role 契约（契约-only）** | `wink-micro-os/codegen/roles/<role>.yaml` — verbs + `error_class` + 可选 `isr_safe`；**禁止**内嵌 DAL/Jinja 模板 |

| **内置例外** | `wink-tools/tools/codegen/drivers/<type>.py` — 见 [codegen/README.md § exceptions](../../codegen/README.md#builtin-python-exceptions-p3-exit--owner-sign-off-list) |



### 3.2 生成流水线



```text

devices[].role（可缺省 → driver.default_role）

        │

        ▼

若 JSON 显式写了 role：必须 ∈ role_bindings.keys()（或 builtin role_verbs），否则 codegen 退出码失败

        │

        ▼

for verb in role_bindings[role].verbs:

    沙箱 Jinja 渲染 template → static inline {name}_{verb}

        │

        ▼

device_tree.h

  #include role_headers…

  /* Role-based instance APIs */

  static inline … {name}_{verb}(…) { … dal_* / wink_* … }

```



无 `default_role` 且 JSON 未写 `role` → **不生成**任何 role wrapper（仅 DAL 句柄 `extern`）。



### 3.3 现网已挂 Role 的驱动（截至文档编写时）



| `type` | `default_role` | 描述来源 |

|--------|----------------|----------|

| `led` | `binary_indicator` | OS YAML `drivers/led.yaml` |

| `ultrasonic` | `distance_sensor` | OS YAML `drivers/ultrasonic.yaml` |

| `ssd1306` | `text_display` | OS YAML `drivers/ssd1306.yaml` |

| `button` | `binary_sensor` | 内置 `drivers/button.py` |

| `rc_servo` | `angular_actuator` | 内置 `drivers/rc_servo.py` |

| `dc_motor` | `open_loop_actuator` | 内置 `drivers/dc_motor.py` |

| `encoder` | `pulse_counter` | 内置 `drivers/encoder.py` |



以 `list_drivers --check` 输出的 `(os)` / `(builtin)` 为准；本表仅作导航。



---



## 4. 如何给驱动挂上 Role（操作步骤）



前置：该 `type` 的 DAL 已可用（见 [adding-peripheral.md](./adding-peripheral.md)）。



### 4.1 优先复用已有标准角色



先查活规范角色表（`binary_indicator` / `binary_sensor` / `distance_sensor` / `text_display` / …）。  

控制语义对得上 → **复用同名 role**，在 `role_bindings` 里实现相同动词集合（或子集需与规范对齐，勿擅自删减已对外承诺的动词）。



控制语义对不上 → 再开新 `role` 名，并**回写** `01-app-business-logic.md` 角色表（否则 App/AI 无 SSOT）。



### 4.2 在 driver YAML 声明 `role_bindings`（主路径）



在 `codegen/drivers/<type>.yaml` 设置 `default_role`，并为每个 role 键提供 `headers` 与 `verbs`：



```yaml

default_role: binary_indicator



role_bindings:

  binary_indicator:

    headers: []

    verbs:

      activate:

        template: "static inline void {{ name }}_activate(void) { WINK_IGNORE_RESULT(dal_led_on(&{{ name }})); }"

      deactivate:

        template: "static inline void {{ name }}_deactivate(void) { WINK_IGNORE_RESULT(dal_led_off(&{{ name }})); }"

      toggle:

        template: "static inline void {{ name }}_toggle(void) { WINK_IGNORE_RESULT(dal_led_toggle(&{{ name }})); }"

```



模板为**单行** Jinja 字符串；可用 `{{ name }}`、`{{ auto_poll_ms }}` 等 device spec 字段（来自 driver `fields:`，非旧四表）。复杂 wrapper 可参考 `drivers/ultrasonic.yaml`（Convenience + `_status` 成对、`headers` 引 BAL 域头）。



最小可抄：`codegen/drivers/led.yaml`（短）、`ultrasonic.yaml`（多动词 + headers）。



### 4.3 内置例外：仍用 `drivers/<type>.py`



`button`、`dc_motor`、`encoder`、`rc_servo` 等复杂驱动暂留 tools 内置插件。操作方式不变：



```python

default_role = "open_loop_actuator"

role_verbs = {

    "open_loop_actuator": ["set_speed", "coast", "brake", "safe_off"],

}



def render_role_wrapper(self, dev_name: str, role: str, verb: str, spec: dict) -> str:

    # … 见 drivers/dc_motor.py

    return ""

```



迁出至 OS YAML 前**不要**为同一 `type` 写 competing YAML（见 [codegen/README.md](../../codegen/README.md)）。



### 4.4 错误层级（生成风格）



与活规范一致，常见三类：



| 层级 | 典型形态 | 生成注意 |

|------|----------|----------|

| Fire-and-forget | `void {name}_activate(void)` | 内部 `WINK_IGNORE_RESULT(dal_…)` |

| Convenience | `bool` / `float` 等 | 失败时约定返回值（如距离 `-1.0f`）；可忽略 status |

| Normal / Fatal | `wink_status_t` + out 指针 | `WINK_WARN_UNUSED_RESULT`；**禁止**吞掉错误 |



同一物理量常成对提供 Convenience + `_status`（见 `binary_sensor` / `distance_sensor`）。`roles/*.yaml` 用 `error_class` 显式声明；`wink lint --pack drivers` 以 **warn** 校验签名 / init 透传 / `isr_safe`（升 error 条件见 [codegen/README.md](../../codegen/README.md#safety-lint-wink-lint---pack-drivers-task-t11)）。



### 4.5 JSON 用法



```json

"left_motor": {

  "type": "dc_motor",

  "role": "open_loop_actuator",

  "pwm_channel": 0,

  "dir_pin_a": 18,

  "dir_pin_b": 19

}

```



- 省略 `role` → 使用 `default_role`（若 driver 描述写了）。

- 显式 `role` 且不在 `role_bindings` / `role_verbs` → **codegen 失败**（有意 fail-closed）。

- 一个 driver 可在 `role_bindings` 里声明**多个**角色键（少见）；实例一次只选一个 `role`。



### 4.6 可选：`new-dal --role`



```text

python wink-tools/wink.py new-dal <type> --category … [--role <name>] …

```



会生成 `codegen/roles/<role>.yaml` 骨架 + driver 内空 `role_bindings.<role>.verbs` 占位；**真正生成 wrapper 仍依赖**你补全各 verb 的 `template`。



### 4.7 验收



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

| 新角色先改活规范再铺 bindings | 只改 YAML/Python、文档表不更新 |

| 无 role 的驱动保持可编译 | 强迫所有 `type` 必须有 `default_role` |

| 需要算法闭环时 App/BAL 显式调用 | 把 BAL 组件伪装成 `devices[].role` |



**Role vs BAL 再强调**：即使某个 verb 内部调用了 `wink_ultrasonic_enable_distance_events`（BAL/域头），**角色声明仍在 codegen driver 描述**；BAL 源码树不因「写了 role」而自动多一个组件。



---



## 6. 相关阅读



- 手册索引：[README.md](./README.md)

- 新增 DAL：[adding-peripheral.md](./adding-peripheral.md)

- 扩展根布局：[codegen/README.md](../../codegen/README.md)

- 字段 / type vs role 摘要：[dal-best-practices.md §3.0](./dal-best-practices.md)

- App 快速上手：[dal-quickstart.md](./dal-quickstart.md)

- 角色与动词 SSOT：[01-app-business-logic.md](../../../docs/design/03-app-codegen/01-app-business-logic.md)

- BAL 边界：[06-bal-layer.md](../../../docs/design/02-wink-micro-os/06-bal-layer.md)

- `wink-app.json`：[../wink-app-json-guide.md](../wink-app-json-guide.md)


