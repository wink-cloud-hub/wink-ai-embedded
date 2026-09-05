# `wink-app.json` role / 意图层演进计划（未来）

> **For agentic workers:** 本文是 **未来演进计划（Deferred）**，**当前里程碑不要求实现代码**。执行前置见 §4；开工前须另开 tech-design 并经 Owner 确认。  
> Domain skill：执行时对照 `embedded-best-practice` + codegen（ADR-0046）；文档回写对照 `docs-adr.md`。

**Goal:** 在保留 `devices[].type` ↔ DAL 驱动绑定（现状合理、ADR-0046 SSOT）的前提下，为 `wink-app.json` 规划可选的 **role / 运动意图** 字段与渐进披露，使低代码画布、AI 与业务代码默认只关心「轮子 / 关节 / 要速度还是到位」，而不被迫选择具体电机型号；硬件绑定仍由 `type`（或显式 `binding.driver`）承担。

**Architecture:** **双平面模型**——用户平面（role + intent + motion profile）与驱动平面（`type` / DAL 插件）共存于同一 `wink-app.json`；简单模式隐藏驱动平面（由板卡模板填充），进阶模式可改 binding。DAL 继续按控制语义细分（`dal_dc_motor` / `dal_rc_servo` / …）；BAL 继续做闭环编排；本演进**不**把 `type` 改成纯意图枚举。

**Tech Stack:** `wink-app.json` schema / codegen（`tools/codegen/drivers/`）/ 前端画布（后续）/ 可选 capability 宏生成。

## Global Constraints

- **现状不破坏**：在 Phase 0～1，`type` 仍为必填且必须是 `known_types()` 之一；不得删除或弱化 ADR-0046 registry。
- **本计划默认状态 = ⏸️ 暂停 / 未来触发**：不阻塞 [2026-07-28-dal-bal-followup-plan.md](./2026-07-28-dal-bal-followup-plan.md)（含 `dal_dc_motor` 改名、FOC ADR）。
- 「上层不关心型号」≠「系统没有型号」——binding 必须存在，仅对普通用户默认隐藏。
- 意图族至少覆盖：**位置型**与**速度型**；不可把全品类收成「只有目标角度 + 曲线」。
- Commit / 实现仅在 Owner 将本计划状态改为 🔄 且 tech-design Accepted 后开始。

---

## 1. 元数据表（🔴 必选）

| 字段 | 内容 |
|------|------|
| **计划编号** | `PLAN-20260728-WINK-APP-ROLE-INTENT` |
| **创建日期** | 2026-07-28 |
| **目标平台/SoC** | 文档全平台；落地时 `host` / `wasm` / `ESP32` |
| **工具链/SDK版本** | 沿用仓库当时 wink-tools / codegen |
| **计划状态** | ⏸️ **暂停（未来演进）** — 思想归档；未排期实现 |
| **优先级** | ⚪ P2（产品体验 / 低代码演进；不阻塞当前固件整改） |
| **计划版本** | `v1.2` |
| **关联技术设计** | 开工前另写 `docs/tech-designs/YYYY-MM-DD-wink-app-role-intent-design.md`（尚未创建） |
| **关联设计规范** | [01-app-business-logic.md](../../design/03-app-codegen/01-app-business-logic.md)、[03-ai-dsl-and-codegen-pipeline.md](../../design/03-app-codegen/03-ai-dsl-and-codegen-pipeline.md)、[`wink-app-json-guide.md`](../../../../wink-micro-os/docs/wink-app-json-guide.md) |
| **关联评审记录** | [2026-07-28-dal-actuator-motor-taxonomy-review.md](../../reviews/core/2026-07-28-dal-actuator-motor-taxonomy-review.md) |
| **关联 ADR** | [ADR-0046](../../decisions/core/0046-dal-driver-registry-ssot.md)（`type` SSOT）；拟与 followup 中 **ADR-0048**（actuator 控制语义命名）交叉；落地时可能新增 **ADR-00xx role/intent 平面** |
| **目标里程碑** | 低代码 / 画布体验 Wave（待定）；**非**当前 DAL/BAL followup 门禁 |
| **前置依赖计划** | 建议先完成 [dal-bal-followup](./2026-07-28-dal-bal-followup-plan.md) 的 T1（`dal_dc_motor` 正名）与 A2（`comm/`），减少命名漂移；**非硬阻塞文档归档** |
| **替代/废弃** | 无 |
| **计划负责人** | 项目 Owner + Agent |
| **所需子代理技能** | 开工时：`writing-plans` 细化 tech-design → `subagent-driven-development` |

