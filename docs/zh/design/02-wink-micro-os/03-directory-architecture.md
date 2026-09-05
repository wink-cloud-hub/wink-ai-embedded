# 3.3 WinkMicroOS 内核目录架构设计

> 本文件是 **WinkMicroOS 内核（`wink-micro-os/`）目录布局的权威活规范**。任何对内核目录的增删/重构必须先读本文件，并保持本文件与代码同步（SSOT 闭环，遵循 [docs-adr.md §2](../../../.claude/rules/docs-adr.md)）。

---

## 🎯 入门：各目录角色速查表

（小白入门先看这张表，3 秒理解整个项目分工）

| 目录 | 角色 | 维护者 | 多久改一次 | 一句话解释 |
|------|------|--------|-----------|----------|
| `wink-micro-os/` | SDK 平台内核 | 库开发者 | 加新硬件、修驱动 bug 时 | 操作系统核心代码：PAL/DAL/BAL/runtime/trace，所有芯片通用 |
| `wink-micro-app/` | 业务应用代码 | 用户开发者 | 每次写新功能 | 你的业务逻辑：避障小车、OLED 仪表盘等 |
| `esp32_firmware/` | ESP32 编译打包器 | 双方都很少改 | 改芯片配置、编译参数时 | 把 wink-micro-os 编译成能烧到 ESP32 的 .bin 固件 |
| `@wink-ai/unisim` | 浏览器仿真引擎 | 双方都很少改 | 改仿真逻辑时 | `wink-ai/packages/unisim/`，把 wink-micro-os 编译为在浏览器跑的 .wasm |
| `test/` | 单元测试 | 库开发者 | 加新功能补测试时 | PC 上跑自动化测试，验证逻辑正确 |

---

## 1. 设计定位

**WinkMicroOS = 一个「轻量协作式运行时/应用框架（OS Runtime/Framework）」**；其上层有两类消费者：

- **App（顶层）**：由硬件连线 + 状态机逻辑组成，**AI 生成**，每项目一次性，注入式。
- **BAL（工具层）**：包含物理增强 (`input`/`output`/`sensor`/`actuator`/`display`/`comm`)、`math` 纯算法、`control` 闭环编排三域，**为 wink-micro-os 内核静态组件库** (`wink-micro-os/bal/`)。

二者都是 wink-micro-os 的"上层用户程序"，只 link 本内核的**公共 API 面**（见 §6）。wink-micro-os 自身拥有：**PAL + DAL + BAL + runtime + trace + targets + test**。

> 💡 **架构注**：本运行时为协作式事件循环（Cooperative Event Loop），不包含传统微内核（Microkernel）的地址空间隔离（MMU/MPU）或进程间通信（IPC）机制。文档中的 "OS/内核" 统指该轻量级嵌入式运行时骨架。

## 2. 五层模型与仓库边界

| 层 | 职责 | 归属 | 依赖方向 |
|---|---|---|---|
| **App（顶层）** | 硬件连线 + 状态机，AI 生成 | 外部（生成/注入） | ↓ 调 BAL + DAL |
| **BAL（工具层）** | 物理增强 + 算法 + 闭环编排（物理增强/math/control三域） | **wink-micro-os** (`bal/`) | ↓ 调 DAL |
| **DAL** | 外设最小单元（舵机、超声波…） | **wink-micro-os** | ↓ 调 PAL |
| **PAL** | HAL（屏蔽芯片差异、集成各家 HAL）+ OSAL（类 OS API，FreeRTOS 等） | **wink-micro-os** | ↓ 调 target 实现 |
| **runtime / trace** | app 生命周期 + 调度主循环 / Golden Trace | **wink-micro-os** | runtime↓调 trace、PAL |
| **Targets** | 各平台 PAL/桥接/入口实现 | **wink-micro-os** | 绑定芯片 |

> **关键推论**：wink-micro-os 的 `include/` 不仅是内部组织，更是**跨模块稳定契约**——上层生成的 App 只能 link 此公共面。故"公共头"与"内部实现/端口"的边界必须划得极干净。

## 3. 架构原则（决策依据）

本目录布局由以下软件架构原则裁决，每条都有出处：

| 原则 | 在本设计的体现 |
|---|---|
| **Ports & Adapters（六边形）** | `targets/` = 适配器（PAL/runtime 的端口）；`pal/dal/runtime/trace` = 领域核心。host 与 esp32 同为适配器，故 host 升一等 target。 |
| **Stable Dependencies Principle** | 依赖自下而上单调趋稳：`pal`(INTERFACE，最稳) ← `dal` ← `runtime` ← App/BAL(最易变)。 |
| **Screaming Architecture** | `trace/` 独立顶层 peer 层——因为 [`01-system-overview §3`](../01-system-overall/01-system-overview.md) 已把 Trace System 列为与 BAL/DAL/PAL 平级的层。折进 runtime 会制造与领域模型相悖的心智模型。 |
| **架构边界 = 按变化速率/团队切分（Conway）** | codegen 模板/工具属构建/前端系统（06-build/03-bal/07-registry），变化周期与内核无关，**不与内核同居**（仅留 `samples/` 示例）。 |
| **YAGNI + Rule of Three** | PAL `include/` 暂保持扁平（3 头临界可接受），不预建 `hal/`/`osal/` 子目录；第 4 总线到来再拆 flat per-bus 头（规则见 §8）。 |

## 4. 完整目录树（带库类型标注）

