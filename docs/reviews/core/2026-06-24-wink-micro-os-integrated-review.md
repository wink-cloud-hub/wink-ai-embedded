# Wink-AI 嵌入式（WinkMicroOS）· 代码与架构综合评审报告

| 项 | 内容 |
|---|---|
| 评审日期 | 2026-06-24 |
| 评审对象 | `wink-micro-os/` 代码实现、PAL/DAL/runtime/trace/targets/samples、构建测试体系与长期架构演进边界 |
| 综合来源 | `2026-06-24-wink-micro-os-code-review.md` + `2026-06-24-wink-micro-os-supplemental-review.md` |
| 评审基线 | `.claude/skills/embedded-best-practice`、`.claude/rules/c-code.md`、ADR-0001/0002/0003/0004/0005 |
| 评审视角 | 资深嵌入式架构师：安全关键编码纪律、实时性、硬件资源安全、双 target 同源、AI CodeGen 可控性 |
| 综合评分 | **8.0 / 10**：静态分发与 host/仿真基础优秀；Wasm、真机实时性、资源治理与 Fail-Safe 仍需闭环 |

---

## 一、总体判断

当前代码在静态分发（POD + 命名 API + 编译期绑定）、负数错误码、BAL/PAL 分层、零动态分配、host 虚拟时间测试、仿真旁路收窄方面方向正确。超声波仿真已从旧的距离直通演进为 trigger/echo 脉宽来源旁路，距离换算同源，符合 ADR-0003。

但若目标是 ESP32/物理硬件生产级运行，仍不能直接视为安全基线。核心缺口集中在：Wasm Asyncify 挂起配置失效；超声波真机 busy-wait 破坏 10ms tick；DAL 缺显式 init 与资源冲突治理；WDT/Fail-Safe 缺硬件级闭环；PAL 错误语义不统一；trace 并发契约缺失；运行时结构体对齐与序列化边界尚未制度化。

---

## 二、主要亮点

| # | 亮点 | 位置/依据 |
|---|---|---|
| 1 | DAL 设备为 POD，无 vtable、`container_of`、`*_ops` 器件抽象 | `dal/include/*`，ADR-0004 |
| 2 | `wink_status_t` 已采用 `0=OK，负数=错误/降级`，并有 `wink_status_is_error()` 与 `WINK_WARN_UNUSED_RESULT` | `pal/include/wink_status.h` |
| 3 | App 基本只调 DAL 命名 API，不直接触碰 PAL/寄存器 | `samples/avoidance_car/app_main.c` |
| 4 | 仿真旁路粒度收窄到物理信号来源，换算逻辑同源 | `dal/src/dal_ultrasonic.c`、`test/test_dal_ultrasonic_sim.c` |
| 5 | host target 用虚拟时间推进，使部分真机 polling 分支可被 host 测试覆盖 | `targets/host/pal_hal_host.c` |
| 6 | trace/device_tree 静态存储，基本满足零动态分配纪律 | `trace/src/wink_trace.c`、`device_tree.c` |
| 7 | 测试构建使用 `-Wall -Wextra -Werror` 与 Unity | `test/CMakeLists.txt` |

---

## 三、综合发现清单

### P0-1. Wasm Asyncify 挂起配置失效，且缺栈安全门禁

**位置**：`wink-micro-os/CMakeLists.txt:33`、`targets/wasm/pal_osal_wasm.c`

`ASYNCIFY_IMPORTS` 仍指向已删除的 `js_sim_get_ultrasonic_distance`。代码已演进为 `js_sim_trigger_ultrasonic` + `js_sim_measure_echo_pulse_us`，真正需要挂起的是 `pal_delay_ms/us` 背后的 `js_pal_delay_ms/us`。当前配置可能导致浏览器主循环卡死、`Asyncify.StateError` 或栈恢复失败；host 测试无法覆盖。

**建议**：改为 `js_pal_delay_ms`、`js_pal_delay_us`，或按 Emscripten 最新方式实测自动分析；开启 `-sSTACK_OVERFLOW_CHECK=2`、`-sASSERTIONS=1`、合理设置 `-sASYNCIFY_STACK_SIZE`；CI 加 Wasm 构建与深调用链挂起/恢复测试；同步删除文档和 CMake 中的旧符号。

### P0-2. 超声波真机测距阻塞，破坏 10ms runtime tick

**位置**：`dal/src/dal_ultrasonic.c:38-59`、`runtime/include/wink_runtime.h`