---

## 2. 背景与目标（🔴 必选）

### 2.1 问题陈述

当前 `wink-app.json` 的 `devices[].type` **正确且合理地**映射到 DAL codegen 插件（如 `"rc_servo"` → `dal_rc_servo`，`"dc_motor"` → `dal_dc_motor`）。这对设备树生成、引脚绑定、`WINK_USE_*` 裁剪是必需的。

但低代码 / AI / 教育用户的业务心智是「左轮要速度」「云台要转到 90°」，而不是「选 H 桥直流还是 SG90」。若长期只有 DAL `type` 一等公民，画布与 AI 会被迫暴露硬件型号，与 App 层「聚焦业务流」的目标冲突。

同时，把全品类收成「目标角度 + 过渡曲线」**不够**：轮子要速度、夹爪可能要力矩、步进还要回零——需要**少量意图族**，而不是单一角度 API。

### 2.2 核心思想（已对齐的结论）

1. **DAL 按控制语义划分保持不变**（`dal_dc_motor` / `dal_rc_servo` / `dal_stepper` / …）——硬件诚实面。  
2. **`type` ↔ DAL 继续保留**——工程绑定面；现状设计合理，不是错误。  
3. **未来在 devices 上扩展 `role`（及可选 motion/intent）**——用户 / 业务平面；默认隐藏具体型号。  
4. **`wink-app.json` 不是 BAL 专属文件**：它是整机配方（DAL 绑定为主，BAL 编排可选）；role 层加在配方之上，服务 App/画布/AI。

### 2.3 技术/业务目标（触发落地后）

- ✅ Schema：`devices` 支持可选 `role`、可选 `motion` / `commands`；`type`（或 `binding.driver`）仍解析到 `known_types()`
- ✅ 简单模式：板卡 / 器件库模板自动填 `type`；用户主要编辑 role + 意图参数
- ✅ 进阶模式：可改 `type` / binding，换硬件不改 role 与业务图
- ✅ Codegen：可为 role 生成 capability 风格别名（如 `left_wheel_set_velocity`），底层仍静态绑定具体 `dal_*`
- ✅ 文档：`wink-app-json-guide` + App 规范写明双平面与渐进披露
- ✅ **本波（归档时）**：仅文档；零代码变更

### 2.4 成功指标（仅当计划翻为执行中）

| 指标 | 通过标准 | 验证方法 |
|------|----------|----------|
| 兼容 | 无 `role` 的旧 JSON 仍 100% codegen 成功 | 现有 sample `wink-app.json` golden |
| 双平面 | 含 `role` 的 JSON 能生成绑定 +（若启用）capability 别名 | 新 golden |
| 用户叙事 | 指南明确「默认看 role，进阶改 type」 | 文档评审 |
| 非目标守住 | 未把 `type` 改成 `joint`/`wheel` 唯一枚举 | schema / ADR |

---

## 3. 变更范围与影响分析（触发落地时）

### 3.1 预期触及面（未来）

| 路径 | 变更 | 说明 |
|------|------|------|
| `wink-app.json` schema / 校验 | ✏️ | 可选 `role`、`motion`、`commands` |
| `tools/codegen/**` | ✏️ | 解析 role；生成 capability / 文档注释；**不**用 role 替代 `get_driver(type)` |
| `wink-app-json-guide.md` | ✏️ | 双平面说明 |
| `03-app-codegen/*.md` | ✏️ | App/DSL 用户视图 |
| 前端画布（若有） | ✏️ | 简单/进阶 UI |
| DAL / BAL 生产 API | ❌ 原则上不改 | 绑定仍走现有 `dal_*` / BAL |

### 3.2 接口影响

| 层 | 破坏性 | 说明 |
|----|--------|------|
| JSON 旧文件 | ❌ 否（Phase 1） | `role` 可选 |
| DAL `type` 字符串 | ⚠️ 独立议题 | 与 followup T1 相关：C 改名不必立刻改 JSON `"motor"` |
| App C API | ⚠️ 可选 | capability 宏增量生成 |

### 3.3 架构红线

> 1. **禁止**删除或架空 `type`→DAL registry（ADR-0046）。  
> 2. **禁止**仅用 `role=joint` 而无 binding 就期望 codegen 猜驱动。  
> 3. **禁止**把全部执行器收成单一「角度+曲线」API。  
> 4. App 仍禁直接 `pal_*`；role 不改变分层红线。

