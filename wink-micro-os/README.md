# wink-micro-os

`wink-micro-os` 是为 **Wink-AI 低代码平台** 打造的跨平台嵌入式硬件运行时框架。它集成了 **PAL（平台抽象层）** 与 **DAL（器件抽象层）**，旨在通过“业务语义级 API”支持图形化/AI 自动生成的业务逻辑在 **Web 浏览器沙箱 (Wasm)** 与 **实体物理微控制器 (ESP32/STM32等)** 之间的无缝切换与同源执行。

---

## 1. 核心架构分层

内核采用 **Ports & Adapters（A\*）** 架构：`pal`（纯契约 INTERFACE）← `dal` ← `runtime` + `trace`（两横切一等 peer 层），`targets/`（wasm/host/esp32）为适配器端口，App/BAL 仅 link 公共 include 面。

> **术语澄清**：
> - ✅ **App 层**：用户代码/AI 生成的一次性业务逻辑（`init_status` / `app_loop` / `on_fault_status`）
> - ✅ **BAL 层**（[ADR-0023](../docs/design/decisions/0023-bal-business-abstraction-layer.md) / [ADR-0038](../docs/design/decisions/0038-bal-naming-hard-cut-and-layer-ssot.md)）：**Business Abstraction Layer**，可复用业务服务（LED blink、button events、ultrasonic poll、servo sweep、telemetry 等），位于 `bal/`，由 App 显式 `*_start()` / `enable_*`，**禁止** include `pal_*.h`；公开树禁用 `*_helper` / `*_controller` / `sonar`

```
┌────────────────────────────────────────────────────────┐
│  App（AI 生成，一次性业务） + BAL（业务服务，bal/ 正式层）   │
├────────────────────────────────────────────────────────┤
│  runtime（回调注入主循环） + trace（Golden Trace） ◄ peer 一等层
├────────────────────────────────────────────────────────┤
│      2. 器件抽象层 (DAL - Device Abstraction Layer)    │
├────────────────────────────────────────────────────────┤
│      3. 平台抽象层 (PAL - INTERFACE 契约，无 .c)        │
├────────────────────────────────────────────────────────┤
│  targets/：wasm 仿真 / host 一等 target / esp32 真机    │
└────────────────────────────────────────────────────────┘
```

1. **App (应用逻辑层)**：由低代码/codegen 生成的业务逻辑。经 `wink_app_callbacks_t`（`init_status` / `loop` / `on_fault_status` / `on_boot`）注入 runtime；调用 BAL 服务或 DAL 语义 API，**不对接**裸 GPIO/I2C。
2. **BAL (业务抽象层)**：`bal/` 静态库 `wink_bal`。强类型双轨 API（`_start` 初学者默认 / `_start_ex` 专家覆盖栈/优先级/核）。典型用法：`wink_button_enable_events(&btn, &cfg)`（ADR-0032 B 类）。SSOT：[06-bal-layer.md](../docs/design/02-wink-micro-os/06-bal-layer.md)；迁移见 [CHANGELOG.md](./CHANGELOG.md)。
3. **runtime + trace**：一等 peer 层。`runtime` 用回调注入跑协作式主循环（无 `extern app_*` 强依赖，二进制解耦）；`trace` 用静态环形缓冲记录故障（零动态分配）。DAL/PAL 驱动**禁**直接调 `wink_trace_*`，故障捕获收敛在 App 回调。
4. **DAL (器件抽象层)**：管理具体的传感器和执行器（如超声波测距仪、舵机、温湿度计）。
   * **双模运行能力**：在仿真模式下，DAL 驱动仅旁路最底层物理信号来源（trigger 时序、echo 脉宽），换算与超时判定两端同源；在真机模式下，它调用 PAL 接口操作物理引脚。
5. **PAL (平台抽象层)**：纯契约 INTERFACE 库（仅头、无符号）。统一包装跨平台的操作系统服务（OSAL，任务与微秒定时器）与硬件总线控制（HAL，如 GPIO, PWM, I2C）。所有实现下沉到 `targets/`。
6. **Targets (平台适配器)**：具体平台的 PAL 实现端口（wasm/host/esp32）。host 为一等 target，供 PC 上跑完整 PAL→DAL→runtime→App 测试。

---

## 2. 目录架构说明

本仓库按组件化结构组织（A\* 架构，详见 [03-directory-architecture.md](../docs/design/02-wink-micro-os/03-directory-architecture.md)）：