```
wink-micro-os/
├── CMakeLists.txt              # 顶层：TARGET_PLATFORM 路由 · 层库聚合 · 选 target · 注入 app
├── README.md
│
├── pal/                        # 【libpal = INTERFACE 库】纯契约，无 .c
│   ├── CMakeLists.txt          #   target_include_directories(PUBLIC include include/hal include/osal)
│   └── include/                #   ★硬件契约面（内核内部；BAL/App 禁入 pal_hal/pal_osal）
│       ├── pal.h               #   聚合头（include hal+osal+services+status）
│       ├── wink_status.h       #   统一错误码（SSOT←07-platform-governance/02 §2）—基础类型例外，全层可见
│       ├── pal_log.h           #   分级日志（E/W/I/D、LOG_TAG 隐式注入、ISR 安全分流）
│       ├── pal_resource.h      #   资源占用治理服务（GPIO/PWM/I2C 冲突检测）
│       ├── pal_storage.h       #   非易失存储服务（NVS/内存键值对）
│       ├── pal_irq.h           #   中断抽象核心（统一优先级、临界区）
│       ├── pal_irq_advanced.h  #   中断高级特性（可选，同步/共享中断等—ADR-0018 门控）
│       ├── hal/                #   硬件抽象层子目录（物理总线基元）
│       │   ├── pal_hal.h       #   HAL 基元 API（GPIO/PWM/I2C/pulse_in）
│       │   ├── pal_rmt.h       #   ESP32 RMT 扩展（脉冲捕获，DAL超声波内部使用）
│       │   └── pal_pwm_router.h # PWM 引脚路由配置
│       └── osal/               #   OS 抽象层子目录
│           └── pal_osal.h      #   OSAL API（delay/tick/mutex/task）
│
├── dal/                        # 【libdal.a = STATIC 库】可预编译，两端同源
│   ├── CMakeLists.txt          #   link: pal(INTERFACE)；pal_* 符号留待 target 终链解析
│   ├── include/                #   ★公共 API 面（BAL/App 可 link）
│   │   ├── sensor/
│   │   │   └── dal_ultrasonic.h
│   │   ├── actuator/
│   │   │   └── dal_rc_servo.h
│   │   ├── input/
│   │   │   └── dal_button.h
│   │   ├── output/
│   │   │   └── dal_led.h
│   │   ├── display/
│   │   │   └── dal_ssd1306.h
│   │   ├── comm/
│   │   │   └── dal_gps.h
│   │   └── storage/
│   │       └── dal_eeprom.h
│   └── src/                    #   #ifdef SIMULATION 仅旁路最底层物理量（ADR-0003 决策2）
│       ├── sensor/dal_ultrasonic.c
│       ├── actuator/dal_rc_servo.c
│       ├── input/dal_button.c
│       ├── output/dal_led.c
│       ├── display/dal_ssd1306.c
│       ├── comm/dal_gps.c
│       └── storage/dal_eeprom.c
│
├── runtime/                    # 【libwink_runtime.a = STATIC 库】★OS 运行时一等层
│   ├── CMakeLists.txt          #   link: pal；调 app_* 钩子(外部) + wink_trace_*(trace 库)
│   ├── include/                #   ★公共 API 面（App/BAL 消费）
│   │   ├── wink_app.h          #   wink_app_callbacks_t 回调结构体定义 + wink_app_delay_ms
│   │   └── wink_runtime.h      #   wink_runtime_run(callbacks) —— OS 主循环入口（target entry 调它）
│   └── src/
│       └── wink_runtime.c      #   tick 调度：执行 init 回调一次 → while(1){loop 回调; wink_app_delay_ms}（MVP 单任务）
│
├── trace/                      # 【libwink_trace.a = STATIC 库】★Golden Trace 一等 peer 层
│   ├── CMakeLists.txt          #   link: pal；被 runtime/dal/app 消费（横切基础服务）
│   ├── include/
│   │   └── wink_trace.h        #   wink_trace_fault() / 事件记录 API
│   └── src/
│       └── wink_trace.c        #   ring buffer + fault 记录（MVP 极简；replay/compare 后置）
│
├── osal/                       # 【NEW】操作系统/运行环境适配层（OS 维度）
│   ├── CMakeLists.txt          # OSAL 源文件与包含路径 SSOT
│   ├── common/
│   │   └── pal_osal_ringbuf.c  # OSAL 共享环形缓冲实现
│   ├── baremetal/
│   │   └── pal_osal_baremetal.c # 裸机 OSAL 适配（关中断临界区）
│   ├── freertos_esp32/
│   │   └── pal_osal_freertos_esp32.c # ESP-IDF FreeRTOS 适配
│   ├── wasm/
│   │   ├── pal_osal_wasm.c     # Wasm 浏览器沙箱 OSAL（Asyncify 虚拟时钟）
│   │   └── sim_ctx_emscripten_fiber.c
│   └── host/
│       ├── pal_osal_host.c     # Host 仿真 OSAL（Windows Fiber 调度）
│       └── sim_ctx_win32_fiber.c
│
├── targets/                    # 【REFAC】硬件/物理适配层（物理芯片维度，仅含 HAL 与驱动）
│   ├── wasm/                   #   浏览器仿真外设端口
│   │   ├── pal_hal_wasm.c      #   HAL 聚合（GPIO/PWM/I2C/pulse_in/debug_printf）
│   │   ├── pal_irq_wasm.c      #   IRQ 适配（虚拟中断 + pending 队列）
│   │   ├── pal_storage_wasm.c  #   存储适配（localStorage/内存持久）
│   │   ├── pal_wasm_fault.c / pal_wasm_fault_domain.c / pal_wasm_physical.c  # wasm 独占子系统
│   │   ├── pal_wasm_internal.h #   wasm target-private 头
│   │   ├── wasm_bridge.h       #   ★SSOT：所有 js_pal_* + js_sim_* extern 声明集中（修漂移）
│   │   ├── wasm_entry.c        #   main() + trigger_wasm_interrupt
│   │   ├── wink_sim_js.js / wink_sim_stub.js  # JS 库（默认实现 / 桩）
│   │   └── CMakeLists.txt
│   ├── esp32/                  #   真机外设端口（ESP-IDF 组件模式）
│   │   ├── pal_log_esp32.c            #   分级日志后端（esp_log_writev + ISR ROM 通路）
│   │   ├── pal_hal_gpio_esp32.c       #   HAL GPIO 外设
│   │   ├── pal_hal_i2c_esp32.c        #   HAL I2C（v5/v6 双 API 兼容）
│   │   ├── pal_hal_pwm_esp32.c        #   HAL PWM (LEDC)
│   │   ├── pal_hal_rmt_esp32.c        #   HAL RMT（超声波脉冲捕获）
│   │   ├── pal_irq_esp32.c            #   中断适配（ISR/寄存器级）
│   │   ├── pal_hal_internal_esp32.h   #   HAL 跨 TU 私有头
│   │   ├── pal_atomic_esp32.h         #   SMP 原子/屏障辅助
│   │   ├── pal_resource_esp32.c       #   资源冲突检测实现
│   │   ├── pal_storage_esp32.c        #   NVS/Flash 存储实现
│   │   ├── esp32_entry.c              # app_main() 入口
│   │   └── CMakeLists.txt
│   ├── host/                   #   ★一等 target（host 测试/PC 仿真外设）
│   │   ├── pal_hal_host.c      #   HAL 聚合（协作式虚拟时间 + pwm 记录）
│   │   ├── pal_storage_host.c  #   存储适配（内存 stub）
│   │   ├── pal_log_host.c      #   物理日志实现
│   │   └── CMakeLists.txt
│   └── common/                 #   跨 target 共享仿真与算法基础设施（无平台 tag）
│       ├── include/sim_ctx.h / include/wink_sim_physical.h / include/wink_sim_scheduler.h
│       └── src/pal_resource.c / src/wink_sim_physical.c / src/wink_sim_scheduler.c
│                               #   wink_sim_physical：物理退化 + 电机/转子 plant（ADR-0009；plant 方程禁止进 dal/src SIMULATION）
│
├── frameworks/                 # 【仿真拦截层 / 外国生态兼容，Axis B；仅 host/wasm 编译】
│   └── mcs51/                  #   8051/Keil C51 零侵入仿真（ADR-0070；ESP_PLATFORM 下 return()，真机零增量）
│       ├── include/            #   REGX52.H / REG_CMS8S.H / cms8s78xx.h / mcs51_adc.h / ADC0832.H / mcs51_trap.h / absacc.h …
│       ├── src/                #   mcs51_bridge.cpp（codegen 缝 + init）、mcs51_sfr.cpp、mcs51_adc0832.cpp、cms8s_adc.cpp …
│       ├── tools/mcs51_cleanup.py  # Keil .c → 构建树 .cpp（ISR 重写、UTF-8/GBK、--transcode）
│       └── CMakeLists.txt      #   wink_mcs51_compat 静态库（EXCLUDE_FROM_ALL，test 链接）
│
├── test/                       # host 测试（PC gcc + Unity）；link pal+dal+runtime+trace+targets/host
│   ├── CMakeLists.txt
│   ├── mcs51/                  #   8051 拦截层测试：samples/（未修改 Keil 源码）、unit/、wasm/、apps/iron_ntc/wink-app.json
│   ├── unity/                  #   vendor（from chigo-micro）
│   ├── stubs/                  #   仅留测试专用注入控制 API + js_sim 桩
│   │   ├── host_test_ctrl.{c,h}    #   sim_set_echo_timing / sim_last_pwm_duty（驱动 targets/host）
│   │   └── js_sim_host_stub.{c,h}  #   仿真分支(-DSIMULATION) js_sim_* 桩
│   └── test_*.c
│
└── samples/                    # 示例 App（注入点演示，非内核代码）
    └── avoidance_car/          #   app_callbacks.c + device_tree.{c,h} + board_config.c + wink_app.json
```