真机分支两段 busy-wait 等 echo，上升沿与高电平宽度各 30ms 时最坏约 60ms，超过 10ms tick 六倍。BAL 在 `app_loop` 同步调用会饿死协作式调度、迟滞控制逻辑、缩小看门狗余量；host 虚拟时间会掩盖真实 wall-clock 阻塞。头文件若声明 `MAX 30ms`，也与实际最坏值不符。

**建议**：优先采用 PAL 硬件级脉宽捕获（GPIO 双沿中断、定时器捕获或 ESP32 RMT），DAL 对上提供非阻塞缓存读取/状态查询。若短期引入 `pal_gpio_pulse_in(pin, level, timeout_us, pulse_us)`，必须标明 Blocking、最大 timeout、ISR-safe、可重入性，并禁止从 BAL 10ms tick 直接调用。不要用 10ms tick 采样 HC-SR04 微秒级 echo。

### P0-3. DAL 缺显式 init，GPIO/PWM 生命周期与资源占用不受控

**位置**：`dal/include/dal_ultrasonic.h`、`dal/src/dal_ultrasonic.c`、`dal/src/dal_servo.c`、`samples/avoidance_car/device_tree.c`

超声波直接读写 trig/echo，但未见 `dal_ultrasonic_init` 配置 GPIO 方向；舵机每次 `set_angle` 可能重复 `pal_pwm_init`，缺一次性 init/health 状态。更大的隐患是 GPIO、PWM channel、Timer、I2C/UART bus、ADC channel 等多维 MCU 资源缺静态和运行期防冲突机制。

**建议**：补齐 `dal_xxx_init`、health、幂等初始化契约；`dal_servo_init` 一次性配置 PWM，`set_angle` 只设 duty；CodeGen 生成 `device_tree.c` 前做多维资源冲突校验；Debug PAL init 层加轻量资源占用表，冲突返回 `WINK_ERR_BUSY`，资源不足返回 `WINK_ERR_RESOURCE_EXHAUSTED`。若新增 `WINK_ERR_COLLISION`，需先更新 error model/ADR/设计规范。

### P0-4. WDT 与 Fail-Safe 缺硬件级安全闭环

**位置**：`runtime/src/wink_runtime.c`、未来 board/platform 支持层、执行器 DAL

仅在 tick 尾部喂狗或在可恢复 fault 中执行软件安全函数，无法覆盖 HardFault、总线死锁、CPU 卡死、WDT 硬复位。复位瞬间 MCU 引脚可能 Hi-Z 或弱拉，执行器存在失控窗口。

**建议**：板级硬件通过外部上拉/下拉、使能脚默认态或电源门控保证复位期间安全关断；runtime 引入 Actuator Registry，在 fault/panic/on_fault 中统一关闭执行器；启动阶段读取 reset reason，WDT/Panic 后保持执行器失能直到外部确认；复杂 target 使用独立 watchdog/supervisor。

### P1-1. PAL 失败型 API 返回 `bool`，丢错误语义

**位置**：`pal/include/pal_hal.h`、`pal/include/pal_osal.h`

`pal_gpio_init`、`pal_pwm_init`、`pal_i2c_transfer`、`pal_mutex_lock` 等返回 `bool`，导致 DAL 把所有失败折叠成 `WINK_ERR_IO`，丢失 `INVALID_ARG`、`BUSY`、`TIMEOUT` 等诊断信息，也增加 AI 生成判错逻辑不一致风险。

**建议**：分阶段迁移为 `wink_status_t`：先改硬件 IO/外设配置 API，再改 OSAL mutex；调用点统一用 `wink_status_is_error(status)` 或 `status < 0`。

### P1-2. 单行控制语句无大括号

**位置**：`dal/src/dal_servo.c`、`dal/src/dal_ultrasonic.c`

存在 `if (dev == NULL) return ...;`、`if (angle < 0.0f) angle = 0.0f;` 等写法，违反安全关键编码硬规则。

**建议**：机械补齐所有 `if/for/while/do-while` 大括号，并通过 clang-tidy/MISRA/CERT 子集或自定义 lint 建门禁。

### P1-3. `wink_trace` 并发契约未声明

**位置**：`trace/src/wink_trace.c`、`trace/include/wink_trace.h`

`s_count++`、`s_head` RMW 在当前单线程主循环下可接受，但公共 API 未声明 thread-safety。未来 fault 若从 ISR、工作线程、异步回调上报，会产生数据竞争；`volatile` 不能替代原子或临界区。

