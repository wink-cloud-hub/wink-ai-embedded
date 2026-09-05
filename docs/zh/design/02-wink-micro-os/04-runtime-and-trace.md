# 3.4 运行时生命周期与 Golden Trace 契约

> ✅ **架构决策回写**：本节是 [ADR-0007](../../decisions/core/0007-cooperative-loop-execution-model.md)（协作式执行模型）与 [ADR-0010](../../decisions/core/0010-boot-safe-lock-recovery-threshold.md)（Boot Safe-lock 恢复阈值）的正式落地，为系统单一事实来源。

本文件落地 [03-directory-architecture.md](./03-directory-architecture.md) §7：runtime 与 trace 两层 peer 的 API 契约、回调注入模型与 target entry 接线流程。

## 1. runtime：回调注入主循环

### 1.1 设计动机
`main()` 与 OS 主循环历史上埋在 `targets/wasm/pal_hal_wasm.c`，无统一归宿。A* 把它提炼为 target-agnostic 的 `runtime` 层，各 target 的 `*_entry.c` 只负责"启动 runtime"。

### 1.2 回调注入（无 extern app_*）
App 经 `wink_app_callbacks_t` 注册生命周期钩子，runtime 库**不持有**对外部 `app_*` 符号的强 `extern`，达成二进制级解耦（便于单测/Mock）。

```c
typedef struct {
    /* New Status-Returning Signatures (Recommended) */
    wink_status_t (*init_status)(void);
    wink_status_t (*on_fault_status)(uint32_t fault_code);
    void          (*on_boot)(void);

    /* Legacy Signatures (Deprecated, kept for compatibility) */
    void          (*init)(void);
    void          (*on_fault)(uint32_t fault_code);

    /* Core execution loop callback */
    void          (*loop)(void);
} wink_app_callbacks_t;

void wink_app_delay_ms(uint32_t ms);
wink_status_t wink_runtime_run(const wink_app_callbacks_t *callbacks, uint32_t max_ticks);
```

- `init_status`/`on_fault_status`/`loop`/`on_boot` etc. are all allowed to be NULL.
- `max_ticks`: host/testing passes finite value to avoid `while(1)`; real hardware/WASM passes `0` for infinite loop.
- If `init_status` is provided and returns a non-OK status, or if `init` fails, the runtime initiates the Fault lifecycle immediately.


### 1.3 接线流程
```
wasm_entry.c::main() / esp32_entry.c::app_main() / host 样例 main()
        │  实例化 wink_app_callbacks_t（来自 App 的 wink_app_get_callbacks()）
        └─► wink_runtime_run(&cb, 0/ N)
                   ├─ cb.init()  (一次)
                   └─ while(1){ cb.loop(); wink_app_delay_ms(TICK); }
```

## 2. trace：Golden Trace 一等 peer

### 2.1 定位
独立顶层 peer（非 runtime 子特性，见 03-directory-architecture.md §3 Screaming Architecture）。横切基础服务，被 runtime/App 消费。

### 2.2 隔离契约（§6.1 约束2）
DAL/PAL 驱动**禁**直接调 `wink_trace_*`；只返 `wink_status_t`。故障捕获与 trace 记录收敛在 App 回调（`on_fault`）或 runtime。

### 2.3 API
```c
#define WINK_TRACE_CAPACITY 32   /* 静态环形缓冲，零动态分配 */
void wink_trace_reset(void);              /* TASK 上下文 */
void wink_trace_fault(uint32_t code);     /* TASK 上下文（pal_os_critical_enter/exit） */
void wink_trace_fault_from_isr(uint32_t code); /* ISR 上下文（pal_os_critical_enter_isr/exit_isr），ADR-0016 */
uint32_t wink_trace_count(void);          /* TASK 上下文（诊断/查询） */
uint32_t wink_trace_last(void);           /* TASK 上下文；无记录返回 0 */
```

### 2.4 并发契约（ADR-0016 task/ISR 双入口）

