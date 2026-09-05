# 对外部 WinkMicroOS 综合评审报告的批判性核验与补充意见

| 项 | 内容 |
|---|---|
| 评审日期 | 2026-07-01 |
| 评审对象 | `C:\Users\77174\.gemini\antigravity-ide\brain\95e0a207-c87f-4292-8367-3b72172d9b36\wink_micro_os_comprehensive_review.md` |
| 评审角色 | 资深嵌入式架构师视角 |
| 评审目的 | 核验外部评审论断与当前代码的一致性，识别失真项，并补充报告未覆盖的关键问题 |
| 关联记忆 | `external-plans-critical-review.md`、`sim-scheduler-plan-decisions.md` |
| 关联规范 | [CLAUDE.md](../../../../CLAUDE.md)、[ADR-0001](../../decisions/core/0001-error-code-sign-convention.md)、[ADR-0004](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md)、[ADR-0008](../../decisions/core/0008-dynamic-device-tree-config-flash.md) |

---

## 摘要（TL;DR）

该外部报告整体骨架合理，但**近一半具体论断已被 2026-06-24 之后的近期 commit 覆盖或超越**（v2.2 中断子系统重构、ADR-0009 PAL Resource、超声波非阻塞 API 补齐、`wink_trace` 临界区保护、REALTIME 显式拒接、协作式调度器实施计划 v1.3）。若照报告的 P0/P1 直接排期，会：

1. 重复投入到已修复项（撤回类见 §一）；
2. 错过报告未提及但对北极星（AI Codegen 友好 + 两端同源仿真保真）杠杆更大的事项（新增建议见 §二）；
3. 把两条与"下一款目标板"相关的抽象排到 P0/P2，与 MVP 阶段实际优先级不匹配（异议项见 §三）。

**核心结论**：报告的分层框架（架构 / 规范 / 中断 / 仿真 / 可扩展 / 行动路线）适合作为**目录**参考，但**具体待办项必须以本核验重排后的清单为准**（§四）。

---

## 一、报告论断的逐条核验

以 medium 强度基于当前代码逐条核验，结论分四档：**符合 / 部分正确 / 已过时 / 错误**。

### 1.1 撤回类（"错误"或"已过时"，不应再排期）

#### 1.1.1 DAL 单行 `if` 未加大括号 —— **错误**

- **报告原文**：`dal/src/dal_servo.c` 和 `dal/src/dal_ultrasonic.c` 等存在 `if (dev == NULL) return WINK_ERR_INVALID_ARG;` 单行未加大括号，违反 BARR-C:2018 与 MISRA-C。
- **实际情况**：`wink-micro-os/dal/src/**/*.c` 全部 `if (dev == NULL) ...` **均已加大括号**。示例：
  - `dal_servo.c:13, 29, 47, 55`
  - `dal_ultrasonic.c:19, 35, 68, 102, 125`
  - `dal_eeprom.c / dal_gps.c / dal_led.c / dal_button.c / dal_ssd1306.c` 同。
  - `pal/src/` grep 命中 0 处未加括号的单行 return。
- **建议**：撤回本条。若担心复发，落一条 L0 lint 规则拦截未加大括号的单行分支（可参考已有的 `ESP_PLATFORM` 密度 lint 实现模式，见近期 commit `15eb1fc`）——这比"再修一遍"对未来的价值大。

#### 1.1.2 `wink_trace_fault` 并发无保护 —— **已过时**

- **报告原文**：`s_count++` 和 `s_head` 写路径没有关中断/临界区保护，多任务/中断并发下有数据竞争。
- **实际情况**：`wink-micro-os/trace/src/wink_trace.c:14-27` 中 `wink_trace_fault` 已用 `pal_os_critical_enter/exit(key)` 包住 `s_buffer / s_head / s_count` 的读写；`reset / count / last` 亦同。文件顶部注释显式声明 "Thread-safe / ISR-safe"。
- **建议**：撤回本条。但需要新增一个**衍生**关注点（见 §二.6）：`pal_os_critical_enter` 的**任务 / ISR 双入口契约**尚未在 API 层锁死。

#### 1.1.3 ESP32 上 REALTIME 优先级静默降级 —— **已过时**