## 4.1 示例/注入点应用（App）目录结构与各文件职责

在 `samples/`（或用户导出的 App 项目工程）下，每一个具体应用目录（如 `samples/avoidance_car/`）都包含以下核心文件。它们各自的角色分工与生成来源如下：

| 文件名 | 职责角色 | 含义与作用 | 生成/维护来源 |
| :--- | :--- | :--- | :--- |
| **`wink_app.json`** | 应用描述清单 | **应用元数据唯一真理源 (SSOT)**。<br>定义应用的核心参数（如 `tick_ms`、`target_board`）和外设物理引脚配置。 | 💻 低代码前端拖拽导出 |
| **`device_tree.h` / `device_tree.c`** | 静态设备树 | **实现“零动态内存分配”的核心**。<br>根据清单自动声明和实例化静态全局外设 POD 结构体变量，防止运行时 `malloc` 引起的内存碎片与溢出风险。 | ⚙️ Wink 编译器静态 Codegen 生成 |
| **`app_callbacks.c`** | 业务逻辑与协程 | **应用层业务逻辑核心载体**。<br>实现 `init()` 初始化、包含协作式无栈协程 (`WINK_PT_*`) 线性逻辑的 `loop()` 循环、以及故障状态回调 `on_fault()`。 | 🤖 AI (LLM) 自动生成 |
| **`board_config.c`** | 开发板引脚路由 | **开发板物理引脚强覆盖映射（可选）**。<br>为特定硬件板卡提供底层引脚路由的强定义（如覆盖 `pal_pwm_pin_map`、`pal_i2c_pin_map`），解耦通用外设与具体硬件的映射。 | 🛠️ 板卡级固件包/系统工程师提供 |
| **`CMakeLists.txt`** | 构建描述文件 | **定义应用的编译与链接规则**。<br>指示 CMake 调用 Python 脚本解析 `wink_app.json` 并生成 `wink_config.h`（注入 Tick 配置），然后将应用代码与微内核运行时链接。 | 📋 构建系统静态模板 |

