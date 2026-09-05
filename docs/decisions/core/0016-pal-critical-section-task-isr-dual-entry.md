# ADR-0016：`pal_os_critical_enter/exit` 分裂为 task/ISR 双入口

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳）** |
| 日期 | 2026-07-01（提议）；2026-07-02（采纳） |
| 触发 | [2026-07-01 外部综合评审批判性核验](../../reviews/core/2026-07-01-external-comprehensive-review-critique.md) §二.6；[PLAN-20260701-WMOS-CODE-OPTIMIZATION-Q3](../../implementation-plans/core/2026-07-01-wmos-code-optimization-q3-plan.md) Track D |
| 影响范围 | `pal/include/osal/pal_osal.h`；`targets/{esp32,host,wasm,baremetal}/pal_osal_*.c`；`trace/include/wink_trace.h` / `trace/src/wink_trace.c`；未来所有可能被 ISR 调用的 PAL/RUNTIME 模块 |
| 决策者 | 待定（架构委员会评审） |
| 关联评审 | [2026-07-01-external-comprehensive-review-critique](../../reviews/core/2026-07-01-external-comprehensive-review-critique.md) |
| 关联实施计划 | [PLAN-20260701-WMOS-CODE-OPTIMIZATION-Q3](../../implementation-plans/core/2026-07-01-wmos-code-optimization-q3-plan.md) §Track D（M2）、[2026-07-01-sim-cooperative-scheduler-plan](../../implementation-plans/unisim/2026-07-01-sim-cooperative-scheduler-plan.md)（前置） |
| 关联既有 ADR | [ADR-0007 协作式执行模型](0007-cooperative-loop-execution-model.md)、[ADR-0012 契约诚实](0012-contract-honesty-over-silent-degradation.md)（Proposed） |
| 关联设计规范 | `02-wink-micro-os/02-pal-platform-abstraction.md`（Accepted 后回写 OSAL §临界区章节） |

---

## 背景（Context）

WinkMicroOS 的 OSAL 层提供 `pal_os_critical_enter/exit` 用于保护共享静态状态的原子读写(`pal_osal.h:131,137`)。当前实现:

- **ESP32**(`pal_osal_esp32.c:167-177`):
  ```c
  static portMUX_TYPE s_global_mux = portMUX_INITIALIZER_UNLOCKED;
  uint32_t pal_os_critical_enter(void) {
      portENTER_CRITICAL(&s_global_mux);   /* task-only 原语 */
      return 0;
  }
  void pal_os_critical_exit(uint32_t key) {
      (void)key;
      portEXIT_CRITICAL(&s_global_mux);
  }
  ```
- **host / wasm**:单线程退化,通常 no-op 或返 0。
- **baremetal**:通常关中断实现。

`wink_trace.c` 中 `wink_trace_fault` / `wink_trace_reset` / `wink_trace_count` / `wink_trace_last` **全部通过 `pal_os_critical_enter/exit` 保护共享 `s_buffer/s_head/s_count`**(见 `wink_trace.c:14-47`)。文件顶部 INVARIANT 注释显式声称"Thread-safe / ISR-safe"。

### 现况的核心矛盾

1. **`wink_trace_fault` 契约声称 ISR-safe,但 ESP32 实现 task-only**:`portENTER_CRITICAL` 是 FreeRTOS 里**只允许 task context 调用**的原语。若 ISR 上下文调用 `wink_trace_fault`(例如硬件 fault handler 记录一次 IRQ 上报的异常),会触发 Xtensa `assert` 或 SMP 环境下的锁死。
2. **ESP-IDF 官方约定**:`portENTER_CRITICAL(&mux)` vs `portENTER_CRITICAL_ISR(&mux)` 是**必须分场景使用**的两套 API,前者是 task-only,后者是 ISR-only,二者内部使用不同的 spinlock 获取路径。
3. **协作式调度器**(`2026-07-01-sim-cooperative-scheduler-plan.md`)落地后,单虚拟核下调用 `wink_trace_fault_from_isr` 的场景会立即出现——定时器/硬件中断上报 fault 是典型链路。**协作式调度器 T5 落地时,若临界区双入口尚未敲定,fault trace 从 ISR 调用会直接崩**。

