# Ultrasonic Distance Events + avoidance_car L1 Refactor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `embedded-best-practice` for C/BAL/DAL/codegen edits; prefer `subagent-driven-development` or `executing-plans` task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add platform B-class `enable_distance_events` (ultrasonic → global `wink_event` queue), then refactor `avoidance_car` to an `oled_dashboard`-style App: `init` + `on_event`, no business `loop`.

**Architecture:** Mirror button’s ADR-0031/0032 pattern. A new BAL module owns periodic measure + completion detect + `wink_event_post`. Codegen emits Role `{name}_enable_distance_events()`. App reacts in `app_on_event` with Role verbs only. Existing A-class `wink_sonar_helper_start` stays “measure only, no queue.”

**Tech Stack:** C (BAL/runtime/DAL), Python codegen plugins, host Unity e2e, golden codegen tests, CMake/`sample_common.cmake`.

## Global Constraints

- ADR-0032: queue-facing API must be B-class `enable_*` / `disable_*` (not `start` for this path).
- ADR-0023: JSON describes static capability/period only; App still **explicitly** calls `enable_distance_events()` (no auto-start in `device_tree_init`).
- ADR-0022: post from soft-timer/LIGHT context only; App business only in `on_event` / init / fault — never ISR.
- ADR-0002 / 0004: dual-target App C; Role + POD named APIs; no vtable.
- ADR-0017: no blocking ultrasonic read in App tick/`on_event`.
- Keep `wink_sonar_helper_*` as **A-class** (period trigger, no events) unless a later ADR merges them.
- Do not git commit unless the user asks.
- Flash-override / hand-written `device_tree` (ADR-0008) for avoidance_car is **moved out of the gold path** (archive note in README or retain as optional L2 doc elsewhere) — L1 sample uses codegen like `oled_dashboard`.

---

## 1. Metadata

| 字段 | 内容 |
|------|------|
| **计划编号** | `PLAN-20260716-US-DIST-EVT` |
| **创建日期** | 2026-07-16 |
| **计划状态** | ✅ MVP 已落地（2026-07-16）— ADR Accepted；BAL + codegen + avoidance_car L1；host unit/e2e PASS |
| **优先级** | 🟡 P1（平台能力）+ 🟢 P0（avoidance_car 样板体验） |
| **前置 ADR** | 新建 **ADR-0033**（本计划 Task 0） |
| **关联 ADR** | ADR-0022、0023、0031、0032 |
| **关联规范** | `coding-conventions.md` §3（已预留 `enable_distance_events`） |
| **样板对照** | `wink-micro-app/oled_dashboard/app_callbacks.c` |
| **目标平台** | host（必验）→ wasm → ESP32 HIL（可后置） |

### Acceptance export

| 指标 | 通过标准 |
|------|----------|
| ADR-0033 | Accepted + 回写 `01-app-business-logic.md` / schema / coding-conventions 示例 |
| Unit | `enable` → 至少一次 `WINK_EVENT_DISTANCE_READY`；`disable` 后不再投递 |
| Codegen golden | Role 含 `enable_distance_events` / `disable_distance_events` |
| avoidance_car | App **无** `app_loop` 业务；行为：距障 &lt; 20cm → 舵机 180°，否则 90° |
| oled_dashboard | 回归 e2e 仍 PASS（事件类型扩展不破坏按键路径） |
| A/B 不混淆 | `wink_sonar_helper_start` 仍不投递 distance 事件 |

---

## 2. Design locks（执行前锁定，勿在实现中临时改口径）

### 2.1 事件契约

```c
WINK_EVENT_DISTANCE_READY = 4,   /* 新样本可用 */
WINK_EVENT_DISTANCE_FAULT = 5,   /* 可选：测量失败（超时等）；MVP 可先只做 READY */
```

`wink_event_t` 用法：

| 字段 | 约定 |
|------|------|
| `device` | `&front_radar`（`dal_ultrasonic_t *`） |
| `type` | `WINK_EVENT_DISTANCE_READY` |
| `param` | **距离毫米** `uint32_t`（`cm * 10` 四舍五入，或 `cm` 整数×10 固定一点；见下） |
| `timestamp` | `pal_os_get_ms()` |

**距离编码（锁定推荐）：** `param = (uint32_t)(distance_cm * 10.0f + 0.5f)` → 单位 **0.1 cm**（即 mm）。App 可读：

```c
float cm = evt->param / 10.0f;
```

同时允许 `front_radar_read_distance()` 作为二次确认（缓存应一致）。无效样本不投递 READY；可选 FAULT 的 `param` 放错误码。

**不扩展** `wink_event_t` 加 `float`（避免破坏 POD / 队列布局）。

### 2.2 BAL API（B 类）

