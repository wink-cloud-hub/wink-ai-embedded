# ADR-0010：Boot safe-lock 连续复位计数与恢复策略（修订 ADR-0007）

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳）** |
| 日期 | 2026-06-28 |
| 触发 | ESP32 DevKitC 真机实测：长按 BOOT >3s 触发 WDT 复位测试（smoke S8）后，设备进入永久锁死态——LED 常亮、不闪烁、按键不计数、无逻辑输出；与 S8「复位后应自动恢复」验收口径冲突 |
| 影响范围 | runtime（`wink_runtime.c` boot safe-lock）/ PAL OSAL（新增 boot-count 接口）/ ESP32 target（RTC 持久化）/ 冒烟验收 S8 / 设计规范 §3.4 |
| 决策者 | 项目负责人 |
| 关联 | 修订 [ADR-0007](0007-cooperative-loop-execution-model.md) 的 boot safe-lock 硬约束；前置冒烟报告 [2026-06-27](../../reviews/core/2026-06-27-devkitc-smoke-hardware-verification.md) S8 |

---

## 背景（Context）

### 1. 现状 boot safe-lock（ADR-0007 硬约束 1）

`wink_runtime_run` 启动时（`wink_runtime.c:94-101`）：

```c
rr = pal_get_reset_reason();
if (rr == WATCHDOG || rr == PANIC) {
    wink_trace_fault(8001);
    wink_actuator_safe_off_all();
    wink_runtime_fault(callbacks, 8001);   /* 内部再 trace 8001 + safe_off + on_fault */
    return WINK_ERR_LOCKED;                /* 绝不执行 cb.init()/cb.loop() */
}
```

设计意图（ADR-0007）：WDT/PANIC 复位后**严禁再跑用户代码**，防止「崩溃→复位→再跑同一份崩溃代码→再崩溃」的死循环。

### 2. 实测暴露的问题

用户长按 BOOT >3s（smoke S8 的 WDT 复位测试）后：

| 现象 | 根因 |
|------|------|
| LED 常亮、不闪烁 | `app_loop` 被锁死、未执行；且 `app_init` 未跑 → GPIO 未配置、执行器注册表为空 → `safe_off_all` 空操作、`dal_led_off` 因 `initialized==false` 不碰 GPIO |
| 按键不计数 | `app_init` 未注册 GPIO ISR，`app_loop` 未采样 |
| `Faults: 3` | 8001 被连 trace 三次（safe-lock 一次 + `wink_runtime_fault` 一次 + 样例 `on_fault` 一次）|
| 栈告警 `free=0B` | runtime 任务 `return WINK_ERR_LOCKED` 后 `vTaskDelete(NULL)` 自删，`app_main` telemetry 对悬空句柄调 `uxTaskGetStackHighWaterMark` 返回 0（误报，见评审记录）|

**核心矛盾**：safe-lock 把「单次 WDT/PANIC 复位」一律当作「死循环信号」永久锁死。但 WDT/PANIC 复位有两种成因，safe-lock 在复位原因寄存器层面**无法区分**：

- **真死循环**：app 代码自身有 bug，每次启动都崩/挂死 → 反复 WDT 复位。这才是 safe-lock 要防的。
- **单次/偶发复位**：冒烟测试故意触发、或一次性毛刺。复位后代码其实能正常跑，**不会**死循环。

当前策略对后者误伤：用户按一次 BOOT 测试，设备就永久锁死、需现场断电恢复。

### 3. smoke S8 验收口径与实现的冲突

S8 期望「WDT 复位后 app 照跑、打印 `watchdog: PASS`、设备继续工作」（`app_callbacks.c:206` 的 PASS 打印在 `app_init` 内）。但 safe-lock 跳过 `app_init`，该 PASS 永远打不出。**S8 的「PASS」此前对不上真机行为**（与 S1 栈告警 PASS 同属过度乐观标注）。

---

## 方案比选（Options）

### 方案 A：维持现状（一次异常复位即永久锁死）
- **优点**：实现最简，死循环防护最强。
- **缺点**：单次/偶发复位误锁，开发与冒烟体验差；与 S8 验收口径冲突；现场必须断电恢复。

