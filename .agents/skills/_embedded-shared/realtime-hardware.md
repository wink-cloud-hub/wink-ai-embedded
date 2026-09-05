# 实时性与硬件交互 + 双 Target 同源（范式无关）

> 适用范围：两种架构风格共用。核心决策见 **ADR-0002**（双 target 编译）、**ADR-0003**（仿真保真边界）。

---

## RTC / 事件驱动框架合规

采用事件驱动框架 / RTC（Run-To-Completion，运行至完成）执行模型时，**整条调用链必须非阻塞**：

1. **活动对象**处理一个事件后立即返回。
2. 事件处理路径**无阻塞调用**。
3. **无忙等循环**。
4. 事件处理路径**不等 mutex**。
5. **延迟处理**——可能阻塞的操作交给内部工作线程。

> chigo-micro 的 PID 跑在 `esp_timer` 1kHz 回调里，就是 RTC 思想的快路径体现：回调必须运行至完成、不阻塞。

### 最坏执行时间（WCET）预算（强制 RTC 路径）

每个 RTC 事件处理器必须有明确的 WCET 预算，超过预算断言失败：

```c
/* ✅ 正确：WCET 检查 */
void pid_timer_callback(void *arg)
{
    uint32_t start = get_cycle_count();
    
    pid_calculate(&g_pid);  /* 核心逻辑 */
    
    /* WCET 预算：500us */
    uint32_t elapsed = get_cycle_count() - start;
    ASSERT_MSG(elapsed < (CPU_FREQ_HZ / 2000), 
               "PID WCET exceeded: %lu cycles", elapsed);
}
```

> 预算表：
> - 1kHz PID 回调：< 500us
> - 100Hz 安全任务：< 5ms
> - 普通事件处理器：< 10ms

---

## 非阻塞驱动模式：工作线程 + 消息队列 + 回调

与硬件交互的驱动**必须在内部实现非阻塞**：

```
应用调 driver->send(data, len)
    → 请求入 消息队列（公共 API 立即返回）
        → 内部 工作线程 出队执行（此处可阻塞，因为是工作线程）
            → 硬件操作
                → 回调通知应用（结果 / 错误）
```

要点：公共 API 入队后立即返回；工作线程由驱动内部创建；周期性轮询留在工作线程内（不对外暴露 poll 接口）；结果/错误走回调；应用感知不到工作线程的存在；内部细节不出现在头文件。

---

## DMA 圆形 + 环形缓冲 + D-Cache（非阻塞 UART 接收架构）

```
硬件 UART → DMA 自动搬运 → 环形缓冲 → 应用读取
              ↑ Idle-Line / 半传输 / 全传输中断释放信号量唤醒任务
```

5 个关键组件：
1. **环形缓冲**：DMA 写、应用读，读写索引分离——**无需加锁**。
2. **DMA Circular 模式**：硬件搬运，CPU 不参与。
3. **UART Idle-Line 检测**（`HAL_UARTEx_ReceiveToIdle_DMA`）：支持变长接收。
4. **ISR 只释放信号量**，不处理数据。
5. **读写索引分离**：ISR 更新写索引，应用任务更新读索引。

**D-Cache（Cortex-M7）**：读 DMA 数据前**必须失效缓存行** `SCB_InvalidateDCache_by_Addr(...)`，否则 CPU 读到陈旧缓存。替代：把 DMA 缓冲放进 non-cacheable 区（MPU 或链接脚本）。
**错误恢复**：`HAL_UART_ErrorCallback` 里 abort DMA → 重置环形 → 重启 DMA → 错误回调通知上层。

---

## 寄存器级验证清单（任何 HAL 调用前）

> 「验证到底层」——绝不假设某 API 是非阻塞的，沿调用链查到寄存器。

- [ ] 寄存器地址 / 位域定义正确
- [ ] 需要时用正确的读-改-写（RMW）序列
- [ ] 所有硬件寄存器指针用 `volatile`
- [ ] 外设访问宽度正确（8/16/32-bit）
- [ ] 寄存器访问间有必要延时
- [ ] 访问前已启用外设时钟
- [ ] 引脚复用 / GPIO 复用功能已配置

---

## 看门狗作设计原语（非仅「喂狗」）

看门狗不是事后补丁，而是**设计期约束**：

- 每个关键任务 / 状态都有自己的超时预算——看门狗复位 = 某任务卡死，**可定位**而非黑盒重启。
- 推荐**分层**：IWDG 兜底全局、WWDG 监控关键时序任务（窗口看门狗）。
- 喂狗必须放在「任务正常推进的**证据点**」（如主循环成功处理一帧后），不能放在无脑定时器里
  ——后者任务卡死仍喂狗，看门狗形同虚设。
- 复位后读取并记录复位源（RCC CSR / reset reason），事后可区分 WDG / 软复位 / 上电。

> 这是 [safety-checklist.md](./safety-checklist.md) 阶段 9「所有路径喂看门狗」的设计化落地。

### 窗口看门狗（WWDG）使用规范

WWDG 不仅监控超时，还监控执行过快：

```c
/* ✅ 正确：窗口看门狗配置 */
#define WWDG_WINDOW_US     (900)   /* 喂狗不能早于 900us
#define WWDG_TIMEOUT_US    (1000)  /* 喂狗不能晚于 1000us

void safety_task(void *arg)
{
    uint32_t last_feed = get_time_us();
    
    while (1) {
        check_safety();
        
        uint32_t now = get_time_us();
        uint32_t elapsed = now - last_feed;
        
        /* 窗口检查：既不能太快也不能太慢 */
        ASSERT(elapsed >= WWDG_WINDOW_US, "Task running too fast!");
        ASSERT(elapsed <= WWDG_TIMEOUT_US, "Task timeout!");
        
        wdt_feed();
        last_feed = now;
        
        task_delay_ms(1);
    }
}
```