```c
typedef struct {
    uint32_t period_ms;   /* 触发周期；MVP 强制 >= 50（与 sonar helper 一致，防串扰） */
} wink_ultrasonic_distance_event_config_t;

wink_status_t wink_ultrasonic_enable_distance_events(
    dal_ultrasonic_t *dev,
    const wink_ultrasonic_distance_event_config_t *cfg);

void wink_ultrasonic_disable_distance_events(dal_ultrasonic_t *dev);
```

- Role：`front_radar_enable_distance_events()` / `disable_distance_events()` → 内联转发，JSON `auto_poll_ms`（或 `distance_event_period_ms`）烘焙进 cfg。
- 命名与 `coding-conventions` 矩阵一致；**禁止**把队列路径叫 `proximity_start`。

### 2.3 后端（MVP = soft_poll only）

每 `period_ms` 的 LIGHT/periodic tick：

1. 若无进行中测量 → `dal_ultrasonic_request_measurement`
2. `get_cached_distance`：
   - `WINK_OK` 且相对上次已投递样本为**新完成** → `wink_event_post(DISTANCE_READY)`
   - `WINK_ERR_BUSY` → 等待
   - 其它错误 → 可选 `DISTANCE_FAULT`（MVP：`wink_trace_fault` 或 FAULT 事件二选一，ADR 锁定）

**完成检测：** 用 DAL 状态从 BUSY→OK 的边沿，或比较 `last_distance` + 序列号；禁止对同一缓存样本重复灌队列。

**队列满：** `post` 失败 → `wink_trace_warn`（新 warn code）；不阻塞 tick。

**与 A 类关系：**

| API | 行为 |
|-----|------|
| `wink_sonar_helper_start` | 只周期 `request_measurement`，**不** post |
| `wink_ultrasonic_enable_distance_events` | 周期测 + **post**；同一 `dev` 禁止与 helper 同时占用（返 `INVALID_STATE`） |

### 2.4 JSON schema（ultrasonic）

MVP 字段（对齐 button 精神，保持简单）：

```json
"front_radar": {
  "type": "ultrasonic",
  "trig_pin": 4,
  "echo_pin": 5,
  "use_rmt": false,
  "auto_poll_ms": 50
}
```

- `auto_poll_ms`：distance-events 周期（缺省 50）。仅当 App 调用 `enable_distance_events` 时生效。
- 本阶段**不做** `event_drive: gpio_irq`（超声波完成路径与按键不同；后续另 ADR）。

### 2.5 avoidance_car 目标 App 形态（对齐 oled_dashboard）

```c
static wink_status_t app_init_status(void)
{
    WINK_TRY(wink_device_tree_init());
    WINK_TRY(front_radar_enable_distance_events());
    neck_servo_set_angle(90.0f);   /* Role；安全位 */
    return WINK_OK;
}

static void app_on_event(const wink_event_t *evt)
{
    if (evt->device == &front_radar &&
        evt->type == WINK_EVENT_DISTANCE_READY) {
        float cm = evt->param / 10.0f;
        if (cm > 0.0f && cm < 20.0f)
            neck_servo_set_angle(180.0f);
        else
            neck_servo_set_angle(90.0f);
    }
}
/* loop / on_boot omitted (NULL) */
```

Servo：codegen 补 Role `set_angle`（转发 `wink_servo_set_angle` / `dal_servo_set_angle`）；safe-off 仍由 `device_tree_init` actuator 注册（与其它 actuator 一致）。

---

## 3. File map

| File | Responsibility |
|------|----------------|
| `docs/decisions/core/0033-ultrasonic-distance-events.md` | **New** ADR |
| `docs/tech-designs/core/2026-07-16-ultrasonic-distance-events.md` | **New** 技术设计（状态机、与 sonar helper 互斥、param 编码） |
| `runtime/include/wink_event.h` | 增加 `DISTANCE_READY` / `FAULT` |
| `bal/include/sensor/wink_ultrasonic_distance_events.h` | **New** public B API |
| `bal/src/wink_ultrasonic_distance_events.c` | **New** slot + periodic + post |
| `bal/CMakeLists.txt` | 编入新源 |
| `bal/include/sensor/wink_sonar_helper.h` | 文档注明与 B 类互斥 |
| `tools/codegen/drivers/ultrasonic.py` | Role verbs + cfg macros |
| `tools/codegen/drivers/servo.py` | Role `set_angle`（及文档） |
| `tools/codegen/templates/device_tree_api.md.j2` | 文档模板 |
| `tools/codegen/tests/golden_*` | 更新期望 |
| `test/test_ultrasonic_distance_events.c` | **New** host unit |
| `wink-micro-app/avoidance_car/*` | 切 codegen；重写 `app_callbacks.c`；删/归档手写 tree |
| `wink-micro-app/README.md` | avoidance_car 从「纯 L2」改为「L1 事件样板（雷达）」；Flash 覆写另寻样板 |
| `docs/design/03-app-codegen/*.md` | ADR Accepted 后回写 |
| `docs/design/07-platform-governance/coding-conventions.md` | 矩阵从「可选」改为「已落地」 |

