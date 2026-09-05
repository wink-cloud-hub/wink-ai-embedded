# BAL/DCST 架构重构方案——外部评审报告的批判性质询

**评审对象**：`docs/tech-designs/core/2026-07-06-bal-dcst-architecture-refactor.md`（v3，2026-07-06，待 Owner 复核）
**被质询的外部评审**：Gemini/Antigravity IDE 生成的《BAL/DCST 架构重构方案——资深嵌入式架构师评审意见与优化建议》（brain id `e23622a1-1a72-4146-b4f9-77a10ea3ee2f`，下称"外部评审"）
**质询方法**：逐条将外部评审的断言与设计文档原文、现有代码库（runtime/PAL/DAL/helpers）做逐字核对
**结论**：外部评审 7 条建议中仅 **1 条命中真实 bug，0.5 条命中真模糊点**；3 条已被设计文档明确覆盖却作为"新发现"提出；2 条方向可参考但严重程度或解法与项目现状严重不符；4 处事实性错误。

---

## 一、总体评价

外部评审的行文风格是"资深架构师喊狼来了"——把合理的 hardening 项一律提升到"严重安全漏洞 / HardFault / 系统崩溃"级别，对 ESP32 具体 API 行为的把握多处失真，并且有相当一部分内容是设计文档已经显式承诺、甚至用"MUST"粗体标出的要求，却被作为新发现提出。

整体定调：**本评审并非对方案的否决**，也不应阻断设计进入 Stage -1（ADR-first）。真实问题需要修，但外部评审把"值得修的并发健壮性 bug"和"未来 hardening 方向"都包装成"严重安全隐患"，会让 Owner 误判修复优先级。

本评审对外部评审的 7 条意见分四档处理：
- 🔴 **真实 bug，必修**：1 条（Slot TOCTOU 竞态）
- 🟡 **方向可参考，局部采纳**：3 条（堆碎片 P2、debug 泄漏 ASSERT、stage 1 上下文澄清）
- ⚪ **设计已覆盖，无需补充**：3 条（I2C 9 脉冲、µs 级 WCET、xTaskAbortDelay）
- ❌ **解法错误，明确拒绝**：2 条（Mutex 替代状态机、safe_off 必须寄存器直写、deinit 强制 stop）

---

## 二、逐条核对

### 🔴 第 1 条：Slot 池的并发与竞态条件漏洞

**外部评审结论**：严重安全漏洞，可能导致空指针解引用及系统崩溃；推荐方案 B（Mutex 串行 start/stop）。

#### 核对

**1.1 竞态窗口真实存在，必须修。**

设计文档 §3.3.2 给出的伪代码顺序是：

```c
pal_irq_enter_critical(&cs);
slot->dev = dev;                // 先占 slot
slot->period_ms = period_ms;
pal_irq_exit_critical(&cs);
// ← 这里发生调度，stop 能看到 dev 已指向但 periodic_h 还是 BSS 零值
wink_periodic_handle_t h = wink_periodic_start_ex(...);  // 阻塞，可能 yield
slot->periodic_h = h;                                    // ← 无保护写
```

`stop()` 进来在临界区内拿到 `h = slot->periodic_h = 0`（BSS 零初始；注意 0 是 INVALID handle 因为现有代码里 `wink_periodic_handle_t` 的编码是 `slot + 1`，从不返回 0），把 `slot->dev` 清 NULL 就退出，不调用 `wink_periodic_stop`。于是 `wink_periodic_start_ex` 成功后 `slot->periodic_h = h_valid` 写入一个 `dev == NULL` 的 slot，周期任务随后访问 `slot->dev` 触发空指针。这是标准的 TOCTOU 窗口，属于需要修的并发 bug。

**1.2 严重程度被夸大。**