- **`wink_trace_fault` = TASK 上下文**；`wink_trace_fault_from_isr` = ISR 上下文；二者共享同一静态环形缓冲，task/ISR 互斥由 PAL 全局 mux 保证（ESP32 上 `portENTER_CRITICAL_ISR` 与 `portENTER_CRITICAL` 共享 `s_global_mux`）。
- **`reset/count/last` 保持 TASK-only**——诊断/查询接口无 ISR 变体。
- **ISR 路径禁止同步调用 `wink_runtime_fault`**：ISR 只做静态日志（`wink_trace_fault_from_isr`），Safe-off 关断链必须延迟到 TASK 层（主 Loop tick 回收期）。
- Host / Wasm 单线程仿真下，`pal_os_set_sim_isr_context(true/false)` sim-hook 会包住 ISR 边界；`wink_trace_fault` 与 `wink_trace_fault_from_isr` 在同一 fault code 序列下的 buffer/head/count 状态**bit-for-bit 等价**，由 `test_wink_trace_isr_equivalence` 硬门槛保证。

## 3. Fail-Safe / WDT / 执行器关断注册表（Phase 5，P0-4）

### 3.1 执行器关断注册表（Actuator Registry）
runtime 维护静态注册表（`wink_actuator_registry.h`，容量 16，零动态分配）。执行器 DAL 在 init 阶段
注册**自己语义正确**的 safe-off 回调；fault/panic/boot-lock 路径统一调 `wink_actuator_safe_off_all`
关断全部（即使单项失败也继续遍历，失败项 trace 记录）。
```c
typedef wink_status_t (*wink_actuator_safe_off_fn)(void *ctx);
wink_status_t wink_actuator_register(wink_actuator_safe_off_fn fn, void *ctx);  /* 幂等 */
void wink_actuator_safe_off_all(void);   /* 不得阻塞 */
```
> ⚠ safe-off 语义边界：duty=0 对舵机=limp=安全，但对未来 DC 电机可能是 coast 而非 brake。各执行器
> 须注册各自语义正确的关断（电机=制动/断使能）。`dal_rc_servo_safe_off` 仅适用舵机，不外推为通用范式。
> ⚠ 硬件级默认安全态（引脚 Hi-Z 弱拉、执行器使能脚默认关断、电源门控）**必须由板级电路保证**——
> 软件无法覆盖 HardFault/CPU 卡死/WDT 硬复位瞬间，registry 只补软件闭环。

### 3.2 故障处理三阶段 (Phase 1/2/3) 模型与初始化回滚契约（ADR-0024）

系统采用明确划分执行时间、上下文限制与安全性职责的**三阶段故障模型**：

```text
[Fault Detected / init_status Non-OK]
                  │
                  ▼
┌──────────────────────────────────────────────┐
│ Phase 1: Safe-Off (硬实时/快速关断)          │  Time: ≤100µs, Context: Task/ISR
│ - 调 wink_actuator_safe_off_all()            │  Allowed: Non-blocking write, GPIO reset
│ - 关断所有执行器物理输出                     │  Prohibited: Mutex, print, task create/stop
└─────────────────┬────────────────────────────┘
                  │
                  ▼
┌──────────────────────────────────────────────┐
│ Phase 2: App Callback & Graceful Stop        │  Time: ≤500ms, Context: Fault Task
│ - 调 callbacks->on_fault_status() / on_fault │  Allowed: Log, Telemetry, sequence stops
│ - 业务逻辑安全停机                           │  Prohibited: Infinite loops, blocking >50ms
└─────────────────┬────────────────────────────┘
                  │
                  ▼
┌──────────────────────────────────────────────┐
│ Phase 3: Hardware Watchdog Reset             │  Time: ~1000ms, Context: Hardware WDT
│ - 硬件级彻底重启（WDT 动作）                 │  Purpose: Restart from clean BSS/BSS
└──────────────────────────────────────────────┘
```