- **报告原文**：`REALTIME` 在 ESP32 上被静默合并到 `HIGHEST`（Level 3），未做显式 `WINK_ERR_UNSUPPORTED` 拦截。
- **实际情况**：`wink-micro-os/targets/esp32/pal_irq_esp32.c:163-170` 现在**显式拒接**：
  ```c
  if (prio == PAL_IRQ_PRIO_REALTIME) return WINK_ERR_UNSUPPORTED;
  ```
  映射表 189-191 行注释同步："REALTIME 已在入口处被拒接，此处不映射"。
- **建议**：撤回本条。可以作为"防止未来回归"的 lint 用例（编写一个 host 单测：注册 REALTIME 优先级，断言返回 `WINK_ERR_UNSUPPORTED`）。

#### 1.1.4 `dal_ultrasonic_init` / `dal_servo_init` 缺失 —— **错误**

- **报告原文**：P0 补齐 `dal_ultrasonic_init` 和 `dal_servo_init`。
- **实际情况**：`dal_ultrasonic.h:73` 与 `dal_servo.h:48` 分别声明 `dal_ultrasonic_init(dev, cfg)` / `dal_servo_init(dev, cfg)`，实现分别在 `dal_ultrasonic.c:34` 和 `dal_servo.c:13`，均已完整。
- **建议**：撤回本条。但**"init 是否真的向 `pal_resource` 登记冲突"** 尚未验证——这是我要独立提出的一条 P0，见 §二.4。

#### 1.1.5 需要"多 `asyncify_data` Spike"作为 Blocker —— **已被更高抽象方案取代**

- **报告原文**：合入调度器前必须做 Task 0.5 Spike，独立验证多 Fiber 上下文切换无串扰。
- **实际情况**：`docs/implementation-plans/unisim/2026-07-01-sim-cooperative-scheduler-plan.md`（v1.3 草稿）T5 明确采纳 **Emscripten 官方 `<emscripten/fiber.h>` + Windows Fibers + `sim_ctx_*` 抽象**（新增 `targets/host/sim_ctx_win32_fiber.c` 等）。这是**官方**支持的多协程 API，其底层就是 Asyncify 但由 Emscripten 团队维护上下文切换正确性，**无需手工换 `__asyncify_data`**。
- **建议**：撤回"独立 spike"提法，改写为"按当前实施计划 T5 分阶段推进"。若合入过程中官方 API 遇到已知 issue（例如 Emscripten 高版本下的 stack scan 误伤），再按 issue 决定是否降级——但那不再是**前置** blocker。

#### 1.1.6 Host target 已用 Windows Fibers，需 `if(NOT WIN32) FATAL_ERROR` —— **错误 / 时序错位**

- **报告原文**：Host target 已直接调用 `ConvertThreadToFiber` / `SwitchToFiber`，Linux/macOS 编译将失败，需 CMake 拦截。
- **实际情况**：`grep ConvertThreadToFiber|SwitchToFiber|CreateFiber|WIN32` 在 `targets/host/` **0 命中**。当前 `pal_osal_host.c:186-195` 是"single-threaded synchronous execution for tests" 的退化实现。
- **建议**：撤回"事后修补"表述，改为**前置约束**：在协作式调度器实施计划 T5 落地 `sim_ctx_win32_fiber.c` 的**同一 PR** 内加上 CMake 平台守卫 + `sim_ctx_ucontext.c` stub（POSIX 降级路径）。**不要**等代码合入才补——这是本项目的既定纪律，属于计划本身的一部分。

#### 1.1.7 报告 5.1"逻辑中断号需靠 ADR-0008 设备树 codegen 消除"—— **ADR 关联错误**

- **报告原文**：ADR-0008 规划"逻辑设备树 codegen"消除 `TEST_IRQ_UAF 7` 之类硬编码。
- **实际情况**：ADR-0008 主题是**"基于 Flash (SPIFFS) 动态设备树配置的免编译快速调试逃生通道"**，状态 Accepted (2026-06-28)。它只谈**DAL 引脚参数运行时覆写**，**不涵盖 IRQ 号 codegen**。`TEST_IRQ_UAF` 仍是 `samples/smp_uaf_test/device_tree.h:13` 的硬编码宏。
- **建议**：撤回本条与 ADR-0008 的绑定。逻辑 IRQ 号 codegen 若真要做，属于**新 ADR**——但我认为不该做，见 §三.1。

### 1.2 部分正确类（论断本身对，但结论/建议需要修正）

