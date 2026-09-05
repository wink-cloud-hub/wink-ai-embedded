# DAL / PAL 接口契约规范 (API Contracts)

为了解决静态分发下由于缺乏接口强继承约束，导致 AI 生成代码时随意发明 API、吞错误码、或引入多线程与中断安全隐患的问题，项目推行**“显式接口契约模板”**。

每个公共的 DAL 和 PAL 接口必须在头文件中使用统一的**结构化契约注释注释**。该契约不仅约束人类开发者，更作为 AI 编写逻辑和 Linter 验证的行为准则。

---

## 1. 结构化契约 Doxygen / YAML 模板

> ⚠️ **注意**：以下代码块仅作为“Doxygen / YAML 注释书写规范”的**格式范例**，并非系统中真实的 API 运行定义。真实 API 细节以 active header 和 Device Model 为准。

所有 DAL / PAL API 在头文件中必须包含以下格式的注释块（推荐使用 YAML Block 形式嵌入 Doxygen 中，方便 AI 提取和 Codegen 解析）：

```c
/**
 * @brief [API 功能简述，例如：读取超声波当前测量距离]
 *
 * @param dev 实例句柄
 * @param distance_cm 输出距离 (单位: cm)
 * @return wink_status_t 执行状态 (0 为成功，负数为具体错误码)
 *
 * @note API Contract:
 *   - Blocking: Yes (MAX 30ms timeout, relies on software polling loop)
 *   - Thread-safe: No (requires external mutex lock if accessed by multiple tasks)
 *   - ISR-safe: No (contains blocking delay, must NOT be called from ISR)
 *   - Callback-context: N/A
 *   - Input-range:
 *       - dev: Not NULL, must be fully initialized.
 *       - distance_cm: Not NULL, memory must be pre-allocated.
 *   - Error-codes:
 *       - WINK_OK: Success.
 *       - WINK_ERR_INVALID_ARG: dev or distance_cm is NULL.
 *       - WINK_ERR_TIMEOUT: Sensor didn't echo within 30ms.
 *   - Preconditions:
 *       - dal_ultrasonic_init(dev) must be called and return WINK_OK.
 *   - Postconditions:
 *       - dev->last_distance is updated with the returned distance_cm on WINK_OK.
 */
wink_status_t dal_ultrasonic_read(dal_ultrasonic_t *dev, float *distance_cm);
```

### 契约字段解释说明：

*   **Blocking (阻塞行为)**：
    *   `No`：立即返回，绝不在函数内死等或调用任何 `delay`。
    *   `Yes (参数说明)`：说明最大阻塞时间、是否带有超时参数。对于硬实时任务，AI 只能选用 `Blocking: No` 的 API。
*   **Thread-safe (线程安全性)**：
    *   `Yes`：内部具有互斥保护，可在多个 RTOS 任务间无锁调用。
    *   `No`：无内部锁保护，并发调用必须由上层（App/BAL）通过外部互斥锁进行同步。
*   **ISR-safe (中断安全)**：
    *   `Yes`：可以在硬件中断服务函数（ISR）中直接调用。这意味着函数内部绝对不能调用任何会引起 RTOS 上下文切换或睡眠的锁（如 `pal_mutex_lock`）或 `pal_delay`。
    *   `No`：绝对禁止在 ISR 中调用，否则会导致内核奔溃或死锁。
*   **Callback-context (回调执行上下文)**：
    *   如果 API 允许注册回调函数（例如按键触发），必须指明该回调是在哪个线程/中断中执行的：
        *   `ISR context`：在硬件中断中执行（要求回调函数极快、无阻塞、无堆分配）。
        *   `RTOS Task context`：在驱动内部的工作线程中异步执行。
*   **Input-range (参数边界)**：
    *   明确限制指针、数值边界（如角度必须在 `0.0f` ~ `180.0f`）。AI 在生成实现代码时，**必须在函数开头校验这些边界**。
*   **Error-codes (错误码集合)**：
    *   列出所有可能的负数错误码。禁止吞掉中间调用的任何错误，遇到非预期状态必须透传错误码。
*   **Preconditions / Postconditions (前置与后置条件)**：
    *   调用该 API 前对象必须满足的状态，以及调用成功后对象发生的实质改变，协助 AI 建立逻辑时序。

---

## 2. 长期演进：元数据定义规范 (YAML IDL)

为彻底消灭 AI 在生成驱动和 JS 仿真桥接代码时的“签名冲突”，项目在 `07-platform-governance/` 中建立了设备树接口定义（IDL）元数据机制。

> ⚠️ **注意**：下方展示的 YAML 结构为**定义规范的 Schema 范例**。系统真实的设备元数据请到项目对应的元数据目录或 `docs/design/07-platform-governance/01-device-model-registry.md` 中查阅。

每一个外设在代码生成器中均由一个 `.yaml` 声明定义。Codegen 通过该 YAML 文件作为 **SSOT（单一事实源）**，自动生成对应的 `.h` 头文件以及 TypeScript/JavaScript Wasm 仿真接口，确保三方签名 100% 绝对一致：

```yaml
# dal_ultrasonic.yaml 范例（非系统真实配置，仅作格式参考）
device_type: dal_ultrasonic
description: "Ultrasonic range sensor"
attributes:
  - name: trig_pin
    type: uint16_t
    access: const
  - name: echo_pin
    type: uint16_t
    access: const
  - name: last_distance
    type: float
    access: mutable

apis:
  - name: dal_ultrasonic_read
    blocking: true
    blocking_max_ms: 30
    thread_safe: false
    isr_safe: false
    inputs:
      - name: dev
        type: "dal_ultrasonic_t *"
        nullable: false
    outputs:
      - name: distance_cm
        type: "float *"
        nullable: false
    returns: wink_status_t
    errors:
      - WINK_ERR_INVALID_ARG
      - WINK_ERR_TIMEOUT
```

> **AI 编码守则**：AI 在尝试编写任何 DAL/PAL 层面的 C 代码或仿真旁路桥接代码前，**应优先查阅该外设的 YAML 声明或 API 契约块**，严禁臆造任何未在契约中注册的字段和函数参数。