#### 3.2.1 故障阶段规约
1. **Phase 1 · Safe-Off (硬实时/快速关断)**:
   - **触发上下文**：检测到 fault 的 Task 或 ISR 同步调用 `wink_runtime_fault`，非 panic/HardFault 状态。
   - **时间预算**：硬性限制在 **≤100µs** 以内。
   - **操作限制**：仅允许无锁、非阻塞的硬件操作（如 `pal_gpio_write`、`pal_gpio_reset_pin`）。**严禁**调用任何可能导致等待/挂起的 API（如 Mutex/Sem、动态分配、`printf`/`LOG` 格式化、以及 Task 创建或销毁）。
   - **目的**：以最快速度物理关断全部执行器（通过 `wink_actuator_safe_off_all`）。
2. **Phase 2 · App Callback (应用层优雅停机)**:
   - **触发上下文**：专门的 Fault Task 上下文（中等优先级控制任务）。
   - **时间预算**：限制在 **≤500ms** 以内。
   - **操作限制**：允许调用 callbacks（`on_fault_status`）、写 flash 日志、发送遥测告警消息、以及在临界区外优雅调用各 BAL 服务的 `stop`（如 `wink_ultrasonic_poll_stop`）。
3. **Phase 3 · WDT Reset (硬件复位)**:
   - **目的**：若发生 unrecoverable fault 或 Phase 2 超时挂死，依靠硬件看门狗在 ~1s 内强制复位，清除所有软件脏状态。

#### 3.2.2 初始化失败“谁启动、谁回滚”契约

为了防范 `device_tree_init()` 过程中某个外设初始化失败（如 echo pin 冲突导致 ultrasonic 驱动退出）引发的系统悬挂或资源泄漏：
1. **本地完全回滚**：任何 DAL `init` 内部若有多步资源 claim（GPIO/PWM 等），如果在中途失败，该 `init` 函数内部**必须**在退出前将自己本步骤已 claim 的所有资源释放干净。
2. **系统不盲目清理**：当 `device_tree_init()` 检测到某个设备 init 失败时，系统将直接触发 Phase 1 关断所有已注册的执行器，并抛出 init 故障。运行时**不承诺**去调用其他已初始化成功外设的 deinit——因为系统无法安全猜测复杂的总线依赖关系。未成功的初始化由 WDT 硬件复位作为最终清理兜底。


### 3.3 PAL WDT / 复位原因（`pal_osal.h`）
```c
typedef enum { PAL_RESET_REASON_UNKNOWN, PAL_RESET_REASON_POWER_ON,
               PAL_RESET_REASON_WATCHDOG, PAL_RESET_REASON_PANIC } pal_reset_reason_t;
pal_reset_reason_t pal_get_reset_reason(void);
wink_status_t pal_watchdog_init(uint32_t timeout_ms);
wink_status_t pal_watchdog_feed(void);
```
host：WDT 为无操作 stub（OK），reset reason 可配置（供测试）；wasm：reset reason 恒 UNKNOWN、WDT
`WINK_ERR_UNSUPPORTED`（无浏览器 watchdog 策略）；esp32：待映射 `esp_reset_reason()` + ESP-IDF watchdog（随 P2-6）。

### 3.4 Boot safe-lock（连续复位计数 + 恢复，ADR-0010 修订 ADR-0007）
`wink_runtime_run` 启动时读 `pal_get_reset_reason()` 与持久化的「连续异常复位计数」（`pal_get/set_abnormal_boot_count`，ESP32 存 `RTC_NOINIT`）：
- `POWERON` → 计数清零（断电重启，干净）；
- `WATCHDOG`/`PANIC` → 计数 +1：
  - **≥ `WINK_BOOT_LOCK_THRESHOLD`(3)** → **锁死**：`wink_runtime_fault`（trace 8001 一次 + safe-off + on_fault）+ `return WINK_ERR_LOCKED`，**不执行** `cb.init()/cb.loop()`。仅真死循环（每次启动必崩）累加到此阈值。
  - **< 阈值** → **放行恢复**：正常跑 `cb.init()/cb.loop()`（单次/偶发复位自动恢复，不 trace——恢复非故障）。
