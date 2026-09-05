# ADR-0035：Arduino 兼容层多态沙箱与内核隔离设计

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已接受）** |
| 日期 | 2026-07-17 |
| 影响范围 | `wink-micro-os/frameworks/arduino/`、CMake 编译规则、CI 校验脚本 |
| 关联 ADR | [ADR-0004 编译期静态分发](./0004-static-dispatch-vs-runtime-ops.md)、[ADR-0023 BAL 业务分发层](./0023-bal-business-abstraction-layer.md) |

---

## 背景（Context）

根据项目既有的设计规范与核心决策 [ADR-0004](./0004-static-dispatch-vs-runtime-ops.md)，WinkMicroOS 的设计核心是 **“编译期静态分发”**，即：
* **严禁在内核中使用 C++ 虚函数、虚表（vtable）或任何运行期的多态机制**，以此保证极高且可预测的运行效率与零动态分配开销。

然而，官方 `ArduinoCore-API` 是基于 C++ 面向对象设计的：
* 标准的 `Print`、`Stream`、`HardwareSerial`、`HardwareI2C` 等核心类定义了大量纯虚函数（例如 `virtual size_t write(uint8_t) = 0`），其全部子类与第三方外设库（如 Adafruit 系列）都深度依赖运行时虚表分发。

直接引入 Arduino 核心 API 会在内核中引发 C++ 运行时虚表多态的泄露，直接破坏 [ADR-0004](./0004-static-dispatch-vs-runtime-ops.md) 的底层设计。

---

## 决策结论（Decision）

为了同时兼容 Arduino 生态并维持内核的静态分发底座，我们采取**“多态沙箱与单向依赖隔离”**的架构决策：

### 1. 设立 `arduino_compat` 编译沙箱
* 允许在 `wink-micro-os/frameworks/arduino/` 目录内使用 C++ 类继承和虚函数分发。
* 该目录将被编译为一个孤立的静态库目标（CMake 目标：`wink_arduino_compat`）。

### 2. 隔离规则（三条红线）

1. **叶子节点链接规则（仅限 App 链接）**：
   * `wink_arduino_compat` 必须是依赖树上的叶子节点。
   * 严禁任何内核静态库（如 `pal`、`dal`、`wink_runtime`、`wink_bal`）链接 `wink_arduino_compat`。
   * 只有用户层的 App 目标（可执行程序或仿真可执行程序）允许链接 `wink_arduino_compat`。

2. **头文件严格单向可见（No Leakage）**：
   * 只有 `wink-micro-os/frameworks/arduino/` 和应用层代码（`app/`）允许包含 `<Arduino.h>`、`<Wire.h>` 或任何来自 `ArduinoCore-API` 的头文件。
   * 内核头文件（`pal/include/`、`dal/include/` 等）**严禁**包含任何 Arduino 相关的类或头文件。

3. **依赖方向倒置（Dependency Inversion）**：
   * 兼容层通过直接调用 `pal_gpio_*`、`pal_i2c_*` 等内核的 C API 来驱动硬件，而不是让内核去适配兼容层。

### 3. 机械化 CI 守护（Mechanical Gatekeeping）
为了防止人肉 Code Review 的疏漏和 AI 生成代码产生的依赖污染，我们在构建与 CI 流程中加入两条强制检查：
* **CMake 包含路径白名单**：内核组件的 `target_include_directories` 中不提供 `ArduinoCore-API` 头文件路径，在编译期物理隔绝包含的可能。
* **CI 正则扫描**：在 `python wink-tools/wink.py test` 和代码扫描中，扫描 `pal/`、`dal/`、`targets/` 等目录。一旦检测到出现 `#include.*Arduino.h`、`HardwareSerial` 或 `TwoWire` 等字样，直接终止编译并报错。

---

## 后果与约束（Consequences & Constraints）

### 正面后果
* **内核纯净度**：Wink 核心内核的纯 C 与静态分发特征被 100% 保留，原本的 C 语言固件体积不受任何影响。
* **无缝桥接**：在上层（应用层）提供了完全符合 Arduino 社区规范的 C++ 运行沙箱，使得所有 Adafruit 外设库可以通过多态机制在沙箱中正常编译。

### 约束
* **调试与定位**：在 `arduino_compat` 发生的多态死锁或虚函数悬空调用（如 `__cxa_pure_virtual`）会通过适配层被捕获，并在 `pal_panic()` 中终止程序，调试信息不会渗透到内核层。