---

## 4. Phased approach options（已选）

| 方案 | 内容 | 结论 |
|------|------|------|
| **C1 MVP** | soft_poll + READY 事件 + avoidance_car L1 化 | ✅ **本计划采纳** |
| C2 | 同时做 RMT-IRQ 完成回调投递 | ❌ 后置；依赖更多 DAL 完成回调 |
| C3 | 扩展 `wink_event_t` 加 float payload | ❌ 破坏面大；用 `param` mm×0.1cm |
| C4 | 只改 App、用 sonar helper + 仍留 loop 读缓存 | ❌ 不符合「平台级事件化」目标 |

---

## 5. Tasks

### Task 0: ADR-0033 + tech-design（文档门禁）

**Files:**
- Create: `docs/decisions/core/0033-ultrasonic-distance-events.md`
- Create: `docs/tech-designs/core/2026-07-16-ultrasonic-distance-events.md`

**Interfaces:**
- Produces: Locked names, event IDs, param encoding, A/B split, MVP soft_poll only.

- [ ] **Step 1:** Draft ADR（Context / Options / Decision / Consequences），状态 Proposed → 等 Owner Accepted。
- [ ] **Step 2:** Tech-design：时序图（tick → request → BUSY → OK → post → on_event）、槽位、互斥规则、warn 码。
- [ ] **Step 3:** Owner Accepted 后回写活规范（可与 Task 6 合并提交）。
- [ ] **Step 4:** Stop for review gate — **未 Accepted 不写生产 C API 符号**（测试草稿除外）。

---

### Task 1: Runtime 事件类型

**Files:**
- Modify: `wink-micro-os/runtime/include/wink_event.h`
- Modify: `wink-micro-os/test/test_wink_event.c`（可选：新 type round-trip）

**Interfaces:**
- Produces: `WINK_EVENT_DISTANCE_READY`、`WINK_EVENT_DISTANCE_FAULT`（若 ADR 采纳 FAULT）。

- [ ] **Step 1:** 在 `BUTTON_LONG_PRESS` 之后追加枚举值（保持 `USER_START = 1000`）。
- [ ] **Step 2:** 现有 `test_wink_event` / button e2e 回归编译通过。
- [ ] **Step 3:**（用户要求时）commit：`feat(event): add DISTANCE_READY event type`

---

### Task 2: BAL `wink_ultrasonic_enable_distance_events`（TDD）

**Files:**
- Create: `bal/include/sensor/wink_ultrasonic_distance_events.h`
- Create: `bal/src/wink_ultrasonic_distance_events.c`
- Create: `test/test_ultrasonic_distance_events.c`
- Modify: `bal/CMakeLists.txt`、host test CMake

**Interfaces:**
- Consumes: `dal_ultrasonic_*`、`wink_periodic_start_ex` / soft_timer、`wink_event_post`
- Produces: enable/disable API as in §2.2

- [ ] **Step 1:** 写失败单测：init ultrasonic → enable(period=50) → 推进时间/tick → `pend` 得 READY 且 `param` 合理。
- [ ] **Step 2:** 跑测 → fail。
- [ ] **Step 3:** 最小实现：slot pool、tick 内 request/get_cached、边沿 post、disable 停 periodic。
- [ ] **Step 4:** 单测：重复同一样本不二次 post；disable 后无事件；`period_ms < 50` → `INVALID_ARG`；与 `sonar_helper_start` 同 dev → `INVALID_STATE`。
- [ ] **Step 5:** 全 host 相关测 PASS。
- [ ] **Step 6:**（可选 commit）`feat(bal): ultrasonic distance events B-class API`

---

### Task 3: Codegen Role + schema

**Files:**
- Modify: `tools/codegen/drivers/ultrasonic.py`
- Modify: `tools/codegen/drivers/servo.py`（`set_angle` Role）
- Modify: `tools/codegen/templates/device_tree_api.md.j2`
- Modify: golden expected trees
- Test: `tools/codegen/tests/test_golden.py`

**Interfaces:**
- Produces Role:

```c
front_radar_enable_distance_events(void);
front_radar_disable_distance_events(void);
neck_servo_set_angle(float angle);
```

- [ ] **Step 1:** `role_verbs` 增加 `enable_distance_events` / `disable_distance_events`；保留既有 C 类动词。
- [ ] **Step 2:** `enable` 生成 `static const wink_ultrasonic_distance_event_config_t` + 调用 BAL。
- [ ] **Step 3:** `auto_poll_ms` 缺省 50；&lt;50 codegen ERROR 或钳制（与 ADR 一致，推荐 **codegen ERROR**）。
- [ ] **Step 4:** Servo Role `set_angle` → `wink_servo_set_angle`（或 dal）。
- [ ] **Step 5:** 更新 golden；`test_golden.py` PASS。