- tick 循环跑满 `WINK_BOOT_HEALTHY_TICKS`(200，≈2s) → 计数清零（证明已越过崩溃点，后续偶发故障不累积误锁）。

> 设计要点：safe-lock 无法在复位原因层面区分「真死循环」与「单次/测试复位」，故用「连续异常复位计数 + 健康里程碑」区分——真死循环的签名是每次启动都崩、永远跑不到稳定态。决策原文见 [ADR-0010](../../decisions/core/0010-boot-safe-lock-recovery-threshold.md)（修订 ADR-0007 硬约束 1）。

### 3.5 软定时器调度器（ADR-0007，已实现 + 真机验证）
ADR-0007 把原「预留分频调度」落地为**静态软定时器调度器**，打破单一 `app_loop` 10ms 强制同步轮询：组件注册不同频率的 tick 回调（1ms 电机 PID、20ms 超声波状态机、500ms 日志心跳），由调度器在 Tick 边界统一分发。
```c
typedef enum { WINK_TIMER_ONESHOT, WINK_TIMER_PERIODIC } wink_timer_mode_t;
typedef wink_status_t (*wink_soft_timer_callback_t)(void* arg);
wink_status_t wink_soft_timer_init(void);                              /* 须在 wink_runtime_run() 前调用 */
int32_t       wink_soft_timer_create(cb, arg, mode, period_ms);        /* 返回 handle，STOPPED 态，须 start() */
wink_status_t wink_soft_timer_start(int32_t handle);
wink_status_t wink_soft_timer_stop(int32_t handle);
void          wink_soft_timer_dispatch(void);                          /* wink_runtime_run 每 Tick 末尾调用 */
```
- **零动态分配**：`WINK_MAX_SOFT_TIMERS` 槽编译期静态分配；按创建顺序调度保证确定性。
- **Tick 对齐**：`period_ms` 必须为 `WINK_RUNTIME_TICK_MS` 整数倍，回调执行时刻落在 Tick 整数倍。
- **per-callback WCET**：每个回调独立计时，避免多速率任务在同一 Tick 槽重合执行造成 8002 虚警（ADR-0007 硬约束3）。
- **Tick 周期 SSOT**：`WINK_RUNTIME_TICK_MS` 由 `wink_app.json` 经 `wink-tools/tools/codegen/config_h.py` 生成（ADR-0007 硬约束4，host/wasm 与 ESP32 IDF 组件均接入）。
- ✅ ESP32 DevKitC 真机闭环验证通过（2026-06-28）。决策原文见 [ADR-0007](../../decisions/core/0007-cooperative-loop-execution-model.md)。

## 4. 多核物理隔离与跨核逃生舱通信（ADR-0007）

ADR-0007 针对多核 MCU（ESP32 双核）采取**系统级非对称物理隔离**，严禁将应用协程/定时器自动分发到多核并行（避免多线程锁竞态，破坏 Wasm 同源仿真对齐）：

| 核 | 职责 | 说明 |
|---|---|---|
| **Core 0（系统通信核）** | Wi-Fi / BT / lwIP 协议栈、系统后台 | 把网络中断抖动隔离在 Core 0，不干扰控制算法 |
| **Core 1（应用控制核）** | 独占运行 `wink_runtime_run` 协作式主循环（含全部软定时器/业务协程） | 周期确定性极高、超低抖动 |