---

## 4. 依赖、触发条件与风险

### 4.1 何时从「暂停」改为「执行」

满足任一产品触发即可排期（Owner 勾选）：

- [ ] 低代码画布需要「按角色拖拽」且用户抱怨必须选电机型号  
- [ ] AI codegen 频繁把步进/舵机/直流用错 `type`  
- [ ] 多板卡模板需要「同一业务图换 binding」的一等公民支持  

建议前置（降低返工）：

- [ ] followup **T1** `dal_dc_motor` 已合入或 JSON `type` 兼容策略已定  
- [ ] followup **T0 / ADR-0048** 控制语义命名已 Accepted（与 role 词汇表交叉）

### 4.2 风险（落地时）

| ID | 风险 | 缓解 |
|----|------|------|
| R-1 | role 与 type 语义重复、两套真相 | ADR 钉死：role=业务，type=驱动；校验表约束合法 pair |
| R-2 | AI 只填 role 忘填 type | 简单模式强制模板补全 type；校验失败给明确修复建议 |
| R-3 | 意图 API 膨胀成第二套 DAL | 意图族冻结为小集合（位置/速度/可选力矩）；新硬件只加 binding |
| R-4 | 与 BAL `control/` 职责重叠 | role 偏「声明与别名」；跨器件闭环仍走 BAL（chassis 等） |

---

## 5. 双平面设计基线（思想正文）

### 5.1 两平面

```text
┌─────────────────────────────────────────────────────────┐
│  用户平面（未来加强）                                      │
│  role / commands / motion profile                        │
│  例：role=wheel, commands=[set_velocity], ramp_ms=200    │
├─────────────────────────────────────────────────────────┤
│  驱动平面（现状 + 永久保留）                                 │
│  type → codegen drivers registry → dal_*                 │
│  例：type=motor（或未来 dc_motor）, pwm_channel, dir_pin… │
└─────────────────────────────────────────────────────────┘
```

### 5.2 意图族（业务侧最小集合）

| 意图族 | 典型 role | 用户关心的参数 | 常见 binding |
|--------|-----------|----------------|--------------|
| 位置型 | `joint` / `gimbal` | 目标位置/角、时长、线性/S 曲线、限速 | `servo`、步进、总线伺服 |
| 速度型 | `wheel` / `belt` | 目标速度、加减速 ramp | `dc_motor`、步进、FOC 速度环 |
| 力矩/限流型（进阶） | `gripper` | effort / 电流上限 | 智能驱动 / FOC |
| 传感/开关 | `sensor` / `indicator` | 事件、频率 | `ultrasonic` / `button` / `led` |

> 「目标角度 + 过渡曲线」是**位置型**的好默认，不是全品类唯一模型。

**核心结论（回顾用）：**