---

### Task 4: 重构 `avoidance_car` → L1 事件样板

**Files:**
- Modify: `wink-micro-app/avoidance_car/app_callbacks.c`（重写）
- Modify: `wink-micro-app/avoidance_car/wink-app.json`（补 `auto_poll_ms`）
- Modify: `wink-micro-app/avoidance_car/CMakeLists.txt` → `wink_app_prepare_codegen`
- Remove or relocate: 手写 `device_tree.c/h`、`board_config.c`（若 codegen 覆盖）
- Create/Modify: `avoidance_car/test_*_e2e.c`（host：注入距离 → 断言舵机角或 trace）
- Modify: `wink-micro-app/README.md` 分级表

**Interfaces:**
- Consumes: Task 3 Role APIs + Task 1 events
- Target shape: §2.5

- [ ] **Step 1:** CMake 切到与 `oled_dashboard` 相同的 codegen 路径；确认生成 `wink_device_tree_init`、Role、actuator safe-off。
- [ ] **Step 2:** 重写 `app_callbacks.c`：无 loop / 无 on_boot / 无直调 dal（L1）；`WINK_TRY` + Role。
- [ ] **Step 3:** e2e：mock/sim 近距 → 期望 180°；远距 → 90°。
- [ ] **Step 4:** README：avoidance_car = **L1 雷达事件金样**；原 ADR-0008 Flash 演示改指向其它文档或 `devkitc_smoke` 注释。
- [ ] **Step 5:** host 构建 + e2e PASS。

---

### Task 5: 文档回写与回归

**Files:**
- Modify: `docs/design/03-app-codegen/01-app-business-logic.md`
- Modify: `docs/design/03-app-codegen/02-project-manifest-schema.md`
- Modify: `docs/design/07-platform-governance/coding-conventions.md` §3.3 矩阵
- Modify: 本计划状态 → ✅

- [ ] **Step 1:** 文档示例改为「雷达 enable_distance_events + on_event」。
- [ ] **Step 2:** 跑 `oled_dashboard` e2e + ultrasonic distance unit + avoidance e2e。
- [ ] **Step 3:** 标记本计划 Acceptance export 全绿。

---

### Task 6（可选后置）: FAULT 事件 / ESP32 HIL / 完成 IRQ

- [ ] DISTANCE_FAULT 投递策略与 App 安全舵机位。
- [ ] ESP32 真机：HC-SR04 + 舵机目视验收。
- [ ] DAL 异步完成回调 → 减少 soft poll（新 ADR，不在本 MVP）。

---

## 6. Risk register

| 风险 | 影响 | 缓解 |
|------|------|------|
| host「单 tick READY」与 ESP32 真异步行为差 | e2e 假绿 | 单测覆盖 BUSY→OK 两边沿；ESP32 手工/HIL 后置 |
| 事件洪水填满队列 | 丢事件、舵机滞后 | period≥50；完成边沿才 post；满则 warn |
| 与 sonar helper 双开 | 双触发损坏时序 | 同 dev 互斥 `INVALID_STATE` |
| avoidance_car 丢掉 Flash 演示 | 专家路径样例减少 | README 标明迁移；Flash 保留在规范/其它 sample |
| Servo 无 Role 时 App 仍 dal_* | L1 纯度破损 | Task 3 必须先着陆 `set_angle` Role |

---

## 7. Suggested execution order

```text
Task 0 (ADR Accepted)
  → Task 1 (event enum)
  → Task 2 (BAL + unit TDD)
  → Task 3 (codegen + golden)
  → Task 4 (avoidance_car)
  → Task 5 (docs + regression)
  → Task 6 (optional)
```

**Estimated effort:** ~2–4 focused days for MVP (Task 0–5) if ADR 当日 Accepted；Task 6 separate.

---

## 8. Out of scope（本计划明确不做）

- QP / HSM 框架引入
- 改 `oled_dashboard` 业务逻辑
- 超声波 `event_drive: gpio_irq`
- 合并删除 `wink_sonar_helper`（仅文档互斥）
- 强制所有现有 loop 样板迁移

---

## 9. Owner checkpoint（执行前请确认）

请确认以下锁定项（回复即可开始 Task 0）：

1. **param 编码** = 0.1 cm 单位的 `uint32_t`（`cm*10`）— OK？
2. **MVP 仅 soft_poll + READY**（FAULT 后置）— OK？
3. **avoidance_car 去掉手写 device_tree / Flash 覆写**，改为 codegen L1 — OK？
4. **A/B 并存**：`sonar_helper_start` 保留且不投递事件 — OK？
