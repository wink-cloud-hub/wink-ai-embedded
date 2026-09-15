# ADR-0082：MCS51 复位语义、纤程退出与 main 重入机制

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳，2026-09-15）** |
| 日期 | 2026-09-15 |
| 触发 | GAP-07 闭环、Checklist §7（编号 33/34/35 复位核心用例验证）、实施计划 `PLAN-20260915-MCS51-RESET-FIDELITY` Task 0 交付物 |
| 影响范围 | `wink-micro-os/frameworks/mcs51/`（`cms8s_sys.cpp`、`REG_CMS8S78XX.H`、`mcs51_context.cpp`、`mcs51_bridge.cpp`、`wink_mcs51_wdt.h`）、`targets/wasm/`；Layer-① 设计规范与红线手册 |
| 决策者 | 项目架构团队 / Owner |
| **关联 ADR** | [ADR-0012](0012-contract-honesty-over-silent-degradation.md)（契约诚实）、[ADR-0070](0070-mcs51-zero-code-simulation-interception-layer.md)（零侵入拦截层）、[ADR-0072](0072-dual-clock-domain-and-quota-catchup.md)（双时钟域与微步记账）、[ADR-0076](0076-mcs51-sim-backends-native-vs-iss-channel-roadmap.md)（Native/ISS 行为等价与拦截禁令） |
| **关联实施计划** | `docs/implementation-plans/mcs51/2026-09-15-wdt-and-reset-fidelity-plan.md`（v2.1） |
| **关联设计规范** | `docs/vendors/Cmsemicon/CMS8S78XX_EXAMPLE_CHECKLIST.md` §7、保真度文档 `2026-09-08-mcs51-simulation-vs-silicon-fidelity-and-test-limits.md`、红线手册 `2026-08-27-mcs51-user-code-compatibility-and-limitations-guide.md` |

---

## 1. 背景（Context）

在 MCS51 仿真（特别是 CMS8S78xx 芯片模型）中，现有看门狗与系统复位机制存在严重保真度断层（GAP-07）：
1. **WDT 假复位**：`WDCON.WDTRE = 1` 溢出后仅累加 `s_wdt_triggered` 计数器，未触发芯片复位；
2. **SWRST 软复位缺失**：未处理 `WDCON.SWRST`（bit 7）写入，软件复位指令被静默忽略；且缺少 StdDriver shim（`SYS_Enable/DisableSoftwareReset` 等），官方示例 33 无法编译；
3. **复位标志寄存器与粘性丢失**：`PORF`/`WDTRF` 缺乏置位/清除规则。硅片上 `PORF` 具备**粘性（Sticky）**（仅冷启动 POR 置 1，仅软件写 0 可清除，热复位保持不变）。当前 `mcs51_context_reset` 直接 `memset(ctx, 0, ...)`，导致热复位时粘性标志被抹去；
4. **死循环纤程挂死与上下文无法重入**：官方 33 号原厂代码触发软复位后立即进入 `while(1) { ; }`（转译为 `while(1) { _nop_(); }`）。若仅在底层设置标志而不主动打断调用栈，纤程将继续在死循环中空转耗尽配额，无法回到 `main()` 入口；
5. **Host CTest 与 Wasm Runner 的验证断层**：Wasm 场景外部有 TypeScript Runner；但 Host 原生 CTest 是独立 C++ 进程，缺少外部调度器重载机制，现有 `wink_runtime_run()` 每次调用均重置调度器，无法在单测中自闭环验证“复位→状态保持→重入 `main()`”；
6. **全局队列与中断嵌套残余污染**：热复位若不显式 deinit 事件队列，旧 boot 积压事件会泄漏给新 boot；若复位在 ISR 内部触发，`in_service_depth` 与优先级堆栈未清零将引发新 boot 中断锁死。

## 2. 方案比选（Options）

### 2.1 纤程死循环退出与重入方案

| 方案 | 描述 | 优 | 劣 | 结论 |
|---|---|---|---|---|
| A. 仅置标志，等待自然退出 | 复位时置 `reset_pending=1`，期待用户 `main()` 退出 | 无栈操作风险 | 用户代码中普遍存在 `while(1)` 死循环，永不退出，导致纤程配额耗尽挂死 | ❌ 否决 |
| B. 抛出 C++ 异常 | 拦截点 `throw ResetException()`，上层 `catch` 重启 | C++ 原生支持栈解包 | 项目全局启用 `-fno-exceptions`，在 C-ABI 边界与嵌入式环境下禁止异常 | ❌ 否决 |
| C. **微步拦截 + `setjmp`/`longjmp` 解包（内核微重启）** | 在 `wink_mcs51_microstep()` 拦截点检测 `reset_pending`；在 `mcs51_app_loop` 与测试 Harness 建立 `setjmp` 保护帧；触发复位时通过 `longjmp` 解包栈帧跳出 `while(1)` | 完全兼容 `-fno-exceptions`，零 C++ 异常开销；可精确打断深度调用栈；在 Host CTest 下自闭环 | 需防范跨 Asyncify 帧长跳转（已通过宿主层 Runner 状态解耦化解） | ✅ **采纳（内核层）** |
| D. **外部 Runner 销毁重载（宿主级真重置）** | Wasm 侧导出复位状态，由 TypeScript Runner 捕获复位事件并重启 Wasm 实例 | 彻底重置所有内存包括全局变量与静态存储区 | 依赖外部 Runner，无法在原生 Host CTest 单测中自闭环 | ✅ **采纳（宿主层协同）** |

