# 仿真保真分级与错误注入策略 (Simulation & Error Injection)

在 Wink-AI 平台中，Wasm 仿真（UniSim）的性能和可靠性直接决定了“低代码运行体验”。为兼顾 Web 渲染的高帧率与安全代码的严格测试，项目对仿真分流和测试提出了明确的保真分级与注入方法。

> ⚠ `js_sim_*` 桥接（Wasm ↔ JS 内存写入）属**安全沙箱范畴**，完整约束见 `docs/design/07-platform-governance/03-security-sandbox.md`；本页只讲保真分级与错误注入（跨界写入契约见 §3.4）。

---

## 1. 仿真保真度分级 (Simulation Fidelity Levels)

项目根据不同的应用场景和测试深度，将仿真保真度划分为以下五个等级（L0 - L4）：

| 级别 | 名称 | 仿真机制 | 适用典型场景 | JS-Wasm 开销 |
| :--- | :--- | :--- | :--- | :--- |
| **L0** | **纯 Mock 仿真 (Mock API)** | 仅保证 API 可链接，返回固定伪数据（如温度永远返回 25℃） | 引导编译、框架冒烟测试 | 极低 |
| **L1** | **行为物理仿真 (Behavioral)** | 结合网页前端 3D 渲染器（如 Three.js Raycaster）返回合理的仿真数据 | Web 网页端用户可视化交互、小车避障路线演练 | 低（按采样周期触发） |
| **L2** | **时序近似仿真 (Timing-approx)** | 模拟真实的物理测量时序（如超声波 trigger→echo 脉宽往返，含 30ms 超时保护） | 验证 App/BAL 层是否因耗时操作发生看门狗复位或实时调度冲突 | 中等 |
| **L3** | **错误注入测试 (Error Injection)** | **[核心]** 支持在仿真侧动态注入物理总线错误、超时、随机丢包 | **自动化 CI/CD 测试**，验证 AI 代码的异常恢复和安全机制 | 低 |
| **L4** | **HIL 对齐仿真 (HIL Alignment)** | 使用真实物理设备的数据记录（Data Replay）作为仿真源输入 | 精密控制算法参数校准、物理极限数据测试 | 较低 |

---

## 2. 仿真直通旁路 (DAL Value Bypass) 运行机制

传统电平级仿真（在 Wasm 沙箱中高频翻转虚拟 GPIO 引脚）会由于高昂的 JS-Wasm 跨界 IPC 调用导致网页卡死。项目采用 **DAL 语义直通旁路**（L1级物理仿真）：

*   **真机路径**：编译时走真实驱动，控制 GPIO 脉冲时序。
*   **仿真路径**：编译时引入宏 `#ifdef SIMULATION`，在**最低物理信号层**旁路——通过 JS 导入函数（JS Import）向前端 Three.js 索取超声波的 **trigger 时序与 echo 脉宽（μs）**；其上的脉宽换算、超时判定、错误处理与真机分支**同源**（ADR-0003 决策2 / c-code.md §2）。旁路的只是物理信号**来源**，不是测距语义。

### 双模直通实现模板：

```c
/* dal_ultrasonic.c */
#include "dal_ultrasonic.h"
#include "pal_osal.h"

/* 两端共享：超时阈值 + 脉宽换算（换算/超时不属旁路之列，与真机同源） */
#define ULTRASONIC_TIMEOUT_US 30000u
float dal_pulse_us_to_cm(uint32_t pulse_us);

#ifdef SIMULATION
// JS 侧导入的直通仿真 API：仅旁路 trigger 时序与 echo 脉宽（最低物理信号层），
// 其上的脉宽换算 / 超时 / 错误处理与真机分支同源 (ADR-0003 决策2 / c-code.md §2)。
// extern 签名抄自 wasm_bridge.h（SSOT 闭合）。
#include "wasm_bridge.h"
extern void     js_sim_trigger_ultrasonic(uint16_t trig_pin);     /* 触发，无返回量 */
extern uint32_t js_sim_measure_echo_pulse_us(uint16_t trig_pin);  /* 返回 echo 脉宽 μs */

wink_status_t dal_ultrasonic_read(dal_ultrasonic_t *dev, float *distance_cm) {
    if (dev == NULL || distance_cm == NULL) return WINK_ERR_INVALID_ARG;

    /* 1. trigger 时序旁路（真机侧为 GPIO 10us 脉冲） */
    js_sim_trigger_ultrasonic(dev->trig_pin);

    /* 2. echo 脉宽测量旁路（真机侧为 while 循环测高电平） */
    uint32_t pulse_us = js_sim_measure_echo_pulse_us(dev->trig_pin);
    if (pulse_us >= ULTRASONIC_TIMEOUT_US) return WINK_ERR_TIMEOUT;

    /* 3. 换算：与真机分支同源 */
    dev->last_distance = dal_pulse_us_to_cm(pulse_us);
    *distance_cm = dev->last_distance;
    return WINK_OK;
}
#else
wink_status_t dal_ultrasonic_read(dal_ultrasonic_t *dev, float *distance_cm) {
    /* 真机：trigger 10us → 等 echo 上升沿 → 测高电平脉宽。
       超时判定、脉宽换算、错误处理与上方 SIMULATION 分支逐行同源（仅物理信号来源不同）。 */
}
#endif
```

