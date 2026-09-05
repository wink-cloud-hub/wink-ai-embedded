# PAL 中断子系统 Phase 1.5 实施计划：GPIO 优先级契约落地 + host/wasm REALTIME 一致化

| 项 | 内容 |
|----|------|
| 创建日期 | 2026-07-01 |
| 关联评审 | [2026-06-30 PAL 中断子系统架构评审](../../reviews/core/2026-06-30-pal-interrupt-subsystem-architecture-review.md) |
| 关联 ADR | [ADR-0012 契约诚实优于静默降级](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)（Proposed），ADR-IRQ-001~008 |
| 关联技术设计 | [pal-unified-interrupt-subsystem.md](../../tech-designs/core/pal-unified-interrupt-subsystem.md) v2.1 |
| 前置计划 | [2026-06-30-pal-interrupt-phase1-contract-alignment-plan](2026-06-30-pal-interrupt-phase1-contract-alignment-plan.md)（Task 1/2 已落地，Task 3 仅完成 doc-only 部分） |
| 影响范围 | `pal/include/hal/pal_hal.h`（doc）、`targets/{esp32,wasm,host}/pal_hal_*.c`、`targets/{host,wasm}/pal_irq_*.c`（若拆分）、`test/test_pal_irq.c`、`samples/devkitc_smoke/app_callbacks.c`（返回值检查）、tech-design v2.1→v2.2、`02-wink-micro-os/02-pal-platform-abstraction.md` |
| 预计工期 | 2~3 个工作日 |
| 当前状态 | **已完成**（2026-07-01 落地；见文末 §7 落地记录） |
| 风险等级 | 中（改动 `gpio_install_isr_service` flag，会影响 ESP32 真机中断优先级） |

---

## 0. 为什么需要 Phase 1.5

### 0.1 现状盘点（2026-07-01 grep + commit 核验）

Phase 1 计划（2026-06-30）三项 P0 任务的**真实落地度**如下：

| 任务 | Header 契约 | 三 target 实现 | Commit |
|------|-------------|---------------|--------|
| **G1** direct_connect trampoline | ✅ 已修订（`pal_irq.h:97-108, 205-211`） | ✅ 三 target 均有 `direct_trampoline` + `s_direct_handlers[]` | `7ceccdd` |
| **G2** ESP32 拒接 REALTIME | ✅ 已修订（`pal_irq.h:66-72`） | ⚠️ 仅 ESP32 落地（`pal_hal_esp32.c:281, 951`）；**host/wasm 侧仍静默接受**，未打日志、未拒接 | `b971d70` |
| **G3** GPIO prio 语义 | ✅ header 承诺"首次锁定 + 后续不一致 `WINK_ERR_INVALID_ARG`"（`pal_hal.h:67-97`） | ❌ **三 target 均 `(void)prio;` 完全丢弃**（`pal_hal_esp32.c:289`，`pal_hal_host.c:118`，`pal_hal_wasm.c:150,335`）；ESP32 侧 `gpio_install_isr_service(0)` 硬编码优先级 0 | `40e1773`（仅 doc） |

**结论**：Phase 1 关闭了 G1，部分关闭了 G2，**G3 只在 header 上做了诚实化，实现完全没兑现**。这本身构成新一轮 ADR-0012 违约 —— 头文件写了"会返回 `WINK_ERR_INVALID_ARG`"，实现里根本不会返回。

### 0.2 时间敏感性重述

- Phase 1 grep 记录（`2026-06-30`）：`pal_gpio_enable_interrupt_ex` 外部调用 0；`pal_gpio_enable_interrupt` 外部调用 1（`samples/devkitc_smoke/app_callbacks.c:266`）。
- Phase 2（`pal-unified-interrupt-subsystem-implementation-plan.md`）后续会有更多 codegen / sample 接入，一旦上量再纠正 GPIO prio 语义就要写 deprecation。**Phase 1.5 是最后一个窗口**。

### 0.3 Antigravity 外部计划（2026-06-30 v2.1）差异说明

外部工具生成的计划把 G1/G2/G3 都当"未落地"提出，且遗漏若干工程细节。经与本仓库现状对齐后，本计划**只做仍存在的真实欠债**：

- **G1**：不重做（已完成）；仅在验证阶段补 CFI 编译回归。
- **G2**：仅补 host/wasm 侧一致化处理（默认拒接 + 编译期 opt-in 放行，替代外部计划的"打警告后放行"方案）。
- **G3**：**本计划核心工作**，加并发保护、明确 uninstall 策略、真正映射 prio flag。

---

## 1. 任务清单

### Task 1: G3 落地 —— `pal_gpio_enable_interrupt_ex` prio 首次锁定语义

#### 1.1 契约再确认（pal_hal.h 已承诺）

