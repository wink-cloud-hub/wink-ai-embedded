# ADR-0036：WinkMicroOS C++ 子集编译与内存策略（面向 Arduino 兼容）

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已接受）** |
| 日期 | 2026-07-17 |
| 影响范围 | CMake 构建脚本、`arduino_compat` 运行期内存、C++ 异常/RTTI 配置 |
| 关联 ADR | [ADR-0001 错误码负值规范](./0001-error-code-sign-convention.md)、[ADR-0002 双 target 同源编译](../unisim/0002-dual-target-compilation.md) |

---

## 背景（Context）

在引入 C++ 构建支持（以支持 `ArduinoCore-API` 编译）后，如果不加限制，默认的 C++ 运行库和语言特性将带来巨大的 Flash 空间与 RAM 消耗（通常膨胀 150KB - 500KB）。
此外，Arduino 类库（如 `String` 和第三方外设库）频繁在运行时进行动态内存分配（`malloc`/`new`），这极易造成堆碎片化，甚至导致内核级 OOM（内存溢出）。

为了保证双 target（ESP32 和 Wasm 浏览器仿真）上的极致小巧性与确定性，必须在工具链编译参数、内存分配策略以及语言运行时行为方面确立一套强制性的 **C++ 裁剪与约束规范**。

---

## 决策结论（Decision）

### 1. 编译选项裁剪矩阵（Flags Matrix）
无论是使用 `xtensa-esp32-elf-g++` 还是 `em++` (Wasm Clang)，编译 `wink-micro-os` 项目中的 C++ 代码时，必须强制加入以下编译选项：

| 编译标志 | 作用 | 目的 |
|---|---|---|
| `-fno-exceptions` | 关闭 C++ 异常处理 | 彻底移除 C++ 异常栈回溯产生的 `.eh_frame` 和 `.gcc_except_table` 段，节省 100KB+ Flash |
| `-fno-rtti` | 关闭运行时类型识别 | 移除 `dynamic_cast` 和 `typeid` 依赖的类元数据符号，减小二进制体积 |
| `-fno-threadsafe-statics` | 关闭局部静态变量的线程安全锁 | 避免编译器自动生成 `__cxa_guard_acquire` 等锁原语，防止静默拉入底层 pthread/OS 线程库 |
| `-nostdlib++` | 不链接 C++ 标准库 | 强行切断对 `libstdc++` / `libc++` 的依赖，避免标准容器和流操作静默打包，所有基础数据结构自备 |

### 2. 内存防线与 Arena 堆分区
为了防止第三方 Arduino 库（尤其是 `String` 拼接）因内存泄露或碎片化导致 Wink 内核级崩溃，实施 **“双轨堆隔离”** 策略：

1. **内核堆（Kernel Heap）**：
   * Wink 核心与 DAL/PAL 驱动分配使用 FreeRTOS `heap_4`，具有固定配额，不受 Arduino 任务影响。
2. **Arduino 沙箱堆（Arduino Arena Heap）**：
   * **ESP32**：在 BSS 段静态分配一块专用内存区域（例如 32KB）作为 `arduino_arena_heap`，通过独立的分配器（如 TLSF）接管。所有重载的 `operator new`/`delete` 强制在此区域内分配。
   * **Wasm**：在 Wasm 虚拟 RAM 空间中指定一个独立内存池。
   * **OOM 政策**：如果沙箱堆内存耗尽，分配器不得返回 `NULL` 导致程序产生空指针随机 Crash，必须触发 `pal_panic(WINK_ERR_OUT_OF_MEMORY)` 实施**“快速失败”（Fail-Fast）**，以利于低代码开发者和仿真器诊断。

### 3. 自定义 Placement New 与 CXX 运行时 Stub
* **禁止引入 `<new>` 头文件**：在 `arduino_compat` 内部重载全局 placement new，防止标准库头文件泄露。
  ```cpp
  inline void* operator new(size_t, void* __p) noexcept { return __p; }
  inline void* operator new[](size_t, void* __p) noexcept { return __p; }
  ```
* **纯虚函数捕获 Stub**：
  必须实现全局的 `__cxa_pure_virtual()` 函数，内部调用 `pal_panic()`。若程序意外调用了未实现的纯虚函数，将立刻断点挂起，绝不执行未知指令。

---

## 后果与约束（Consequences & Constraints）

### 正面后果
* **极致体积控制**：经过裁剪后，启用 C++ 的基础运行时开销（C++ Runtime Overhead）接近 **0 字节**。引入的实际体积仅为 Arduino 兼容层本身的大小（约 15KB ~ 25KB）。
* **内存高安全性**：应用层的动态内存碎片绝不会污染内核，Wink 内核的稳定性与看门狗机制得到物理层面的保障。

### 约束与代价
* **标准库缺失**：开发人员无法使用 `std::vector`、`std::map`、`std::string` 等 C++ 标准库容器，必须使用 `ArduinoCore-API` 提供的 `String`、`RingBuffer` 等轻量替代品。
* **无异常机制**：代码中严禁出现 `try`、`catch`、`throw` 关键字，在编译期会被 `-fno-exceptions` 直接拦截报错。

