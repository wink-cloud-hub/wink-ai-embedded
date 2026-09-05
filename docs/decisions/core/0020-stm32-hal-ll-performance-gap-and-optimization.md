# ADR-0020：STM32 HAL/LL 性能开销分析与运行优化决策

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳）** |
| 日期 | 2026-07-04（提议），2026-07-04（采纳） |
| 触发 | 评估 WinkMicroOS 物理真机移植（STM32 硬件目标）与直接使用 stm32HAL/LL 库的性能差距 |
| 影响范围 | `runtime/`、`targets/stm32`（待移植）、构建配置、编码设计规范 |
| 决策者 | 架构委员会 & 用户 |
| 关联既有 ADR | [ADR-0004 静态分发](0004-static-dispatch-vs-runtime-ops.md), [ADR-0012 契约诚实](0012-contract-honesty-over-silent-degradation.md), [ADR-0016 双入口临界区](0016-pal-critical-section-task-isr-dual-entry.md), [ADR-0018 中断 API 收窄](0018-pal-irq-api-narrowing.md) |

---

## 背景（Context）

WinkMicroOS 作为面向 AI 低代码生成的跨平台嵌入式运行时，其核心设计理念是通过 PAL（平台抽象层）与 DAL（器件抽象层）的彻底解耦，达成 **PC Host 单元测试 - Web 浏览器 Wasm 仿真 - 物理微控制器真机** 的“三端同源运行”与“无锁线性协作执行”。

然而，这种高抽象度的“App -> DAL -> PAL -> 硬件驱动”调用链，与嵌入式开发者直接在硬件上编写 STM32 HAL 或 LL（Low-Level）代码相比，必然会引入额外的运行开销。在将运行时向 STM32 芯片移植的前夕，我们需要系统性地分析潜在的性能开销，并在架构层面明确优化策略与技术边界，以确保在低端 MCU 上的流畅度和确定性。

### 潜在开销来源分析

1. **函数跳转与包装（Function Wrap Overhead）**：
   - LL 库均通过 `static inline` 直接读写寄存器；HAL 库是普通 C 函数调用。
   - WinkMicroOS 通过 `pal_gpio_write` 包装了底层 HAL/LL 驱动，如果没有编译器内联，每次外设读写将多出一层压栈跳转，调用开销从几个 CPU 周期升至数十个周期。

2. **引脚与参数翻译（Pin Mapping & Translate）**：
   - 框架采用统一的 `wink_pin_t`（`int16_t`）进行表示，这在 STM32 适配层中必须翻译为对应的物理端口（`GPIO_TypeDef*`，如 `GPIOA`）和物理引脚号（`uint16_t`，如 `GPIO_PIN_5`）。
   - 这种翻译通常需要查表或 `switch-case` 条件分支，带来运行期 CPU 时钟开销。

3. **WCET 监控与高频 get_us() 耗时**：
   - 协作式内核在每次 `app_loop` 回调前后调用 `pal_os_get_us()` 来测量 WCET 耗时。
   - 在 STM32 上，SysTick（默认 1ms Tick）无法直接提供微秒级精度，必须通过公式 `(ms * 1000) + (LOAD - VAL) / CPU_FREQ` 实时读取硬件寄存器计算，或占用一个硬件定时器（1MHz 脉冲计数）。频繁读取会有明显计算耗时。

4. **中断路由与多核自旋锁（Interrupt Trampoline & SMP Locks）**：
   - 为了多核安全与调试断言（如 `pal_os_in_sim_isr_context`），中断服务例程被包装进统一的派发器。
   - 原本用于多核 ESP32 的自旋锁和原子屏障，在单核 STM32 上会导致无意义的指令开销。

### 平台对比与对齐：以 ESP32 适配层作为架构参照

在评估 STM32 的 HAL/LL 双重开发路线时，我们可以与已实现的 ESP32 目标（`targets/esp32/`）进行横向架构对比，以验证通用性设计：