#### 1.2.1 `pal_irq_direct_connect` 未真正硬件矢量直连 —— **部分正确**

- **报告原文**：ESP32 上仍走 `generic_isr_wrapper` 软件分发，API "契约虚标"。
- **实际情况**：`pal_irq_esp32.c:250-277` 中 `pal_irq_direct_connect → direct_trampoline → pal_irq_enable → esp_intr_alloc(..., generic_isr_wrapper, ...)`，确实**未使用真正矢量直连**；但注册处已加 `ESP_INTR_FLAG_IRAM`（第 207 行），即"IRAM 保驻内存 + 共享 wrapper 派发"。所以"实际走软件分发"符合，但"未 IRAM"的暗示不成立。
- **我的建议（与报告不同方向）**：**改契约而不是补功能**。ESP32 上真正的 vector-direct 需要 IRAM 汇编级挂钩，收益小（ESP32 ISR 分发延迟本就 <1μs），成本高。建议：
  1. 将 `pal_irq_direct_connect` 改名为 `pal_irq_iram_bind`，明确"IRAM handler + 共享派发"语义；或
  2. 在其他 target（wasm/host）显式返回 `WINK_ERR_UNSUPPORTED`，防止 codegen 误用；
  3. 更新 `pal_irq.h` doxygen 契约，将"零延迟分发"字样删除，改为"IRAM-resident handler with shared dispatch"。
- 一致性 > 完备性。删虚标 API 比补齐虚标功能更符合本项目"AI codegen 友好"的北极星。

#### 1.2.2 超声波驱动 60ms 忙等 —— **部分正确**

- **报告原文**：`dal_ultrasonic_read` 是 `app_loop` 中同步忙等 60ms，缺乏非阻塞 API。
- **实际情况**：`dal_ultrasonic.c` 现同时提供：
  - **非阻塞** `dal_ultrasonic_request_measurement` + `dal_ultrasonic_get_cached_distance`（第 67-117 行，含 IDLE/MEASURING/READY/ERROR 状态机）；
  - 标注 `@deprecated @blocking` 的 `dal_ultrasonic_read`（第 124-146 行，头文件 104-113 行注明 worst-case ≈60ms，"Not allowed in cooperative runtime loop"）。
- **我的建议（与报告不同方向）**：报告说"缺乏非阻塞 API"已不成立，但**双 API 并存**在 AI Codegen 场景下是个新陷阱（见 §二.1）。真正要做的是**从可见符号里剔除 deprecated API 或加编译期告警**，而不是"重构非阻塞"（已完成）。

#### 1.2.3 PAL 返回值 `bool` vs `wink_status_t` —— **部分正确**

- **报告原文**：`pal_gpio_init` 等仍返回 `bool`，需升为 `wink_status_t`。
- **实际情况**（`pal_hal.h`）：
  - `pal_gpio_init` 已返回 `wink_status_t`（第 59 行）；
  - `pal_pwm_init / set_duty` 已 `wink_status_t`（第 51/54 行）；
  - **但** `pal_gpio_read` **仍返回 `bool`**（第 63 行）；
  - `pal_gpio_write` 返回 `void`（第 61 行）。
- **我的建议（延展报告方向）**：升级方案需要 out-param 化，见 §二.2 详述。这是 P1 事项。

#### 1.2.4 `pal_resource` 冲突拦截 —— **部分正确**（框架有，接线未验证）

- **报告原文**：DAL 缺失资源占用冲突表，需在 init 时 claim GPIO/PWM。
- **实际情况**：`pal_resource.h` 完整定义 `pal_resource_type_t { GPIO_PIN, PWM_CHANNEL, I2C_PORT, I2C_ADDR }`，claim/release + 静态表（`PAL_RESOURCE_MAX_CLAIMS`）齐备；三 target 实现 `pal_resource_{host,esp32,wasm}.c` 均存在。
- **我的判断**：报告说"未落地"不成立，但**接线是否闭环**未验证。`dal_ultrasonic_init` / `dal_servo_init` 内部是否真的调用了 `pal_resource_claim(GPIO_PIN, ...)`？若只是"接口就位、DAL 未接线"，用户实际不会看到 `WINK_ERR_COLLISION`，这个层就是空壳。**这是我要独立立项的 P0**（§二.4）。

### 1.3 符合类（报告正确、代码符合）