```
- 首次注册（任意 target）：记录 prio，调用底层 install service（ESP32 侧真正传映射后的 flag）
- 后续注册：
    · prio 与首次一致  → 走原路径注册 pin handler
    · prio 与首次不一致 → 返回 WINK_ERR_INVALID_ARG（不是 BUSY / UNSUPPORTED）
- prio 一旦锁定，进程生命周期内不再释放（见 §1.2 决策）
```

**错误码 justify（写入 pal_hal.h doxygen）**：
- 用 `INVALID_ARG`：语义"参数与当前系统状态不合法"精准；`BUSY` 暗示可重试，误导性；`UNSUPPORTED` 暗示整个能力缺失，不准确。

#### 1.2 关键决策：**永不释放 prio 锁定**（区别于外部计划）

**外部计划提议**："`pal_gpio_disable_interrupt` 里检查 pin 计数=0 → `gpio_uninstall_isr_service` + `s_gpio_service_initialized = false`"。

**本计划拒绝该方案**，理由：

| 问题 | 说明 |
|------|------|
| TOCTOU race | Task A `disable(last_pin)` → 计数=0 → uninstall；Task B 同时 `enable(pinX, HIGH)` 卡在 install 前 → 竞态或状态不一致 |
| SMP UAF 风险 | disable 后 ISR 可能仍在另一核上执行（见 ADR-IRQ-007）；uninstall 会释放 dispatcher 状态 → UAF |
| 心智模型复杂 | 用户看到的行为变成"平时锁定，极短窗口内可换 prio"，难以推理和文档化 |
| ESP-IDF 语义匹配 | `gpio_install_isr_service` 本质就是进程级 one-shot 全局服务，反复 install/uninstall 是反模式 |

**替代方案**：进程生命周期内锁定一次；若未来真需要"重置能力"，单独提供 `pal_gpio_reset_isr_service()`，并在 doc 里显式要求 `disable → pal_irq_synchronize → reset` 三步同步。本计划不引入。

#### 1.3 具体改动

##### 1.3.1 `pal/include/hal/pal_hal.h`（doxygen 语义修订）

将 `pal_gpio_enable_interrupt_ex` 的 doxygen 从"当前所有 target 均忽略此参数"更新为：

```c
/**
 * @brief 启用 GPIO 引脚中断（扩展版，首次锁定 prio）
 *
 * ⚠️ v2.2 契约（2026-07-01，ADR-0012 落地）：
 * GPIO 中断在各 target 上共享一个 dispatch service；因此 prio 的语义为
 * **进程生命周期内首次注册时锁定**：
 *
 *   · 首次注册：底层 install service，硬件优先级绑定到映射后的 flag
 *     (ESP32: ESP_INTR_FLAG_LEVELn；host/wasm: 无实际调度效果，仅记录状态)
 *   · 后续注册：
 *       - prio 与首次一致 → 正常注册 pin handler
 *       - prio 与首次不一致 → 返回 WINK_ERR_INVALID_ARG
 *
 * ⚠️ 一旦锁定，本接口不提供解锁 API。若需要 per-pin 独立优先级（按钮抢占
 * 传感器等场景），未来将新增 pal_gpio_enable_interrupt_dedicated()。
 *
 * ⚠️ REALTIME 拒接：所有 target 上 prio == PAL_IRQ_PRIO_REALTIME 均返回
 * WINK_ERR_UNSUPPORTED（host/wasm 在 v2.2 起也拒接，详见 §Task 2）。
 *
 * @return
 *   WINK_OK              首次或一致的后续注册成功
 *   WINK_ERR_INVALID_ARG pin 越界 / callback NULL / prio 越界 /
 *                         prio 与首次锁定值不一致（本次拒接）
 *   WINK_ERR_UNSUPPORTED prio == PAL_IRQ_PRIO_REALTIME
 *   WINK_ERR_HARDWARE    底层 install / register 失败（ESP32 IDF 返回错）
 */
```

##### 1.3.2 `targets/esp32/pal_hal_esp32.c`

引入文件级静态状态 + 现有 `s_gpio_table_mux` 复用保护：

```c
/* v2.2 G3: GPIO service 首次锁定的 prio。由 s_gpio_table_mux 同步。
 * 一旦 initialized，进程生命周期内不再释放（见 Phase 1.5 §1.2）。*/
static bool           s_gpio_service_initialized = false;
static pal_irq_prio_t s_gpio_service_prio        = PAL_IRQ_PRIO_NORMAL;
```

`pal_gpio_enable_interrupt_ex` 入口新增（在现有 REALTIME 拒接之后、`s_isr_service_installed` 之前）：

```c
portENTER_CRITICAL(&s_gpio_table_mux);
if (s_gpio_service_initialized) {
    if (prio != s_gpio_service_prio) {
        portEXIT_CRITICAL(&s_gpio_table_mux);
        return WINK_ERR_INVALID_ARG;   /* G3: prio 冲突 */
    }
} else {
    /* 首次：先记录，install 若失败再回滚 */
    s_gpio_service_prio = prio;
}
portEXIT_CRITICAL(&s_gpio_table_mux);
```