外部评审定性为"严重安全漏洞 / 空指针解引用 / HardFault / 崩溃"，但触发前提是**两个并发执行上下文对同一个 `dev` 同时发起 start/stop**。典型嵌入式应用里一个外设的生命周期由单一状态机管理，出现这个并发是应用层 bug 而非运行时必遭遇的攻击面。在 sim 目标上（ADR-0013/0014 协作式调度器 + 单虚拟核）这个窗口根本无法被调度命中；只有 ESP32 SMP 双核 + 用户在两个 task 里对同一 dev 乱序调 start/stop 才会触发。合理定级为"并发健壮性 bug，P0 必修"，不要拔高到"严重安全漏洞"。

**1.3 推荐方案 B（Mutex）是劣解，应采用方案 A（状态机）。**

外部评审方案 B 的理由是"_start_ex 和 _stop 本身属于 MAY_BLOCK 路径，涉及任务创建/销毁，不应该在中断临界区中保护整个生命周期"。这暗示设计文档把整个生命周期都放进了临界区——**但文档 §3.3.2 第 597-604 行明确写了相反的取舍**：

> 临界区只保护 slot 元数据，`wink_periodic_stop`（可能最长阻塞 500ms）在临界区外调用。

文档已经把阻塞操作移出临界区，外部评审的前提就不成立。

Mutex 方案的问题：
- Mutex 在 start_ex 全程（含 `wink_periodic_start_ex` 任务创建）和 stop 全程（含 500ms sem 等待）持锁，粒度太粗，把"保护 slot 元数据"和"等待任务真的停稳"绑在一起。
- Mutex 不能在 ISR 里拿；虽然 BAL helper start/stop 不是 ISR API，但 ADR-0018 已经把临界区宏定为 PAL 层唯一的 KISS 原语，BAL 再引入一套 mutex 序列化规则是风格倒退。
- 真正需要的是状态机——把"slot 被预留但底层 task 还没起来"这个瞬态显式表达出来，stop 看到这个瞬态时要么 CAS 标记"起来后自毁"，要么（更 KISS）在临界区内用 generation 计数让刚启动完的 start 自己发现已被请求停止然后回滚。外部评审方案 A 已经描述了正确方向，但为什么推 B 不推 A 没有解释。

**1.4 外部评审对初始值的描述错了。**

外部评审说"`slot->periodic_h` 尚未被赋值（仍为初始值或 `-1`）"。BSS 零初始，不是 `-1`。现有 `wink_periodic` 的 INVALID 约定是 0（handle = slot+1 永不返回 0），stop 里 `if (h >= 0) wink_periodic_stop(h)` 在 h=0 时会走 `wink_periodic_stop(0)`，这是另一个小坑（返回 `WINK_ERR_INVALID_ARG` 还是走到越界要看实现）。修竞态时顺手把 slot 的 INVALID 常量明确下来。

**补充发现**：伪代码里用的 `pal_irq_critical_state_t / pal_irq_enter_critical / pal_irq_exit_critical` 这组 API **在现有 PAL 里不存在**。当前实际存在的是：
- `uint32_t pal_irq_save_rtos_safe(void);` + `void pal_irq_restore(uint32_t mask);`
- 宏 `PAL_CRITICAL_SECTION({ ... })`
- OSAL 层 `pal_os_critical_enter/exit()` 及 `_isr` 变体

设计文档的伪代码虚构了一组 API，修 §3.3.2 时要换成上述真实存在的原语之一，不然实现者落地时会困惑。

**处理结论**：P0 必修。
- §3.3.2 slot 结构体加 `state` 字段（`FREE / STARTING / RUNNING` 三态，或者用 `FREE <-> (dev != NULL && periodic_h == INVALID) <-> RUNNING` 两字段状态机）。
- start_ex：临界区内状态设 `STARTING` → 临界区外启动 task → 临界区内检查是否被并发 stop 请求了撤销（用一个 `stop_pending` 标志或 generation counter），如果被撤销就调 `wink_periodic_stop` 并回滚；没撤销则设 `RUNNING`。
- stop：临界区内若 `RUNNING`，摘出 handle、设 dev=NULL、状态回 FREE；若 `STARTING`，设 `stop_pending` 标志让 start_ex 回滚。
- 伪代码 API 名换成真实存在的 `pal_irq_save_rtos_safe/pal_irq_restore` 或 `PAL_CRITICAL_SECTION`。