**建议**：短期声明 `Thread-safe: No，仅限 runtime 主循环单上下文调用`；若支持多上下文，使用关中断临界区或 PAL lock 保护写入路径，并定义 ISR-safe 版本。

### P1-4. `simulation.md` 与代码/构建 SSOT 漂移

**位置**：`.claude/skills/embedded-best-practice/references/static-dispatch/simulation.md`

文档仍展示旧 L1 直通 `js_sim_get_ultrasonic_distance`，代码与 `wasm_bridge.h` 已是 trigger/echo 两段式旁路；同一死符号残留在 CMake 造成 P0-1。

**建议**：更新文档为 `js_sim_trigger_ultrasonic` + `js_sim_measure_echo_pulse_us`，明确只旁路物理信号来源，换算、超时和错误处理同源。

### P1-5. 运行时结构体禁止 `packed`，序列化结构需隔离

**位置**：`dal/include/*.h` 及未来协议/持久化结构

普通 DAL POD 若使用 `packed`，在 ARM/Xtensa 上可能导致未对齐访问、性能下降甚至 Alignment Fault/HardFault。跨架构布局差异应只在传输/持久化层处理。

**建议**：运行时结构体自然对齐，成员按对齐需求大致降序排列；wire/flash 结构单独定义，可使用 `packed`、version、endianness、CRC，通过 serialize/deserialize 转换，禁止直接 `memcpy` 运行时结构体。

---

## 四、P2 跟踪项

| 编号 | 问题 | 建议 |
|---|---|---|
| P2-1 | `dal_servo_set_angle` 每次 `pal_pwm_init` | 并入 `dal_servo_init` 一次性完成 |
| P2-2 | App 对 servo 返回值存在 `(void)r` 吞错 | init/loop 中也应 trace fault 或进入降级安全态 |
| P2-3 | `180.0f`、`20.0f`、`0.0f` 等魔法数字 | 提取 `SERVO_MAX_ANGLE_DEG`、`SERVO_PERIOD_MS` 等常量 |
| P2-4 | Wasm 整数与函数指针 cast | 明确信任边界，理想方案用 Emscripten function table/addFunction 索引 |
| P2-5 | DAL 头文件契约字段不全 | 补 Preconditions、Blocking、ISR-safe、Thread-safe、Callback-context |
| P2-6 | ESP32 PAL 仍为 ROADMAP 骨架 | 移植前补真实 GPIO/PWM/I2C/OSAL/WDT/reset reason 实现 |

---

## 五、覆盖审计补充

以下细节来自两份源 review，虽已并入 P0/P1/P2，但为避免信息压缩造成遗漏，单独保留审计口径：

1. **Asyncify 死符号不一定导致链接失败**：`js_sim_get_ultrasonic_distance` 若未被 C 代码调用，可能被 DCE 移除，因此问题不是“必然链接失败”，而是真正挂起点 `js_pal_delay_ms/us` 未插桩，`wink_runtime_run(cb, 0)` 无限循环无法可靠让出。
2. **超声波阻塞与项目设计回答冲突**：当前 DAL 将阻塞传播到 BAL，违背 grilling Q5“PAL 阻塞必须由 DAL 封装为非阻塞 API”的结论，也违反零容忍阻塞原则。
3. **PAL pulse_in 的最终架构目标**：若抽 `pal_gpio_pulse_in`，Wasm 只能旁路该底层物理量读取，ESP32 通过硬件/定时器捕获；长期目标是让 DAL 完全移除 `#ifdef SIMULATION`，同时保持换算、超时与错误处理同源。
4. **Wasm 回调 cast 的具体风险**：`(pal_gpio_isr_t)(uintptr_t)callback_index` 在 wasm32 可工作，但 wasm64 有截断风险；`isr != NULL` 不能防错误非零索引。理想方案是使用 Emscripten function table / `addFunction` 管理真实函数索引。
5. **Fail-Safe 启动闭环**：WDT/Panic reset 后必须避免“启动-复位-启动”循环反复驱动执行器；外部确认或安全恢复条件满足前，执行机构应保持失能。
6. **资源冲突错误码口径**：已有错误码优先使用 `WINK_ERR_BUSY`（已被占用）与 `WINK_ERR_RESOURCE_EXHAUSTED`（资源不足），不应误用 `WINK_ERR_PERMISSION`。

---

## 六、建议优先路线图