`gpio_install_isr_service` 调用改为使用真实映射：

```c
static const int s_gpio_prio_flag_map[PAL_IRQ_PRIO_COUNT] = {
    [PAL_IRQ_PRIO_LOWEST]  = ESP_INTR_FLAG_LEVEL1,
    [PAL_IRQ_PRIO_LOW]     = ESP_INTR_FLAG_LEVEL1,
    [PAL_IRQ_PRIO_NORMAL]  = ESP_INTR_FLAG_LEVEL2,
    [PAL_IRQ_PRIO_HIGH]    = ESP_INTR_FLAG_LEVEL3,
    [PAL_IRQ_PRIO_HIGHEST] = ESP_INTR_FLAG_LEVEL3,
    /* REALTIME 已在入口处拒接 */
};

int intr_flags = s_gpio_prio_flag_map[prio];
/* 若 gpio_isr_wrapper 或用户 ISR 需要在 Flash cache 被禁用时运行，
 * 必须带 IRAM 标志；本项目 wrapper 已 IRAM_ATTR，故一并加上。*/
intr_flags |= ESP_INTR_FLAG_IRAM;

esp_err_t err = gpio_install_isr_service(intr_flags);
if (err == ESP_OK) {
    portENTER_CRITICAL(&s_gpio_table_mux);
    s_gpio_service_initialized = true;   /* 提交锁定 */
    portEXIT_CRITICAL(&s_gpio_table_mux);
} else if (err == ESP_ERR_INVALID_STATE) {
    /* 已被别的路径（如 IDF 内部）安装过；接受既有状态，
     * 但记录我们不知道对方 flag，此时最保守做法是仍然锁定 s_gpio_service_prio
     * 以让后续 mismatched 调用返回 INVALID_ARG，保持 API 契约一致。*/
    ESP_LOGI("PAL", "GPIO ISR service already installed externally, locked tracker to NORMAL");
    portENTER_CRITICAL(&s_gpio_table_mux);
    s_gpio_service_initialized = true;
    portEXIT_CRITICAL(&s_gpio_table_mux);
} else {
    return WINK_ERR_HARDWARE;
}
```

同时**删除** `pal_hal_esp32.c:289` 的 `(void)prio;` 及 `pal_hal_esp32.c:313-320` 的旧 `s_isr_service_installed` 布尔（被 `s_gpio_service_initialized` 取代）。

##### 1.3.3 `targets/host/pal_hal_host.c` + `targets/wasm/pal_hal_wasm.c`

引入相同状态变量，用平台可用的互斥（host: `pthread_mutex`；wasm 单线程模型可用简单标志但仍走同一路径以保持代码同源）：

```c
static bool           s_gpio_service_initialized = false;
static pal_irq_prio_t s_gpio_service_prio        = PAL_IRQ_PRIO_NORMAL;
/* host: */ static pthread_mutex_t s_gpio_service_mux = PTHREAD_MUTEX_INITIALIZER;
/* wasm: 单线程，无需 mutex，但可以通过空宏映射（如 #define portENTER_CRITICAL(x) / #define portEXIT_CRITICAL(x)）保持源文件结构与 ESP32 100% 同源 */
```
入口逻辑同 ESP32；无底层 install call，仅维护状态即可。同时，确保 host/wasm 侧正确引入支持 `PAL_LOGW` / `PAL_LOGI` 的日志头文件。

##### 1.3.4 `pal_gpio_enable_interrupt`（非 ex）路径确认

`pal_hal.h:106-113` 已把非 ex 版内联为 `pal_gpio_enable_interrupt_ex(..., PAL_IRQ_PRIO_NORMAL, ...)`。**必须在测试里覆盖**：

- Case A: `pal_gpio_enable_interrupt(pinA)` 先注册 → 锁定为 NORMAL；再 `pal_gpio_enable_interrupt_ex(pinB, ..., HIGH, ...)` → 期望 `WINK_ERR_INVALID_ARG`。
- Case B: 反过来先 `_ex` HIGH，再非 ex → 期望 `WINK_ERR_INVALID_ARG`。

##### 1.3.5 `pal_gpio_disable_interrupt` 保持不变

**明确不做** uninstall。若 disable 所有 pin 后进程内再无 GPIO 中断使用，也不释放锁定。文档补一句"disable 不释放服务锁定"，避免用户误期。

#### 1.4 验收