### 与既有 ADR 的关系

- **ADR-0007**(协作式执行模型):该 ADR 承认"协作式模型不排除 ISR 上下文回调进入系统模块"。这直接对应本 ADR 要解决的痛点。
- **ADR-0012**(契约诚实,Proposed):`wink_trace.c:8-9` INVARIANT 声称 "ISR-safe" 而 ESP32 实现 task-only,是典型的"头文件承诺 vs 实现不匹配"契约债务。**本 ADR 是 ADR-0012 在 OSAL 临界区场景的具体落地**。

---

## 方案比选（Options）

### 选项 A：Context-aware 单一入口(内部 detect ISR 上下文)

`pal_os_critical_enter/exit` 保持单一签名,内部自动分流:

```c
uint32_t pal_os_critical_enter(void) {
    if (xPortInIsrContext()) {                    /* ESP32 API */
        portENTER_CRITICAL_ISR(&s_global_mux);
    } else {
        portENTER_CRITICAL(&s_global_mux);
    }
    return 0;
}
```

- ✅ 优点:调用方无需感知上下文,单一入口。
- ✅ 优点:接口零破坏,`wink_trace.c` 代码不变。
- ❌ 缺点(决定性):**违反 ADR-0012 "契约诚实" + AI Codegen 友好北极星**。头文件看不到"这个函数在 ISR 上下文有不同行为",AI Codegen 生成的样例代码无法根据静态语义规划错误处理。
- ❌ 缺点:**掩盖误用**——调用方错把 ISR-only 代码放进 task 路径(或反之),编译期无告警,运行期也不失败,直到某种极端场景下才暴露(例如 SMP 下的 deadlock)。
- ❌ 缺点:`xPortInIsrContext` 在 host/wasm target 上没有对等 API,需要维护三 target 各自的"context detection"实现,成本高但收益低。
- ❌ 缺点:每次调用增加一个 branch,小性能损耗;但更重要的是心智负担。

### 选项 B：双入口显式分流(推荐)

`pal_osal.h` 新增 `_isr` 变体,调用方按上下文显式选择:

```c
/**
 * @brief Enter critical section (TASK context only).
 * @warning Calling from ISR context has undefined behavior on some targets
 *          (e.g., ESP32 uses portENTER_CRITICAL which is task-only).
 *          Use pal_os_critical_enter_isr() from ISR context.
 */
uint32_t pal_os_critical_enter(void);
void pal_os_critical_exit(uint32_t key);

/**
 * @brief Enter critical section from ISR context.
 * @note On ESP32 uses portENTER_CRITICAL_ISR sharing the same mux as task version.
 *       On host/wasm degrades to no-op or task-equivalent (single-threaded).
 *       On baremetal uses interrupt-disable.
 */
uint32_t pal_os_critical_enter_isr(void);
void pal_os_critical_exit_isr(uint32_t key);
```

配套地,`wink_trace` 层拆双入口:

```c
void wink_trace_fault(uint32_t fault_code);          /* task-only */
void wink_trace_fault_from_isr(uint32_t fault_code); /* ISR-only */
```

- ✅ 优点:契约诚实——调用方从头文件就看到"task 版 vs ISR 版";编译期误用可通过 lint/code review 抓到。
- ✅ 优点:与 ESP-IDF 官方 API 命名惯例对齐(`xQueueSend` / `xQueueSendFromISR`),AI Codegen 训练数据与 ESP-IDF 生态一致,无学习成本。
- ✅ 优点:host/wasm 单线程下 ISR 版可退化为 task 版实现(共享 mux),diff 极小;baremetal 上 ISR 版即"关中断"。
- ⚠️ 代价:调用方需要选对入口。协作式调度器**只有一个协作点(yield)**,当前项目中 ISR 上下文调用点数量可控(主要是 `wink_trace_fault`、GPIO ISR wrapper),Code Review 门槛可控。
- ⚠️ 代价:`pal_osal.h` 头文件新增 4 个函数声明,略变胖(可接受)。

### 选项 C：保持单一入口 + 严禁 ISR 调用(doxygen-only)

