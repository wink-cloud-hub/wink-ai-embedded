# Phase 0: 机械修复与文档同步

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.
>
> **核验状态（2026-06-24）：** 本计划所有 file:line 均已对照 `wink-micro-os/` 真实源码逐行确认。事实清单见各 Task 的「Source-of-truth check」。

**Goal:** 机械性修复 review 中 P1-2（大括号 + 门禁）、P1-3（trace 并发契约声明）、P1-4（simulation.md SSOT 漂移）、P2-3（魔法数提取）——不改变函数签名与控制流语义，完成后 host 测试全绿、文档与代码 SSOT 闭合。

**Architecture:**
- Task 0-1 / 0-2 / 0-3 / 0-5 为**真·零风险机械操作**（格式 / 注释 / 文档 / lint 规则），不涉及语义。
- Task 0-4 为**一次谨慎的魔法数微重构**（常量派生），风险面小但非纯格式化——须与 0-1 串行。
- 全阶段不改变任何 `wink_status_t` 语义、不改变 `if/while` 条件真值。

**Tech Stack:** C99, Unity test, ctest, clang-tidy（可选门禁）

## Global Constraints
- 不改变任何函数签名
- 不改变任何控制流逻辑：`if/while/for` 的条件表达式与各分支返回值**语义**保持不变（仅补 `{}`、仅替换字面量为等值常量）
- 所有现有 host 测试必须保持 PASS（`python wink-tools/wink.py test`，8 个测试二进制全绿）
- **并行约束**：Task 0-1 与 Task 0-4 都修改 `dal_servo.c`，**禁止并行**（见各 Task 的 Depends-On）

## Sequencing（执行编排，给 subagent-driven-development 用）
- **可并行**（文件互不相交）：0-2（trace）、0-3（simulation.md）、0-5（lint 规则）
- **必须串行**：`0-1 → 0-4`（同改 `dal_servo.c`；0-1 纯格式先行，0-4 微重构后行）
- **收尾**：0-5 lint 门禁应在 0-1 落地后启用，以锁定大括号成果防回退

## Verification Gate（本阶段统一出口，三项全过才算 Phase 0 完成）
1. `python wink-tools/wink.py test` → `[PASS] All tests passed`（8/8）——证明 0-1/0-4 无行为回归
2. `Select-String -Path ".claude\skills\embedded-best-practice\references\static-dispatch\simulation.md" -Pattern "js_sim_get_ultrasonic_distance"` → **0 命中**——证明 0-3 文档 SSOT 闭合
3. `Select-String -Path "wink-micro-os\trace\include\wink_trace.h" -Pattern "Thread-safety"` → 命中——证明 0-2 契约落地

---

### Task 0-1: 单行控制语句补大括号（DAL 全量）

**Files:**
- Modify: `wink-micro-os/dal/src/dal_servo.c`
- Modify: `wink-micro-os/dal/src/dal_ultrasonic.c`

**Depends-On:** 无；**Blocked-By 并行约束**：与 0-4 共改 `dal_servo.c`，二者不可并行（建议 0-1 先行）。

**Source-of-truth check:** 已逐行核对两文件当前内容，行号与改动点如下。

**Precise changes:**

`dal_servo.c`（5 处）——补 `{}`，条件与返回值不变：
- L7 `if (dev == NULL) return WINK_ERR_INVALID_ARG;` → `if (dev == NULL) { return WINK_ERR_INVALID_ARG; }`
- L9 `if (angle < 0.0f) angle = 0.0f;` → `if (angle < 0.0f) { angle = 0.0f; }`
- L10 `if (angle > 180.0f) angle = 180.0f;` → `if (angle > 180.0f) { angle = 180.0f; }`
- L17 `if (!pal_pwm_init(dev->pwm_channel, SERVO_PWM_FREQ_HZ)) return WINK_ERR_IO;` → 补 `{}`
- L18 `if (!pal_pwm_set_duty(dev->pwm_channel, duty_percent)) return WINK_ERR_IO;` → 补 `{}`

`dal_ultrasonic.c`（5 处，**跨 `#ifdef SIMULATION` 两份副本，两侧都要改**）：
- 仿真分支：L21（`dev/distance_cm` NULL 判定）、L28（`pulse_us >= ULTRASONIC_TIMEOUT_US`）
- 真机分支：L39（同判空逻辑的第二份副本）、L47（`wait_start` 超时）、L52（`echo_start` 超时）