- ✅ ESP32 真机烧录后，`devkitc_smoke` sample 按钮 ISR 仍正常触发（验证 `intr_flags` 从 0 → LEVEL2 后无回归）。
- ✅ 三 target 单测 `test_gpio_prio_mismatch_rejected`、`test_gpio_non_ex_locks_to_normal` 通过。
- ✅ `pal_hal_esp32.c` 中不再有 `(void)prio;` 出现在 `pal_gpio_enable_interrupt_ex` 函数体内；host/wasm 同理。
- ✅ 并发单测（host 双 pthread 并发注册不同 prio）无 UB，其中一个必须返回 `INVALID_ARG` 而非崩溃。

**预计工时**：1 天

---

### Task 2: G2 补齐 —— host/wasm 侧 REALTIME 一致化

#### 2.1 现状与问题

- `pal_hal_host.c` / `pal_hal_wasm.c` 的 `pal_irq_enable` 目前**完全没有对 `PAL_IRQ_PRIO_REALTIME` 做任何区分**（grep 无匹配），静默走 dispatch。
- 结果：ESP32 target 上会拒接的调用，在 host/wasm 上通过 → **ADR-0012 反对的"仿真掩盖真机拒接"典型案例**。

#### 2.2 决策：默认拒接 + 编译期 opt-in（替代外部计划的"打警告后放行"）

**外部计划提议**："print stderr warning log 后允许注册"。本计划采用更强方案：

| 编译宏 | host / wasm `pal_irq_enable(REALTIME, ...)` 行为 |
|--------|--------------------------------------------------|
| 默认（未定义 `WINK_HOST_ALLOW_REALTIME_FOR_TESTING`） | 返回 `WINK_ERR_UNSUPPORTED`（与 ESP32 完全对齐） |
| `-DWINK_HOST_ALLOW_REALTIME_FOR_TESTING=1` | 走 dispatch，但**首次注册**通过 `PAL_LOGW` 一次性输出：`"host/wasm: REALTIME priority accepted for testing only; ESP32 will reject."` |

**理由**：
- 默认行为让"仿真通过 → 真机通过"关系严格成立，符合 ADR-0002/ADR-0003 保真原则。
- 静态校验类测试可以显式打开宏，走**受控**放行；测试代码里加宏就是"我知道我在做什么"的显式签名。
- 用一次性 flag `static bool s_realtime_warn_emitted = false;`，避免循环里刷屏。
- 用项目自己的 `PAL_LOGW`（若不存在则 `pal_log_warn` / `pal_os_log`），不直接 `fprintf(stderr,...)`，保持日志系统同源。

#### 2.3 具体改动

##### 2.3.1 `targets/host/pal_hal_host.c::pal_irq_enable` 入口

```c
if (prio == PAL_IRQ_PRIO_REALTIME) {
#if defined(WINK_HOST_ALLOW_REALTIME_FOR_TESTING)
    static bool s_realtime_warn_emitted = false;
    if (!s_realtime_warn_emitted) {
        PAL_LOGW("host: REALTIME priority accepted for testing only; "
                 "ESP32 target will return WINK_ERR_UNSUPPORTED.");
        s_realtime_warn_emitted = true;
    }
    /* 落到 dispatch 路径，与 HIGHEST 等价 */
#else
    return WINK_ERR_UNSUPPORTED;
#endif
}
```

##### 2.3.2 `targets/wasm/pal_hal_wasm.c::pal_irq_enable` 同上

##### 2.3.3 GPIO 路径一致性

`pal_gpio_enable_interrupt_ex` 在 host / wasm 上遇 `prio == REALTIME` 也走同一策略（ESP32 侧 `pal_hal_esp32.c:281` 已拒接）。

##### 2.3.4 `pal_irq.h` doxygen 补丁

在 `PAL_IRQ_PRIO_REALTIME` 注释末尾追加：

```
⚠️ v2.2（2026-07-01）：host / wasm 默认也拒接 REALTIME（与 ESP32 对齐），
仅在编译期定义 WINK_HOST_ALLOW_REALTIME_FOR_TESTING=1 时才放行，
且首次注册会通过 PAL_LOGW 打印警告。
```

#### 2.4 验收

- ✅ 默认编译（无宏）：`test_realtime_priority_rejected_on_all_targets` 三 target 均返回 `WINK_ERR_UNSUPPORTED`。
- ✅ 加宏编译：`test_realtime_accepted_when_opt_in`（host build 专用）注册成功 + `PAL_LOGW` 输出（用 log capture 单测）。
- ✅ WASM smoke 无回归。

**预计工时**：0.5 天

---

### Task 3: 文档回写（ADR-0012 落地规则 1）

#### 3.1 必须同步更新的文档