### 方案 B：仅按复位类型区分（PANIC 锁、WDT 放行）
- **思路**：PANIC（硬 fault）几乎必为代码崩溃 → 锁；WDT 可能是测试/挂死 → 放行。
- **缺点**：WDT 同样可由代码挂死（如死循环饿死看门狗）反复触发，放行 WDT 会漏防这类死循环。**防护不完整，否决。**

### 方案 C：连续异常复位计数 + 健康里程碑清零（采纳）
- **思路**：用「连续异常复位次数」区分「偶发」与「真死循环」。真死循环的签名是**每次启动都崩、永远跑不到稳定态**；偶发复位后系统能跑到稳定态。
- **机制**：
  - 异常复位（WDT/PANIC）计数 +1（持久化在 ESP32 RTC 内存，跨复位保留、断电清零）。
  - POWERON 复位计数清零。
  - 计数 ≥ 阈值 N → 锁死（同现状）。
  - 计数 < N → 放行恢复，正常跑 `init`/`loop`。
  - **健康里程碑**：`init` 成功且持续运行满 M tick（≈2s）不崩 → 证明已越过崩溃点 → 计数清零。
- **优点**：精确区分真死循环（永远到不了里程碑）与偶发/测试复位（能到里程碑）；单次测试自动恢复；不削弱对真死循环的防护。
- **代价**：新增 PAL boot-count 接口（双 target stub）；ESP32 用 RTC_NOIT + magic 持久化。

---

## 决策结论（Decision）

采纳 **方案 C**，参数经用户确认：

| 参数 | 取值 | 含义 |
|------|------|------|
| `WINK_BOOT_LOCK_THRESHOLD` | **3** | 连续 3 次异常复位才锁死；单次/双次自动恢复 |
| `WINK_BOOT_HEALTHY_TICKS` | **200** | ≈2s（默认 10ms tick）；`init` 成功 + 跑满 200 tick 不崩则清零计数 |

### 状态机（替换 `wink_runtime.c:94-101`）

```c
rr = pal_get_reset_reason();
if (rr == PAL_RESET_REASON_POWER_ON) {
    pal_set_abnormal_boot_count(0);                 /* 断电重启：清零（兼覆盖 RTC 垃圾值）*/
} else if (rr == PAL_RESET_REASON_WATCHDOG || rr == PAL_RESET_REASON_PANIC) {
    uint32_t c = pal_get_abnormal_boot_count() + 1;
    pal_set_abnormal_boot_count(c);
    if (c >= WINK_BOOT_LOCK_THRESHOLD) {            /* 真死循环：锁死 */
        wink_runtime_fault(callbacks, WINK_FAULT_BOOT_AFTER_RESET);  /* trace+safe_off+on_fault，仅一次 trace */
        return WINK_ERR_LOCKED;
    }
    /* c < N：放行恢复（不 trace，恢复非故障）*/
}
/* SW/BROWNOUT/UNKNOWN：不动计数，放行 */

/* tick 循环内 */
if (tick == WINK_BOOT_HEALTHY_TICKS) {
    pal_set_abnormal_boot_count(0);                 /* 跑过崩溃点且稳定 ~2s：清零 */
}
```

### 行为矩阵

| 场景 | 计数轨迹 | 结果 |
|------|---------|------|
| 长按测试 1 次 | 0→复位→1→放行→跑满 2s→清零 | **自动恢复闪烁** ✓ |
| 真死循环（崩在 init） | 1→2→3（永不到里程碑） | **第 3 次锁死** ✓ |
| 真死循环（崩在 loop 前 2s 内） | 1→2→3（跑不满 200 tick） | **第 3 次锁死** ✓ |
| 断电重启 | POWERON→清零 | 正常启动 |

> **关于「连续手动长按 N 次」**：因里程碑清零，连续手动长按测试会在每次中途清零，**不会触发锁死**——只有「代码自己崩、永远跑不到 2s」才锁。这是方案 C 的预期语义（手动测试恒恢复，真崩溃循环才锁）。

### 顺带修复：8001 三连 trace
锁死路径原先 trace 8001 三次（safe-lock + `wink_runtime_fault` + 样例 `on_fault`）。改为锁死路径仅经 `wink_runtime_fault` 调用一次（runtime 侧 trace 一次）；样例 `app_on_fault` 作为通知回调不再重复 trace（runtime 已 trace）。