1. **驱动层特性的差异**：
   * **ESP32**：乐鑫官方提供的 ESP-IDF 是高度整合的单一组件框架，其将寄存器级操作（LL 层）和高层 API（Driver 层）紧密封装在统一的驱动组件中。因此，ESP32 适配层天然只需要一套基于标准 ESP-IDF 的实现，无需像 STM32 那样面临二选一的物理或目录割裂。
   * **STM32**：ST 官方明确分裂为 HAL 库与 LL 库，极大地影响了 Flash/RAM 占用和执行效率，因此本 ADR 特别提出了基于 CMake 编译期门控的双模自适应架构。

2. **架构优化思想的一致性**：
   * 虽然库文件结构不同，但本 ADR 提出的核心性能消减技术（编译器 LTO 跨文件内联、WCET 运行期监控裁剪、引脚静态转换）在 ESP32 适配器中是完全一致的，并已被验证成功。

3. **多核（SMP）自旋锁与单核轻量化（CPSR 关中断）的差异化对齐**：
   * **ESP32**：属于双核物理架构（Core 0 处理通信，Core 1 执行 App），当进入临界区或处理中断分发时，必须依赖多核自旋锁（`portENTER_CRITICAL(&s_gpio_table_mux)`）和原子在途计数（`s_irq_in_flight`）进行跨核数据保护。
   * **STM32**：大部分微控制器属于单核物理架构。在 STM32 目标下，临界区和中断同步应当直接消除任何跨核锁的额外损耗，退化为极轻量的内核单条指令关中断（`__disable_irq()`），实现对标 LL 库的零等待锁开销。

4. **中断管理职责的通用划分（NVIC 与外设 IRQ 闭环）**：
   * 在 ESP32 上的成功实践中，平台通用中断管理（`pal_irq_esp32.c`）仅负责核心优先级路由；而具体的 GPIO 中断挂起标志位清除与业务派发，则完全闭环在 `pal_hal_esp32_gpio.c` 中。
   * 对齐到 STM32 上：NVIC 中断控制由 `pal_irq_stm32.c` 通用实现，直接调用 **ARM CMSIS 原生标准 API**（如 `NVIC_EnableIRQ` / `NVIC_SetPriority`），这在 HAL/LL 模式下 100% 共享；而外设（如 EXTI 标志、I2C 事件）的中断清除则各自收敛在相应的外设适配文件（如 `pal_hal_stm32_gpio_ll.c`）中，确保核心中断文件无冗余分支。

---

## 方案比选（Options）

### 选项 A：不做特殊优化，维持一致的物理包装
* **做法**：STM32 Target 仅作为普通物理接口适配器。完全调用 STM32 原生 HAL 库，保留全套 WCET 运行时监控，引脚动态查表。
* **优点**：代码风格在各平台上高度一致，完全没有平台特化分支。
* **缺点**：性能开销被放大。在低端芯片（如 Cortex-M3）上跑 WCET 监控会拖慢主循环；GPIO 反转频率被限制在数十万次以内，不适合要求毫秒内高频采样的协议驱动。

### 选项 B：激进的底层破壁，允许应用层直接绕过抽象
* **做法**：允许 DAL 层或应用层直接 `#include "stm32f4xx_ll_gpio.h"` 操纵寄存器以获得极限性能。
* **优点**：性能与原生 LL 库完全一致，无任何跳转和翻译开销。
* **缺点**：彻底破坏了“双模仿真（Wasm）”与“三端同源”的设计初衷。AI Codegen 将无法在浏览器端对其进行仿真和行为轨迹校验，丧失了低代码平台的核心价值。