## 3. 决策结论（Decision）

### D1. 纤程在 `while(1)` 中的主动拦截与退出协议

1. **安全拦截点**：统一设于 `wink_mcs51_microstep()`（所有 `_nop_()`、SFR 读写必经之路）。
2. **拦截逻辑**：
   - 入口处检测 `wink_mcs51_has_pending_reset()`；
   - 若为真，立即截断后续外设轮询与中断派发；
   - 执行残余清污（见 D5）与硬件复位状态机（见 D3）；
   - 若当前处于可重入环境（`s_reentry_active == true`，如 `mcs51_app_loop` 或专用测试 Harness），调用 `std::longjmp(s_reset_jmp_buf, 1)` 强制解包调用栈，退出 `wink_mcs51_user_main()` 的死循环。
3. **嵌套与重入防护（P15）**：`reset_pending` 置位后，处理中忽略二次触发；若处于复位执行流程中，丢弃后续复位源并递增诊断计数器。

### D2. 双层解耦重入架构（Host CTest 自闭环 + TS Runner 挂接）

| 环境 | 层级 | 机制 | 职责 |
|---|---|---|---|
| **Host CTest** | 内核层 (Bridge) | `mcs51_app_loop` 协作重入环路 / `wink_mcs51_test_run_reentry_loop` | 原生 CTest 无需 TS Runner，自包含验证“多轮复位脉冲输出、标志保留、事件清污” |
| **Wasm / Unisim** | 宿主层 (Runner) | 导出 C-ABI：`pal_wasm_has_pending_reset()`、`pal_wasm_get_reset_reason()`、`pal_wasm_clear_pending_reset()` | 供 Unisim TS Runner 捕获复位事件，无缝对齐未来姐妹仓 ADR-0070 D-005b 实例级物理重载 |

### D3. WDCON 位级写语义与 PORF/WDTRF 粘性时序规范

1. **WDCON 位级写语义矩阵**：
   - `SWRST (bit 7)`：需 TA 解锁。**0→1 沿触发**软件复位；复位后硬件自清为 0；写 0 为合法前置（不触发）。
   - `PORF (bit 6)`：**无需 TA 保护**（对齐原厂 `system.c:394`）。固件写 0 清除；写 1 忽略（不可由固件置位）。
   - `WDTIF (bit 3)`：需 TA 解锁。W0C（写 0 清除）；写 1 忽略。
   - `WDTRF (bit 2)`：需 TA 解锁（对齐原厂 `system.c:309`）。W0C（写 0 清除）；写 1 忽略。
   - `WDTRE (bit 1)`：需 TA 解锁。0→1 使能溢出复位，同时重臂喂狗基准。
   - `WDTCLR (bit 0)`：需 TA 解锁。写 1 喂狗，硬件自清为 0。
2. **粘性标志保持时序**：
   - 在 `mcs51_context_reset` 执行 `memset(ctx, 0, ...)` 之前，先提取暂存：
     `uint8_t saved_porf = ctx->sfr_shadow[0x97] & 0x40u;`
   - 全量硬件清零与硅片种子应用完成后：
     - 若复位源为上电冷启（POR）：`PORF` 强制置 1，`WDTRF` 清 0；
     - 若复位源为热复位（SWRST / WDT / EXT）：**回填 `saved_porf`（保持前态粘性）**；
     - 若复位源为 WDT：`WDTRF` 强制置 1；其余复位源 `WDTRF` 清 0。

### D4. 原厂 34 号 `WDTRE=1 && WDTIE=1` 双使能仲裁