- **§3.1 双等级临界区 / SMP 中断同步屏障 / RCU 共享中断链**：`pal_irq_esp32.c`、`pal_shared_chain.c` 现况均符合，v2.2 重构成果扎实。
- **§2.1 Doxygen 契约 / `WINK_WARN_UNUSED_RESULT` / Protothread 栈毒化**：`runtime/include/wink_app.h:75-83` 定义 `WINK_PT_POISON_STACK`（`WINK_PT_DEBUG` 下 16×`uint32_t=0xDEADBEEF`），`WINK_PT_YIELD` 在每次 yield 前调用（第 114-119 行）；非 debug 分支退化为 `((void)0)`。
- **§1.1 `wink_app_callbacks_t` 回调注入**：`runtime/include/wink_app.h:289` 定义；`wink_runtime.c:79, 164` 中 `wink_runtime_run / fault` 均以 `const wink_app_callbacks_t*` 为参；`targets/wasm/wasm_entry.c:21-25`、samples `smp_uaf_test/app_callbacks.c:78`、`oled_dashboard/app_main.c:81-82` 均实例化并注入。
- **§5.2 `pal_shared_chain.c` 用 `malloc/free`**：符合，但见 §三.2 我对是否要改的异议。

### 1.4 核验结果汇总

| 报告小节 | 论断 | 核验结论 |
|---------|------|---------|
| §1.1 完全二进制解耦 App/BAL | 回调注入 | 符合 |
| §1.1 DAL 双模语义 | 细粒度旁路 | 符合 |
| §1.1 PAL 契约化 | 静态分发 | 符合 |
| §1.2 静态分发范式评估 | AI codegen 友好 / 无父类模板膨胀 | 符合 |
| §2.1 Doxygen 契约 | 每个 API 都锁死前置条件 | 符合 |
| §2.1 `WINK_WARN_UNUSED_RESULT` | 强制返回值检测 | 符合 |
| §2.1 Protothread 栈毒化 | debug 下 DEADBEEF | 符合 |
| §2.2-1 DAL 单行 if 无大括号 | 违反 BARR-C | **错误**（撤回） |
| §2.2-2 `wink_trace_fault` 数据竞争 | 无临界区保护 | **已过时**（撤回） |
| §3.1 双等级临界区 | `pal_irq_save_rtos_safe` | 符合 |
| §3.1 SMP 中断同步屏障 | `pal_irq_synchronize` | 符合 |
| §3.1 RCU 共享中断链 | Copy-on-Write | 符合 |
| §3.2-1 `pal_irq_direct_connect` 虚标 | 走 generic_isr_wrapper | **部分正确**（改契约而非改实现） |
| §3.2-2 REALTIME 静默降级 | 未显式拒接 | **已过时**（撤回） |
| §4.1 虚拟时间 + 物理旁路 | JS Worker SSOT | 符合 |
| §4.2-1 多 asyncify Spike Blocker | 需前置 spike | **已过时**（更高抽象方案已采纳） |
| §4.2-2 Windows Fibers 已绑定 | 需 CMake FATAL | **错误**（尚未引入，改前置约束） |
| §5.1 ADR-0008 IRQ codegen | 消除硬编码 | **ADR 关联错误** + 建议不做 |
| §5.2 共享链 malloc | 需换静态池 | 符合但建议**降级为 P3** |
| §6.1-1 P0 wasm fiber spike | 前置验证 | 已被 §4.2-1 覆盖，撤回 |
| §6.1-2 P0 超声波非阻塞重构 | 消除忙等 | **部分正确**（非阻塞 API 已有，改为剔除 deprecated） |
| §6.1-3 P0 补齐 `dal_*_init` | 冲突表 | **错误**（已实现），改为**验证接线**（新 P0） |
| §6.2-1 P1 `pal_gpio_init` bool → status_t | 统一错误码 | **部分正确**（init 已改，read/write 未改，需 out-param 化） |
| §6.2-2 P1 单行 if 大括号 | BARR-C | 撤回 |
| §6.2-3 P1 simulation.md SSOT | 文档漂移 | 未核验（保留在建议中） |
| §6.3-1 P2 共享链静态池 | 剔除 malloc | 降级为 P3 |
| §6.3-2 P2 Host 协程 CMake 守卫 | Linux/macOS | 改为前置约束 |

---

## 二、报告未提及但更值得排期的问题