---

## 3. 错误注入策略 (Error Injection Strategy)

为了在 CI 自动化流水线中验证 AI 控制代码对于物理故障的容错能力（如：传感器突然掉电、I2C 线上有噪波、信号线断路），我们必须在仿真层提供**错误注入接口**。

### 3.1 注入实现机制
1.  在 JS 仿真管理器中维护一个“错误注入寄存器”（例如 `window.simMockErrors`）。
2.  在 CI 脚本中，可以通过 JS 执行 `simSetDeviceError("front_radar", WINK_ERR_TIMEOUT)`。
3.  当 Wasm 执行到 `js_sim_measure_echo_pulse_us` 时，JS 端拦截：注入**超阈值脉宽**（≥ `ULTRASONIC_TIMEOUT_US`，30ms）或直接注入超时语义，使 C 侧在换算前即返回 `WINK_ERR_TIMEOUT`（`-2`），不再去计算 3D 碰撞射线。
4.  C 代码接收到非 `WINK_OK` 错误码后，开始执行错误恢复。

### 3.2 错误注入 JS 桩模板：

```typescript
// 仿真管理器 JS 侧实现（trigger/echo 两段式旁路 —— 与 dal_ultrasonic.c 同源）
const ULTRASONIC_TIMEOUT_US = 30000;              // 30ms 超时（与 C 侧 ULTRASONIC_TIMEOUT_US 同源）
const simMockErrors: Record<string, number> = {}; // 错误注入寄存器：注入超阈值脉宽

// CI 通过该 API 注入物理故障：注入「超阈值脉宽」→ C 侧换算前即判 WINK_ERR_TIMEOUT
function inject_device_error(deviceName: string) {
    simMockErrors[deviceName] = ULTRASONIC_TIMEOUT_US; // 注入 ≥ 超时阈值的脉宽
}

// JS Import 1：触发超声波（对应 C 侧 js_sim_trigger_ultrasonic，无返回量）
function js_sim_trigger_ultrasonic(trigPin: number): void {
    // 前端 3D 渲染器据 trigPin 发射射线、准备 echo 回波
}

// JS Import 2：测 echo 脉宽 μs（对应 C 侧 js_sim_measure_echo_pulse_us）
function js_sim_measure_echo_pulse_us(trigPin: number): number {
    const deviceName = findDeviceNameByPin(trigPin);

    // --- 1. 检查是否存在错误注入（注入超阈值脉宽 → C 侧换算前即 WINK_ERR_TIMEOUT） ---
    if (simMockErrors[deviceName] !== undefined) {
        const injectedPulse = simMockErrors[deviceName];
        console.warn(`[Sim-Error-Injection] Injecting pulse ${injectedPulse}us (>= timeout) to ${deviceName}`);
        return injectedPulse;   // 返回 ≥ 超时阈值的脉宽，由 C 侧统一判超时
    }

    // --- 2. 正常无错误注入：由 3D 碰撞射线换算 echo 脉宽（往返时间） ---
    const distanceCm = calculate3DRaycastDistance(deviceName);
    return cmToEchoPulseUs(distanceCm);   // 返回脉宽 μs，换算仍由 C 侧 dal_pulse_us_to_cm 同源完成
}
```

### 3.3 C 固件业务层测试代码：
```c
/* 验证业务层是否具有容错设计 */
void check_radar_avoidance(void) {
    float dist = 0.0f;
    wink_status_t status = dal_ultrasonic_read(&front_radar, &dist);
    
    if (status == WINK_ERR_TIMEOUT) {
        // ✓ L3 错误注入成功验证：传感器超时时，小车必须紧急停车
        emergency_stop();
        set_status_led(COLOR_RED);
    } else if (status == WINK_OK) {
        process_normal_avoidance(dist);
    }
}
```

### 3.4 `js_sim_*` 跨界写入契约（安全沙箱）

`js_sim_*` 桩向 Wasm 线性内存写物理量时，**必须走 Emscripten 的类型化 heap 抽象**，禁止裸地址算术——否则内存布局变化会导致野写、且无运行时检查：

```typescript
/* ✓ 走 Emscripten 类型化 API（ccall/cwrap 返回值，或 setValue / HEAPF32） */
Module.setValue(distancePtr, simDistance, 'float');          // 单值
Module.HEAPF32.set(float32Array, distancePtr >> 2);          // 批量

/* ❌ 禁止：用 HEAP8.buffer + 偏移做裸地址运算 */
const view = new Float32Array(Module.HEAP8.buffer, distancePtr, 1); // 禁
view[0] = simDistance;
```

规则：
1. **优先**让 `js_sim_*` 通过返回值传状态 + 用 `Module.setValue` / `HEAPF32.set` 写数据，而非手算偏移。
2. **禁止** `js_sim_*` 侧自行用 `Module.HEAP8.buffer` + 偏移做地址运算。
3. 一旦 Wasm 线性内存布局变化（增删全局、改导出），`distancePtr` 等必须由 Emscripten 重新导出的符号获取，绝不硬编码。

> 完整沙箱边界（`js_sim_*` import 数量治理、多线程/异步路径越界、野写防护）见 [03-security-sandbox.md](../../../../../docs/design/07-platform-governance/03-security-sandbox.md)。