| 优先级 | 任务 | 理由 |
|---|---|---|
| P0 | 修 Wasm Asyncify 与栈门禁 | Wasm target 能运行的前置条件 |
| P0 | 重构超声波为硬件捕获 + DAL 非阻塞语义 | 解决真机实时性与测距精度 |
| P0 | 补 DAL init 与多维资源冲突校验 | 防止未配置 GPIO、PWM 毛刺和资源冲突 |
| P0 | 建立 WDT/Fail-Safe/Actuator Registry/Boot Reason 闭环 | 物理安全底座 |
| P1 | PAL 失败型 API 分阶段迁移到 `wink_status_t` | 保留错误语义，提升 AI CodeGen 一致性 |
| P1 | 补大括号、trace 契约、simulation.md SSOT | 安全编码与文档真实度 |
| P2 | 对齐/序列化规范、BAL 错误对称性、Wasm 回调边界、ESP32 适配 | 长期可移植性与工程成熟度 |

---

## 七、分模块评分矩阵

| 模块 | 架构合规 | 规范符合 | 安全/实时 | 仿真同源 | 综合 |
|---|---|---|---|---|---|
| PAL（HAL+OSAL 契约） | 8.5 | 6.5（bool 丢语义） | 假锁/资源守卫待补 | — | 7.2 |
| DAL（servo/ultrasonic） | 8.5 | 7.0（缺 init/health） | 阻塞测距违背实时性 | 9.0 | 7.5 |
| runtime | 8.0 | 7.5 | WCET 受 DAL 阻塞放大 | — | 7.5 |
| trace | 8.0 | 7.0（并发契约缺失） | 单线程安全/多任务隐患 | — | 7.5 |
| targets/wasm | 8.0 | 6.0（Asyncify 失效） | 主循环挂起链路待修 | 7.0 | 6.5 |
| targets/host | 9.0 | 8.5 | 虚拟时间设计好但会掩盖 wall-clock 阻塞 | 9.0 | 8.8 |
| targets/esp32 | — | — | ROADMAP 骨架 | — | N/A |
| samples（App 示例） | 8.5 | 7.5（错误处理不对称） | 受 DAL 阻塞影响 | — | 8.0 |
| build/test | 8.0 | 8.5 | Wasm 路径缺实测 | 7.5 | 7.5 |

---

## 八、Safety Review

```text
Safety review:
- Risk level: 高（涉及 Wasm 挂起失效、真机实时性、硬件资源冲突、Fail-Safe 安全闭环）
- Checklist phases run: 1,2,3,4,7,8,9,10,11,12
- Findings:
  · P0: Asyncify 失效、ultrasonic 阻塞、DAL init/资源治理缺口、WDT/Fail-Safe 缺口
  · P1: PAL bool 返回、单行 if、trace 并发契约、simulation.md 漂移、packed/序列化边界
  · P2: servo 重复 init、BAL 吞错、魔法数、Wasm cast、契约字段、ESP32 骨架
- Fixed: 无（本次仅合并评审文档）
- Assumptions: esp32 target 仍为 ROADMAP；Wasm 精确行为需 emcc 环境验证；trace 当前仅单上下文调用
- Commands run: 仅文件读取/写入，未运行构建测试
```

---

## 九、整改跟踪表

| 发现 | 落地位置 | 状态 |
|---|---|---|
| P0-1 Asyncify 与栈门禁 | `wink-micro-os/CMakeLists.txt`、CI | 未开始 |
| P0-2 超声波非阻塞/硬件捕获 | `dal_ultrasonic.*`、`pal_hal.*` | 未开始 |
| P0-3 DAL init + 资源冲突校验 | `dal_*`、CodeGen、PAL Debug guard | 未开始 |
| P0-4 WDT/Fail-Safe | runtime、board、actuator DAL | 未开始 |
| P1-1 PAL status 化 | `pal_hal.h`、`pal_osal.h`、targets | 未开始 |
| P1-2 大括号门禁 | DAL C 文件、lint | 未开始 |
| P1-3 trace thread-safety | `wink_trace.h/.c` | 未开始 |
| P1-4 simulation.md 同步 | `simulation.md` | 未开始 |
| P1-5 对齐/序列化规范 | DAL headers、协议/持久化规范 | 未开始 |
| P2 项 | 散落多处 | 未开始 |

---

*本报告为 2026-06-24 时点综合评审快照。归档后按 reviews 约定只读；后续整改应回写至对应 ADR、设计规范和代码。*