```text
wink-micro-os/
├── CMakeLists.txt              # 顶层：TARGET_PLATFORM 路由 · WINK_APP_DIR 注入 · 层库聚合
├── tools/                      # SDK 统一工具链 CLI、CodeGen、Lint 引擎与板级元数据 (参见 tools/docs/)
│   ├── wink.py                 # 统一 CLI 总入口 (build/test/lint/esp32/doctor)
│   ├── docs/                   # 工具链模块化详细使用指南
│   ├── codegen/                # 代码生成器 (device_tree / wink_config.h / boards/*.json)
│   ├── lint/                   # YAML 分层/API 代码规约引擎 (ADR-0043)
│   └── toolchain/              # 工具链自动探测与校验 (ADR-0029/0030)
├── pal/                        # 平台抽象层 (INTERFACE 契约库，仅头无 .c)
│   └── include/  pal.h · wink_status.h · pal_hal.h · pal_osal.h
├── dal/                        # 器件抽象层 (STATIC，两端同源)
│   ├── include/  dal_ultrasonic.h · dal_servo.h · dal_led.h · …
│   └── src/
├── bal/                        # 业务抽象层 (STATIC，ADR-0023/0038)
│   ├── include/  wink_bal_opts.h · output/wink_led_blink.h · input/wink_button_events.h · …
│   └── src/      output/ · input/ · sensor/ · actuator/ · comm/ · math/ · control/ · …
├── runtime/                    # OS 运行时 (STATIC，回调注入主循环)
│   ├── include/  wink_app.h · wink_runtime.h
│   └── src/      wink_runtime.c
├── trace/                      # Golden Trace (STATIC，静态环形缓冲)
│   ├── include/  wink_trace.h
│   └── src/      wink_trace.c
├── osal/                       # OS 适配（ADR-0041；按 WINK_OSAL_TYPE 装配）
│   ├── wasm/     pal_osal_wasm.c · sim_ctx_emscripten_fiber.c
│   ├── host/     pal_osal_host.c · sim_ctx_win32_fiber.c
│   ├── freertos_esp32/ pal_osal_freertos_esp32.c
│   └── common/   pal_osal_ringbuf.c
├── targets/                    # 硬件/仿真 HAL 端口（不含 OSAL）
│   ├── wasm/     pal_hal_wasm.c · wasm_bridge.h · wasm_entry.c
│   ├── host/     pal_hal_host.c · pal_log_host.c   # 一等 target
│   └── esp32/    pal_hal_*_esp32.c · esp32_entry.c
├── test/                       # host 单元/端到端测试 (Unity)
    ├── unity/    unity.{c,h} + unity_internals.h
    ├── stubs/    host_test_ctrl.h · js_sim_host_stub.{c,h}
    └── test_*.c
```

---

## 3. 构建与编译说明

项目推荐使用 **Wink CLI 工具链（`python tools/wink.py`）** 进行统一构建与测试管理，通过 CMake 静态绑定编译目标。

### 3.1 编译为 WebAssembly 仿真组件

使用统一 CLI 命令进行构建：

```bash
# 1. 默认构建 wink-micro-app/avoidance_car 避障小车应用
python tools/wink.py build wasm --app avoidance_car

# 2. 换构建其它 App 变体 (如 oled_dashboard / devkitc_smoke 等)
python tools/wink.py build wasm --app oled_dashboard
```

**底层原生 CMake 指令（供参考）**：
```bash
emcmake cmake -S . -B build-wasm -DTARGET_PLATFORM=wasm -DWINK_APP_DIR=$(pwd)/wink-micro-app/avoidance_car
cmake --build build-wasm
```

**产出**：`build-wasm/wink_simulator.wasm` + `build-wasm/wink_simulator.js`（MODULARIZE 胶水，UMD 导出 `WasmSandbox`）。

**Node 侧烟测（编译期契约门禁）**：

```bash
node targets/wasm/wink_sim_stub.js                            # 默认 build-wasm/
node targets/wasm/wink_sim_stub.js --build-dir=build-wasm-oled  # 换其它变体
```

Stub 静态解析 wasm 二进制 imports 集合，与 `wasm_bridge.h` SSOT 交叉核验（多出未声明符号 → fail；DCE 掉的报 warn 不 fail）；在 `worker_threads.Worker` 里加载 `wink_simulator.js` 验证 `onRuntimeInitialized` 到达。**必须走 worker**——Asyncify 循环与 Node 主线程 event loop 同居会 starve timer 并 OOM。同理 Workbench 前端仓也必须把 wasm 关进 Web Worker，不能直接在 UI 主线程加载。

