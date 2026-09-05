# Wink-AI 嵌入式（WinkMicroOS）· 架构设计补充评审报告

| 项 | 内容 |
|---|---|
| 评审日期 | 2026-06-24 |
| 评审对象 | `wink-micro-os/` 核心架构与长远演进（可移植性 / 安全性 / 仿真边界 / 对齐边界） |
| 评审基线 | [ADR-0001](../../decisions/core/0001-error-code-sign-convention.md) · [ADR-0002](../../decisions/unisim/0002-dual-target-compilation.md) · [ADR-0003](../../decisions/unisim/0003-simulation-fidelity-boundary.md) · [ADR-0004](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md) · [ADR-0005](../../decisions/core/0005-degraded-status-segment.md) |
| 评审视角 | 资深嵌入式架构师（高安全关键系统设计惯例 + 异构编译对齐验证） |
| 评审方法 | 异构可移植性审计 + 安全状态转移与故障收敛机制链条核对 |

---

## 一、 总体判断

本补充评审报告作为首份代码评审报告的补充与深化，结合架构设计讨论共识，侧重于系统在**长远演进、可移植性、安全健壮性以及 AI 自动代码生成的安全保障**方面的深层次架构死角。

目前框架在单任务协作调度、基本双模仿真上搭建了良好的基础，但由于缺乏硬件资源防冲突、高可靠的硬件级失控保护（看门狗/物理安全状态机）以及异构通信的数据对齐隔离，仍难以直接应用于真正的生产级或安全关键的嵌入式硬件中。需要在架构后续演进中补充这部分拼图。

---

## 二、 架构与设计补充发现清单