---

### 🟡 第 2 条：动态启停与堆内存碎片化风险

**外部评审结论**：长期运行后可能因堆碎片导致任务创建失败；建议静态任务分配或任务池复用。

#### 核对

**2.1 方向正确，但严重程度被严重高估。**

外部评审的论据是"ESP32-DevKitC RAM 仅数百 KB，跑数天/周后无法找到连续栈内存"。这里有几处事实错位：

1. 设计文档**已经**用 codegen 静态容量 + BAL 静态 slot 池解决了"slot 数量"的资源耗尽问题（文档原话"100% guarantee no RESOURCE_EXHAUSTED"）。外部评审不否认这一点，但把矛头指向 PAL 层 `pal_os_task_create` 底层用 `xTaskCreate`（堆分配 TCB + 栈）。这是对的——静态 slot 不代表 task 栈也是静态的。

2. **但碎片化风险被夸大**：
   - FreeRTOS 默认堆实现是 `heap_4`（首次适应 + 相邻空闲块合并），不是 `heap_1/2`。反复创建/销毁**相同大小**的栈块（同一个 helper 每次 start 都用同样的 `stack_hint`）属于同尺寸反复借还，外部碎片几乎不累积——那块内存会被反复复用。产生显著碎片需要不同尺寸交错分配释放，这在 BAL helper 场景里并不成立。
   - 低功耗动态启停是**专家路径**（§3.4.3、§4.2 明确标为 optional expert flow）；典型 app 在 `app_init` 里一次性 start 所有 helper 后永不 stop，碎片场景在主路径上不存在。
   - ESP32-DevKitC 不是"数百 KB 极限量级"，是 520KB SRAM，IDF 启用 SPIRAM 后更多；几百字节栈块重复分配不是"跑几天就失败"的典型故障模式。

3. **静态 task 池/`xTaskCreateStatic` 是有价值的 hardening**，但属于确定性/可认证性场景（工业、车规）的加分项，不是当前消费级/教育级开发板阶段的阻断项。把它写进 §11"未来演进"或 P2 hardening 清单是合适的，不要因为这个让 Stage 1 的落地被阻塞。

**处理结论**：P2 hardening 留档，不阻断当前架构。
- 在 §3.3 或 §6 末尾加一条 future work："长期运行 hardening：评估将 `pal_os_task_create` 在 `WINK_CONFIG_STATIC_TASKS` 下映射到 `xTaskCreateStatic` + 静态栈 slot，或在 BAL 层实现 task 句柄持久化（sem 挂起/唤醒替代销毁重建）。"

---

### 🟡 第 3 条：低功耗状态转换下的"悬垂任务"与硬件冲突

**外部评审结论**：若开发者忘记 stop 某个 helper 就调 `wink_device_tree_deinit()`，周期任务继续访问已注销硬件导致死锁；建议加固 deinit：ASSERT 或强制 stop 所有 running helper。

#### 核对

**3.1 设计明确选择了"不自动 stop"，强制 stop 直接违反这个决策。**

§3.4.1（第 660-663 行）给出了 runtime 不做 auto-cleanup 的明确理由——ordered stop 有依赖关系（OLED animation 必须在 I2C bus deinit 前停；servo PWM 必须在 GPIO deinit 前停），通用链表/registry 无法表达顺序。Runtime 强制 stop 的顺序是 slot 数组顺序，可能先停 I2C 再停 OLED，反而制造新 bug。

这个决策还贯穿了 §3.4.2 的三阶段 Fault 模型（stage 2 调 `app_on_fault` 让应用自己按序 stop，runtime 不代劳）和 §4.2 的 expert 低功耗示例（示例里确实手动列了所有 `stop` 调用）。外部评审建议"强制 stop"是把设计**已明确拒绝**的自动清理方案又提了一遍。