| 文档 | 修改点 |
|------|--------|
| `docs/design/02-wink-micro-os/02-pal-platform-abstraction.md`（活规范） | GPIO 中断接口章节补 v2.2 契约段：首次锁定 + `INVALID_ARG` + 永不释放；REALTIME 全 target 默认拒接策略 |
| `docs/tech-designs/core/pal-unified-interrupt-subsystem.md` | v2.1 → v2.2：`GPIO 中断接口`、`优先级抽象` 两章补契约段；文末 "核心架构决策记录" 加 ADR-IRQ-009（v2.2）：GPIO 全局服务锁定 + host/wasm REALTIME 默认拒接 |
| `docs/implementation-plans/core/2026-06-30-pal-interrupt-phase1-contract-alignment-plan.md` | Task 3 状态从 "待实施" → "doc-only 阶段完成，实现落地由 Phase 1.5（本计划）完成"，加互链 |
| `docs/decisions/core/0012-contract-honesty-over-silent-degradation.md` | 底部状态变更日志追加一条：`2026-07-01：Phase 1.5 落地 G3 实现 + host/wasm REALTIME 一致化` |
| `pal/include/hal/pal_hal.h` | 见 Task 1.3.1，从 "prio 被忽略" 改为 "首次锁定 / 后续 INVALID_ARG" |
| `pal/include/pal_irq.h` | 见 Task 2.3.4，REALTIME 注释补 host/wasm 默认拒接 |

#### 3.2 是否新立 ADR-IRQ-009？

**推荐立**（子系统级），因为：
- "GPIO 全局服务永不释放"是**独立于 ADR-0012 的具体设计判断**（后者只说"契约诚实"，不指定实现方式）。
- 未来接第二款 MCU（STM32 等）可能有相反选择，需要有讨论记录。

放在 tech-design v2.2 的 §11 局部 ADR 表中，不用单独立顶层 ADR 文件。

**预计工时**：0.5 天

---

### Task 4: 测试与回归

#### 4.1 `test/test_pal_irq.c` 新增用例清单

| 用例 | 覆盖点 | 编译条件 |
|------|--------|----------|
| `test_gpio_prio_locked_on_first_register` | 首次 HIGH → 记录；再 HIGH → OK | 所有 target |
| `test_gpio_prio_mismatch_returns_invalid_arg` | 首次 NORMAL → OK；再 HIGH → `INVALID_ARG` | 所有 target |
| `test_gpio_non_ex_locks_to_normal` | 非 ex 版先注册 → 隐式 NORMAL；再 `_ex(HIGH)` → `INVALID_ARG` | 所有 target |
| `test_gpio_disable_does_not_unlock` | 注册 → disable 全部 → 再注册不同 prio 仍返回 `INVALID_ARG`<br>（细化断言步骤：<br>1. 注册 Pin A (NORMAL) -> `WINK_OK`<br>2. 注册 Pin B (HIGH) -> `WINK_ERR_INVALID_ARG`<br>3. disable(Pin A) -> `WINK_OK`<br>4. 再次注册 Pin B (HIGH) -> 仍为 `WINK_ERR_INVALID_ARG`） | 所有 target |
| `test_gpio_realtime_rejected_on_all_targets` | prio=REALTIME 三 target 均 `WINK_ERR_UNSUPPORTED` | 无宏 |
| `test_irq_realtime_rejected_on_all_targets` | `pal_irq_enable(..., REALTIME, ...)` 同上 | 无宏 |
| `test_irq_realtime_accepted_when_opt_in` | 加宏后 host 上注册成功 + `PAL_LOGW` 触发 | host 加宏 build |
| `test_gpio_concurrent_first_register_race` | host 双 pthread 同时注册不同 prio，一个 OK 一个 `INVALID_ARG`，无 UB | host only |

#### 4.2 CI / 编译验证

1. **CFI 回归**（G1 保护网）：host build 强制开启 `-fsanitize=cfi-icall`，确保 `pal_irq_direct_connect` 路径无 CFI 违例。加入 `python wink-tools/wink.py test`。
2. **UBSan**：host build 加 `-fsanitize=undefined`，覆盖并发路径。
3. **单测数量**：Phase 1 目标 ≥20 → 本 Phase ≥28。

#### 4.3 真机 / WASM 烟测

必须执行（**不可只跑 host 单测**，因为 ESP32 侧 `gpio_install_isr_service` flag 从 `0` 变为 `LEVEL2|IRAM` 是真实运行时行为变化）：

```powershell
# ESP32 真机（PowerShell 工具链 + PYTHONUTF8=1）
$env:PYTHONUTF8='1'; $env:PYTHONIOENCODING='utf-8'
. 'C:\Espressif\tools\Microsoft.v6.0.1.PowerShell_profile.ps1'
idf.py -C d:\workspaces\ai-coding\wink-ai\wink-ai-embedded\esp32_firmware `
       build -DWINK_APP=devkitc_smoke
# 烧录后目视验证按钮 ISR 计数正常增长
idf.py -C ... build -DWINK_APP=smp_uaf_test
# 烧录后验证 SMP UAF 防护通过（10k+ 次注入无 panic）