### 选项 C：分层优化策略（编译期内联 + 生产模式监控裁剪 + Fast-Path 逃生门）（推荐）
* **做法**：
  1. **构建期优化**：在物理真机编译 Release 版本时，强制开启编译器 **LTO**（Link-Time Optimization）。
  2. **监控宏裁减**：引入 `WINK_CFG_WCET_MONITOR` 配置宏，生产固件下设为 0，彻底从汇编层面裁剪掉 `app_loop` 中的 `get_us` 时间差计算。
  3. **引脚静态映射**：STM32 平台的 PAL 层使用静态只读 `const` 表管理引脚转换。
  4. **合规的 Fast-Path 逃生门**：在 DAL 层驱动内，如果对于时序有极端要求（如模拟单总线协议），在严格的 `TARGET` 宏保护下直接调用寄存器/LL 宏，但对外暴露的业务接口依然保持一致，确保仿真时走旁路插桩。
  5. **单核裁剪**：在单核单任务（或单核裸机）目标下，OSAL 临界区退化为简单的全局关中断指令（`__disable_irq` / `__enable_irq`），不引入任何 SMP 自旋锁开销。
* **优点**：在保持 Wasm 仿真保真度、架构解耦的同时，将真机上的运行开销压缩到接近原生 LL/HAL 的水平。

---

## 决策结论（Decision）

采纳 **选项 C**。我们将通过一整套**“编译期消除 + 生产模式裁剪 + 局部 Fast-Path 逃生”**的复合机制，来消减 WinkMicroOS 相比直接使用 HAL/LL 库带来的性能损耗。

---

## 落地规则

### 1. 生产构建强制启用 LTO (链接时间优化)
在 CMake 的真机构建脚本（如 `targets/stm32` 或整体 Release 配置）中，默认开启 LTO，将函数跳转开销压缩至 0：
```cmake
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE) # 启用 LTO
endif()
```
* **效果**：在编译优化下，`pal_gpio_write(...)` 中无复杂逻辑的分支会被直接内联（Inline）到 DAL 的汇编代码中，消除跨 Translation Unit (TU) 的函数跳转开销。

### 2. WCET 时间监控允许条件编译裁剪
在 [wink_runtime.c](../../../wink-micro-os/runtime/src/wink_runtime.c) 中，增加 `WINK_CFG_WCET_MONITOR` 宏：
```c
#ifndef WINK_CFG_WCET_MONITOR
#define WINK_CFG_WCET_MONITOR 1
#endif

static void wink_runtime_monitor_wcet_loop(void (*callback)(void), const char* name) {
    if (callback == NULL) return;

#if WINK_CFG_WCET_MONITOR
    uint64_t start_us = pal_os_get_us();
    callback();
    uint64_t elapsed_us = pal_os_get_us() - start_us;
    if (elapsed_us > (WINK_RUNTIME_TICK_MS * 1000U / 2U)) {
        wink_trace_fault(WINK_WARN_WCET_EXCEEDED);
    }
#else
    callback(); // 生产模式下无测量开销，直接调用
#endif
}
```
* **效果**：关闭此宏后，主循环开销退化到与 Super Loop 相同的裸 C 函数调用指针级别。

### 3. 单核芯片 OSAL 临界区轻量化实现
对于单核无 RTOS（或单核 RTOS）芯片目标（如 STM32）：
* 严禁在中断与临界区使用 ESP32 多核自旋锁（`s_gpio_table_mux`）及 SMP 同步等待。
* 临界区进入/退出函数 `pal_os_critical_enter()` 直接退化为读取 CPSR 并屏蔽中断指令：
```c
// targets/stm32/pal_osal_stm32.c 伪代码
uint32_t pal_os_critical_enter(void) {
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
}
void pal_os_critical_exit(uint32_t key) {
    __set_PRIMASK(key);
}
```
* **效果**：临界区切换耗时由数百个周期降至个位数 CPU 周期。