**3.2 debug-build 泄漏 ASSERT 是合理的采纳点。**

开发期发现"忘记 stop"是有价值的。实现方式：
- 在 `wink_device_tree_deinit()` 入口加一段 `#if WINK_PT_DEBUG` 代码，遍历所有已注册 BAL helper 的 slot 数组，发现仍有 `state == RUNNING` 的就 `LOG_E + WINK_ASSERT(0 && "helper not stopped before deinit")`，把 helper 名打出来。
- Release 构建零开销，遍历代码被编译掉。
- 这和 §3.2.7"三道防线"里 runtime assertion 风格一致。

**处理结论**：部分采纳。
- ✅ §3.4.4 deinit 检查单加一项："Debug 构建泄漏断言——遍历所有 BAL slot，发现 RUNNING 态立即 fault"。
- ❌ 拒绝"强制 stop 所有 running helper"。

---

### 🟡 第 4 条：Fault 阶段 1 的 NMI / HardFault 上下文安全

**外部评审结论**：stage 1 safe_off 严禁调用任何带锁/分配/RTOS API 的函数，必须用底层寄存器直接写操作。

#### 核对

**4.1 设计文档对 stage 1 的上下文定义确实模糊，这是真问题。**

§3.4.2 写"IRQ/Fault 硬实时上下文（非阻塞，≤100µs）"，把**普通 IRQ 上下文**和 **HardFault/Panic/NMI 上下文**混在一起。这两种上下文在 ESP32 上语义差很多：
- 普通 IRQ 上下文：调度器知道中断嵌套，IDF 5.x/6.x 的 `gpio_set_level` 是 ISR-safe 的（内部用 per-bus spinlock 或原子操作，不会自死锁）。
- HardFault/Panic 上下文：panic handler 会 freeze 另一个核，该核持有的 spinlock 永远不会释放；panic 路径里若去拿 spinlock 会死锁，连 WDT 都救不回来（但 IDF panic handler 自己会关中断并进入 panic 专用路径，普通 SDK 函数不应在这个路径被调用）。

**4.2 关键事实：当前 stage 1 根本不在 HardFault 里跑。**

现有代码 `runtime/src/wink_runtime.c:260-276` 的 `wink_runtime_fault()` 是被 `WINK_CHECK`/`WINK_TRY` 等**应用层错误检测宏**同步调用的，不是挂在 CPU HardFault vector / panic hook 上。仓库里目前没有任何代码把 runtime fault 接到 `esp_set_panic_handler()` 或 `g_panic_info` 上。所以 stage 1 的真实调用上下文是"某个检测到错误的 task 或 ISR 同步调用 `wink_runtime_fault()`"——在 task 上下文里所有 SDK ISR-safe API 都可用，在 ISR 上下文里 IDF 的 ISR-safe API（`gpio_set_level` 等）也可用。

**4.3 "必须寄存器直写"是过度约束。**

- ESP32 GPIO 寄存器地址在不同芯片系列（ESP32 / S3 / C3 / C6）和 IDF 版本间有变动，强制直接寄存器写会大幅增加维护成本和移植负担。
- 当 stage 1 从普通 fault 路径（task 或 ISR）调用时，`gpio_set_level` 完全安全；禁用它是"为了未来可能接入的 HardFault hook 而让今天 100% 的正常路径难写难测"。
- 正确做法：
  - **现在**：把 stage 1 上下文澄清为"fault detect 路径（task 或 medium-priority ISR），可用 ISR-safe SDK API，禁止 mutex/sem/dynamic alloc/printf"——这和现有 §3.4.2 的禁令列表其实一致，只是不要把它说成"NMI/HardFault 安全"。
  - **未来若要接入 panic handler**：独立设计一套 `wink_panic_minimal_safe_off()`，专门用寄存器直写、与正常 `safe_off` 回调分离，不要污染正常 safe_off 的签名约束。这是一个新 ADR 的工作量，不是本方案要承诺的。

