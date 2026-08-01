# Wink Micro OS 全栈按需自动化裁剪指南 (Full-Stack Pruning Architecture)

本文档详细说明 Wink Micro OS 的**全栈按需自动化裁剪架构**。系统根据应用描述文件 `wink-app.json` 与应用源码特征，自动将未使用的硬件驱动、业务组件、仿真虚拟外设及扩展框架切除，实现**0 冗余代码编译 (Zero-Bloat)**。

---

## 1. 核心设计理念

1. **自动无感 (100% Automated)**：开发者只需在 `wink-app.json` 中配置应用用到的硬件设备树节点。工具链与 CMake 构建树会自动推导依赖全链条，**开发者无需手动配置任何 C/C++ 编译宏或 CMake 标志**。
2. **全栈零死重 (Zero-Bloat)**：全全量裁剪覆盖 **Frameworks $\rightarrow$ BAL $\rightarrow$ DAL $\rightarrow$ Targets/WASM** 整个系统栈。未引用的源文件 **0 行参与编译**。
3. **零破坏与全兼容 (Zero-Breakage)**：裁剪仅作用于 C/C++ 源文件编译节点，API 头文件与接口（ABI）保持曝光。若应用误调已被裁剪模块的 API，编译期会抛出明确友好的告警信息（如“Device driver not enabled in wink-app.json”）。

---

## 2. 自动化推导与依赖控制全景图

```
                +------------------------------------+
                |  应用配置文件 (wink-app.json)       |
                +------------------------------------+
                                  |
                                  v  app_codegen.py 自动解析
                +------------------------------------+
                |  生成的转换配置 (app_options.cmake) |
                |  WINK_USE_<DRV> = ON / OFF         |
                +------------------------------------+
                                  |
   +------------------------------+------------------------------+
   |                              |                              |
   v                              v                              v
+-----------------------+  +-----------------------+  +-----------------------+
|  DAL (器件抽象层)      |  |  BAL (业务抽象层)      |  |  Targets (仿真外设层)  |
|  仅编译 dal_button.c  |  |  仅编译 wink_button.c |  |  仅编译 wasm_dev_*.c  |
|  仅编译 dal_led.c     |  |  仅编译 wink_led.c    |  |  跳过未使用的虚拟外设  |
+-----------------------+  +-----------------------+  +-----------------------+

                              【额外源码扫描】
                   App 源码包含 .ino 或 Arduino.h ?
                                /           \
                             [YES]         [NO]
                               |             |
                               v             v
                    编译并链接 Arduino      完全跳过 Arduino 框架
                    (wink_arduino_compat)  (0 行 C++ 代码参与编译)
```

---

## 3. 按层裁剪标准与实现细节

### 3.1 应用框架层 (Frameworks Layer)
- **目标目录**：`frameworks/arduino`
- **裁剪原理**：
  - 为 `wink_arduino_compat` 静态库目标添加 `EXCLUDE_FROM_ALL` 延迟构建属性。
  - CMake 在配置阶段扫描 `WINK_APP_SOURCES` 及 `WINK_APP_DIR` 下的 App 源码，检测是否存在 `.ino` 文件或 `#include <Arduino.h>`。
  - **检测结果**：
    - 若 App 为纯 C/C++ 应用（无 Arduino 依赖），`wink_simulator` **不链接** `wink_arduino_compat`。因为开启了 `EXCLUDE_FROM_ALL`，CMake **完全跳过**该框架下所有 7 个 C++ 源文件的编译。
    - 若 App 使用了 Arduino 兼容语法，系统自动注入链接。

### 3.2 硬件器件抽象层 (DAL Layer - ADR-0039)
- **目标目录**：`dal/`
- **裁剪原理**：
  - `app_codegen.py` 读取 `wink-app.json` 中 `devices` 字典的 `type` 类型（如 `button`, `led`, `mono_oled`）。
  - 在生成目录产生 `app_options.cmake`，自动生成 `set(WINK_USE_BUTTON ON)`、`set(WINK_USE_ULTRASONIC OFF)` 等变量。
  - `dal/CMakeLists.txt` 根据大写宏 `WINK_USE_<DRV>`，仅将选中的 `dal_*.c` 加入 `target_sources(dal PRIVATE ...)` 编译列表。