### 🛠️ 应用层与底层平台的解耦原理 (Compile-time Target Static Routing)
Wink-AI 保证应用层代码在物理平台（裸机、各类 RTOS、Wasm）之间 **100% 同源且零感知编译**：
1.  **代码零感知**：应用层逻辑（`app_callbacks.c`）绝不包含任何平台的 `#ifdef` 条件宏，仅面向统一的 `pal_osal.h` 和 `dal` 公共 API。
2.  **构建期静态分发**：当编译某个应用时，CMake 解析该应用下的 `wink_app.json` 确定 `target_board`（例如 `esp32`）。构建系统按 `TARGET_PLATFORM` × `WINK_OSAL_TYPE` 正交装配：HAL 来自 `targets/<plat>/`，OSAL 来自 `osal/<variant>/`（例如 `esp32` × `freertos_esp32` → `osal/freertos_esp32/pal_osal_freertos_esp32.c`，见 [ADR-0041](../../decisions/core/0041-hal-osal-directory-orthogonality.md)）。

## 4.2 Target 目录文件命名规范（活规范）

> 本节是所有 `targets/<plat>/` 目录下源文件/头文件命名的**单一事实来源**。新 target 接入、target 内文件拆分/新增时必须遵守；AI 生成代码/重构改名时须以本节为准。

### 🎯 核心公式

#### 1. 平台实现源文件命名

```
pal_<domain>[_<detail>]_<plat>.c/h
```

| 段 | 含义 | 取值举例 |
|---|---|---|
| `pal_` | 固定前缀（PAL 命名空间） | 全部以 `pal_` 开头，grep 可一网打尽，避免和第三方/RTOS 符号冲突 |
| `<domain>` | PAL 子系统 | `hal`（硬件总线基元）、`osal`（OS 抽象）、`irq`（中断）、`resource`（资源表）、`storage`（存储）、`atomic`（原子操作）、`rmt`（RMT 脉冲捕获，ESP32 专用 HAL 扩展）… |
| `[_<detail>]` | 可选细分类（外设/子模块） | 当一个 domain 拆成多个 TU 时，用 detail 区分，如 `gpio`、`i2c`、`pwm`、`internal`（跨 TU 私有头） |
| `<plat>` | 平台标签（最后一段，永远在末尾） | 与目录名一致：`esp32`、`wasm`、`host`、`baremetal`（不是 `bare`） |

> **分层边界**：具体传感器/器件（如超声波 ultrasonic、舵机 servo）属于 **DAL 层**，不是 PAL 层。PAL 只提供硬件基元（GPIO/PWM/I2C/pulse_in/RMT），DAL 基于基元封装出语义级器件 API。因此 PAL 没有 `pal_ultrasonic_*`，也不应有 `pal_ultrasonic_esp32.c`——这类器件实现应属于 DAL。

#### 2. PAL API 函数前缀命名（无冗余层级）

```
pal_<domain>_<action>()
```

**规则**：函数前缀直接使用 domain 名，**不重复中间层级**（不使用 `pal_hal_gpio_*` 双重前缀）。

| API 组 | 函数前缀 | 示例 |
|--------|---------|------|
| HAL 基元（GPIO/PWM/I2C/pulse_in） | `pal_gpio_*` / `pal_pwm_*` / `pal_i2c_*` | `pal_gpio_init()`, `pal_pwm_set_duty()`, `pal_gpio_pulse_in()` |
| HAL 平台专用扩展（RMT） | `pal_rmt_*` | `pal_rmt_ultrasonic_measure()`（仅 ESP32，DAL 内部调用） |
| OSAL | `pal_os_*` | `pal_os_sleep_ms()`, `pal_os_busy_wait_us()` |
| 系统服务 | `pal_storage_*` / `pal_resource_*` / `pal_debug_*` | `pal_storage_read()`, `pal_resource_claim()` |
| 中断抽象 | `pal_irq_*` | `pal_irq_lock()`, `pal_gpio_enable_interrupt()` |

**设计理由**：
- 扁平前缀简洁一致，避免 `pal_hal_xxx_` 冗余层级增加记忆负担
- 与 Zephyr/ESP-IDF/RT-Thread 等主流 RTOS 的扁平前缀惯例对齐
- grep 友好：所有 PAL 符号 `^pal_` 一网打尽

**设计理由（源文件命名）**：
1. **平台 tag 固定在末段**——对齐 Zephyr RTOS / Linux kernel 的 `drivers/<class>/<class>_<plat>.c` 主流惯例，嵌入式工程师跨项目可秒懂
2. **目录按平台隔离 + 文件名按外设聚类 = 互补**：`ls targets/esp32/` 天然按平台聚类，Ctrl+P 模糊打开 `pal_hal_gpio` 时 `pal_hal_gpio_esp32.c` 和 `pal_hal_gpio_wasm.c` 并排出现，直接看出"这个外设哪些平台实现了"
3. **可正则解析**：`^pal_([^_]+)(?:_(.+))?_([^_]+)\.c$` → group1=domain, group2=detail(optional), group3=plat，代码脚手架/生成器可稳定解析
4. **演进友好**：从单文件（`pal_hal_esp32.c`）拆出细分外设时只需追加 `_<detail>` 段，不需要重排 tag 位置

### 📋 分类命名矩阵

以当前各 target 为例（SSOT 参考）：