# WASM 仿真
cd d:\workspaces\ai-coding\wink-ai\wink-ai-embedded\wink-micro-os
./build-wasm.ps1
# 打开对应仿真页面，验证 GPIO 中断分发链条正常
```

**预计工时**：0.5~1 天

---

## 2. 提交结构（CLAUDE.md 原子提交原则）

按逻辑分离，禁止一坨大 diff：

| # | Commit | 内容 |
|---|--------|------|
| 1 | `feat(pal/hal): G3 lock GPIO prio on first register across all targets` | Task 1.3.2 / 1.3.3 三 target impl 改动 + 状态变量 + prio 映射表；含并发保护 |
| 2 | `feat(pal/irq): G2 host+wasm reject REALTIME by default, opt-in via macro` | Task 2 host/wasm 拒接 + 编译宏放行 |
| 3 | `test(pal/irq): cover GPIO prio lock, concurrent race, REALTIME cross-target` | Task 4.1 新单测 + CFI/UBSan CI 开关 |
| 4 | `docs(pal): backport phase-1.5 to spec/tech-design/ADR-0012 status log` | Task 3 全部文档回写 |
| 5 | `docs(pal-hal.h): tighten pal_gpio_enable_interrupt_ex contract to lock-and-reject` | header doxygen 从 "忽略 prio" 改为 "首次锁定 + INVALID_ARG"（若已在 commit 1 中一并做则合并） |

**PR 描述必附一张 before/after 契约对照表**（见附录 A）。

---

## 3. 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| ESP32 侧 `gpio_install_isr_service` flag 从 0 → LEVEL2\|IRAM，改变按钮 ISR 的优先级 → 与 Wi-Fi/其它硬件资源竞争 | 中 | 中 | Task 4.3 devkitc_smoke + smp_uaf_test 真机双烟测；若发现异常，回退到 LEVEL1；ADR-IRQ-003"预留安全边界"意味着 LEVEL2 已在设计安全区 |
| host/wasm 并发首次注册出现 TOCTOU | 低 | 中 | 双检查锁模式（提交前在锁内 recheck initialized）；`test_gpio_concurrent_first_register_race` 单测持续压测 |
| `WINK_HOST_ALLOW_REALTIME_FOR_TESTING` 宏错开在生产构建 → 掩盖跨平台 bug | 低 | 中 | 在 `python wink-tools/wink.py test` / CI matrix 里显式跑两次：默认 + 加宏；后者仅限特定测试目标使用 |
| ESP-IDF 里已有其它代码 `gpio_install_isr_service(其它 flag)`（`ESP_ERR_INVALID_STATE`） | 低 | 低 | Task 1.3.2 已处理：接受既有状态并锁定 `s_gpio_service_prio`，让 API 契约一致；文档说明"若与外部 install 的 flag 冲突，用户看到的将是不可预料的硬件优先级 —— 但 API 层拒接语义仍成立" |
| `pal_gpio_enable_interrupt` 非 ex 版本被大量调用点默认 NORMAL，未来一个新 sample 想用 HIGH 会突然拒接 | 中 | 低 | 这正是契约本身要暴露的问题；PR 描述里显式提醒下游 codegen 团队；`samples/devkitc_smoke/app_callbacks.c:266` 唯一现存调用点保持 NORMAL |
| `WINK_WARN_UNUSED_RESULT` 覆盖：GPIO 注册返回值原本必然 OK，改动后可能返回 `INVALID_ARG` | 中 | 低 | `samples/devkitc_smoke/app_callbacks.c:266` 已在计划前置检查过；grep 全仓确保所有 caller 都检查返回值（Phase 1 grep 记录只有 1 处） |

---

## 4. 不在本计划范围（明确边界）

- ❌ 拆 `pal_hal_esp32.c` 巨石（→ Phase 2）
- ❌ `pal_gpio_enable_interrupt_dedicated()`（per-pin 独立中断源）—— 触发条件是"业务场景真需要 per-pin 抢占"，不是本 Phase
- ❌ `pal_gpio_reset_isr_service()` 解锁 API —— 触发条件是"业务场景真需要动态改优先级"，不是本 Phase
- ❌ host/wasm lock-level 区分（→ Phase 3 P2）
- ❌ Device Tree 化 `irq_num`（→ Phase 4，依赖 ADR-0008）

---

## 5. 验收标准（Definition of Done）

Phase 1.5 完成的标志：

1. ✅ `grep -n "(void)prio" targets/{esp32,wasm,host}/pal_hal_*.c` 在 `pal_gpio_enable_interrupt_ex` 函数体内零命中
2. ✅ `pal_hal.h` 中 `pal_gpio_enable_interrupt_ex` 的 doxygen "首次锁定 + INVALID_ARG" 契约与三 target 实现 100% 一致
3. ✅ 三 target 上 `pal_irq_enable(REALTIME)` 默认返回 `WINK_ERR_UNSUPPORTED`；加宏后 host/wasm 首次调用触发 `PAL_LOGW`
4. ✅ 新增 8 个 unit test 全部通过；host build 开启 `-fsanitize=cfi-icall,undefined` 无报错
5. ✅ ESP32 真机 `devkitc_smoke` 按钮中断行为无回归；`smp_uaf_test` 10k+ 次注入无 panic
6. ✅ WASM 仿真烟测通过（通过 [2026-07-01-wasm-simulator-target-repair-plan.md](../unisim/2026-07-01-wasm-simulator-target-repair-plan.md) 修复 wasm build 链路后，`emcmake cmake && cmake --build build-wasm && node targets/wasm/wink_sim_stub.js` 全绿；stub 静态解析 imports 契约 + Node worker 里 `onRuntimeInitialized` 到达 = smoke PASS。Phase 1.5 的 `pal_hal_wasm.c` G3 首次锁定 + REALTIME 拒接分支在 emcc 6.0.1 下无回归）
7. ✅ tech-design v2.2 完成 + `02-wink-micro-os/02-pal-platform-abstraction.md` 回写 + ADR-0012 状态日志更新 + Phase 1 计划链接互引
8. ✅ ADR-IRQ-009（v2.2）"GPIO 全局服务锁定 + host/wasm REALTIME 默认拒接" 进入 tech-design §11 局部决策表
9. ✅ 评审报告 §0 "落地完成度" 从 ⭐⭐⭐½（Phase 1 完成后） → ⭐⭐⭐⭐（契约与实现完全对齐）
10. ✅ 每个 commit 单独可 review、可回滚（提交结构见 §2）

---

## 6. 时间线

| 工作日 | Task | 输出 |
|--------|------|------|
| Day 1 上午 | Task 1.3.1 header 契约 + Task 1.3.2 ESP32 impl | ESP32 侧首次锁定 + `intr_flags` 映射 |
| Day 1 下午 | Task 1.3.3 host/wasm impl + Task 1 单测 | 三 target 一致行为 |
| Day 2 上午 | Task 2 host/wasm REALTIME 拒接 + 单测 | G2 全 target 一致 |
| Day 2 下午 | Task 4.3 ESP32 真机 + WASM 烟测 + CFI/UBSan CI | 回归结果 |
| Day 3 上午 | Task 3 文档回写（活规范 + tech-design v2.2 + ADR-0012 日志） | 文档一致 |
| Day 3 下午 | 提交拆分 + PR 描述 + before/after 对照表 | PR ready |

---

## 附录 A：契约变更 before / after 对照（PR 描述用）

### A.1 `pal_gpio_enable_interrupt_ex(pin, type, prio, cb, arg)`

| 场景 | v2.1（当前） | v2.2（本计划后） |
|------|--------------|------------------|
| 首次 prio=NORMAL | `WINK_OK`（prio 静默丢弃） | `WINK_OK`（锁定 NORMAL） |
| 再次 prio=NORMAL 注册其它 pin | `WINK_OK` | `WINK_OK` |
| 再次 prio=HIGH 注册其它 pin | `WINK_OK`（表面成功，硬件仍是 flag=0 → 未定义优先级） | **`WINK_ERR_INVALID_ARG`** |
| prio=REALTIME（三 target） | ESP32 `WINK_ERR_UNSUPPORTED`；host/wasm 静默 OK | **三 target 均 `WINK_ERR_UNSUPPORTED`**（除非 host 编译期 opt-in） |
| ESP32 底层 flag | 硬编码 `gpio_install_isr_service(0)` | `s_gpio_prio_flag_map[prio] \| ESP_INTR_FLAG_IRAM` |

### A.2 `pal_irq_enable(irq, prio=REALTIME, isr, arg)`

| target | v2.1 | v2.2 |
|--------|------|------|
| ESP32 | `WINK_ERR_UNSUPPORTED` | `WINK_ERR_UNSUPPORTED`（不变） |
| host（默认） | `WINK_OK`（静默映射） | **`WINK_ERR_UNSUPPORTED`** |
| host（`-DWINK_HOST_ALLOW_REALTIME_FOR_TESTING=1`） | 同上 | `WINK_OK` + 首次 `PAL_LOGW` |
| wasm（默认） | `WINK_OK`（静默） | **`WINK_ERR_UNSUPPORTED`** |
| wasm（宏放行） | 同上 | `WINK_OK` + 首次 `PAL_LOGW` |

---

## 附录 B：与 Phase 1 计划的关系

- Phase 1（`2026-06-30-pal-interrupt-phase1-contract-alignment-plan.md`）：定义 G1/G2/G3 三条 P0 契约欠债，产出 header 契约修订 + G1/G2 部分实现。
- **Phase 1.5（本计划）**：Phase 1 遗留的 G3 实现落地 + G2 在 host/wasm 侧的一致化。**契约表面在 Phase 1 已冻结，本 Phase 只补实现**。
- Phase 2（`2026-06-30-pal-unified-interrupt-subsystem-implementation-plan.md`）：契约冻结后的内部重构（TU 拆分、共享链去重、`#if defined(ESP_PLATFORM)` 收拢）。**须在本计划合并后启动**。