- **钉核 API**：`pal_task_create(..., pal_core_id_t core_id, ...)`（`PAL_CORE_0` / `PAL_CORE_1` / `PAL_CORE_ANY`）；runtime 主循环钉 Core 1，仅 `PAL_CORE_ANY` 才用 `tskNO_AFFINITY`。
- **跨核逃生舱**：重型背景计算（如 AI 视觉）须通过 OSAL 创建 Core 0 后台任务，且**仅允许**用 OSAL 的非阻塞、线程安全环形缓冲通信：
```c
typedef struct pal_ringbuf* pal_ringbuf_handle_t;
pal_ringbuf_handle_t pal_ringbuf_create(uint32_t size);                                              /* size 须为 2 的幂 */
wink_status_t pal_ringbuf_push(pal_ringbuf_handle_t rb, const void* data, uint32_t size);            /* 非阻塞，满→WINK_ERR_FULL */
wink_status_t pal_ringbuf_pop(pal_ringbuf_handle_t rb, void* data, uint32_t size);                   /* 非阻塞，空→WINK_ERR_EMPTY */
```
  Core 1 仅在 Tick 边界无锁轮询该队列，**禁止任何跨核阻塞同步**。ESP32 实现基于 FreeRTOS `xRingbuffer*`（bytebuf）。
- **多系统适配（OSAL）**：裸机（忙等/优雅降级，`pal_task_create` 返回 `WINK_ERR_UNSUPPORTED`）/ RTOS（映射 `vTaskDelay` 等，运行时整体作为一个高优 OS 线程）/ Wasm（Asyncify 桥接 JS 定时器）——同一套 OSAL 契约三态自适应。
- ✅ Core 1 钉核隔离 + `pal_ringbuf` 跨核通信 ESP32 真机验证通过（2026-06-28）。

### 4.4 MPU/Linux 多核部署与并发隔离规约

在高级微处理器（MPU，如 ARM Cortex-A、x86）运行 Linux 的主板上，WinkOS 退化为用户空间（User Space）的一个轻量级控制沙箱进程。为了充分利用 Linux 多核算力并保证控制确定性，平台确立了以下两类部署与拓扑规范：

#### 4.4.1 单进程多线程拓扑 (In-Process Concurrency) — 设备级多任务
适用于单个复杂边缘设备（如带视觉识别的智能车）内部的并发：
*   **线程隔离**：WinkOS 主运行循环作为一个高优先级 POSIX 线程（`pthread`）运行并独占一个 CPU 核心（通过 `pthread_setaffinity_np` 钉核）。通过 `pal_os_task_create` 派生的后台重计算任务（如 OpenCV 图像处理）作为普通优先级的 Linux 线程，交由系统调度器自动分发到其他物理核心上并行运行。
*   **通信限制**：后台线程与主循环之间**严禁共享任何互斥锁（Mutex）**。所有数据交换必须通过 OSAL `pal_os_ringbuf`（在 Linux 平台上封装了互斥保护）进行单向无锁消息传递，彻底规避应用层竞态和死锁，维持与 ESP32 真机内存模型的 100% 同源对齐。

#### 4.4.2 多进程沙箱隔离拓扑 (Multi-Process Sandbox) — 系统级分布式
适用于多子系统协同（如工业流水线多设备联合控制）的并发：
*   **进程隔离**：在 Linux 上拉起多个独立的 WinkOS 二进制进程，每个进程内均运行一个极简、安全的单线程协作式 WinkOS 实体。各进程拥有完全独立的虚拟内存空间，实现物理级崩溃隔离（单个进程野指针崩盘不拖累其他控制进程）。
*   **跨进程 IPC**：进程间通过 Linux 系统级通信机制（如 Unix Domain Sockets、共享内存或本地 MQTT Broker）传递消息。网页端仿真时，每个进程会被实例化为一个独立的 Wasm 模块，在 3D 浏览器中实现高度解耦的分布式多机联合仿真。

---

## 5. 后置（roadmap）
- trace replay/compare/CI 回归（Golden Trace 对比真机）。
- `WINK_PT_*` 无栈协程宏（Protothreads 语法封装）：footgun 检查器 `wink-tools/tools/lint/check_pt_variables.py` 与调试头文件 `wink_pt_debug.h` 已就位，更多高级包装按需演进。
- 高速硬实时闭环（FOC / Fast PID）：协作式 + 软定时器不满足时，需任务硬实时化或调小 `wink_app.json` Tick。