| 分类 | 规则 | esp32 示例 | wasm 示例 | host 示例 |
|---|---|---|---|---|
| **domain 聚合主文件**（domain 层不拆 TU 时，或承载残留公共逻辑） | `pal_<domain>_<plat>.c` | `pal_irq_esp32.c`、`pal_resource_esp32.c`、`pal_storage_esp32.c`、`pal_log_esp32.c` | `pal_hal_wasm.c`、`pal_irq_wasm.c`、`pal_storage_wasm.c`、`pal_log_wasm.c` | `pal_hal_host.c`、`pal_storage_host.c`、`pal_log_host.c` |
| **外设/子模块拆分 TU**（domain 内按外设拆 file） | `pal_<domain>_<detail>_<plat>.c` | `pal_hal_gpio_esp32.c`、`pal_hal_i2c_esp32.c`、`pal_hal_pwm_esp32.c`、`pal_hal_rmt_esp32.c` | — | （host 暂不拆，RMT/pulse_in 等都聚合在 pal_hal_host.c 内） |
| **Target 私有头（跨 TU 共享）** | `pal_<domain>_<detail>_<plat>.h`（detail 用 `internal` 表示跨 TU 私有）<br>或 `pal_<domain>_<plat>.h`（单 TU 自洽的子模块） | `pal_hal_internal_esp32.h`（HAL 跨 TU 内部声明）、`pal_atomic_esp32.h`（atomic 子模块，被 gpio/irq 共享） | `pal_wasm_internal.h`（见下） | — |

### ⚠️ 特殊命名规则（非 PAL 模块）

下列文件**不属于** PAL 实现层，不走 `pal_<domain>_<plat>` 公式，各自遵循以下规则：

| 类别 | 命名模式 | 示例 | 理由 |
|---|---|---|---|
| **Target 启动入口** | `<plat>_entry.c`（无 `pal_` 前缀，平台开头） | `esp32_entry.c`、`wasm_entry.c` | entry 文件包含 `main()/app_main()`，不实现任何 PAL 契约，是启动粘合代码而非 PAL 模块；和 pal_* 聚组分开避免误导。host 测试的 `main()` 由测试二进制提供，所以 host 无 entry 文件 |
| **平台独占子系统**（无跨平台公共 PAL 契约，只在某一个 target 存在） | `pal_<plat>_<detail>.c/h`（plat 是 domain 本身） | wasm：`pal_wasm_fault.c`、`pal_wasm_fault_domain.c`、`pal_wasm_physical.c`、`pal_wasm_internal.h` | 对这类文件，"wasm" 不是平台 tag 而是 **domain 本身**（这些能力只在 wasm 仿真里存在：fault latch、物理退化仿真等），所以没有第三个 plat 段。判别法："如果删除这个 target，这堆文件的功能还存在于其他 target 吗？"——不存在就是平台独占子系统 |
| **JS 桥接层** | `wasm_bridge.h`（JS 看到的 C 符号 SSOT） | `wasm_bridge.h` | 是 JS ↔ C 的 ABI 契约头，不是 PAL 模块；用 `wasm_` 前缀命名空间 |
| **JS 库文件** | `wink_sim_*.js` | `wink_sim_js.js`（默认实现）、`wink_sim_stub.js`（桩） | JS 侧文件，不参与 C 命名，用 `wink_sim_` 独立命名空间 |
| **Fiber/协程后端**（居 `osal/<variant>/`） | `sim_ctx_<toolchain>_fiber.c` | `sim_ctx_emscripten_fiber.c`（osal/wasm）、`sim_ctx_win32_fiber.c`（osal/host） | sim_ctx 属 OSAL 调度原语（ADR-0041）；一个变体可挂多个后端（host 未来可加 posix/ucontext） |
| **OSAL 变体实现**（`osal/<variant>/`，ADR-0041 命名例外） | `pal_osal_<variant>.c` 或目录即变体 | `osal/freertos_esp32/pal_osal_freertos_esp32.c`、`osal/wasm/pal_osal_wasm.c`、`osal/host/pal_osal_host.c` | OS 维度与芯片目录解耦；**禁止**再放回 `targets/<plat>/pal_osal_*.c` |
| **跨 target 共享实现**（targets/common/） | 不带任何平台 tag，按模块名 | `pal_resource.c`、`wink_sim_physical.c`、`wink_sim_scheduler.c` | 跨 target 共享，不应出现平台 tag；`common/` 目录本身已经说明"共享"，文件名再加平台会矛盾。**`wink_sim_physical`**：物理退化算法 + **电机/转子 plant model**（电磁/动力学差分方程归属此处，非 `dal/src/`）。**注意：** `pal_osal_ringbuf.c` 已迁至 `osal/common/` |

### 🚫 禁止/反模式

1. **平台 tag 不在末段**：例如历史上 `pal_hal_esp32_gpio.c`（esp32 在中间）就是错的——必须是 `pal_hal_gpio_esp32.c`
2. **平台 tag 与目录名不一致**：如 `targets/baremetal/` 下曾用 `pal_osal_bare.c`（tag `bare` ≠ 目录名 `baremetal`）——必须是 `pal_osal_baremetal.c`
3. **entry 文件加 `pal_` 前缀**：`pal_esp32_entry.c` 是错的，entry 不实现 PAL 契约，保持 `<plat>_entry.c`
4. **平台独占子系统套成后缀公式**：`pal_fault_wasm.c` 错——fault 是 wasm 独占，正确是 `pal_wasm_fault.c`
5. **公共头放进 target 目录**：公共契约（被两个以上 target 或上层 dal/runtime/app include 的头）必须放在 `pal/include/` 或 `dal/include/`，不能放到 `targets/<plat>/`
6. **PAL API 函数使用双重前缀**：`pal_hal_gpio_init()`、`pal_hal_pwm_set_duty()` 是错的——domain 名直接作为前缀，不重复 `hal_` 层级，正确是 `pal_gpio_init()`、`pal_pwm_set_duty()`（历史上 `pal_hal_ultrasonic_*` 曾短暂存在，但超声波属于 DAL 层，PAL 本来就不应该有该 API）
7. **器件/传感器层放在 PAL 而非 DAL**：超声波、舵机、按键、OLED 等语义级器件属于 DAL，不应在 `pal/include/` 下定义；PAL 只提供硬件基元（GPIO/PWM/I2C/pulse_in/RMT），DAL 基于基元封装器件逻辑

