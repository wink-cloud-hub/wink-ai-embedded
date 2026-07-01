# wink-micro-os

`wink-micro-os` 是为 **Wink-AI 低代码平台** 打造的跨平台嵌入式硬件运行时框架。它集成了 **PAL（平台抽象层）** 与 **DAL（器件抽象层）**，旨在通过“业务语义级 API”支持图形化/AI 自动生成的业务逻辑在 **Web 浏览器沙箱 (Wasm)** 与 **实体物理微控制器 (ESP32/STM32等)** 之间的无缝切换与同源执行。

---

## 1. 核心架构分层

内核采用 **Ports & Adapters（A\*）** 架构：`pal`（纯契约 INTERFACE）← `dal` ← `runtime` + `trace`（两横切一等 peer 层），`targets/`（wasm/host/esp32）为适配器端口，App/BAL 仅 link 公共 include 面。

> **术语澄清**：
> - ✅ **App 层**：用户代码/AI 生成的一次性业务逻辑（`app_init/app_loop/app_on_fault`）
> - ✅ **BAL 层**：Business Algorithm Layer（业务算法工具库），可复用的算法组件（如 PID、卡尔曼滤波），**独立仓库维护**，由 App 层调用

```
┌────────────────────────────────────────────────────────┐
│  App（AI 生成，一次性业务） / BAL（独立仓，可复用算法）  │
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

1. **App (应用逻辑层)**：由低代码编排生成的业务逻辑。它经 `wink_app_callbacks_t` 回调注入 runtime，并调用 BAL 算法库或 DAL 暴露的只读/只写业务 API，不对接任何底层的 I2C/GPIO 硬件。
2. **BAL (业务算法层)**：可复用的算法组件库（如 PID、卡尔曼滤波、传感器融合等），独立仓库维护，由 App 层调用。
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
├── run-tests.ps1               # host 测试一键脚本（见 §3.3）
├── pal/                        # 平台抽象层 (INTERFACE 契约库，仅头无 .c)
│   └── include/  pal.h · wink_status.h · pal_hal.h · pal_osal.h
├── dal/                        # 器件抽象层 (STATIC，两端同源)
│   ├── include/  dal_ultrasonic.h · dal_servo.h
│   └── src/      dal_ultrasonic.c · dal_servo.c
├── runtime/                    # OS 运行时 (STATIC，回调注入主循环)
│   ├── include/  wink_app.h · wink_runtime.h
│   └── src/      wink_runtime.c
├── trace/                      # Golden Trace (STATIC，静态环形缓冲)
│   ├── include/  wink_trace.h
│   └── src/      wink_trace.c
├── targets/                    # 平台适配端口
│   ├── wasm/     pal_hal_wasm.c · pal_osal_wasm.c · wasm_bridge.h · wasm_entry.c
│   ├── host/     pal_hal_host.c · pal_osal_host.c   # 一等 target，吸收旧 host 桩
│   └── esp32/    pal_hal_esp32.c · pal_osal_esp32.c · esp32_entry.c (骨架)
├── test/                       # host 单元/端到端测试 (Unity)
│   ├── unity/    unity.{c,h} + unity_internals.h
│   ├── stubs/    host_test_ctrl.h · js_sim_host_stub.{c,h}
│   └── test_*.c
└── samples/avoidance_car/      # 示例 App（device_tree + app_main）
```

---

## 3. 构建与编译说明

项目使用 CMake 构建，通过传入 `-DTARGET_PLATFORM` 参数静态绑定编译目标。

### 3.1 编译为 WebAssembly 仿真组件

使用 Emscripten 工具链编译，生成 Wasm 字节码供 Web 端 Worker 线程载入。默认构建 `samples/avoidance_car`：

```bash
# 从 wink-micro-os/ 目录执行
emcmake cmake -S . -B build-wasm -DTARGET_PLATFORM=wasm
cmake --build build-wasm
```

**产出**：`build-wasm/wink_simulator.wasm` + `build-wasm/wink_simulator.js`（MODULARIZE 胶水，UMD 导出 `WasmSandbox`）。

**换 App 变体**：AI 生成的 App 或其它 sample 通过 `WINK_APP_DIR` 注入：

```bash
emcmake cmake -S . -B build-wasm -DTARGET_PLATFORM=wasm \
    -DWINK_APP_DIR=$(pwd)/samples/oled_dashboard
cmake --build build-wasm
```

**Node 侧烟测（编译期契约门禁）**：

```bash
node targets/wasm/wink_sim_stub.js                            # 默认 build-wasm/
node targets/wasm/wink_sim_stub.js --build-dir=build-wasm-oled  # 换其它变体
```

Stub 静态解析 wasm 二进制 imports 集合，与 `wasm_bridge.h` SSOT 交叉核验（多出未声明符号 → fail；DCE 掉的报 warn 不 fail）；在 `worker_threads.Worker` 里加载 `wink_simulator.js` 验证 `onRuntimeInitialized` 到达。**必须走 worker**——Asyncify 循环与 Node 主线程 event loop 同居会 starve timer 并 OOM。同理 Workbench 前端仓也必须把 wasm 关进 Web Worker，不能直接在 UI 主线程加载。