- ✅ 优点:diff 最小,只需在 `pal_os_critical_enter` doxygen 上加"禁止 ISR 调用"注释。
- ❌ 缺点(决定性):**丢失 ISR 场景下的 fault 记录能力**。协作式调度器 T5 落地后,ISR 上报的 fault 无法安全写入 trace 环形缓冲,数据丢失。
- ❌ 缺点:与 `wink_trace.c:8-9` 现有 "ISR-safe" INVARIANT 直接冲突,需要撤回该 INVARIANT——这本身就是一种"能力回退"。
- ❌ 缺点:未来若引入多虚拟核(ADR-0014 单虚拟核决策若被推翻),ISR 需求会立即出现,此方案无演进路径。

### 选项对比小结

| 维度 | A. Context-aware | B. 双入口显式(推荐) | C. 单一入口 + 禁令 |
|-----|-----------------|-------------------|------------------|
| 契约诚实(ADR-0012) | ❌ 隐藏差异 | ✅ 显式分流 | ⚠️ 需撤回 ISR 能力 |
| AI Codegen 友好 | ❌ 静态无法区分 | ✅ 命名明确 | ⚠️ 靠注释,不硬 |
| 与 ESP-IDF 惯例 | ⚠️ 不一致 | ✅ 对齐 | ⚠️ 不一致 |
| 演进路径 | ⚠️ 修改内部实现即可,但缺乏显式契约 | ✅ 可为其他 OSAL 原语沿用 | ❌ 需要撤回 ISR |
| ISR 能力保留 | ✅ 有 | ✅ 有 | ❌ 丢失 |
| 心智负担 | ⚠️ 隐性 | ⚠️ 显性(明码标价) | ⚠️ 靠人守 |
| 迁移成本 | 低 | 中(拆 trace 双入口) | 极低 |

**选择 B 的关键理由**:显性优于隐性,与 ESP-IDF 生态命名一致,AI Codegen 训练数据无扭曲。

---

## 决策结论（Decision）

**采纳选项 B**:`pal_os_critical_enter/exit` 分裂为 task/ISR 双入口,`wink_trace_fault` 同步拆双入口。

### 落地规则

1. **`pal_osal.h` 接口扩展**(§4 全局临界区章节):

    ```c
    /**
     * @brief Enter critical section (TASK context only).
     * @warning Calling from ISR context has undefined behavior on some targets:
     *          - ESP32: uses portENTER_CRITICAL which is task-only; ISR call
     *            triggers assert / SMP deadlock.
     *          - host/wasm: single-threaded, safe but semantically wrong.
     *          Use pal_os_critical_enter_isr() from ISR context.
     * @return key opaque state to pass into pal_os_critical_exit()
     */
    uint32_t pal_os_critical_enter(void);
    void pal_os_critical_exit(uint32_t key);

    /**
     * @brief Enter critical section from ISR context.
     * @note ESP32: uses portENTER_CRITICAL_ISR(&s_global_mux) sharing the same
     *       mux as task version — mutual exclusion holds across task/ISR.
     *       host/wasm: no-op or task-equivalent (single-threaded).
     *       baremetal: interrupt-disable.
     * @return key opaque state to pass into pal_os_critical_exit_isr()
     */
    uint32_t pal_os_critical_enter_isr(void);
    void pal_os_critical_exit_isr(uint32_t key);
    ```

2. **三 target 实现同步**:
    - **ESP32**(`pal_osal_esp32.c`):新增
      ```c
      uint32_t pal_os_critical_enter_isr(void) {
          portENTER_CRITICAL_ISR(&s_global_mux);  /* 共享同一 mux */
          return 0;
      }
      void pal_os_critical_exit_isr(uint32_t key) {
          (void)key;
          portEXIT_CRITICAL_ISR(&s_global_mux);
      }
      ```
    - **host** / **wasm**: 单线程下 ISR 版实现与 task 版共享锁逻辑。**但为保证真机保真性，在 Debug 构建下必须引入模拟中断上下文强校验**：当仿真器向模拟中断（如 GPIO、定时器）分发回调时，切换全局状态 `s_sim_in_isr`；`pal_os_critical_enter` 内部断言 `!s_sim_in_isr`，`pal_os_critical_enter_isr` 内部断言 `s_sim_in_isr`。借此可在 Host 单测与 CI 中瞬间捕获入口误用。
    - **baremetal**(`pal_osal_bare.c`,若存在):task 版原有实现;ISR 版可用相同关中断原语(或 no-op,视 baremetal 具体上下文)。