### 🔍 快速自检清单（新文件/改名时过一遍）

- [ ] 文件名是否匹配 `pal_<domain>[_<detail>]_<plat>.c/h`？（非 PAL 模块用对应特殊规则）
- [ ] `<plat>` 是否在最后一段？是否与目录名完全一致？
- [ ] 如果是平台独占能力，是否改用 `pal_<plat>_<detail>` 形式？
- [ ] 新增外设 TU 时，CMakeLists.txt 两处 SRCS 列表是否都更新了？
- [ ] 头文件的 `#ifndef` include guard 是否与新文件名一致？（建议用 `WINK_TARGETS_<PLAT>_<DOMAIN>[_<DETAIL>]_<PLAT?>_H`，统一大写下划线）
- [ ] 文件内 `@file` doxygen 标签是否同步更新？跨文件 docblock 里引用本文件名的地方是否同步？

## 5. CMake 库依赖图（无环，自下而上稳定）

```
        pal (INTERFACE, 最稳)          ← 仅头，无符号
         ▲          ▲        ▲
         │          │        │
        dal    wink_runtime  wink_trace     ← STATIC，pal_* 符号外部留待 target 解析
         │          │  │           ▲
         │          │  └──调 trace─┘
         │          │
         ▼          ▼
   [ BAL 外部仓 ]  [ App 生成 ]
         └─────┬────┘  仅 link 公共 include 面(dal+runtime+trace+wink_status)
               ▼
        targets/<platform>   ← 提供 pal_* 实现 + entry(main/app_main)；终链成可执行
         (wasm / esp32 / host)   ← host 同时供 test 链接
```

**云端预编译策略**（[06-build](../06-build-toolchain/01-toolchain-deployment.md) + [ADR-0002](../../decisions/unisim/0002-dual-target-compilation.md) spike 项5）：预编译 `libdal.a` / `libwink_runtime.a` / `libwink_trace.a` / `libpal_<target>.a`；每项目只重编 BAL + device_tree + app_main；终链。**每层独立 `.a` 是该策略的前提**——这正是把 runtime/trace 单列成库的价值。

## 6. 公共 API 面与 BAL 禁入规则

| 头 | 所属 | BAL/App 可用？ |
|---|---|---|
| `dal/include/*.h` | DAL 语义 API | ✅ 主入口 |
| `runtime/include/wink_app.h`、`wink_runtime.h` | OS 生命周期/调度 | ✅ App 注册 `wink_app_callbacks_t`、调 `wink_app_delay_ms` |
| `trace/include/wink_trace.h` | Golden Trace | ✅ App 回调 `on_fault` 中调 `wink_trace_fault` |
| `pal/include/wink_status.h` | 基础类型 | ✅ **例外**（全层需要 `wink_status_t`） |
| `pal/include/pal_hal.h`、`pal_osal.h` | 硬件总线契约 | ❌ **禁入**（BAL 架构约束 §1.2.1） |

> `wink_status.h` 是跨全层基础类型、无 PAL 依赖，故虽物理在 `pal/include/` 却属公共面。BAL 约束的精确措辞为"**禁 `pal_hal.h`/`pal_osal.h` 等硬件契约头**"，不禁基础类型头。

**分层门禁入口（ADR-0043）：** `python wink-tools/wink.py lint --pack layering --pack api`（规则 SSOT：`tools/lint/rules/*.yaml`）。链接/布局类检查仍留在各层 `CMakeLists.txt`。

### 6.1 内核通用架构约束

1. **运行时零动态内存分配 (Zero Dynamic Allocation)**
   - 内核（PAL/DAL/runtime/trace）与生成的 App 在运行期禁止调用 `malloc`/`free`/`realloc`。
   - 所有设备句柄（如 `dal_ultrasonic_t`）与应用上下文必须在初始化时静态分配（全局/静态变量）或栈分配。

2. **DAL/PAL 的 Trace 隔离契约**
   - DAL 与 PAL 的底层驱动只允许以 `wink_status_t` 返回错误码，**严禁直接调用 `wink_trace_*` 或 `wink_trace_fault`**。
   - 故障的捕获与 Trace 记录职责应收敛在 App 层（在回调的 `loop` 或 `on_fault` 中处理）或 `runtime` 调度器。