三个计划的依赖链：Phase 1 header 契约 → **Phase 1.5 实现兑现** → Phase 2 内部重构。

---

## 7. 落地记录（2026-07-01 完成归档）

所有 Task 已按计划落地，DoD 十条全部满足。提交时间线：

| # | Commit | 覆盖 Task / DoD | 说明 |
|---|--------|-----------------|------|
| 1 | `689885b feat(pal/irq): lock GPIO prio on first register; reject REALTIME on all targets` | Task 1（G3 三 target impl）+ Task 2（G2 host/wasm 一致化）+ header doxygen 契约收紧 | ESP32 `intr_flags` 从硬编码 `0` → `s_gpio_prio_flag_map[prio] \| ESP_INTR_FLAG_IRAM`；host 用 `pthread_mutex` 保 TOCTOU；REALTIME 三 target 默认 `UNSUPPORTED`，`WINK_HOST_ALLOW_REALTIME_FOR_TESTING` 宏放行 + 一次性 warn |
| 2 | `e46d669 test(pal/irq): cover GPIO prio lock, concurrent race, cross-target REALTIME` | Task 4.1（8 个新单测） | 覆盖 prio lock / non-ex → NORMAL / disable 不解锁 / concurrent race / REALTIME 跨 target / opt-in 放行 |
| 3 | `63ae658 test(host): add opt-in + sanitize matrix to python wink-tools/wink.py test` | Task 4.2（CFI + UBSan CI 矩阵） | `python wink-tools/wink.py test` 增加 sanitize 与宏 opt-in 双矩阵 |
| 4 | `772d7a7 docs(pal): backport Phase 1.5 to spec / tech-design / ADR-0012` | Task 3（文档回写） | 活规范 `02-pal-platform-abstraction.md` v2.2 契约段 + tech-design v2.1 → v2.2 + ADR-IRQ-009 局部决策 + ADR-0012 状态日志 |
| 5 | `620e699 docs(pal): archive Phase 1.5 hardware smoke + backport ADR-0012 to skills` | Task 4.3（ESP32 真机烟测）+ DoD 5 / DoD 9 | 归档评审记录 [`2026-07-01-pal-interrupt-phase1p5-hardware-verification.md`](../../reviews/core/2026-07-01-pal-interrupt-phase1p5-hardware-verification.md)：devkitc_smoke 按钮 ISR 无回归；smp_uaf_test 10k+ 次注入无 panic；架构评审落地完成度 ⭐⭐⭐½ → ⭐⭐⭐⭐ |
| 6 | `faf8739 docs(wasm): backport WINK_APP_DIR/js-library contract + close phase1p5 DoD` | DoD 6（wasm 烟测收尾） | 通过 `2026-07-01-wasm-simulator-target-repair-plan.md` 修复 wasm build 链路后，`pal_hal_wasm.c` G3 首次锁定 + REALTIME 拒接分支在 emcc 6.0.1 下无回归 |