以下是我作为架构师**在核验代码过程中额外发现**、对北极星（AI Codegen 友好 + 两端同源仿真保真）杠杆更大的事项。

### 2.1【P1】阻塞 / 非阻塞 DAL API 双存在，AI Codegen 会踩坑

**问题**：`dal_ultrasonic` 里同时暴露：
- 推荐：`dal_ultrasonic_request_measurement` + `dal_ultrasonic_get_cached_distance`
- Deprecated：`dal_ultrasonic_read`（标注 `@blocking` "Not allowed in cooperative runtime loop"）

对**人**开发者来说 doxygen 注释足够，但对**AI codegen** 而言，它经常从旧样例 / 旧 README grep 出来直接复用旧 API——注释语义 AI 抓不住。

**风险**：一旦协作式调度器上线（`2026-07-01-sim-cooperative-scheduler-plan.md`），60ms 忙等会立刻违反调度契约，行为在仿真里"看起来对"（Asyncify 会替 `pal_os_delay` 让步），在真机会**挂死 WDT**。这是典型的两端不同源事故源。

**建议**：
1. `dal_ultrasonic.h` 里给 `dal_ultrasonic_read` 加 `WINK_DEPRECATED_MSG("Use request_measurement + get_cached_distance in cooperative loop")` 属性，触发编译警告；
2. 引入项目级 `WINK_BLOCKING` 属性宏，对所有 `@blocking` 标注的 API 挂上，未来可通过 `-DWINK_STRICT_NONBLOCKING` 编译选项**从符号表中剔除**这些 API；
3. Codegen prompt 增补"禁止使用 `WINK_BLOCKING` 属性 API"约束。

这条**必须**在协作式调度器合入前落地——否则 AI 生成的样例代码会一批一批地踩坑。

### 2.2【P1】`pal_gpio_read` / `pal_gpio_write` 是 API 一致性的最后洞

**问题**：
- `pal_gpio_init/config` 已升 `wink_status_t`；
- `pal_gpio_read` 仍返回 `bool`（`pal_hal.h:63`）；
- `pal_gpio_write` 返回 `void`（第 61 行）。

**衍生风险**：
1. `read` 失败无法上报——GPIO 输入若浮空、被短路拉飞、mux 冲突，DAL 层拿到的都是一个"合法电平"，诊断难度极高；
2. `void write` 无法被 `WINK_WARN_UNUSED_RESULT` 覆盖——写失败（例如未 claim）静默丢失；
3. AI codegen 场景下"init 检查错误码，read/write 不检查"的**不对称调用模式**会污染样例。

**建议接口重构**：
```c
wink_status_t pal_gpio_read(pal_gpio_num_t pin, bool *out_level) WINK_WARN_UNUSED_RESULT;
wink_status_t pal_gpio_write(pal_gpio_num_t pin, bool level) WINK_WARN_UNUSED_RESULT;
```

配合 §二.4：如果 `pal_resource` 里该 pin 未被 claim，`pal_gpio_read/write` 直接 `return WINK_ERR_STATE`，能把很多"硬件调试盲区"暴露在 host 单测阶段。

**排期建议**：这是一次跨 target 的公共 API 变更，需要走完整流程——写 ADR、更新 `pal_hal.h`、三 target 同步实现、扫描所有 DAL 调用点。1-2 个 sprint 完成。

### 2.3【P1】`wink_status_t` 错误码目录（AI Codegen 语义手册）尚未成文

**问题**：ADR-0001 定下"负数错误码"，但代码里散落着 `WINK_ERR_INVALID_ARG / IO / TIMEOUT / STATE / UNSUPPORTED / COLLISION / ...` 等一批码值，**没有一份统一的语义手册**说明每个码：
- 语义（是什么错）
- 触发场景（何时会返回）
- 建议恢复模式（重试 / 报障 / safe-off / `WINK_PT_EXIT`）
- 是否可在 protothread 里作为退出条件

**为什么重要**：本项目北极星是"AI 生成的代码可同源、可调试"。AI 不会主动测试错误路径，也没有直觉猜"init 失败 vs claim 冲突 vs 硬件超时"该退到什么状态。**没有这份表，AI 生成的错误处理路径会长期停留在"`if (err) return err;` 一把梭"**——两端同源，但两端一起崩。