3. **标准化 CMake 注入接口**
   - 顶层 CMake 使用 `WINK_APP_DIR` 缓存变量作为标准注入接口，指定 AI 生成的 App 源码路径。若未指定，则默认构建 `wink-micro-app/avoidance_car`。
   - **`wink_config.h`** 仅从 `${WINK_APP_DIR}/wink-app.json` 生成（禁止写死 monorepo 根 `../wink-app.json`）。
   - **Source SDK / M2 消费**：`WINK_SDK_PATH` 可指向解压后的 Source SDK 根（Phase 1 为 `wink-micro-os/` 原样子集，见 [Dual-Mode SDK 规格](../../tech-designs/tools/2026-07-12-wink-micro-os-sdk-release-design.md)）。典型命令：
     ```bash
     python $WINK_SDK_PATH/tools/wink.py build host --app /abs/path/to/my_app
     ```
     打包：`python wink-tools/tools/pack/source.py`。SDK 包排除 `test/`（保留 `test/stubs/`，因 host PAL 依赖），不含 monorepo 兄弟目录。
   - **Binary SDK / M2 BINARY 消费**（Phase 2）：打包脚本 `python wink-tools/tools/pack/binary.py` 产出 `wink-micro-os-sdk-binary-vX.Y.Z.tar.gz`。解压根含 `include/`（公共头白名单汇聚）、`libs/<target>/release/libwink_micro_os.a`（合并预编译库）、`targets/`（CMake 桥接）、`tools/`、`SDK_MANIFEST.txt`（`mode=binary`、`toolchain=`、`cflags=`、`content_sha256=`）。消费方式：
     ```bash
     python $WINK_SDK_PATH/tools/wink.py build host --sdk-mode binary --app /abs/path/to/my_app
     ```
     顶层 CMake 自动探测 `libs/host` 存在即切 BINARY 模式（`WINK_SDK_MODE=binary`），跳过源码子目录，`include` targets/host/wink_binary_import.cmake` 导入预编译库。ABI 版本与工具链矩阵见 [ADR-0028](../../decisions/core/0028-host-binary-abi-toolchain-contract.md)。
   - **host 与 wasm 双分支都走同一接口**：`add_subdirectory(${WINK_APP_DIR})` 后由 App CMakeLists 决定如何贡献源码：
     - **host 分支**：App CMakeLists 定义 `add_executable(app_<name>_e2e ...)` 挂到 CTest（host 是"每 App 一个可执行"的 e2e 测试模型）；若 SDK 无 `test/`  harness 则跳过 e2e target。
     - **wasm 分支**：App CMakeLists 通过 `set(WINK_APP_SOURCES ... PARENT_SCOPE)` 向顶层导出源列表（模式与 `PAL_WASM_SOURCES` 一致），顶层 `add_executable(wink_simulator ${WINK_APP_SOURCES} ...)` 消费——wasm 是"所有 App 变体共用一个二进制 target"的沙箱模型。
     - App CMakeLists 通常写成 `if(EMSCRIPTEN) set(... PARENT_SCOPE); return(); endif()`，隔离两条注入路径；防漏错 gate 参见约束 5。
   - **硬件专属 sample 的 wasm 拒绝契约**：面向真机的 sample（如 `devkitc_smoke`）在其 CMakeLists 顶部对 `EMSCRIPTEN` 分支直接 `message(FATAL_ERROR ...)`——比隐式 link 失败更早、更明确。

4. **wasm 侧 JS 桥接的 SSOT 与注入路径**
   - wasm target 引用的 `extern js_*` 符号全集集中在 `targets/wasm/wasm_bridge.h`（**JS 看到的 C 符号 SSOT**）。任何跨 C→JS 的桩函数只能在此声明，禁散布到 pal/dal 各源文件。
   - 默认实现由 `targets/wasm/wink_sim_js.js` 提供，编译期通过 `emcc --js-library=<path>` 注入到 `wink_simulator.js` 胶水（`set_property(TARGET wink_simulator APPEND PROPERTY LINK_DEPENDS ...)` 让改动能触发 relink）。宿主（Workbench 前端 / node stub）可通过 `Module.js_* = ...` 覆盖默认实现，**无需重编 wasm**。
   - Emscripten 6.x 契约：通过顶层 `Module` 属性直接覆盖不生效——生成的 glue 会把每个 `js_*` 编译成 `abort('missing function: ...')` 桩。正解只能是 `--js-library`（本仓采用）或 `--pre-js`。这个前提写在 [04-wasm-simulation/01-wasm-sandbox-lifecycle.md](../04-wasm-simulation/archive/01-wasm-sandbox-lifecycle.md) §2.2.2 里再展开。

5. **wasm build 期兜底契约（可诊断的错误路径）**
   - 顶层 `WINK_APP_DIR` 分支须校验：
     - `WINK_APP_SOURCES` 未从 App CMakeLists 导出 → `FATAL_ERROR` 指出应加 `set(... PARENT_SCOPE)`；
     - `wink_simulator` 未 link `wink_runtime` → `FATAL_ERROR`（历史上 `baca3cf` 漏了这条 link，wasm build broken 7 天没被发现）；
   - 参见 `wink-micro-os/CMakeLists.txt` `if(TARGET_PLATFORM STREQUAL "wasm" AND EMSCRIPTEN)` 分支。

6. **平台特定配置隐藏**
   - PAL 接口保持通用，平台特定的引脚复用、时钟树初始化等复杂配置，隐式封装在各 Target 的入口函数（如 `esp32_entry.c`）中，不暴露在 PAL 公共头文件中。

## 7. runtime / trace 契约与 target entry 流程

解决历史问题：`main()` 曾被埋在 `targets/wasm/pal_hal_wasm.c` 里，OS 主循环无统一归宿。

```
wasm_entry.c::main()      ─┐
esp32_entry.c::app_main() ─┼──► wink_runtime_run(&callbacks)  [runtime 层，target-agnostic]
host 样例 main()          ─┘         │
                                     ├─ callbacks.init()     (一次)
                                     └─ while(1){ callbacks.loop(); wink_app_delay_ms(tick); }
                                                │              │
                                          调 dal_*           └─► pal_delay_ms()  ← target 实现
                                                                                (Asyncify / vTaskDelay / 虚拟时间)
                          callbacks.on_fault() ──► wink_trace_fault()  [trace 层]