### 4. DAL 器件层的“Fast-Path 逃生门”规范
如果某一器件（例如模拟 DHT11 单总线读写，对微秒级时序敏感）在真机下由于 PAL 接口带来时序误差：
* **禁止**：禁止在 App 层直接写硬件操作代码。
* **允许**：允许在 DAL 的 `.c` 文件内部，使用针对具体芯片的宏门控直接嵌入 LL 库调用，但必须在 Wasm 平台提供等价的虚拟直通旁路（Bypass）：
```c
// dal_dht11.c
wink_status_t dal_dht11_read(float *out_temp) {
#if defined(WINK_TARGET_STM32) && defined(DHT11_USE_FAST_PATH)
    // 真机物理 Fast-Path：直接通过 STM32 LL 库微秒级操作引脚，逃脱 PAL 层性能损耗
    LL_GPIO_SetOutputPin(GPIOA, LL_GPIO_PIN_5);
    // ...
#elif defined(SIMULATION)
    // 仿真端旁路：直接从 Wasm 物理仿真通道直接获取模拟数据，无需引脚级通信
    *out_temp = sim_get_virtual_sensor_dht11();
    return WINK_OK;
#else
    // 默认 PAL 兼容通道
    pal_gpio_write(DHT11_PIN, true);
    // ...
#endif
}
```

### 5. 【下一阶段实现】基于 CMake 门控的 STM32 HAL/LL 双模适配器架构
为了兼顾开发便利度（HAL 库的高层 API 便于快速跑通外设）与极限运行性能（LL 库对标裸寄存器），在下一阶段（STM32 真机物理移植与测试波次）中，我们将实施 **CMake 编译期驱动选型路由**。

#### (1) 目录与文件名规范
不建立物理上隔离的 `targets/stm32LL` 目录，以避免代码割裂。在统一的 `targets/stm32/` 目录下放置两套物理驱动实现，由 CMake 进行条件编译路由：
* `pal_hal_stm32_gpio_hal.c`：基于标准 HAL 库的实现，便于调试与普适外设。
* `pal_hal_stm32_gpio_ll.c`：基于 LL 库的实现，用于对标极限性能。

#### (2) CMakeLists 门控逻辑
通过引入 `-DWINK_STM32_DRIVER_TYPE` 编译选项（支持 `HAL` / `LL`，默认 `HAL`），由构建系统自动编排底层源文件链路：
```cmake
# targets/stm32/CMakeLists.txt 伪代码
set(WINK_STM32_DRIVER_TYPE "HAL" CACHE STRING "STM32 Driver backend: HAL, LL")

if(WINK_STM32_DRIVER_TYPE STREQUAL "LL")
    list(APPEND PAL_STM32_SOURCES 
        pal_hal_stm32_gpio_ll.c
        pal_hal_stm32_i2c_ll.c
    )
    add_compile_definitions(USE_FULL_LL_DRIVER) # 引入 STM32 官方 LL 库宏
else()
    list(APPEND PAL_STM32_SOURCES 
        pal_hal_stm32_gpio_hal.c
        pal_hal_stm32_i2c_hal.c
    )
endif()
```

#### (3) 业务层 100% 零修改保障
此机制确保了：
1. **上层应用（User App）与器件层（DAL）代码 100% 零修改**。其只依赖中立的 `pal_hal.h` 接口契约。
2. 对标 LL 性能的底层替换完全局限在 `targets/stm32` 内部，通过 CMake 切换即可一键重编。

---

## 后续影响与评估（Consequences）

### 优点
1. **彻底消除生产模式隐患**：通过去除高频读取 `get_us()` 的监控开销，打消了用户对框架吞噬算力的顾虑。
2. **极速的临界区响应**：单核架构下的指令级退化，保证了 STM32 目标上的硬件实时性。
3. **架构的灵活性**：DHT11、DS18B20 等“时序地狱”级器件在 DAL 层获得了底层直连的物理合法性，且不破坏 Wasm 三维物理引擎的仿真能力。

### 缺点 / 妥协
1. **Release 版故障追溯力变弱**：关闭 WCET 监控后，App 假死、死循环等行为将仅能依靠看门狗（WDT）硬复位捕获，无法记录 `WINK_WARN_WCET_EXCEEDED` 软故障码。
2. **调试难度增加**：开启 LTO 后，GDB 单步调试的符号合并严重，需要在 Debug 构建（不开启 LTO，开启监控）下进行硬件行为联调。