**处理结论**：局部采纳。
- §3.4.2 stage 1 描述改为："stage 1 在 fault-detect 上下文（task 或 ISR，非 panic/HardFault）调用，允许 ISR-safe SDK 调用（如 `gpio_set_level`），禁止 mutex/sem/dyn-alloc/printf/task-stop；≤100µs。"
- 加一条 future work："panic/HardFault hook 是独立 ADR 议题，需设计专用 minimal safe_off 路径（可能寄存器直写），不与正常 safe_off 回调混用。"
- ❌ 拒绝"所有 safe_off 回调必须寄存器直写"作为通用约束。

---

### ⚪ 第 5 条：I2C 总线死锁与 9 脉冲恢复机制

**外部评审结论**：建议在 dal_i2c init/deinit 里补充 9-Clock Pulse 恢复机制。

#### 核对

设计文档里**三处**明确要求 I2C 总线恢复：
- §3.4.2 WDT 脏复位警告（第 705-706 行）："I2C 发现 SDA 为低时，主机手动 toggle SCL 9 个时钟周期释放总线"。
- §3.4.4 deinit 检查单第 5 行："I2C 总线恢复（WDT 脏复位）—— 手动 toggle SCL 9 个时钟释放总线…`i2c_master_clear_bus()` 或手动 GPIO toggle"。
- §3.4.4 关键后果段再次点名"WDT 复位后 I2C 从设备拉死 SDA"。

外部评审引用 §6.4（codegen 段）作为"只提到了 gpio_reset_pin"的依据——§6.4 讲的是 codegen 调用 deinit 的契约，具体 deinit 行为确实在 §3.4.4 里。评审没有认真读到 §3.4.4 的检查单表格。

**处理结论**：已覆盖，无需补充。

---

### ⚪ 第 6 条：LIGHT 路径 WCET 监控的精度提升

**外部评审结论**：RTOS Tick 为 1ms/10ms，对 ≤100µs 的 LIGHT 回调无法察觉超时；必须用 µs 级高精度定时器。

#### 核对

**6.1 前提错误：现有代码已经用 µs 计时，根本不是 tick。**

`runtime/src/wink_soft_timer.c:140-147`：

```c
start_us   = pal_os_get_us();
status     = timer->callback(timer->arg);
elapsed_us = pal_os_get_us() - start_us;
if (elapsed_us > (WINK_RUNTIME_TICK_MS * 1000U / 2U)) {
    wink_trace_warn(WINK_WARN_WCET_EXCEEDED);
}
```

`pal_os_get_us()` 在 ESP32 上底层是 `esp_timer_get_time()`，后者用 ccount（CPU cycle counter）实现，分辨率亚 µs。host/sim 上是 `clock_gettime(CLOCK_MONOTONIC)` 或等价物。外部评审说"用 Tick 计数器"是没看代码。

**6.2 真正的问题是阈值太粗，这一点设计已经规划了。**

现有阈值是 50% tick = 5000µs（`WINK_RUNTIME_TICK_MS = 10`），确实抓不住 LIGHT 回调 100µs 预算里的 200µs 超时。但 §3.2.7"三道防线"已经规划：
- runtime 防线："在 `wink_periodic` LIGHT dispatch 入口/出口维护 in-LIGHT-context flag，复用 `WINK_ASSERT_NONBLOCKING()`"。
- 在 LIGHT dispatcher 加 per-callback 的 µs 级阈值（100µs）是这条防线的具体实现细节，不是新发现。

**6.3 ESP32 cycle counter 并不比 `pal_os_get_us()` 更精确**——后者已经是基于 cycle counter 实现的。评审建议"用 ESP32 的硬件 Cycle Counter 或 High Resolution Timer"是把同一个东西换个名字再说一遍。

**处理结论**：已覆盖。可以在 §3.2.7 里把"LIGHT 回调 WCET 阈值为 100µs，测量用 `pal_os_get_us()` 已足够"这句话写得更显式，但不构成新需求。

---

### ⚪ 第 7 条：`wink_periodic_change_period` 的 FreeRTOS 实现保障