```

- **解耦设计**：各 target 的 `*_entry.c` 实例化 `wink_app_callbacks_t` 并调用 `wink_runtime_run`。runtime 库内无任何对外部 `app_*` 符号的强 `extern` 依赖，实现二进制级解耦，方便单元测试与 Mock。
- **各 target 的 `*_entry.c` 只负责"启动 runtime"**，业务循环统一在 `wink_runtime.c`。
- **调度器 target-agnostic**：只用 PAL OSAL（`pal_delay_ms`）做 tick，挂起语义由各 target 实现——这是 ADR-0002 双 target 语义对齐的落点。
- 详见 [04-runtime-and-trace.md](./04-runtime-and-trace.md)。

## 8. now / roadmap 与 PAL 拆分规则

| 状态 | 内容 |
|---|---|
| **已落地** | `pal/include/hal/` + `pal/include/osal/` 子目录、`pal/{storage,resource,debug}.h` 移出 `hal/`、target 实现文件命名标准化（Zephyr 风格） |
| **现存** | `pal/include/hal/pal_hal.h`、`dal/{ultrasonic,servo}`、`targets/wasm/pal_hal_wasm.c` |
| **ADR-0003 计划新增** | `pal/include/wink_status.h`、`test/{unity, *_stub, test_*}` |
| **本架构新增** | `runtime/`(骨架)、`trace/`(骨架)、`samples/avoidance_car/`、`pal/` 改 INTERFACE 库 |
| **本架构重构** | `pal_hal_wasm.c` 拆 4 块(HAL/OSAL/bridge/entry)；`test/pal_host_stub.*` 迁 `targets/host/`；测试专用注入控制留 `test/stubs/host_test_ctrl.*` |
| **推迟(roadmap)** | `targets/esp32` 全实现（先骨架）、`targets/stm32`(MVP后)、PAL 按总线拆 flat 头（规则见下）、trace replay/compare、codegen templates/tools（外部，codegen 落地时） |

**PAL 目录划分规则**（已执行，Phase 1 目录重组）：
- `pal/include/hal/`：硬件抽象层，存放物理总线与外设契约（`pal_hal.h`、`pal_rmt.h`、`pal_pwm_router.h` 等）
- `pal/include/osal/`：操作系统抽象层，存放 OS 相关契约（`pal_osal.h`）
- `pal/include/` 根目录：存放跨 HAL/OSAL 的核心能力与系统服务（`pal_irq.h`、`pal_log.h`、`pal_resource.h`、`pal_storage.h`）

**PAL 按总线拆分规则**（文档化，非现在执行）：当 SPI 到来（第 4 个总线），把 `pal_hal.h` 拆分为 `hal/pal_gpio.h` / `hal/pal_i2c.h` / `hal/pal_spi.h` / … + `pal.h` 聚合头。参考 ESP-IDF `driver/`、Zephyr `include/zephyr/drivers/` 惯例。

## 9. 对正在执行的 ADR-0003 计划的迁移影响

本架构与 [ADR-0003 实施计划](../../superpowers/plans/2026-06-23-adr0003-simulation-fidelity-and-code-alignment.md) 有 3 处路径冲突，建议**先执行一次目录对齐重构，再跑 ADR-0003 Task 1~7**，避免在旧结构上施工又返工：

1. **`pal/` 改 INTERFACE 库** → ADR-0003 Task 1 中把 `wink_status.h` 列入 `target_sources PRIVATE` 的步骤可删（该计划附录 P2 已标"冗余但无害"，本架构顺手清掉）。
2. **`test/pal_host_stub.*` → `targets/host/`** → Task 2 的 `pal_host_stub.c` 一分为二（`pal_hal_host.c` + `pal_osal_host.c`）进 `targets/host/`；测试专用注入控制（`sim_set_echo_timing` 等）留 `test/stubs/host_test_ctrl.c`；Task 2 的 `add_wink_test` 改为链接 `targets/host` OBJECT 库。**最大改动项**。
3. **`pal_hal_wasm.c` 拆 4 块** → 与 ADR-0003 决策2（bypass 收窄）协同：`wasm_bridge.h` 集中 `js_sim_*` 契约，正是 ADR-0003 SSOT 闭环的物理落点。

## 10. 遵循与后续（SSOT 闭环）

落本架构时需同步回写的设计文档（遵循 [docs-adr.md §2](../../../.claude/rules/docs-adr.md) 决策回写）：

| 文档 | 动作 |
|---|---|
| [README.md](./README.md) | 分层图加 `runtime` + `trace` 为内核 peer 层（现仅 BAL/DAL/PAL）；目录文档列表加本文件与 04 链接 |
| **本文件** `03-directory-architecture.md` | ✅ 新增（权威目录布局活规范） |
| **新增** `04-runtime-and-trace.md` | runtime 生命周期 + trace 契约规范（现无对应设计文档，§7 详述待此文件落地） |
| [02-pal-platform-abstraction.md](./02-pal-platform-abstraction.md) §4.3 | 注明 `pal/` 为 INTERFACE 契约、实现居 `targets/` |
| [01-system-overview.md](../01-system-overall/01-system-overview.md) §3 | 内核内部补 runtime/trace 层（overview 已列 Trace System 为 peer） |

> 如需把本架构决策正式立档，可追加 **ADR-0006**（Ports & Adapters 内核骨架 + trace 一等 peer + codegen 外置）记录比选与裁决。状态推进由产品/架构师联合拍板。

---

## 附录：方案比选轴裁决记录（A vs C）

本架构（代号 **A\***）在"端口隔离优先（A）"与"领域内聚（C）"两方案间逐轴裁决得出：

| 轴 | A | C | 裁决（出处） | 采纳 |
|---|---|---|---|---|
| ① PAL 头组织 | 扁平、单 `pal_hal.h` 增量长 | 一上来拆 `hal/`+`osal/` 子目录 | YAGNI + Rule of Three | **A**（扁平；按总线拆推迟到文件级，§8） |
| ② Trace 归属 | 折进 `runtime/` | `trace/` 独立一等 peer 层 | Screaming Architecture + overview §3 | **C**（独立 `trace/`） |
| ③ codegen 资产 | 外部（仅 `samples/`） | `apps/{templates,samples}`+`tools/` 与内核同居 | Conway 边界原则 + Registry 已是 SSOT | **A**（外置） |

**结论**：纯 A 命中 2/3 轴、纯 C 命中 1/3 轴；最优解 A\* = A 骨架 + 借 C 的 trace 独立。既不欠结构债（极简渐进方案的病），也不付过早仪式（领域内聚方案的病）。