**建议产出**：
- 新增 `docs/design/01-system-overall/error-code-catalog.md`
- 表格化枚举每个 `wink_status_t` 值 + 上述四栏
- 该文档**塞进 codegen prompt 的 few-shot**

**优先级理由**：这份表列上以后，能明显提升 AI 生成代码的鲁棒性。对北极星的贡献比"补大括号"这种 lint 层面的事项大**一个数量级**。

### 2.4【P0】`pal_resource` 接线闭环验证（写反例样本）

**问题**：`pal_resource.h` 框架已建、三 target 实现存在，但 **DAL init 是否真的向 `pal_resource_claim` 登记**——需要独立验证。

**验证方法**（一个短平快的负样本）：
1. 新建 `samples/resource_conflict/`；
2. 两个 DAL 实例（例如 `dal_servo` 和 `dal_led`）在配置里故意声明**同一个 GPIO 引脚**；
3. 在 host target 下运行；
4. **预期**：第二个 `dal_xxx_init` 应返回 `WINK_ERR_COLLISION`；
5. **实际**：若返回 `WINK_STATUS_OK`（或其他非冲突错），说明 DAL 未接线到 `pal_resource_claim`，`pal_resource` 层是空壳。

**排期**：≤1 天可以完成。执行过程中若发现空壳，立刻升为**新 P0 计划**补接线；若已接线正确，则把这个反例样本沉淀为 CI 回归测试。

### 2.5【P0/P1】`dal_ultrasonic_read` 的 `@deprecated` 需要"硬"化

见 §二.1。此为 §二.1 的具体动作项，与 §二.1 合并。

### 2.6【P1】`pal_os_critical_enter` 的任务 / ISR 双入口契约需要在 API 层锁死

**问题**：`wink_trace.c` 现在依赖 `pal_os_critical_enter/exit`。这在**单虚拟核协作式调度器**（Proposed ADR-0014）上是廉价的（yield 点确定，无抢占），但：

1. `wink_trace_fault` 的调用契约允许 ISR 上下文调用（fault 来自定时器/中断是常见场景）；
2. 未来若引入多虚拟核（当前 ADR-0014 选单核，但方向可能变）；
3. ESP-IDF 的 `portENTER_CRITICAL` vs `portENTER_CRITICAL_ISR` 是**不同**的调用——如果 wink 抽象层没区分，ISR 里直接调用会崩。

**建议**：
- 在 `pal_osal.h` 显式给出 `pal_os_critical_enter_isr()` 变体；
- 或在 doxygen 契约里锁死"trace 不可在 ISR 调用，须走 `wink_trace_fault_from_isr()` 双入口"；
- 二选一，但**必须在协作式调度器计划 T5-T7 落地前**敲定，否则调度器上线后再改会牵动 trace / fault / IRQ / timer 多个模块的调用链。

### 2.7【P2】`pal_irq_direct_connect` API 契约改名

见 §一.2.1，此为该论断的建议动作项。

### 2.8【P2】`simulation.md` 文档漂移（保留报告 §6.2-3）

未核验，但按经验，SSOT 文档漂移几乎总是存在。建议扫一遍 `docs/design/02-wink-micro-os/*simulation*` 相关文档，对齐当前 DAL 细粒度旁路模式。

---

## 三、我不同意报告的两处排期

### 3.1【报告 §5.1 → P0】ADR-0008 逻辑 IRQ 号 codegen —— **不做**

**报告主张**：落地"逻辑设备树 codegen"，由前端编译管线自动生成 `<逻辑 IRQ 号 → 物理源/引脚>` 映射，消除 `TEST_IRQ_UAF 7` 这类硬编码。

**我的异议**：
1. **ADR 关联错**：ADR-0008 主题是 Flash 动态设备树，不管 IRQ 号（见 §一.1.7）。真要做，需要新 ADR，前置成本不低；
2. **只有一款板**：项目当前只有 ESP32 一款目标板，`TEST_IRQ_UAF` 硬编码只影响一个 sample，"移植瓶颈"是**为不存在的第二块板**做的抽象；
3. **STM32 移植不是本 sprint 目标**（memory 也没提）；真要移植 STM32 时会有一批更实际的问题（时钟树、PWM 拓扑、UART DMA 差异），到时候统一做设备树抽象比现在为一个 `#define` 单独立项更合理。