**外部评审结论**：仅改 period_ms 无法唤醒休眠中的任务，必须用 `xTaskAbortDelay`。

#### 核对

§3.3.3 第 1.4b 节（MAY_BLOCK 侧）原文就是：

> ESP32: MUST use `xTaskAbortDelay()` to make long→short period changes take effect immediately — otherwise a change from 10s to 100ms waits out the full 10s (violates "零停摆"/zero-stall). Sim/host fiber sleep needs a similar interrupt point (check the generation flag).

第 632-633 行也写了"仅仅修改内部控制块的 `period_ms` 无法立即唤醒正在休眠的任务"。外部评审把设计文档里**用 MUST 加粗、并明确给出函数名**的要求作为新发现提出。

**处理结论**：已覆盖，无需补充。

---

## 三、对外部评审 ADR 拆分建议的处理

外部评审最后建议把上述内容拆为 ADR-0023/0024/0025。设计文档 Q13 已经规划了 Stage -1（ADR-first, 0.5 天），可以采纳 ADR 拆分，但内容需要重写——剔除被拒绝和已覆盖的项：

| ADR | 应采纳的内容 | 不应纳入的内容 |
|---|---|---|
| **ADR-0023** BAL Helper Slot 并发模型 | Slot 三态状态机（FREE/STARTING/RUNNING）+ generation 回滚；临界区使用 `pal_irq_save_rtos_safe/pal_irq_restore`（替换伪代码里虚构的 `pal_irq_enter_critical`）；start/stop 阻塞操作（任务创建/500ms stop 等待）在临界区外执行；**不**引入 BAL 层 mutex 序列化 | Mutex 替代状态机；初始值 -1 约定 |
| **ADR-0024** Fault 三阶段上下文契约 | Stage 1 上下文 = fault-detect 路径（task 或 ISR，非 panic/HardFault），可用 ISR-safe SDK API，禁 mutex/sem/dyn-alloc/printf/task-stop；panic/HardFault hook 是未来独立议题（需要单独设计 minimal safe_off）；DAL deinit 检查单加 debug-build 泄漏断言 | Safe_off 必须寄存器直写；deinit 强制 stop 所有 helper |
| **ADR-0025** 阻塞与 WCET 约束 | LIGHT 回调 ≤100µs 硬阈值；µs 级测量复用 `pal_os_get_us()`（已为 cycle counter 精度，无需新计时器）；超限在 dev build 升级为 fault；WINK_STRICT_NONBLOCKING 编译期防线保持 | "Tick 无法测 µs"这种错误前提；要求新硬件计时器 |

I2C 9-clk（§3.4.4 检查单）和 xTaskAbortDelay（§3.3.3）已经在设计文档里，不需要新 ADR；静态任务栈池作为 future work 可以在 ADR-0023 里带一句未来方向，不单独开 ADR。

---

## 四、设计文档修订清单（给 Owner）

按优先级排序：

| 优先级 | 位置 | 修订内容 |
|---|---|---|
| **P0** | §3.3.2 | Slot 结构体加 state 字段，按 FREE/STARTING/RUNNING 三态重写 start_ex/stop 伪代码，消灭 TOCTOU 窗口；用 stop_pending/generation 让 start 自我回滚 |
| **P0** | §3.3.2 | 伪代码 API 名 `pal_irq_enter_critical/exit_critical/pal_irq_critical_state_t` 替换为真实存在的 `pal_irq_save_rtos_safe/pal_irq_restore` 或 `PAL_CRITICAL_SECTION` 宏 |
| **P0** | §3.3.2 | 明确 slot 初始状态：`dev = NULL`, `periodic_h = WINK_PERIODIC_INVALID`（把 INVALID 常量从"=0"约定为具名常量），`state = FREE` |
| **P1** | §3.4.2 | Stage 1 上下文从"IRQ/Fault 硬实时"改为精确措辞："fault-detect 路径（task 或 ISR 同步调用），可用 ISR-safe SDK API；panic/HardFault hook 留作未来 ADR" |
| **P1** | §3.4.4 | deinit 检查单加一项："Debug 构建泄漏检测——遍历所有 BAL slot，RUNNING 态立即 LOG_E + WINK_ASSERT" |
| **P1** | §3.2.7 | 三道防线 runtime 项里明确："LIGHT 回调 WCET 硬阈值 100µs，测量复用 `pal_os_get_us()`（µs 级），超限在 dev build 升级为 fault" |
| **P2** | §11 future work | 加一条："长期运行 hardening——评估静态任务栈（`xTaskCreateStatic`）或 task 句柄持久化（sem 挂起/唤醒替代销毁重建）" |
| **P2** | §11 future work | 加一条："panic/HardFault hook 接入，独立设计 minimal register-level safe_off" |