> ⚠️ **为什么两份都要改**：L21/L39 是同一判空逻辑的重复，L28/L47/L52 是超时判定的重复。若只改仿真分支、漏掉真机分支，后续 Phase 4 重构超声波真机路径时 diff 对比会失真。所有 `while` 体当前已合规（L46/L51），无需改动。

**Test:** `python wink-tools/wink.py test` → 8 PASS。其中 `test_dal_servo` + `test_dal_ultrasonic` + `test_dal_ultrasonic_sim` 覆盖行为不变即证明无语义变更。

---

### Task 0-2: trace 并发契约声明

**Files:**
- Modify: `wink-micro-os/trace/include/wink_trace.h`（公共 API 契约）
- Modify: `wink-micro-os/trace/src/wink_trace.c`（共享状态处补 invariant 注释，防回归）

**Depends-On:** 无（与其它 Task 文件不相交，可并行）。

**Source-of-truth check:** 当前 `wink_trace.h` 公共 API（`wink_trace_reset/fault/count/last`）**均无任何 thread-safety / ISR-safe 声明**，确认需补。

**Precise changes:**

`wink_trace.h`——文件头总则补一行，并在每个公共函数 `@note` 追加：
```c
 * @note Thread-safety: NOT thread-safe. 仅限 runtime 主循环单上下文调用。
 * @note ISR-safe: No. ISR / 工作线程 / 异步回调上报须由调用方提供关中断临界区保护。
```

`wink_trace.c`——在 `s_count` / `s_head` 静态变量定义处补 invariant 注释：
```c
/* INVARIANT: 仅 runtime 主循环单上下文访问。
   新增第二调用点（ISR / 线程 / 异步回调）前，必须先在本 RMW 路径加关中断临界区
   或 PAL lock——volatile 不提供原子性，不可替代。 */
```

**Test:** 编译通过；`Select-String wink_trace.h "Thread-safety"` 命中。

---

### Task 0-3: simulation.md 同步为 trigger/echo 两段式旁路

**Files:**
- Modify: `.claude/skills/embedded-best-practice/references/static-dispatch/simulation.md`

**Depends-On:** 无（改文档，与代码文件不相交，可并行）。

> ⚠️ **验证标准**：本 Task 改的是 `.claude/skills/.../simulation.md`（文档），**host 测试不覆盖它**。验证靠 `grep` + 与 `dal_ultrasonic.c:14-34` 逐行语义对照，**不是**跑 `python wink-tools/wink.py test`。

**Source-of-truth check:** 已确认 `simulation.md` 多处仍为旧 `js_sim_get_ultrasonic_distance`：§2 模板（L38/L44）、§1 保真度表 L2 行（L17）、§3.1 注入描述（L66）、§3.2 JS 桩（L81–95）。而 `dal_ultrasonic.c` 已是 trigger/echo 两段式（L24 `js_sim_trigger_ultrasonic` + L27 `js_sim_measure_echo_pulse_us`）。

**分两步执行（不可当 find/replace）：**

**步骤 A — 清除旧符号**（全文应全部消失）：
- §2 双模直通模板 L38/L44（整段重写，见步骤 B）
- §1 L2 保真度表 L17：「Trig 后忙等 10ms 读数」与新模型（30ms timeout、trigger/echo 脉宽）不符，修正描述
- §3.1 L66 注入描述
- §3.2 L81–95 JS 桩模板（重写，见步骤 B）

**步骤 B — 重写而非替换**（签名语义变了，不是改名）：
- **§2 模板**：替换为 `dal_ultrasonic.c:14-34` 当前 SIMULATION 分支真实代码（`js_sim_trigger_ultrasonic` → `js_sim_measure_echo_pulse_us` → `ULTRASONIC_TIMEOUT_US` 比较 → `dal_pulse_us_to_cm` 同源换算）。必须强调：换算、超时、错误处理**与真机分支同源**（ADR-0003 决策2 / c-code.md §2），旁路的只是物理信号**来源**。
- **§3.2 JS 桩**：旧桩 `js_sim_get_ultrasonic_distance(trigPin, distancePtr)` 返回状态 + 写指针。新模型是两个 import：
  - `js_sim_trigger_ultrasonic(trigPin)`：触发，无返回量；
  - `js_sim_measure_echo_pulse_us(trigPin): number`：返回 echo 脉宽 μs。
  错误注入须从「注入距离错误」改为「注入 echo 脉宽 ≥ 超阈值 / 直接注入超时」。整段桩与 §3.3 C 示例对接须相应重写。

**Verification:**
- `Select-String simulation.md "js_sim_get_ultrasonic_distance"` → **0 命中**
- §2 / §3.2 代码段与 `dal_ultrasonic.c:14-34` 逐行语义一致