> 窗口看门狗能检测：
> 1. 任务卡死（超时未喂狗）
> 2. 任务异常快执行（可能被错误优先级抢占）
> 3. 任务周期抖动过大

---

## 中断延迟测量机制（Debug 构建启用）

关键中断路径必须内置延迟测量：

```c
/* ✅ 中断延迟测量 */
void uart_isr(void)
{
    /* 记录中断触发时间
    uint32_t irq_time = get_cycle_count();
    
    /* 释放信号量，唤醒工作线程 */
    semaphore_release_from_isr(&g_uart_sem);
    
    /* 记录中断结束时间 */
    g_uart_isr_duration = get_cycle_count() - irq_time;
    
    /* ISR 自身预算：< 5us */
    ASSERT(g_uart_isr_duration < (CPU_FREQ_HZ / 200000);
}

void uart_worker_task(void)
{
    while (1) {
        semaphore_wait(&g_uart_sem);
        
        /* 测量调度延迟：中断触发到任务被唤醒的时间 */
        uint32_t wake_time = get_cycle_count();
        g_sched_latency = wake_time - g_last_irq_time;
        
        /* 调度延迟预算：< 100us */
        ASSERT(g_sched_latency < (CPU_FREQ_HZ / 10000);
        
        process_uart_data();
    }
}
```

---

## 持久配置 / NVS 加载校验

上电读取持久配置（引脚映射、校准、安全阈值）时，**绝不信任存储内容**：

- 每条配置带 **版本号 + magic + CRC**；加载失败 → 回退**安全默认**，绝不带脏值运行。
- 字段逐一**范围 / 类型校验**（属外部输入，见 [clean-code.md](./clean-code.md) 防御式编程）。
- 关键安全阈值（过流 / 过温）若 NVS 损坏，落到**最保守**安全值，而非「读不到就用 0」。
- 配置结构跨 target 持久化时用显式 marshal，禁位域（见所在 skill 的模板文档）。

### NVS 磨损均衡设计（频繁更新的配置）

频繁更新的配置（如用户偏好、校准值）必须实现磨损均衡：

```c
/* ✅ 磨损均衡：多槽位循环写入 */
#define NVS_CONFIG_SLOTS     (4)   /* 4 个槽位，写入负载分摊 */
#define NVS_MAGIC            (0x5A5A)

typedef struct {
    uint16_t magic;
    uint16_t version;
    uint16_t sequence;       /* 序列号，最大的是最新有效 */
    uint16_t crc16;
    /* 配置数据... */
} nvs_config_slot_t;

nvs_config_slot_t g_config_slots[NVS_CONFIG_SLOTS];

/* 写入时：找序列号最大的，写入下一个槽位 */
void nvs_write_config(const nvs_config_slot_t *config)
{
    /* 1. 找到当前最大序列号的槽位 */
    uint16_t max_seq = find_max_sequence();
    
    /* 2. 写入下一个槽位（循环）
    uint8_t next_slot = (find_latest_slot() + 1) % NVS_CONFIG_SLOTS;
    
    g_config_slots[next_slot].sequence = max_seq + 1;
    g_config_slots[next_slot].crc16 = calculate_crc16(/*...*/);
    
    /* 3. 真正写入 Flash */
    flash_write(&g_config_slots[next_slot]);
}
```

> Flash 擦写次数有限（ESP32 约 10 万次），磨损均衡可将写入寿命提升 N 倍（N=槽位数）。

---

## ⭐ 双 Target 同源编译（ADR-0002）

本项目铁律：**一份 C 源码同时编译为 `wasm32`（Emscripten/Asyncify，浏览器仿真）与 xtensa（ESP-IDF 真机）**，且都过 `-Wall -Wextra -Werror` 零警告。这是「仿真→烧录行为一致」（虚实同源）的技术根。

**硬约束：**
- 跨平台结构体**禁用 `#pragma pack` 与位域**——wasm 与 xtensa 不在边界共享原始结构体内存，不要强行对齐布局。
- **不用 clang-only / GCC-only 特性**；两套工具链都过。
- ISR 签名在**编译期**强制一致。
- **`#ifdef SIMULATION` 隔离要尽可能收窄**——只旁路最低层物理信号源，协议解析 + CRC + 错误检测在仿真与真机间**共享**（ADR-0003 决策 2）。隔离放得越靠下，被同源测试的协议代码越多。

**chigo-micro 对照**：同样是「一份源码两 target」——`platform_esp32.c`（真机）与 `sim/platform_sim.c`（PC 仿真）由 CMake 源列表二选一链接，`platform.h` 接口不变。

> ⚠ 已知风险（ADR-0002）：Asyncify 栈税（`ASYNCIFY_STACK_SIZE` 深嵌套可能不够）、Wasm 单线程无法干净表达 FreeRTOS 抢占多任务、Wasm 中断桥接把函数指针转 `uint32_t` 来回（健壮性待验证）。这些是 sim/real 同源的已知薄弱点。

---
> **源出（溯源）**：zhaoming `hardware-interaction.md`、ADR-0002（`docs/design/decisions/0002-dual-target-compilation.md`）、ADR-0003、chigo-micro `platform.h` + `sim/`。