**拒绝采纳（写入设计决策备注以防后续评审重复提出）**：
- ❌ Mutex 串行 start/stop（违反临界区最小化原则；与 ISR-safe 不兼容）
- ❌ safe_off 回调强制寄存器直写（过度约束；当前 fault-detect 上下文不需要）
- ❌ deinit 强制 stop 所有 running helper（违反 §3.4.1 ordered-stop 决策；顺序不可控反而制造 bug）

---

## 五、对外部评审方法论的评论

这份外部评审暴露出 AI 生成架构评审的几个典型问题，记录下来供后续参考：

1. **狼来了定调**：把所有问题都包装成"严重安全漏洞/HardFault/系统崩溃"。真正资深的评审会分级（P0 崩溃 / P1 健壮性 / P2 hardening / P3 未来方向），而不是一律 C 位警报。这种不分优先级的反馈会稀释真实 P0 问题的信号（本次只有 Slot TOCTOU 是 P0）。

2. **没读完整份文档**：I2C 9-clk、xTaskAbortDelay、临界区只保护元数据这三处都是文档里已写且加粗的内容，评审把它们作为新发现提出，说明评审是按章节跳读而非通读。

3. **没看代码库**：`pal_os_get_us()` 已经是 µs 级、`pal_irq_enter_critical` API 不存在、`wink_periodic_handle_t` 编码是 slot+1（INVALID=0 非 -1）——这些都是翻一下代码就能纠正的事实错误。

4. **给错解法**：Mutex 比状态机更"重"，在 BAL 这种要求 ISR-safe 和临界区最小化的层是劣解；评审因为"_start_ex 会阻塞"就主张 mutex，是把任务创建阻塞和临界区持有阻塞混为一谈。

5. **过度通用化**：从"可能在 HardFault 里死锁"直接推到"所有 safe_off 必须寄存器直写"，跳过了"当前是否真的在 HardFault 里调用"这个关键前提。嵌入式架构评审必须先问"这段代码实际运行在什么上下文"，而不是直接套"最严格"的约束。

对应改进建议：未来引入外部 AI 评审时，要求其**先列出文档里已有的对应约束**再提新意见，否则容易重复发现；同时要求附代码引用而非仅凭文档行文推断。

---

## 六、结论

外部评审**不应**阻断 BAL/DCST 方案进入 Stage -1（ADR-first）和后续落地。方案的核心思想（静态 JSON 资产 + 强类型 BAL Helper + codegen 静态容量 + LIGHT/MAY_BLOCK 双轨 + 三阶段 Fault + deinit 检查单）是稳固的。

真正必须在动工前修的只有一件事：**§3.3.2 Slot TOCTOU 竞态**，修法是状态机而非 mutex。其余都是 P1 澄清 / P2 hardening，不影响 Stage 0/1 的工作节奏。

**建议下一步**：
1. Owner 确认 P0 修订方向（状态机三态 + 真实 PAL 临界区 API）。
2. 按本评审第四章清单修订 tech-design v4。
3. Stage -1 提交 ADR-0023/0024/0025（按第三章重写的内容）。
4. ADR Accepted 后进入 Stage 0（DAL deinit 重写）。
