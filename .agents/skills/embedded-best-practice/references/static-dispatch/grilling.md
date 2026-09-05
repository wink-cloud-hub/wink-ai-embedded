# 架构评审与 ADR 挑战清单 (Grilling Q&A Checklist)

在对 WinkMicroOS 固件进行架构变更、新增外设驱动、或进行 ADR 评审时，架构委员会通常会提出以下深度挑战性问题。

本 Checklist 提供了对这些经典挑战的官方标准设计回答，用于自我审查或 grilling 答辩：

---

### 问题 1：如果一个项目有 8 个超声波传感器，如何枚举和批量处理它们，而不引入统一的基类句柄？
*   **架构回答**：
    *   我们使用 **X-Macros 模式** 在编译期生成设备声明与初始化遍历代码（参见 [templates.md](./templates.md) 形态 5）。
    *   在极少数确实需要在运行时动态遍历的场景下，由 Codegen 在 `device_tree.c` 中自动生成一个静态指针数组：
        ```c
        dal_ultrasonic_t* const s_ultrasonic_list[] = { &front_radar, &back_radar, &side_radar };
        #define ULTRASONIC_COUNT (sizeof(s_ultrasonic_list)/sizeof(s_ultrasonic_list[0]))
        ```
    *   这同样允许我们在不引入虚表指针（Vtable）和 runtime 动态类型反推（`container_of`）的情况下，通过普通 `for` 循环遍历调用静态命名 API `dal_ultrasonic_read(s_ultrasonic_list[i], &dist)`。

---

### 问题 2：如果前端连线拓扑在运行期发生变化，静态 Codegen 是否会失效？
*   **架构回答**：
    *   **是的，但这被显式定义为 Non-goal (非项目目标)**。
    *   Wink-AI 面向的是固化的智能硬件节点。硬件引脚的接线在出厂/部署后是不可改变的。如果用户在低代码画布上修改了拓扑，系统要求必须重新生成代码并重新编译部署。我们不为低频的“运行时重连”而妥协固件的静态安全与内存开销。

---

### 问题 3：如果 WebAssembly 仿真运行环境是单线程的，如何证明真机（如双核 ESP32）上的并发与竞态没有问题？
*   **架构回答**：
    *   **仿真环境不能证明并发安全，真机并发安全由 Target-specific 测试和 Host 单测保证**：
        1.  **Wasm 仿真（UniSim）只保证功能性与业务状态机时序的正确性**。
        2.  **OS 级竞态由 PAL 隔离**：在 Wasm 上，`pal_mutex` 退化为空操作，而在 ESP32 (FreeRTOS) Target 上则是真实的 mutex 互斥锁。
        3.  **测试手段分流**：并发竞态测试通过真机上的 Stress Test（压测任务），以及在 Host 端（PC 平台）编译支持 ThreadSanitizer (TSan) 的单测固件进行动态竞态检测。

---

### 问题 4：如果某个 DAL API 需要回调（如 GPIO 中断），如何声明回调执行上下文以保证安全？
*   **架构回答**：
    *   驱动头文件必须明确在 API 契约（API Contract）中注明 `Callback-context` 的类型（参见 [contracts.md](./contracts.md)）。
    *   若注明为 `ISR context`，则调用者编写的回调函数**禁止包含任何延时、阻塞或堆分配操作**，只能调用发送信号量的非阻塞 PAL。
    *   若注明为 `Task context`，则说明该回调由驱动内部工作线程调度，允许进行普通业务处理。

---

### 问题 5：如果某个 PAL 硬件实现必须阻塞，谁来负责将其封装成非阻塞的 API？
*   **架构回答**：
    *   **由该外设的 DAL 驱动实现负责封装**。
    *   如果底层物理读取极其缓慢且没有 DMA 支持，DAL 驱动内部必须建立一个独立的 RTOS 工作线程（Worker Thread）与消息队列。
    *   当上层调用 DAL 读写 API 时，API 内部只是非阻塞地向队列发送一个请求，然后立刻返回 `WINK_OK`。当工作线程完成后，通过静态回调或事件通知上层，禁止将阻塞传播到业务层（App）。

---

### 问题 6：如果 NVS（持久化闪存）配置损坏，静态生成的设备树如何安全初始化与降级？
*   **架构回答**：
    *   我们在 `device_tree.c` 中生成的设备结构体不仅包含引脚，还包含了默认的安全配置值（配置与状态分离设计，参见 [lifecycle.md](./lifecycle.md)）。
    *   当系统启动读取 NVS 失败时，初始化逻辑必须将 `device_tree` 中的默认 `const` 配置复制到状态寄存器中，并返回降级状态码 `WINK_ERR_CONFIG_CORRUPT_DEGRADED`（-50，ADR-0005）。固件决不能因为闪存损坏而彻底瘫痪，必须降级运行在安全基线上。

---

### 问题 7：静态分发下，随着外设种类增多，如何避免全局命名空间冲突和“代码膨胀”？
*   **架构回答**：
    *   **名字治理**：所有 API 强行遵守 `dal_[器件类型]_[动作]` 前缀（如 `dal_rc_servo_set_angle`）。
    *   **代码隔离**：每种逻辑外设拥有独立的子目录和 `.c`/`.h`，不提供“万能驱动”。
    *   **按需链接**：虽然静态全局设备很多，但 CMake 会根据 Codegen 生成的 `device_tree.c` 实际引用的外设，依靠编译优化（`-ffunction-sections -fdata-sections -Wl,--gc-sections`）自动剔除未使用的外设驱动，真机固件体积并不会因外设增多而膨胀。