---

### Task 0-4: DAL 舵机魔法数提取（P2-3，谨慎微重构）

> ⚠️ 本任务为**微重构**（非纯格式化）。Depends-On: **0-1**（同改 `dal_servo.c`，必须后于 0-1 串行执行）。

**Files:**
- Modify: `wink-micro-os/dal/src/dal_servo.c`（常量定义放 **`.c` 文件头**，不进公共头——见下理由）

**Source-of-truth check:** `dal_servo.c` 当前魔法数：L9 `0.0f`、L10 与 L14 的 `180.0f`、L15 的 `20.0f`（周期）与 `100.0f`（百分比）。L4 已有 `#define SERVO_PWM_FREQ_HZ 50`。

**Placement rationale（为什么不进 `dal_servo.h`）：**
`dal_servo_t` 已将 `min_pulse_ms` / `max_pulse_ms` 设为**实例字段**（支持不同舵机）。角度上限在逻辑上也属器件属性（270° 舵机存在）。公共头硬编码 `SERVO_MAX_ANGLE_DEG` 会与实例模型冲突。本阶段仅在 `.c` 内提取局部常量消除魔法数；器件可配置化（max_angle 入 struct）留待后续，不在此扩面。

**Produces（`dal_servo.c` 文件头）：**
```c
#define SERVO_PWM_FREQ_HZ    50                             /* 已有，保留 */
#define SERVO_PERIOD_MS      (1000.0f / SERVO_PWM_FREQ_HZ)  /* 派生：单一真相源，禁止再写 20.0f */
#define SERVO_MIN_ANGLE_DEG  0.0f
#define SERVO_MAX_ANGLE_DEG  180.0f
#define SERVO_DUTY_FULL_PCT  100.0f
```

> ⚠️ **关键纠错**：`SERVO_PERIOD_MS` **必须派生**自 `SERVO_PWM_FREQ_HZ`（`1000.0f / 50 = 20.0f`），不可独立再写字面量 `20.0f`。否则制造「频率」与「周期」两个必须手动保持一致的真相源，恰违背消除魔法数的本意。

**替换点：**
- L9 `0.0f` → `SERVO_MIN_ANGLE_DEG`
- L10、L14 `180.0f` → `SERVO_MAX_ANGLE_DEG`（两处）
- L15 `20.0f` → `SERVO_PERIOD_MS`；`100.0f` → `SERVO_DUTY_FULL_PCT`

**Test:** `python wink-tools/wink.py test` → 8 PASS。`test_dal_servo` 的角度→占空比数值断言不变即证明派生常量等值正确。

---

### Task 0-5: 大括号门禁（P1-2 闭环的另一半）

> ⚠️ review P1-2 原文要求「补齐 **+ 通过 lint 建门禁**」。Task 0-1 只做了前半句手动补齐；**没有门禁，下一轮 AI-CodeGen 会重新生成无大括号 `if`**，整改白做。本 Task 落地最小门禁锁定成果。

**Files:**
- Create: `wink-micro-os/.clang-tidy`（仓库级最小配置）

**Produces:** 最小可执行 lint 规则，聚焦本次诉求：
```yaml
# 最小门禁：强制 if/for/while/do-while 体使用大括号（review P1-2）
Checks: 'readability-braces-around-statements,-*'
WarningsAsErrors: '*'
```

**Verification（若有 clang-tidy 工具链）：**
```powershell
clang-tidy -p build-test wink-micro-os/dal/src/dal_servo.c wink-micro-os/dal/src/dal_ultrasonic.c
```
→ 0 告警（与 0-1 成果一致）。

**Deferred 声明（防误判闭环）：**
若本仓库 CI 尚未接入 clang-tidy，则**不得**在整改跟踪表把 P1-2 标为「已完成」。应在 `docs/reviews/core/2026-06-24-wink-micro-os-integrated-review.md` 整改跟踪表将 P1-2 状态记为「**部分完成**：手动补齐 ✓（Phase 0 / Task 0-1）；CI 门禁待接入（规则已就位于 Task 0-5）」。

---

## 出口验收（agentic worker 完成自检清单）
- [ ] Verification Gate 三项全过
- [ ] `git diff` 仅含：`{}` 补齐、注释、文档、`.clang-tidy`、常量提取——无控制流/返回值/签名变更
- [ ] 整改跟踪表 P1-2/P1-3/P1-4/P2-3 状态更新（注意 P1-2 为「部分完成」）