### 1. 驱动层与平台宏隔离不彻底（条件编译污染与契约细化）
* **位置**：[dal_ultrasonic.c](../../../../wink-micro-os/dal/src/sensor/dal_ultrasonic.c#L14-L35)
* **问题评估**：dal_ultrasonic.c 中的 `#ifdef SIMULATION` 条件宏已按照 [ADR-0003](../../decisions/unisim/0003-simulation-fidelity-boundary.md) 收窄到“trigger/echo 脉宽来源”，未污染整个 DAL。但在长远的多芯片 target（如 STM32/ESP32）并行演进中，仍然建议将物理脉宽读取剥离到 PAL。
* **架构隐患**：若直接提取 `pal_gpio_pulse_in` 接口，如果其阻塞、超时及中断安全契约不明确，极易把旁路范围无意扩大，破坏“换算与超时两端同源”原则。
* **整改方案**：
  在 [pal_hal.h](../../../../wink-micro-os/pal/include/hal/pal_hal.h) 中抽离出专用于底层物理层获取脉冲宽度的无副作用纯净接口：
  ```c
  wink_status_t pal_gpio_pulse_in(uint16_t pin, bool level, uint32_t timeout_us, uint32_t *pulse_us);
  ```
  - **契约约束**：该接口必须明确其为 `Blocking` 模式，规定最大超时参数契约，并且声明其是否为 `ISR-safe`（中断上下文安全）。
  - **平台实现**：Wasm 平台仅旁路该底层的物理量读取（直通 `wasm_bridge.h` 的 JS 导入）；ESP32 侧通过硬件或定时器捕获；DAL 层完全移除 `#ifdef SIMULATION` 条件分支。

---

### 2. 物理资源冲突与多维资源分配校验缺失（静态设备分配隐患）
* **位置**：[device_tree.c](../../../../wink-micro-app/determinism_fixture/device_tree.c)
* **问题评估**：目前系统仅对引脚引脚号做了 POD 静态声明，完全依赖上层 CodeGen 或人工配置的正确性。
* **架构隐患**：微控制器引脚复用复杂。仅仅在运行时校验 GPIO 占用是不够的，如果多个外设占用了同一个 PWM 硬件通道、硬件定时器（Timer）、I2C/UART 总线或 ADC 通道，依然会引发冲突甚至物理损坏。
* **整改方案**：
  - **CodeGen 静态拦截（核心）**：低代码平台生成端必须在输出 `device_tree.c` 之前，进行多维资源分配（GPIO, PWM channel, Timer, Bus Peripherals）的防冲突静态校验；
  - **多维 Debug 运行时守卫**：在 PAL 相关的初始化接口中，为 Debug 编译模式引入一个轻量级的全局资源占用表，覆盖引脚、通道、总线控制器等复用资源。一旦冲突，返回 [wink_status.h](../../../../wink-micro-os/pal/include/wink_status.h) 中的 `WINK_ERR_BUSY`（已被注册占用）或 `WINK_ERR_RESOURCE_EXHAUSTED`（资源耗尽），不使用无特定含义的 `WINK_ERR_PERMISSION`。

---

### 3. 看门狗（WDT）与失控安全状态（Fail-Safe State）的落地方案修正
* **位置**：[wink_runtime.c](../../../../wink-micro-os/runtime/src/wink_runtime.c#L12-L29)
* **问题评估**：仅在 `wink_runtime_run` 的协作式 Tick 尾部执行软喂狗是不足以保证复位前进入 Safe-State 的。当发生严重 CPU Fault（如 HardFault、总线死锁、硬卡死）时，看门狗超时发生为硬件级硬复位，CPU 根本无法执行软件安全函数。
* **架构隐患**：硬复位瞬间，MCU 引脚默认为高阻（Hi-Z）或默认弱拉状态，此时执行器处于失控高风险窗口。
* **整改方案**：
  - **物理级安全输出态（硬件根基）**：执行器的外部物理电路必须配置下拉/上拉电阻，保证 MCU 处于三态复位（Hi-Z）期间，硬件引脚电平默认使执行器处于安全关断状态；
  - **软件执行器注册表（Actuator Registry）**：运行时引入执行器注册机制，方便在可恢复的 Fault 降级处理中（如 BAL 执行 `on_fault`），能够遍历关闭/复位所有 Active 的物理设备；
  - **引导期复位原因分析（Boot Reason Check）**：系统启动时读取复位状态寄存器（如 ESP32 的复位源寄存器）。如果启动原因是由 WDT 或 Panic 引起的，**系统必须强制将所有执行机构维持在失能的安全状态**，直到通过外部确认，避免“启动-复位-启动”的无限死循环损坏硬件；
  - **独立硬件 Watchdog/Supervisor**：复杂物理 target 应使用外置的独立看门狗芯片监控 MCU，防范 MCU 内部看门狗随内核一同失效。

---

### 4. 内存对齐与跨架构可移植性风险修正（禁止普通结构 packed）
* **位置**：[dal_servo.h](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/dal/include/dal_servo.h) 及其他 DAL POD 结构体。
* **问题评估**：直接为普通运行时 DAL POD 结构体增加 `packed` 属性是极其危险的。在 ARM/Xtensa 等微控制器架构上，对未对齐的地址进行数据访问（Unaligned Access）不仅会导致严重的访存性能下降，甚至直接触发 **Alignment Fault (HardFault)** 导致系统挂起。
* **架构隐患**：异构平台（PC Host, Wasm32, ESP32）对结构体成员填充的对齐差异，仅在参数序列化传输或持久化保存时才会暴露。
* **整改方案**：
  - **自然对齐（Natural Alignment）原则**：运行时结构体成员按照“对齐大小降序”排列（指针/Float -> uint16 -> uint8），依靠编译器进行默认的自然对齐优化，**绝对禁止对运行时结构体使用 packed**；
  - **协议/持久化结构体完全隔离（Serialization Standard）**：涉及持久化（Flash/EEPROM 保存）或网络/串口传输的数据结构，单独定义对应的传输/存储结构体（Wire/Flash Structs），这些专用结构体可使用 `packed`、显式版本号、确定 Endianness 及 CRC 校验。系统通过专门的序列化/反序列化转换层（Serialize/Deserialize Boilerplate）与运行时结构体进行映射转换，**禁止直接 memcpy 运行时结构体**。

---

### 5. Wasm 沙箱与 JavaScript 交互的栈安全校验（Asyncify 优先级与保障）
* **位置**：[pal_osal_wasm.c](../../../../wink-micro-os/osal/wasm/pal_osal_wasm.c)
* **问题评估**：目前 Wasm target 无法正常挂起的最紧急痛点是 [CMakeLists.txt:33](../../../../wink-micro-os/CMakeLists.txt#L33) 中的 `ASYNCIFY_IMPORTS` 白名单还指向已经删除的 `js_sim_get_ultrasonic_distance`。
* **架构隐患**：由于真正的挂起调用点在 [pal_osal_wasm.c](../../../../wink-micro-os/osal/wasm/pal_osal_wasm.c#L9-L11) 的 `js_pal_delay_ms`，白名单配置错误导致 emcc 无法对此调用点插桩，主循环直接卡死。
* **整改方案**：
  - **Step 1：修复挂起白名单**：修改 `ASYNCIFY_IMPORTS` 以引入 `js_pal_delay_ms/us`，打通 Wasm 基础挂起执行链；
  - **Step 2：构建栈边界门禁**：在 CMake 构建 Wasm 时，强制开启 `-sSTACK_OVERFLOW_CHECK=2`、`-sASSERTIONS=1` 并合理划拨 `-sASYNCIFY_STACK_SIZE`（Asyncify 栈缓冲大小），提供可靠的沙箱保护门禁；
  - **Step 3：CI 深度栈回归**：在 CI 测试中加入深度调用链回归测试，专门验证在复杂回调链下 Asyncify 的栈展开与恢复稳定性。

---

## 三、 建议优先动作与演进规划（Roadmap）

结合工程实现约束，重构优先级规划调整如下：

| 优先级 | 任务 | 核心理由 | 预估工作量 |
|---|---|---|---|
| **P1** | **Wasm Asyncify 修复与栈门禁** | 解决 simulation 仿真端启动即挂起的核心阻塞点，是 Wasm target 能运行的前置条件 | 小 |
| **P1** | **WDT 与 Fail-Safe 重构（落地方案）** | 建立物理硬件拉低默认态、软件执行器注册表、启动复位源校验，打通核心物理安全闭环 | 中 |
| **P1** | **多维资源冲突校验** | 由 CodeGen 在编译前执行引脚/通道/定时器多维校验，防止烧毁物理芯片 | 中 |
| **P2** | **PAL pulse_in 契约化提取** | 剥离 DAL 层的仿真编译宏，完成平台彻底解耦，定义明确的超时与阻塞契约 | 小 |
| **P3** | **运行时与序列化结构体隔离** | 规范数据结构自然对齐，禁止普通结构 packed，建立独立序列化传输/持久化协议标准 | 小-中 |