**结论**：**从待办列表移除**。等到"第二款目标板"进入 sprint 规划时重开。

### 3.2【报告 §6.3-1 → P2】共享中断链剔除 `malloc` —— **降级为 P3**

**报告主张**：`pal_shared_chain.c` 用 `malloc/free`（`stdlib.h` 第 11 行、`malloc` 第 54/66 行、`free` 第 91 行），需重构为静态内存池 + 位图。

**核验**：现况符合。

**我的异议**：
1. **发生位置**：注册阶段（初始化期），**不在 ISR 热路径**——运行期无碎片风险；
2. **规模**：`PAL_SHARED_CHAIN_MAX_HANDLERS = 4`，动态分配总量极小；
3. **动机**：这属于 MISRA-C / 航天级客户的强合规偏好，**本项目当前没有车规 / 航天 / 医疗客户**，投入产出比低；
4. **未来路径**：真要做的时候，应该复用 `pal_resource.h` 已有的静态表模式（`PAL_RESOURCE_MAX_CLAIMS`），而不是各建一套池。

**结论**：**降级 P3**。等客户合规约束进入需求时再启动。

---

## 四、调整后的优先级建议

### 4.1 P0（本 sprint 必须完成）

| # | 事项 | 来源 | 关联现有计划 |
|---|------|------|-------------|
| P0-1 | **协作式调度器实施计划推进**（T5 fiber + T7 单虚拟核 + CMake 平台守卫同 PR 内落地） | 报告 §4.2 合并整改 | `2026-07-01-sim-cooperative-scheduler-plan.md` |
| P0-2 | **`pal_resource` 接线闭环验证**（写反例样本 `samples/resource_conflict/`，验证 DAL init 是否真的 claim GPIO/PWM） | 我新提 §二.4 | 无现有计划，需新增 |

### 4.2 P1（1-2 周内完成）

| # | 事项 | 来源 |
|---|------|------|
| P1-1 | `pal_gpio_read/write` 返回值升 `wink_status_t` + out-param | 报告 §6.2-1 扩展 + 我 §二.2 |
| P1-2 | 阻塞 / 非阻塞 DAL API 统一 `WINK_BLOCKING` 属性 + 编译期告警 + deprecated 硬化 | 我新提 §二.1 & §二.5 |
| P1-3 | `wink_status_t` 错误码目录文档（AI codegen 语义手册） | 我新提 §二.3 |
| P1-4 | `pal_os_critical_enter` 任务 / ISR 双入口契约在 API 层锁死 | 我新提 §二.6 |

### 4.3 P2（1 个月内完成）

| # | 事项 | 来源 |
|---|------|------|
| P2-1 | `pal_irq_direct_connect` 契约改名或降级为 `pal_irq_iram_bind` | 报告 §3.2-1 反向 + 我 §二.7 |
| P2-2 | `simulation.md` 及关联 SSOT 文档对齐 | 报告 §6.2-3 |

### 4.4 P3（择机执行）

| # | 事项 | 来源 |
|---|------|------|
| P3-1 | Shared chain 静态池（合并 `pal_resource` 池模式） | 报告 §6.3-1 降级 |

### 4.5 撤回（不再排期）

| # | 事项 | 撤回理由 |
|---|------|---------|
| 撤回-1 | 单行 `if` 大括号 | 已符合，若担心复发落 lint |
| 撤回-2 | `wink_trace_fault` 竞争保护 | 已实现临界区保护 |
| 撤回-3 | ESP32 REALTIME 降级 | 已显式 `WINK_ERR_UNSUPPORTED` |
| 撤回-4 | `dal_ultrasonic_init` / `dal_servo_init` 缺失 | 已实现，改为验证接线（P0-2） |
| 撤回-5 | Windows Fiber 事后守卫 | 尚未引入，改前置约束纳入 P0-1 |
| 撤回-6 | 多 asyncify_data Spike | 已被 fiber 官方 API 方案取代 |
| 撤回-7 | ADR-0008 IRQ codegen | ADR 关联错误 + 单板无必要，见 §三.1 |

---

## 五、方法论层面的观察

这份外部报告呈现出 AI 生成评审报告的**典型失真模式**——它的语言结构对（分层清晰、术语准确、援引 BARR-C / MISRA / Linux 惯例得体），但**具体条目的时效性**跟不上代码。识别与处置这类报告的一般规则：