3. **`wink_trace` 层拆双入口**:
    - `wink_trace.h` 新增:
      ```c
      void wink_trace_fault(uint32_t fault_code);          /* task-only */
      void wink_trace_fault_from_isr(uint32_t fault_code); /* ISR-only */
      ```
    - `wink_trace.c`:
      - 保留现有 `wink_trace_fault` 实现语义(用 `pal_os_critical_enter/exit`);
      - 新增 `wink_trace_fault_from_isr` 用 `pal_os_critical_enter_isr/exit_isr`;
      - 提取公共写入逻辑到 `static inline void s_record_fault_locked(uint32_t code)` 复用,消除代码重复。
    - **`wink_trace_reset/count/last` 保持 task-only**(诊断/查询接口,不需要 ISR 变体)。
    - 修订 `wink_trace.c:8-9` INVARIANT 注释:改为"`fault` 为 task-only;`fault_from_isr` 为 ISR-only;`reset/count/last` 均为 task-only"。

4. **调用方迁移原则**:
    - ISR 上下文调用 fault 记录**必须**使用 `wink_trace_fault_from_isr`。
    - **故障处理与关断流程异步解耦**：在中断上下文（ISR）中**绝对禁止**同步调用 `wink_runtime_fault()` 或执行任何可能导致阻塞/I/O 的用户故障回调。ISR 检测到故障时，仅能通过 `wink_trace_fault_from_isr` 进行静态日志记录，真正的故障关断逻辑（Safe-off）与应用故障通知必须延迟（defer）至 Task 上下文（如主 Loop 的 tick 回收期）中进行。
    - 未来若发现其他"共享静态状态 + 可能被 ISR 触发"的模块(如 IRQ 统计计数器),沿用同样的双入口模式。

5. **禁止的实现路径(红线)**:
    - 🚨 **禁止**在 `pal_os_critical_enter` 内部 detect ISR 上下文并"自动分流"(选项 A)——契约诚实原则要求显式分流。
    - 🚨 **禁止**保留"单一入口 + doxygen 禁令"的方式(选项 C)——ISR fault 记录是明确需求,不能砍。

---

## 后果与约束（Consequences & Constraints）

### 正面后果

- ✅ ISR 上下文调用 fault 记录首次拥有**契约明确**的 API;协作式调度器 T5 落地后 ISR fault 链路可直接启用。
- ✅ 与 ESP-IDF 官方 `xxxFromISR` 命名惯例对齐,AI Codegen 训练数据与生态一致。
- ✅ 未来引入其他 "task/ISR 共享状态" 模块时,双入口模式已建立,直接复用。
- ✅ ADR-0012 契约诚实原则在 OSAL 层再落一子——头文件说得清楚,调用方看得清楚。

### 负面后果 / 约束

- ⚠️ `pal_osal.h` 头文件多 4 个函数声明,视觉复杂度略升(但语义清晰)。
- ⚠️ **调用方需要选对入口**:code review 与 lint 需要覆盖"ISR 上下文误用 task 版"的场景。当前项目 ISR 调用点数量可控(主要是 GPIO ISR wrapper + 未来的定时器 ISR),门槛可控。
- ⚠️ `wink_trace_fault` 与 `wink_trace_fault_from_isr` 需要**等价性测试**(Track D Task D-2 单测):同一 fault code 从 task/ISR 两条路径进入,`buffer/head/count` 状态一致。这属于本 ADR 落地的验收硬门槛。
- ⚠️ **应用故障处理与安全关断异步化约束**：因为 ISR 内禁止直接调用 `wink_runtime_fault`，因此系统必须支持从 ISR 故障记录到 Task 层异步触发关断的延迟机制，防止中断上下文执行阻塞 I/O。
- ⚠️ ADR-0012 未 Accepted 前,本 ADR 关联条目为 "Proposed",不影响本 ADR 自身生效——它们同批推进。

### Code Generation 指南

Codegen 生成需要临界区的代码时:

```c
/* ✅ 推荐:task 上下文(如 app_loop 内)记录 fault */
void app_loop(...) {
    if (some_error) {
        wink_trace_fault(WINK_ERR_IO);
        return;
    }
}

/* ✅ 推荐:ISR 回调内记录 fault */
static void my_gpio_isr(void *arg) {
    (void)arg;
    /* ISR 里第一时间清中断标志(见 pal_hal.h GPIO ISR 契约),然后记录 */
    if (some_hardware_flag) {
        wink_trace_fault_from_isr(WINK_ERR_HARDWARE);
    }
}

/* ❌ 禁止:ISR 里调用 task 版 */
static void my_gpio_isr(void *arg) {
    wink_trace_fault(WINK_ERR_IO);   /* ESP32 上会 assert / SMP deadlock */
}

/* ❌ 禁止:task 里调用 ISR 版(会漏保护) */
void app_loop(...) {
    wink_trace_fault_from_isr(WINK_ERR_IO);  /* task/task 竞态不保护 */
}

/* ❌ 禁止:ISR 里同步调用 wink_runtime_fault */
static void my_gpio_isr(void *arg) {
    wink_runtime_fault(callbacks, WINK_ERR_HARDWARE); /* 🚨 致命错误：中断上下文中同步执行了会触发阻塞/I/O 的用户回调与关断链 */
}
```

未来 codegen prompt few-shot 应包含此模式(见 Track C Task C-4)。

---

## 遵循与后续（Compliance & Follow-up）

### Accepted 后立即执行

1. 启动实施计划 §Track D(M2,3 天),按 Task D-1 → D-3 执行。
2. **回写至 `02-wink-micro-os/02-pal-platform-abstraction.md`** §4 OSAL 临界区章节:同步双入口 API 与"ESP-IDF FromISR 惯例对齐"说明。
3. **回写至 `.claude/skills/embedded-best-practice/`** 或 `_embedded-shared/concurrency.md`:新增"task/ISR context-aware 双入口模式"条目,作为未来 OSAL 原语扩展参考。
4. 更新 codegen prompt few-shot(若接入点确定):增加 `wink_trace_fault` vs `wink_trace_fault_from_isr` 选择示例。

### 与其他 ADR 的关系

- **ADR-0007**(协作式执行模型):本 ADR 直接支撑协作式调度器 T5 落地后 ISR fault 记录场景。
- **ADR-0012**(契约诚实,Proposed):本 ADR 是 ADR-0012 在 OSAL 临界区场景的直接落地——修订"实现 task-only 但契约声称 ISR-safe"的欠债。
- **ADR-0015 / ADR-0017**(本次同批提议):三份 ADR 都是 Q3 优化包的部分;ADR 与代码在同一或相邻 PR 内落地(实施计划 R-6 红线)。

### 未来演进路径

- 若引入多虚拟核(ADR-0014 单虚拟核决策若被推翻),`s_global_mux` 需按核区分,但 API 签名不变——双入口模式已包容此演进。
- 若引入分级临界区(short-critical vs long-critical),可在双入口基础上再增加变体(如 `pal_os_critical_enter_short`),但**不改**现有 API,保持向后兼容。
- **Host 仿真多线程化**：目前 Host 仿真及单测为单线程同步执行。若未来引入多线程测试或并发外设仿真，需将 `s_sim_in_isr` 变量转换为线程局部存储（如 `_Thread_local`）以防竞态。
- **模拟中断嵌套支撑**：目前不支持模拟抢占或嵌套模拟中断。若未来引入高优先级抢占式模拟中断，需将 `s_sim_in_isr` 的布尔值改为嵌套计数器（例如 `s_sim_isr_nesting_level`），规避内层中断退出时提早清除全局标志的问题。


---

*本 ADR 状态变更请在此记录:*
- 2026-07-01:Proposed(伴随 PLAN-20260701-WMOS-CODE-OPTIMIZATION-Q3 提出)
- 2026-07-02:Accepted(架构委员会通过；同 commit 内新增 `02-wink-micro-os/02-pal-platform-abstraction.md` §OSAL 临界区章节,写入 task/ISR 双入口 API 契约与 ESP-IDF `xxxFromISR` 惯例对齐说明)