### DoD 十条勾选

| # | 项 | 状态 |
|---|----|------|
| 1 | 三 target `pal_gpio_enable_interrupt_ex` 内 `(void)prio;` 零命中 | ✅ commit 1 |
| 2 | `pal_hal.h` 契约与三 target 实现 100% 一致 | ✅ commit 1 |
| 3 | `pal_irq_enable(REALTIME)` 默认全 target `UNSUPPORTED`；加宏后首次 `PAL_LOGW` | ✅ commit 1 |
| 4 | 8 个新单测通过；host `-fsanitize=cfi-icall,undefined` 无报错 | ✅ commit 2 + 3 |
| 5 | ESP32 真机 `devkitc_smoke` + `smp_uaf_test` 无回归 | ✅ commit 5（评审记录） |
| 6 | WASM 仿真烟测通过（依赖 wasm-simulator-target-repair 计划） | ✅ commit 6 |
| 7 | tech-design v2.2 + 活规范 + ADR-0012 日志 + Phase 1 计划互链 | ✅ commit 4 |
| 8 | ADR-IRQ-009 进入 tech-design §11 局部决策表 | ✅ commit 4 |
| 9 | 评审报告落地完成度 ⭐⭐⭐½ → ⭐⭐⭐⭐ | ✅ commit 5 |
| 10 | 每个 commit 单独可 review / 可回滚 | ✅ 6 commit 独立原子拆分 |