---

## 后果与约束（Consequences & Constraints）

### 硬约束
1. **计数持久化语义**：ESP32 必须用 `RTC_NOINIT_ATTR`（跨 WDT/panic 复位保留、断电丢失）+ magic 守卫（防 RTC 残留值）。POWERON 分支主动写 0，不依赖 RTC 上电默认值。
2. **双 target 同源（ADR-0002）**：新增 PAL 接口 `pal_get_abnormal_boot_count` / `pal_set_abnormal_boot_count`；ESP32 实现 RTC 持久化，host 可注入（供单测），wasm/baremetal stub（返回 0 / no-op）。
3. **恢复路径不 trace**：单次/偶发复位恢复是正常态，不计入 Faults；如需诊断可后续加 `WINK_INFO_RECOVERED`（follow-up）。
4. **阈值/里程碑为编译期安全常量**：`WINK_BOOT_LOCK_THRESHOLD`、`WINK_BOOT_HEALTHY_TICKS` 是安全策略参数，**不**进 `wink_app.json` codegen（区别于 ADR-0007 硬约束 4 的 Tick SSOT）。

### 兼容性
- **修订 ADR-0007 硬约束 1**：从「一次异常复位即永久锁死」改为「连续 N 次（且非健康）才锁死」。ADR-0007 原文保持只读，本 ADR 记录修订。
- **smoke S8 验收口径更新**：从「复位后照跑打印 PASS」改为「单次复位自动恢复、连续崩溃 3 次才锁」。

### 风险（Risk Register）
1. **RTC 残留值误判**：快速掉电上电可能使 RTC_NOIT 残留旧值。缓解：magic 守卫 + POWERON 主动清零。
2. **里程碑值与 tick 周期耦合**：200 tick 假设 10ms tick（=2s）。若 `WINK_RUNTIME_TICK_MS` 大幅变更需同步复核。当前 codegen 为 10ms，无影响。
3. **真崩溃但每次恰好撑过 2s**：理论上死循环若每次崩在 2s 后会被误判恢复。极端情况，可接受（2s 后崩溃已非启动期死循环）。

---

## 遵循与后续（Compliance & Follow-up）

1. **Backlog**：
   - [x] PAL boot-count 接口（`pal_osal.h` 声明 + ESP32 RTC 实现 + host/wasm/baremetal stub）。
   - [x] `wink_runtime.c` safe-lock 状态机重写 + 里程碑清零 + 8001 去重 trace。
   - [x] `wink_runtime.h` 加 `WINK_BOOT_LOCK_THRESHOLD` / `WINK_BOOT_HEALTHY_TICKS` 常量。
   - [x] host 单测：恢复路径、里程碑清零时序、阈值锁死、8001 单次 trace。
   - [x] 样例 `app_on_fault` 去重 trace（devkitc_smoke）。
2. **回写（Acceptance 后）**：
   - [x] `02-wink-micro-os/04-runtime-and-trace.md` §3.4 更新 safe-lock 语义。
   - [x] smoke S8 验收口径更新（恢复检测改查 `pal_get_abnormal_boot_count`；旧 `wink_trace_last()==8001` 在恢复路径下已成死代码）。
   - [x] ADR-0010 标 Accepted。
3. **真机验证（用户侧）**：
   - ✅ 长按 1 次 → 复位 → 自动恢复闪烁（计数 1→清零）。2026-06-28 用户真机确认。
   - ⚠️ 人为 init 崩溃 3 次 → 锁死：经 host 单测覆盖（`test_boot_safe_lock_after_threshold_consecutive_abnormal`）；真机未实测（按钮路径因里程碑清零无法累加到阈值，需注入崩溃才能触发）。

---

*变更记录：*
- 2026-06-28：Proposed（smoke S8 实测暴露 safe-lock「一次即锁」与「单次复位应恢复」冲突；方案 C 连续计数 + 健康里程碑清零，阈值 3）
- 2026-06-28：Accepted（host 17/17 单测通过、ESP32 构建 0 警告；ESP32 DevKitC 真机验证：长按 BOOT>3s 触发 WDT 复位后设备自动恢复闪烁、按键计数恢复。锁死路径经 host 单测覆盖。决策已回写至 04-runtime-and-trace.md §3.4。）