1. 原厂 34 号源码同时开启了 `SYS_EnableWDTReset()`（WDTRE=1）与 `WDT_EnableOverflowInt()`（WDTIE=1），且在 `isr.c:242` 中实现了 `P33 = ~P33`。
2. **仲裁规则**：当 `WDTRE=1` 使能溢出复位时，复位动作优先于中断派发，溢出时**强制压制 Vector 20 中断派发**，防止 ISR 与复位发生时序竞态。
3. **官方示例 34 验收口径**：源码在 `main.c:92-100` 每轮约 1.3ms 喂狗（远小于 174.76ms 溢出周期），在仿真中**绝不应复位**。采用**负例双引脚断言**：
   - P3.2 正常持续翻转；
   - **P3.3 始终保持初始弱上拉高电平**（证明 WDT 中断未触发、复位未发生）。
4. WDT 复位正例由专用的测试 fixture（关闭喂狗）自闭环验证。

### D5. 全系统残余清污标准清单

在复位生效且重入前，必须依序执行以下清污流程：
1. **事件队列**：显式调用 `wink_event_queue_deinit()` 销毁旧队列与信号量，彻底清空积压事件；随后调用 `wink_event_queue_init()` 重新初始化；
2. **边沿队列**：强制清空 `ctx->edge_head = ctx->edge_tail = 0`；
3. **中断嵌套堆栈**：强制重置 `ctx->in_service_depth = 0`，`ctx->reti_suppress_one = false`，清空当前 ISR 优先级掩码；
4. **GPIO 引脚状态**：复用统一初始化链，重置 32 引脚为弱上拉高电平（0xFF），保证与硅片复位态完全一致；
5. **时钟时基**：`ctx->virtual_us` 重基为 0，单调 PAL 时钟保持平滑连续。

### D6. 用户数据策略（P14：显式限制声明，遵循 ADR-0012）

1. **片内存储器**：`mcs51_context_reset` 全面清零 `sfr_shadow`，根据单片机复位规范重置 IDATA 与寄存器种子，保证微控制器上下文符合芯片定义。
2. **Host C++ 全局/静态变量**：Native 仿真进程中，C++ 静态存储区变量（如未在 Keil 内存模型中的宿主全局变量）在微重启时不重新执行 C 运行时 `.data` 初始化。
3. **契约声明**：遵循 [ADR-0012](0012-contract-honesty-over-silent-degradation.md) 契约诚实原则，在红线手册中显式声明该边界；在 Unisim Wasm 生产环境中未来由 TS Runner 的 Wasm 模块重新实例化实现绝对物理数据段重置。

### D7. 复位响应延迟契约

1. **软件复位 (SWRST)**：写操作触发后，在当前微步结束前生效，响应延迟 ≤ 1 microstep（约 5µs）。
2. **看门狗复位 (WDT)**：溢出检查点检测到超时后立即触发，响应延迟 ≤ 1 调度 quantum。
3. **外部复位 (EXT)**：外部引脚变低后在下一 microstep 检测并响应。

---

## 4. 后果与约束（Consequences & Constraints）

| 正面效益 | 约束与代价 |
|---|---|
| 彻底消除 GAP-07 WDT 假复位缺陷，落地芯片级 Reset 状态机与 `main()` 重入机制 | 复位发生时调用栈被 `longjmp` 解包，局部自动变量不执行 C++ 析构（本项目严格 `-fno-exceptions` 且 C 代码不含非平凡析构对象，安全） |
| Host CTest 原生单测自闭环，摆脱对姐妹仓 TS Runner 的进度阻塞 | 原生 Host 重启下宿主静态全局变量不自动重新初始化（已在 D6 显式声明并在文档中设防） |
| 纠正原厂 PORF 免 TA 清除与粘性保持语义，提升底层 SFR 保真度 | 既有 `test_mcs51_wdt_ta` 契约注释需同步回写并回归 |
| 打通官方示例 33（软复位）与 34（看门狗负例）并建立严密的双引脚判决链 | 官方示例 35 外部复位依赖 GAP-06 CONFIG 建模，需显式标注 deferred，严禁虚打 `[x]` |

---

## 5. 遵循与后续（Compliance & Follow-up）

- [x] 本 ADR Accepted；
- [ ] Task 1：在 `cms8s_sys.cpp`、`REG_CMS8S78XX.H`、`mcs51_context.cpp`、`wink_mcs51_wdt.h` 落地寄存器语义与状态机；
- [ ] Task 2：在 `mcs51_bridge.cpp` 落地微步拦截、清污与重入；
- [ ] Task 3：官方示例 33 适配与场景验证；
- [ ] Task 4：官方示例 34 负例双引脚断言与正例单测；
- [ ] Task 5：官方示例 35 降级为单测 seam；
- [ ] Task 6：回写 Layer-① 文档、红线手册、GAP-07 状态与 Checklist §7 证据链。

---

*本 ADR 状态变更记录：*
- 2026-09-15：Proposed & Accepted（实施计划 `PLAN-20260915-MCS51-RESET-FIDELITY` Task 0 定案）