### 3.3 业务抽象层 (BAL Layer)
- **目标目录**：`bal/`
- **裁剪原理**：
  - BAL 层封装了常用业务逻辑（按键事件流、LED 闪烁、超声波周期轮询、舵机扫频、PID 算法、闭环电机及底盘运动学）。
  - `bal/CMakeLists.txt` 联动 DAL 的 `WINK_USE_<DRV>` 变量进行条件化源文件装载：
    - `WINK_USE_BUTTON` $\rightarrow$ 仅装载 `wink_button_events.c`（及 ESP32 IRQ 驱动）
    - `WINK_USE_LED` $\rightarrow$ 仅装载 `wink_led_blink.c`
    - `WINK_USE_ULTRASONIC` $\rightarrow$ 仅装载 `wink_ultrasonic_poll.c` / `wink_ultrasonic_distance_events.c`
    - `WINK_USE_RC_SERVO` $\rightarrow$ 仅装载 `wink_rc_servo_sweep.c`
    - `WINK_USE_DC_MOTOR` / `WINK_USE_ENCODER` $\rightarrow$ 仅装载 `wink_pid.c` / `wink_chassis.c` 等控制组件

### 3.4 平台仿真外设层 (Target Simulation Devices)
- **目标目录**：`targets/wasm/devices/`
- **裁剪原理**：
  - WASM 仿真器包含为 Web 前端/Shared Memory 建立的虚拟硬件驱动（如 `wasm_dev_servo.c`, `wasm_dev_ultrasonic.c`）。
  - `targets/wasm/CMakeLists.txt` 根据 `WINK_USE_RC_SERVO` 与 `WINK_USE_ULTRASONIC` 动态装载。
  - `wasm_sim_registry.c` 使用 `#if defined(WINK_USE_RC_SERVO)` 保护复位与 PWM 拦截调用，消除未定义符号警告。

### 3.5 二级链接期死代码消除 (Dead Code Elimination - DCE)
- **编译器/链接器配置**：
  - 全局开启 `-ffunction-sections` 与 `-fdata-sections`。
  - 链接阶段下发 `-Wl,--gc-sections`。即使个别辅助函数被编入，链接器也会自动扫描可达性图，剥离未被引用的函数 section。

---

## 4. 实测效果对比（以 `oled_dashboard` 为例）

| 构建模块 | 全方位裁剪前源文件数量 | 全方位裁剪后源文件数量 | 裁剪优化收益 |
| :--- | :--- | :--- | :--- |
| **DAL 驱动层** | 4 个 C 文件 (按需) | 4 个 C 文件 (按需) | 保持 100% 精确 |
| **BAL 业务层** | 12 个 C 文件 (全量无条件) | 5 个 C 文件 (仅按钮/LED) | **减少 ~58% 业务源码编译** |
| **WASM 虚拟外设** | 3 个 C 文件 (全量无条件) | 1 个 C 文件 (仅注册表) | **减少 ~66% 虚拟外设源码编译** |
| **Arduino 框架** | 7 个 C++ 文件 (全量无条件) | 0 个 C++ 文件 (彻底切除) | **100% 切除无用 C++ 编译** |
| **构建警告** | 3 个未定义符号 warning | **0 Warning** | 干净优雅链接 |

---

## 5. 开发者 FAQ

### Q1: 如果我在 C 代码中调用了未在 `wink-app.json` 配置的外设 API 会怎样？
系统的 API 头文件（如 `dal_ultrasonic.h`）对未使能的函数应用了 `WINK_UNAVAILABLE_MSG` 保护。编译器在编译该行代码时会直接给出友好报错：
> `error: "Ultrasonic driver not enabled; add an "ultrasonic" device to wink-app.json"`
只需在 `wink-app.json` 中补全该设备节点，系统就会自动开启全栈相应的编译。

### Q2: 重新编译时需要手动执行 clean 吗？
不需要。CMake 建立了对 `wink-app.json` 的修改监控（`CMAKE_CONFIGURE_DEPENDS`）。当修改 `wink-app.json` 后，下一次运行 `wink build` 时 CMake 会自动重新运行 `app_codegen.py` 并刷新 `app_options.cmake`，实现秒级增量重新配置。