1. **意图族是「要什么」；`type` 是「拿什么硬件怎么做到」。** 同一意图族可以挂多种电机（例如速度 → `dc_motor` / `stepper` / `bldc`）。
2. **不是每种电机都有每种意图**（航模 `servo` 没有一等 `set_velocity`；开环步进没有真 `set_effort`）。
3. **统一意图 ≠ 统一 BAL/DAL 实现**：App/codegen 可用 `left_wheel_set_velocity`；底层按 `type` **静态**绑到不同 control/DAL 路径（禁一套 `wink_*` 内 `switch` 吃掉全部电机，ADR-0004）。
4. **详细配对表、L1～L3 意图清单、role×type 推荐** → **[附录 C](#附录-c意图族--电机类型配对详表)**。

### 5.3 目标 JSON 形态（示意，非现行 schema）

```json
{
  "app_name": "avoidance_car",
  "board": "esp32_devkitc",
  "devices": {
    "neck_joint": {
      "role": "joint",
      "commands": ["goto_position"],
      "motion": {
        "unit": "deg",
        "default_profile": { "type": "s_curve", "duration_ms": 400 }
      },
      "type": "rc_servo",
      "pwm_pin": 18,
      "pwm_channel": 0
    },
    "left_wheel": {
      "role": "wheel",
      "commands": ["set_velocity"],
      "motion": { "unit": "norm", "ramp_ms": 200 },
      "type": "motor",
      "pwm_channel": 0,
      "dir_pin_a": 5,
      "dir_pin_b": 6
    }
  }
}
```

现行合法写法（无 `role`）继续有效，例如 `avoidance_car`：

```json
"neck_servo": { "type": "rc_servo", "pwm_pin": 18, "pwm_channel": 0 }
```

### 5.4 与各层关系

| 层 | 关系 |
|----|------|
| **DAL** | `type` 选中的驱动；按控制语义细分（[ADR-0048](../../decisions/core/0048-actuator-control-semantic-naming.md)）不变 |
| **BAL** | 按**控制算法/编排契约**实现（如 `wink_closed_loop_dc_motor`、`wink_chassis`，[ADR-0049](../../decisions/core/0049-bal-closed-loop-dc-motor-naming.md)）；**不**按 role 收成万能 `wink_wheel_*` 门面；role 不替代 `wink_chassis` |
| **App** | 优先调用 capability / 业务名（可由 role 生成）；不写引脚 |
| **Codegen** | `get_driver(type)` 不变；role 只增加别名与 `(intent, type)` / `(role, type)` 校验 |

```text
left_wheel_set_velocity()          ← role/capability（生成薄名，意图统一）
        │ 编译期按 type 静态绑定
        ├─ wink_closed_loop_dc_motor_*  (+ dal_dc_motor + encoder)
        ├─ 未来 wink_*_stepper 速度路径 (+ dal_stepper)
        └─ 未来 FOC 速度设定缓冲       (+ dal_bldc)
```

### 5.5 现状 `type` 取值来源（备忘）

- SSOT：`wink-tools/tools/codegen/drivers/*.py` 中 `DriverBase.type`  
- 查询：`known_types()` / `list_drivers.py`  
- 当前含：`led`、`button`、`ultrasonic`、`servo`、`dc_motor`（原 `motor`，ADR-0048）、`encoder`、`ssd1306`、`eeprom`、`gps`  
- roadmap / 预留：`stepper`、`industrial_servo`、`bldc`（见 ADR-0048）  
- **不是** BAL 类型枚举；**也不是** role 枚举  

---

## 6. 分阶段演进（未来执行时按此拆 Task）

> 下列 Phase **均未开工**。翻为执行中后，再写成带 checkbox 的详细 Task。

### Phase 0 — 文档与词汇表（可最先做，仍属本演进）

- 冻结 role 词汇表初稿（`wheel` / `joint` / `gripper` / `indicator` / `sensor` …）  
- 冻结意图族与合法 `(role, type)` / `(intent, type)` 配对表（基线见 **[附录 C](#附录-c意图族--电机类型配对详表)**）  
- 回写指南「双平面 + 渐进披露」；**不改 codegen**

### Phase 1 — Schema 可选字段 + 校验

- `role` / `motion` / `commands` 可选  
- 未知 role 或非法 pair → codegen 清晰报错  
- 全量旧 sample 无回归  

### Phase 2 — Codegen capability 别名

- 按实例名 + role/commands 生成稳定宏或薄包装（静态绑定，禁运行期 vtable）  
- golden 测试  

### Phase 3 — 简单模式 UX

- 板卡模板 / 器件库：选 role 自动填 `type` + 默认引脚  
- 画布默认隐藏 `type`；进阶面板可改  

### Phase 4 —（可选）字段重命名清晰化

- 考虑 `type` 保留或显式化为 `binding.driver`  
- JSON `"motor"` → `"dc_motor"` 与 followup T1 策略对齐（compat 窗口）  

---

## 7. 非目标（明确不做）

- ❌ 用本计划替代 followup 的 `dal_dc_motor` / FOC ADR / 闭环单测工作  
- ❌ 在本计划内实现 `dal_stepper` / VESC / SimpleFOC  
- ❌ 删除 `type` 或让 AI「只写 role」即可通过 codegen（无 binding）  
- ❌ 引入运行期器件多态 vtable（ADR-0004）  
- ❌ 立刻大改所有现有 sample 的 JSON（Phase 1 必须兼容）  

---

## 8. 回滚与降级

- Phase 0 仅文档：revert 文档即可  
- Phase 1+：`role` 为可选 → 去掉新字段即可回退旧路径；保留 `type` 解析为主路径  

---

## 9. 参考资料

- [dal-bal-followup 计划](./2026-07-28-dal-bal-followup-plan.md)（当前执行主线；本演进不阻塞）  
- [actuator 电机分类评审](../../reviews/core/2026-07-28-dal-actuator-motor-taxonomy-review.md)  
- [ADR-0046 DAL registry SSOT](../../decisions/core/0046-dal-driver-registry-ssot.md)  
- [ADR-0048 Actuator 控制语义命名](../../decisions/core/0048-actuator-control-semantic-naming.md)  
- [ADR-0049 BAL 闭环正名 `wink_closed_loop_dc_motor`](../../decisions/core/0049-bal-closed-loop-dc-motor-naming.md)  
- [App 业务逻辑规范](../../design/03-app-codegen/01-app-business-logic.md)  
- [Codegen 管线](../../design/03-app-codegen/03-ai-dsl-and-codegen-pipeline.md)  
- [`wink-app-json-guide.md`](../../../../wink-micro-os/docs/wink-app-json-guide.md)  
- [计划模板](../00-IMPLEMENTATION-PLAN-TEMPLATE.md)  

---

### 计划版本变更记录

| 版本 | 日期 | 变更内容 | 变更人 |
|------|------|----------|--------|
| v1.0 | 2026-07-28 | 初稿：双平面思想归档为未来演进计划；状态=暂停 | Agent |
| v1.1 | 2026-07-28 | 增补附录 C：意图族 × 电机类型配对详表；强化 §5.2/§5.4「同意图、多后端」结论；对齐 ADR-0048/0049 | Agent |
| v1.2 | 2026-07-28 | 附录 C / 正文对齐 ADR-0050：`rc_servo` ↔ `industrial_servo` | Agent |

---

## 附录 A：与当前 followup 的分工

| 事项 | 归属 |
|------|------|
| `dal_motor`→`dal_dc_motor`、brake/coast | followup **T0/T1**（近期） |
| FOC ISR / `pal_hwtimer` ADR | followup **Wave B** |
| `communication`→`comm` | followup **A2** |
| devices `role` / 意图 / 画布简单模式 | **本计划**（未来） |

---

## 附录 B：一句话给 Owner

**现在：`type`=DAL，合理且保留。未来：加 `role`（等）让用户默认只谈业务；型号留在绑定层，由模板填。**

**自检签字（归档）**：____________________  
**日期**：2026-07-28

---

## 附录 C：意图族 × 电机类型配对详表

> **目的**：回顾「同意图、多后端」时的 SSOT 草稿。Phase 0 冻结词汇表时以此为基线修订。  
> **图例**：✅ 一等支持 · ⚠️ 能做但不自然/要附加条件 · ❌ 不适合当该意图的一等公民  
> **`type` 列**对齐 [ADR-0048](../../decisions/core/0048-actuator-control-semantic-naming.md)：`dc_motor` / `rc_servo` / `stepper` / `industrial_servo` / `bldc`。

### C.1 核心运动意图族

| 意图族 | 用户说法 | 典型单位 | `dc_motor` | `rc_servo`（航模） | `stepper` | `industrial_servo`（工业/总线） | `bldc`（本地 FOC） |
|--------|----------|----------|------------|-----------------|-----------|----------------------------|---------------------|
| **速度** `set_velocity` | 转多快 | rad/s、rpm、norm、counts/s、steps/s | ✅（常需 encoder 闭环） | ❌（位置舵机）；⚠️ 连续转舵机须另 type | ✅（steps/s + 加减速） | ✅ | ✅ |
| **位置** `goto_position` | 转到/走到某处 | deg、rad、mm、步 | ⚠️（要编码器 + 位置环 BAL） | ✅ | ✅ | ✅ | ✅（位置环） |
| **相对位移** `move_relative` | 再走 N 步/度 | 同上 | ⚠️（同位置环） | ⚠️（现角+Δ，易漂） | ✅ | ✅ | ✅ |
| **力矩/用力** `set_effort` | 夹紧力、限流 | N·m、A、norm | ⚠️（电流传感少则弱） | ❌ | ⚠️（电流/力矩有限） | ✅ | ✅ |
| **停/安全态** `stop` / `safe_off` | 停住或松开 | — | ✅ brake/coast | ✅ limp | ✅ hold/release | ✅ disable/抱闸 | ✅ 三相断 |
| **使能** `enable` / `disable` | 上电励磁/掉使能 | — | ⚠️（常等价 coast/brake） | ⚠️ | ✅ | ✅ | ✅ |

> **步进可以控速度**：意图族仍是 `set_velocity`；实现路径是脉冲定时（steps/s），与 DC 的「PID→占空比」、FOC 的「速度环→Iq」是**同一意图、不同路径**。

### C.2 运动形态 / 剖面（叠在速度或位置上）

| 意图族 | 含义 | 谁常用 | 备注 |
|--------|------|--------|------|
| **点到点 + 曲线** `goto` + `profile=linear\|s_curve` | 位置型默认体验 | rc_servo / stepper / industrial_servo | 「角度+曲线」只属于位置型 |
| **斜坡速度** `set_velocity` + `ramp` | 加减速度限制 | dc / stepper / bldc | 步进尤其重要（防失步） |
| **点动** `jog` | 按住就动、松开停 | 全部位置/速度型 | 多为 UI；底层仍是 velocity/position |
| **回零** `home` | 找原点/限位 | stepper / industrial_servo / 带限位的轴 | dc+encoder 也可；航模舵机通常 ❌ |
| **同步多轴** `sync_move` | 多关节同时到位 | 多 `joint` | BAL/编排层，不是单一 DAL |
| **轨迹跟踪** `follow_trajectory` | 连续路点 | 工业伺服 / 高端 FOC | 教育低代码可后置 |

### C.3 约束与保护意图（进阶）

| 意图族 | 含义 | 适配 |
|--------|------|------|
| **软限位** `set_soft_limits` | 工作区 | 位置型轴 ✅；纯轮式连续转 ❌ |
| **速度上限** `set_vel_limit` | 位置运动中限速 | 位置环后端 ✅ |
| **加速度上限** `set_accel_limit` | 防冲击/失步 | 步进 / 伺服 / FOC ✅ |
| **电流/力矩上限** `set_effort_limit` | 防堵转 | FOC / 智能伺服 ✅；开环步进 ⚠️；航模舵机 ❌ |
| **失步/堵转检测** `on_stall` | 事件 | 步进（传感/Smart）、FOC、带电流的 DC |
| **反馈丢失 fail-safe** | 编码停跳 → 安全关断 | 软件闭环 DC/FOC ✅；开环步进 N/A |

### C.4 Role × 主意图 × 合法 `type`（配对草稿）

| Role（业务） | 主意图族 | 推荐 `type` | 不推荐 |
|--------------|----------|-------------|--------|
| **wheel / belt** | 速度；可选 ramp、stop | `dc_motor`(+enc)、`stepper`、`bldc` | 航模 `servo` |
| **joint / gimbal** | 位置；曲线、软限位；可选 home | `rc_servo`、`stepper`、`industrial_servo`、`bldc` 位置环 | 无反馈的裸 `dc_motor`（除非加编码器当轴） |
| **gripper** | 位置或 effort；effort_limit | `rc_servo`、`industrial_servo`、`bldc` | 连续轮式 DC（除非丝杠+行程） |
| **lift / linear** | 位置；home；软限位 | `stepper`、`industrial_servo`、带丝杠的 DC+enc | 航模角位移舵机（行程不对） |
| **vibrator / on-off** | enable / 弱占空比 | 专用或 `dc_motor` 开环 | 硬塞进 joint 位置 API |

### C.5 建议冻结的意图族清单（由粗到细）

**L1 — 用户平面最少集合（先落地）**

1. `set_velocity`
2. `goto_position`
3. `move_relative`
4. `stop` / `safe_off`（可合成一个命令，模式由 `type` 决定）
5. `enable` / `disable`（总线 / FOC / 步进优先）

**L2 — 运动质量（画布常用）**

6. `motion_profile`（linear / s_curve / ramp）
7. `jog`
8. `home`

**L3 — 安全与进阶（工业/竞赛）**

9. `set_soft_limits`
10. `set_vel_limit` / `set_accel_limit`
11. `set_effort` / `set_effort_limit`
12. `sync_move` / `follow_trajectory`（多轴）

> 落地策略：先 L1+L2；L3 不挡简单模式。意图 API 膨胀风险见 §4 R-3——新硬件只加 binding，不新开意图族，除非 L1～L3 无法表达。

### C.6 与 BAL / DAL 边界（防回潮）

| 做法 | 结论 |
|------|------|
| App/codegen：`left_wheel_set_velocity` 同意图名 | ✅ 推荐 |
| DAL：按控制语义拆 `dal_dc_motor` / `dal_stepper` / … | ✅ 已采纳（ADR-0048） |
| BAL：`wink_closed_loop_dc_motor` 等按后端契约分套 | ✅ 已正名（ADR-0049） |
| BAL：按 role 实现万能 `wink_wheel_*` 内吞所有电机 | ❌ 禁止 |
| 一套 BAL C API + 运行期 `switch(type)` / vtable | ❌ 禁止（ADR-0004） |