**JS 桥接契约**：所有 `extern js_*` 符号声明在 `targets/wasm/wasm_bridge.h`（SSOT），默认实现在 `targets/wasm/wink_sim_js.js`（编译期 `--js-library` 注入）。宿主（Workbench）通过 `Module.js_* = customImpl` 覆盖默认桩，**无需重编 wasm**。详见 [04-wasm-simulation §2.2.2](../docs/design/04-wasm-simulation/01-wasm-sandbox-lifecycle.md)。

### 3.2 编译为 ESP32 真机固件

使用 Wink CLI 一键清理工具链冲突并构建：
```bash
python tools/wink.py esp32 --app devkitc_smoke build
```

（底层将环境净化后调动 ESP-IDF 进行构建。常规底层命令：`idf.py -C esp32_firmware build`）。

> **ESP32 真机**：devkitc_smoke **S1–S11 全 PASS**（含 5 轮 init→deinit 循环，无 GPIO 占用/WDT）。

---

### 3.3 在本机（host）构建并运行测试

内核各层与端到端链路可在 **PC 上用 gcc + cmake 跑测试**（无需真实硬件 / 浏览器），用 Unity 框架。

**前置：工具链环境诊断。**
运行诊断命令检查 `gcc` 与 `cmake` 环境：
```bash
python tools/wink.py doctor
```

**方式一：统一 CLI 总入口（推荐，跨平台 100% 平替）。**
```bash
python tools/wink.py test            # 跑全套 Codegen 单元测试 + Host CTest + 6 大静态 Lint
python tools/wink.py test --full     # 全量 CI 门禁（含 UBSan / ASan Sanitizer 矩阵 Pass）
python tools/wink.py lint            # 执行分层/API/Arduino 代码规约检查 (ADR-0043)
```

**方式二：PowerShell 回归矩阵（Windows 本地底层）。**
```powershell
.\run-tests.ps1            # 增量构建 + 跑全部测试
.\run-tests.ps1 -Full      # 运行完整 Sanitizer 矩阵与全部 Lint 审计
```

**方式三：手动三步。**
```powershell
cmake -B build/test -DTARGET_PLATFORM=host
cmake --build build/test
cd build/test; ctest --output-on-failure
```

**测试矩阵（63 个 C Executable 用例 + 32 项 Python Codegen 单元测试 + WASM 烟测）：**

| 梯队 | 覆盖领域 |
|---|---|
| **Tier 1 · Core / 门禁** | PAL 契约、BAL 服务（button/blink/ultrasonic/servo/telemetry）、runtime change_period、blocking strict |
| **Tier 2 · DAL 外设与物理退化** | servo/ultrasonic/ultrasonic_sim/led/button/ssd1306、dev_config、avoidance_override、button_debounce_e2e |
| **Tier 3 · 协作式调度器与 Phase 2 堆配额** | sim_scheduler / _e2e / _determinism / _stack_clamp / _wcet_fault / _zombie_gc、sim_mutex_e2e、single_task_semantic_regression、sim_physical、ADR-0045 堆配额断言 |
| **Tier 4 · Sample e2e 与 Sanitizers 矩阵** | app_avoidance_car / oled_dashboard / devkitc_smoke / dual_task_demo、sample_resource_conflict、UBSan/ASan 矩阵 Pass |

> `build/test` 为构建产物，**不提交 git**。

---

## 4. 编码规范契约
* **函数命名**：统一使用小写蛇形命名 `dal_[device]_[action]` / `pal_[bus]_[action]` / `wink_[layer]_[action]`。
* **错误码**：所有可能失败的函数返回 `wink_status_t`（`int32_t`，0=`WINK_OK`，负=错误）；判定用 `if (status < 0)`，**禁** `if (status)`（负数在 C 中为真）。详见 [ADR-0001](../docs/design/decisions/0001-error-code-sign-convention.md)。
* **时序与延时**：在 DAL 实现中，非阻塞场景必须使用 OSAL 提供的 `pal_get_us()` 计算时间差；阻塞式微秒延时统一调用 `pal_delay_us()`。
* **仿真条件分支**：`#ifdef SIMULATION` 只旁路最底层物理信号来源（如 trigger 时序、echo 脉宽），换算与超时判定两端同源（ADR-0003）。详见 [.claude/rules/c-code.md](../.claude/rules/c-code.md)。