**JS 桥接契约**：所有 `extern js_*` 符号声明在 `targets/wasm/wasm_bridge.h`（SSOT），默认实现在 `targets/wasm/wink_sim_js.js`（编译期 `--js-library` 注入）。宿主（Workbench）通过 `Module.js_* = customImpl` 覆盖默认桩，**无需重编 wasm**。详见 [04-wasm-simulation §2.2.2](../docs/design/04-wasm-simulation/01-wasm-sandbox-lifecycle.md)。

### 3.2 编译为 ESP32 真机固件
作为 ESP-IDF 工程的组件 (Component) 引入：
```bash
idf.py build
```
> ⚠️ **ESP32 真机代码状态（2026-06）**：GPIO/PWM/I2C/OSAL/WDT/资源治理/RMT 超声波捕获均有实现，
> **但尚未经 `idf.py` 编译验证，亦未经硬件验证**——本仓 host 测试矩阵（§3.3）不编译 `targets/esp32`，
> 故 ESP32 真机函数体（`#if defined(ESP_PLATFORM)` 内）未被任何 CI/本地构建覆盖。
> 已完成：PAL 契约符号（`WINK_ERR_HARDWARE` / `WINK_MUTEX_WAIT_FOREVER` / `PAL_RESET_REASON_*`）补齐、
> 平台判定宏统一为 `ESP_PLATFORM`、RMT 接收缓冲按 ESP-IDF v5.x 契约重写、host 端契约编译探针
> （`test_pal_contract`）守卫符号完整性。**仍待**：`idf.py build` 编译通过 + Wave B 硬件验证
> （示波器测 RMT 中断延迟 < 10us、HC-SR04 测距精度）后，方可声称真机可用。
> 完整 ESP-IDF 移植（含 device_tree 物理引脚路由 P1-3、LEDC timer 分组 P2-2）见后续 Phase 3 / Wave B。

### 3.3 在本机（host）构建并运行测试

内核各层与端到端链路可在 **PC 上用 gcc + cmake 跑测试**（无需真实硬件 / 浏览器），用 Unity 框架。这是日常开发最快的验证回路。

**前置：安装工具链。** 需要 gcc 与 cmake。本机推荐经 winget 安装 WinLibs MinGW（自带 gcc 16.1.0 + cmake 4.3.2）：

```powershell
winget install --id BrechtSanders.WinLibs.POSIX.UCRT -e
# 安装后 gcc/cmake 进入 User PATH；若新窗口未识别，重启电脑使其生效。
gcc --version   # 验证可用
```

**方式一：一键脚本（推荐）。** 在 `wink-micro-os/` 目录下：

```powershell
.\run-tests.ps1            # 增量构建 + 跑全部测试
.\run-tests.ps1 -Clean     # 删 build-test 全量重编（改了 CMake 时用）
.\run-tests.ps1 -Detailed  # 打印每个测试的完整 Unity 输出
```

**方式二：手动三步。**

```powershell
cd wink-micro-os
cmake -B build-test -DTARGET_PLATFORM=host
cmake --build build-test
cd build-test; ctest --output-on-failure
```

看到 `100% tests passed` 即通过。只跑某项：`ctest -R servo --output-on-failure`；直接看单个 exe 输出：`.\test\test_dal_servo.exe`。

**测试矩阵（8 个可执行，约 30 个测试点）：**

| 测试 | 验证 |
|---|---|
| `test_smoke` | `wink_status_t` 错误码语义（负数=错误） |
| `test_trace` | Golden Trace 环形缓冲（满则覆盖） |
| `test_runtime` | 主循环：注册回调 → 跑 N tick → fault 上报 |
| `test_host_pal` | host 虚拟时间推进 + PWM 记录 |
| `test_dal_servo` | 舵机角度→占空比换算、钳位 |
| `test_dal_ultrasonic` | 超声波真机分支：脉宽→距离、超时 |
| `test_dal_ultrasonic_sim` | 仿真分支与真机同源换算（ADR-0003 守卫） |
| `app_avoidance_car_e2e` | 端到端 PAL→DAL→runtime→App（注入障碍→舵机偏转） |

> `build-test/` 为构建产物，**不提交 git**。

---

## 4. 编码规范契约
* **函数命名**：统一使用小写蛇形命名 `dal_[device]_[action]` / `pal_[bus]_[action]` / `wink_[layer]_[action]`。
* **错误码**：所有可能失败的函数返回 `wink_status_t`（`int32_t`，0=`WINK_OK`，负=错误）；判定用 `if (status < 0)`，**禁** `if (status)`（负数在 C 中为真）。详见 [ADR-0001](../docs/design/decisions/0001-error-code-sign-convention.md)。
* **时序与延时**：在 DAL 实现中，非阻塞场景必须使用 OSAL 提供的 `pal_get_us()` 计算时间差；阻塞式微秒延时统一调用 `pal_delay_us()`。
* **仿真条件分支**：`#ifdef SIMULATION` 只旁路最底层物理信号来源（如 trigger 时序、echo 脉宽），换算与超时判定两端同源（ADR-0003）。详见 [.claude/rules/c-code.md](../.claude/rules/c-code.md)。