1. **先核验、后采纳**：任何外部工具生成的评审报告，进入排期前必须先跑一遍代码核验，逐条打标（符合 / 部分正确 / 已过时 / 错误）。这份核验本身的成本低于按错误清单排期的返工成本。
2. **按 ADR 状态过滤**：报告中援引的 ADR 编号务必核对**主题与状态**——AI 常把不同 ADR 的能力混绑（本报告把 ADR-0008 说成"IRQ codegen"就是这类失真）。
3. **区分"技术债"与"洁癖偏好"**：MISRA / BARR-C 的强合规项在没有对应客户的项目里属于**偏好**而非债务，容易被 AI 评审当作 P1 抬高。
4. **对"AI codegen 友好度"这类项目独有北极星**保持独立判断：外部评审的通用嵌入式经验不覆盖这层，需要架构师主动补齐。

---

## 六、后续动作

**建议对本文档的处置**：

1. 用户确认本核验意见后，**将 §四 P0 / P1 事项分别落地为**：
   - **新 ADR**：`pal_gpio_read/write` 接口重构（§P1-1）、`pal_os_critical_enter_isr` 双入口（§P1-4）；
   - **新实施计划**：
     - `implementation-plans/2026-07-XX-pal-resource-wire-verification-plan.md`（§P0-2）
     - `implementation-plans/2026-07-XX-dal-blocking-api-hardening-plan.md`（§P1-2）
     - `implementation-plans/2026-07-XX-wink-status-catalog-plan.md`（§P1-3）
2. 本 review 归档不再修改，作为外部评审报告的**批判性核验历史快照**。

---

*本 review 归档后不再编辑。若代码演进后需要再次评审，请新建 `2026-XX-XX-...-review.md`。*

---

## 落地状态（2026-07-02 追加）

本 review 已由 [`PLAN-20260701-WMOS-CODE-OPTIMIZATION-Q3`](../../implementation-plans/core/2026-07-01-wmos-code-optimization-q3-plan.md) **全量落地**，结项日 **2026-07-02**（原计划 20 个工作日，实际 1 日完成，详见该计划 Amendment Log § 2026-07-02 结项条目）。

**落地映射**（本 review 识别的 5 条真正需执行事项 → 落地 Track）：

| Review 条目 | 落地 Track / ADR | 关键 commit |
|-------------|-----------------|------------|
| §二.1 `pal_resource` 空壳 | **Track A**（无独立 ADR，作为 [ADR-0009 物理仿真](../../decisions/unisim/0009-physical-behavior-simulation-fault-injection.md) 落地闭环） | `c1f20ca` / `2bdecc7` / `81a9751` / `254d19a` |
| §二.2 `pal_gpio_read/write` 静默降级 | **Track B** + [ADR-0015](../../decisions/core/0015-pal-gpio-safe-error-propagation.md) | `3cd7d21` + `5291a95`（wasm 修补） + `b969e73`（-WithWasm） |
| §二.3 `pal_os_critical_enter` ISR 契约 | **Track D** + [ADR-0016](../../decisions/core/0016-pal-critical-section-task-isr-dual-entry.md) | `af5292a` / `bfe4ce9` |
| §二.4 阻塞 API 硬隔离 | **Track E** + [ADR-0017](../../decisions/core/0017-blocking-api-hard-isolation.md) | `4e8fdfc` / `cb74a50` / `8ab8762` |
| §二.5 PAL IRQ 契约虚标 | **Track F**（升级为"API 全面收窄"）+ [ADR-0018](../../decisions/core/0018-pal-irq-api-narrowing.md) | `b75f8d8`（ADR + plan） / `48fb60d`（实现） / `e9ba65e`（B1-B9 clean-up） |
| §四 P1-3 错误码目录 | **Track C** — 落地为 [`07-platform-governance/02-error-fault-model.md`](../../design/07-platform-governance/02-error-fault-model.md) §11「AI Codegen 错误码语义详表」（SSOT 位置调整：见 Q3 plan Amendment），配套 `wink_status.h` 每个 enum 内联 doxygen brief | Q3 plan 结项 commit |

**外部报告 7 条被撤回论断 + 2 条降级** 未产生代码变更，仍以本 review 为对齐记录，不再另立文档。

**本 review 归档到此为止**；后续 PAL/DAL/IRQ 演进请新建 review 而非编辑本文件。

