# 高保真波形与四大通道 API 指南 (High-Fidelity & APIs Guide)

> **目标**：指导开发者如何使用底层硬件 API 与波形注入机制，实现微秒级高保真仿真与 100% 确定性。  
> **面向对象**：编写高精度时序外设、物理传感器、协议总线外设及 Display 设备的插件开发者。  
> **入口索引**：[README.md](./README.md) | **底层 SSOT 规范**：[unified-peripheral-channel-architecture.md](../../packages/unisim/docs/design/unified-peripheral-channel-architecture.md) | [07-peripheral-registry.md](../../../wink-ai-embedded/docs/design/04-wasm-simulation/02-mechanisms/07-peripheral-registry.md)

---

## 🏛️ 1. 高保真仿真核心设计原则

在 UniSim 3.1 仿真体系中，高保真仿真（High-Fidelity Simulation）遵循以下底层硬原则：

1. **绝对虚拟时间坐标系 (`VirtualClock`)**：所有时间戳统一使用 `this.ctx.nowUs()`（返回 `bigint` 微秒），严禁使用宿主机真实时间（`Date.now()` / `performance.now()` / `setTimeout`），确保在任何性能的机器上运行结果逐字节一致（Deterministic）。
2. **C 驱动单一数据源 (C-Driven SSOT)**：通过 `injectWaveform` 将波形边沿预加载推入底层 C 事件队列（512 容量环形缓冲区），由 C 仿真引擎在微秒时刻精准排空并同步反向回调 `js_pal_notify_pin_edge` 翻转 `PinArbiter` 电平。
3. **PinArbiter 电气四态与多源仲裁**：引脚不仅支持 `0` (LOW) 与 `1` (HIGH)，还原生支持 `'Z'` (Hi-Z 高阻态/浮空) 与 `'X'` (CONFLICT 短路冲突)，配合 `SUPPLY / PULL / WEAK` 驱动强度实现真实的开漏线与 (Line-Wired AND) 仲裁。
4. **世代令牌与自动抢占 (Generation Token & Preemption)**：波形包含世代令牌 `generation`；每次触发新物理测量时自动递增，底层会自动作废并拦截前一世代尚未触发的延迟边沿。
5. **能力降级信息量保留 (Capability-Based Degradation)**：在 Behavioral（行为级）模式下，自动降级为使用 TS `VirtualClock.deferUs()` 产生边沿，**绝对保留微秒级脉宽语义**，严禁折叠丢弃脉宽信息。

---

## 🔌 2. 四大硬件通道 API 详细使用指南

UniSim 3.1 将底层物理抽象划分为四大硬件通道，开发者必须依据数据吞吐与物理类型选择正确的通道 API：

```text
                               UniSim 物理数据通道划分
 ┌─────────────────────────┬─────────────────────────┬──────────────────────┬───────────────┐
 │ 通道 1: 电平/波形 (GPIO) │ 通道 1b: 定时调制 (PWM) │ 通道 2: 协议总线     │ 通道 3: 模拟量 │
 │ (超声波Echo / 按键 / 编码)│ (SG90舵机 / 电机调速)   │ (I2C / SPI / UART)   │ (电位器 / NTC) │
 └─────────────────────────┴─────────────────────────┴──────────────────────┴───────────────┘
 ┌──────────────────────────────────────────────────────────────────────────────────────────┐
 │ 通道 4 / DMA 零拷贝帧传输 (WS2812 / OLED / SharedArrayBuffer 传输)                         │
 └──────────────────────────────────────────────────────────────────────────────────────────┘
```

---

### 2.1 通道 1：数字 GPIO / 电气四态波形 (`injectWaveform` & `writePin`)

用于超声波回声 (HC-SR04)、红外 NEC 编码、1-Wire (DS18B20)、旋转编码器等需要微秒级精确时序与电气状态的外设。

#### API 定义

```typescript
this.ctx.injectWaveform(pinName: string, waveform: Waveform, options?: WaveformInjectOptions): void;
this.ctx.writePin(pinName: string, level: LogicState | boolean): void;
this.ctx.readPin(pinName: string): LogicState;
```

#### 四值逻辑与驱动强度 (4-Value Logic & Drive Strength)

根据 `wink-ai-embedded` 底层 `PinArbiter` 规范，引脚逻辑状态定义如下：

```typescript
export type LogicState = 0 | 1 | 'Z' | 'X';
// 0: LOW  (低电平)
// 1: HIGH (高电平)
// 'Z': Hi-Z (高阻态 / 浮空)
// 'X': CONFLICT (多强驱动不一致引起的短路冲突)

export enum DriveStrength {
  SUPPLY = 3, // VCC/GND 直连或推挽 GPIO 强驱动
  PULL = 2, // 电阻上拉/下拉 (如 I2C 外部 4.7kΩ 上拉)
  WEAK = 1, // 弱内部上拉/开漏释放/浮空输入
}

export interface WaveformEdge {
  tUs: bigint; // 绝对虚拟时间戳 (VirtualClock.nowUs() 坐标系)
  level: LogicState; // 目标逻辑电平 (0, 1, 'Z', 'X')
  strength?: DriveStrength; // 驱动强度 (默认 SUPPLY)
}

export interface Waveform {
  edges: WaveformEdge[]; // 按 tUs 严格升序排列的边沿数组
  generation?: number; // 世代令牌，用于取消/抢占旧波形
}
```

#### WASM ABI 4 态数值映射表

跨 WASM 传输时，四态按 uint8 映射（与 C 语言 `pal_wasm_bridge.h` 100% 对齐）：

- `LOW (0)` ➔ `0` (`JS_GPIO_STATE_LOW`)
- `HIGH (1)` ➔ `1` (`JS_GPIO_STATE_HIGH`)
- `Hi-Z ('Z')` ➔ `2` (`JS_GPIO_STATE_HIZ`)
- `CONFLICT ('X')` ➔ `3` (`JS_GPIO_STATE_CONFLICT`)

#### 代码示例 1：超声波 Echo 回声波形注入

```typescript
const now = this.ctx.nowUs();
const responseDelayUs = 200n; // 硬件芯片逻辑响应延时
const echoPulseUs = 5831n; // 100cm 对应的双程往返时间 (微秒)

// 一次性注入 ECHO 上升沿与下降沿波形序列
this.ctx.injectWaveform('ECHO', {
  edges: [
    { tUs: now + responseDelayUs, level: 1 }, // 上升沿：回声开始
    { tUs: now + responseDelayUs + echoPulseUs, level: 0 }, // 下降沿：声波接收完成
  ],
  generation: ++this._echoGeneration, // 世代令牌严格递增
});
```

#### 代码示例 2：1-Wire (DS18B20) 开漏与 High-Z 释放

```typescript
// 响应 1-Wire Master 复位脉冲：释放引脚为 Hi-Z ('Z') 让外部上拉电阻拉高
this.ctx.injectWaveform('DQ', {
  edges: [
    { tUs: now + 15n, level: 0, strength: DriveStrength.SUPPLY }, // 拉低 DQ 产生 Presence 脉冲
    { tUs: now + 75n, level: 'Z', strength: DriveStrength.WEAK }, // 释放 DQ 为 Hi-Z 高阻态
  ],
});
```

> ⚠️ **容量与限流约束**：单次 `injectWaveform` 注入的边沿数不得超过 `128` 条（C 侧环形缓冲区容量上限为 `512`）。超过 128 条的长数据包应分批注入或升维至通道 4。

---

### 2.2 通道 1b：PWM 占空比与定时器调制 (`setPwmDuty` & `onDutyChange`)

用于 SG90 舵机、PWM 调光 LED、直流电机驱动板、无源蜂鸣器等受 PWM 占空比驱动的外设。

#### API 定义与监听

```typescript
// 插件向物理引脚输出 PWM 占空比 [0.0, 100.0]
this.ctx.setPwmDuty(pinName: string, dutyPercent: number): void;

// 监听固件 MCU 输出的 PWM 占空比变化（如 MCU 驱动舵机转动）
onDutyChange(channel: number, dutyPercent: number): void {
  // 根据 PWM 占空比换算出目标物理角度并 publish 给 Canvas 绘制
}
```

#### 代码示例（SG90 舵机占空比控制）

```typescript
// 将 90 度角转换为 PWM 占空比 (例如 20ms 周期内 1.5ms 高电平 = 7.5% 占空比)
const dutyPercent = angleToPwmDuty(90, this.props);
this.ctx.analogWrite('PWM', dutyPercent);
```

---

### 2.3 通道 2：总线协议 (I2C / UART / SPI)

用于 IMU 姿态传感器、OLED 屏幕、温湿度计、串口蓝牙等遵循标准总线协议的外设。

#### 2.3.1 I2C 从设备注册与线与仲裁 (`registerI2cDevice`)

```typescript
this.ctx.registerI2cDevice({
  address: 0x68, // MPU6050 7位 I2C 地址
  onTransfer(writeBuf: Uint8Array, readLen: number): Uint8Array {
    const regAddr = writeBuf[0];
    if (regAddr === 0x3b && readLen === 6) {
      // 固件读取 6 轴加速度传感器原始数据
      return new Uint8Array([0x00, 0x10, 0x00, 0x20, 0x00, 0x30]);
    }
    return new Uint8Array(0);
  },
});
```

#### 2.3.2 串口数据收发 (`writeUart` & `registerUartDevice`)

```typescript
// 向指定 UART 串口发送数据包 (如 NMEA GPS 数据帧)
const nmeaSentence = new TextEncoder().encode(
  '$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A\r\n',
);
this.ctx.writeUart(0, nmeaSentence);
```

#### 2.3.3 SPI 从设备注册与 CS 片选双线协同 (`registerSpiDevice`)

SPI 是主从式全双工总线。在 UniSim 3.1 架构中，SPI 遵循 **数据面 (`onFrame`) 与 控制面 (片选 CS 监听) 双线协同** 范式：

1. **数据面 (`onFrame`)**：处理全双工字节流移位传输（`tx` ➔ `rx` 1对1等长返回）。
2. **控制面 (片选 CS 边沿)**：在插件的 `onPinChange('CS')` 监听片选引脚；当 MCU 将 CS 拉高 (De-assert, HIGH 或 Hi-Z) 时，**重置从机内部命令解析缓冲区**，防止残余帧数据污染下一次传输。

#### 代码示例（W25Q64 SPI Flash 存储芯片）

```typescript
export class SpiFlashPlugin extends BaseSimulationPlugin<FlashState, FlashProps> {
  private commandQueue: number[] = [];

  protected onBound(): void {
    // 1. 数据面：注册 SPI 从设备
    this.ctx?.registerSpiDevice({
      deviceId: 'w25q64',
      onFrame: (txBuf: Uint8Array, mode: SpiMode): Uint8Array => {
        const rxBuf = new Uint8Array(txBuf.length);
        const cmd = txBuf[0];

        if (cmd === 0x9f) {
          // 0x9F: Read JEDEC ID
          rxBuf.set([0x9f, 0xef, 0x40, 0x18], 0);
        } else if (cmd === 0x03) {
          // 0x03: Read Data (从指定 24 位地址读取字节)
          const addr = (txBuf[1] << 16) | (txBuf[2] << 8) | txBuf[3];
          const data = this.readFlashMemory(addr, txBuf.length - 4);
          rxBuf.set(data, 4);
        }
        return rxBuf;
      },
    });
  }

  // 2. 控制面：监听 CS 片选脚引脚电平变化
  onPinChange(pinName: string, level: LogicState): void {
    if (pinName === 'CS') {
      if (level === 1 || level === 'Z') {
        // CS 被拉高 (De-assert)：MCU 结束传输，解绑使能，清空内部命令解析状态机
        this.commandQueue = [];
      }
    }
  }
}
```

---

### 2.4 通道 3：模拟量 ADC (`readAdc` & `analogWrite`)

用于旋钮电位器、光敏电阻、摇杆、NTC 热敏电阻等输出/接收连续模拟电压信号的外设。

#### API 定义与 C 侧退化机制

```typescript
// 读取模拟量输入引脚电压值 normalizedValue [0.0, 1.0]
const rawVal = this.ctx.readAdc(pinName);

// 写入模拟量输出 (如电位器滑动输出电压给 PinArbiter)
this.ctx.analogWrite(pinName: string, normalizedValue: number): void;
```

> 📌 **C 侧退化引擎**：JS 侧仅传递 `[0.0, 1.0]` 归一化浮点数；C 侧 `pal_wasm_adc.c` 会自动叠加 RC 低通滤波、高斯白噪声与预热采样判定（ADR-0057）。

---

### 2.5 通道 4：大缓冲区 / 帧 Payload 传输 (`writeWs2812` & FrameBuffer)

用于 WS2812 RGB 灯串、电子墨水屏、LCD 显示屏等传输大量帧 Payload 的外设。

#### 外设数据传输与内存模型 3 级选型规范：

| 层级                   | 代表外设                    | 数据吞吐 / 帧率        | 内存传输模型 (Memory Model)  | 说明                                          |
| :--------------------- | :-------------------------- | :--------------------- | :--------------------------- | :-------------------------------------------- |
| **Tier 1: 消息级**     | 按键、超声波、电位器        | < 100 Byte / 事件驱动  | **JSON / PostMessage**       | `ctx.publish()` 结构化消息，开发最简          |
| **Tier 2: 屏级块传输** | SSD1306 (128x64)            | 1KB ~ 32KB / 10~30 FPS | **Transferable ArrayBuffer** | I2C/SPI 传输结束在 STOP 信号触发所有权转移    |
| **Tier 3: 像素海量流** | ILI9341 TFT, 1024 颗 WS2812 | 32KB ~ 8MB+ / 60+ FPS  | **SharedArrayBuffer + DMA**  | Worker 与 UI 端到端零拷贝共享内存，无 GC 卡顿 |

#### 代码示例（WS2812 RGB 灯条数据传输）

```typescript
// 传输 RGB 字节数组 (3 字节 per LED) 到 WASM 线性内存
const rgbData = new Uint8Array([255, 0, 0, 0, 255, 0, 0, 0, 255]); // 红、绿、蓝 3 颗 LED
this.ctx.writeWs2812('DIN', rgbData);
```

---

## 🔄 3. Behavioral 模式能力降级与保护规范

当系统切换为 `behavioral`（行为模式）或无 WASM 的纯 TS 单测环境时，引擎执行**能力降级 (Capability-Based Degradation)**，但严守 **“绝对不破坏或丢失脉宽信息量”** 的硬铁律：

| 模式                          | `injectWaveform` 降级行为                    | 信息量保护与语义                                                                              |
| :---------------------------- | :------------------------------------------- | :-------------------------------------------------------------------------------------------- |
| **Timing + WASM (高保真)**    | 推入 C 侧 `pal_wasm_push_pin_event` 事件队列 | 微秒级 C 侧 ISR 中断 + Jitter-free 微观波形                                                   |
| **Behavioral (行为模式)**     | 使用 `VirtualClock.deferUs()` 产生双沿       | **能力降级**：不触发 C 队列，但**完整保留上升沿/下降沿脉宽 µs 测距数据**，绝对不折叠为 0 电平 |
| **Timing + 纯 TS (单测保底)** | 使用 `VirtualClock.deferUs()` 调度 TS 边沿   | 保障无 WASM 编译环境下的单元测试确定性运行                                                    |

---

## ⚡ 4. 控制面中断与全生命周期回调契约

### 4.1 中断引脚向 MCU 投递与 C-Pull 安全契约 (InterruptQueue)

当外设（如 MPU6050 传感器 FIFO 溢出中断、加速度计自由落体 INT、触摸屏 PENIRQ）需要向 MCU 引脚发送硬件中断信号时，**严禁在 JS 侧直接强行同步调用 WASM 的 ISR 函数指针**（这会破坏 Asyncify 恢复堆栈导致 `unreachable` 崩溃）。

UniSim 3.1 建立了基于 **[Axis D] `InterruptQueue` 中断队列 + C-Pull 轮询拉取** 的控制面契约：

```text
 ┌─────────────────────────┐                            ┌─────────────────────────┐
 │ 1. MCU 固件初始化 (C)   │ ── pal_gpio_attach_irq ──> │ 2. InterruptQueue (TS)  │
 │    注册 ISR 函数指针与 Pin│                            │    记录 (pin, isr_ptr)  │
 └─────────────────────────┘                            └─────────────────────────┘
                                                                     ▲
 ┌─────────────────────────┐                                         │ 引脚边沿触发
 │ 3. 外设插件 (JS)        │ ── injectWaveform('INT') ─> ┌─────────────────────────┐
 │    触发物理中断 (如FIFO满) │                            │ PinArbiter (TS)         │
 └─────────────────────────┘                            └─────────────────────────┘
                                                                     │
                                                                     ▼
 ┌─────────────────────────┐                            ┌─────────────────────────┐
 │ 5. 固件 ISR 安全执行    │ <─ dispatch_pending ────── │ 4. WASM C-Pull 轮询点   │
 │    (C 侧 ISR 上下文)    │   (js_pal_poll_interrupt)  │    (每个 1ms / Tick 边界)│
 └─────────────────────────┘                            └─────────────────────────┘
```

#### 外设开发者开发规范：

1. **外设无感化入队**：外设开发者**不需要也不应当直接操作 `InterruptQueue` 底层 API**。外设只需像真实芯片一样，使用 `injectWaveform` 或 `writePin` 驱动 INT 引脚翻转。
2. **底座自动托管**：底座框架的 `PinArbiter` 监听到 INT 引脚边沿后，会自动检测该 Pin 是否被 C 固件绑定了中断，并自动调用 `irqQueue.push(pin)` 入队。
3. **C 侧 Safe Context 出队**：C WASM 引擎在每个 1ms 量子边界通过 `js_pal_poll_interrupt()` 出队，在安全的 C 侧上下文内调用固件编写的 `isr(arg)`。

#### 实用代码范例（MPU6050 INT 触发）

```typescript
// 外设检测到 FIFO 达到水位线，向 MCU 发送低电平脉冲中断
private triggerFifoWatermarkInterrupt(): void {
  const now = this.ctx.nowUs();

  // 驱动 INT 脚产生 50us 低电平脉冲（自动触发 PinArbiter ➔ InterruptQueue ➔ C ISR 闭环）
  this.ctx.injectWaveform('INT', {
    edges: [
      { tUs: now, level: 0 },       // 下降沿：触发中断
      { tUs: now + 50n, level: 1 }, // 50us 后恢复高电平
    ],
  });
}
```

---

### 4.2 外设全生命周期图谱与 API 契约速查

根据 UniSim 稳定 ABI 契约规范（`plugin-abi.ts` & `peripheral-types.ts`），外设插件在整个仿真周期内遵循以下生命周期图谱：

```text
                        ┌──────────────────────────────┐
                        │  1. 实例创建 (Class Constructor)│
                        └──────────────┬───────────────┘
                                       │
                        ┌──────────────▼───────────────┐
                        │  2. onInit / beforeBind      │ 静态属性解析与引脚映射
                        └──────────────┬───────────────┘
                                       │
                        ┌──────────────▼───────────────┐
                        │  3. onReady (Async)          │ 异步预热/PLL锁定/总线握手
                        └──────────────┬───────────────┘
                                       │
                        ┌──────────────▼───────────────┐
                        │  4. onBound                  │ 发布初始 StateChannel
                        └──────────────┬───────────────┘
                                       │
             ┌─────────────────────────┴─────────────────────────┐
             │                  5. 仿真运行主循环                  │
             │  • onPinChange / onDutyChange    (引脚/PWM响应)   │
             │  • onEvent / _SET_XXX            (UI/手势刺激)    │
             │  • step(dt, simTime)             (物理微分步进)   │
             └──────┬────────────────────────────────────┬───────┘
                    │                                    │
    [用户点击系统复位] │                                    │ [修改属性面板参数]
                    ▼                                    ▼
       ┌────────────────────────┐            ┌────────────────────────┐
       │ 6. onReset             │            │ 7. onPropertyChange    │
       │ 重置物理状态机/清空FIFO  │            │ 动态更新物理参数       │
       └────────────────────────┘            └────────────────────────┘
                    │                                    │
                    └─────────────────┬──────────────────┘
                                      │
                       ┌──────────────▼───────────────┐
                       │ 8. serializeState             │ 保存工程/快照持久化
                       └──────────────┬───────────────┘
                                      │
                       ┌──────────────▼───────────────┐
                       │ 9. onDestroy / onPowerOff     │ 释放 releasePin / 解绑
                       └──────────────────────────────┘
```

#### 外设生命周期 API 契约速查表：

| 生命阶段       | Hook 函数签名                             | 触发时机与核心职责                                | 代码场景指导                                              |
| :------------- | :---------------------------------------- | :------------------------------------------------ | :-------------------------------------------------------- |
| **初始化**     | `beforeBind(ctx, pins, props)`            | 静态属性解析前触发                                | 用于新旧格式配置向下兼容转换                              |
| **预热收敛**   | `onReady?(): Promise<void>`               | **第 1 个 Tick 前 `await`**，解决上电非零初始瞬态 | 模拟传感器 PLL 锁定、I2C 上电稳定                         |
| **绑定完成**   | `onBound(ctx, pins, props)`               | 引脚映射与默认值解析完成                          | 返回 `Partial<State>` 覆盖初始 StateChannel               |
| **系统热复位** | **`onReset?(): void`**                    | **用户点击 MCU 复位或重新刷写固件**               | **清空 FIFO、重置世代令牌、恢复默认电平**（无需解绑管脚） |
| **属性变更**   | `onPropertyChange?(key, oldVal, newVal)`  | 用户在 Schema 面板修改参数                        | 动态重算物理参数（如重算分压比）                          |
| **事件响应**   | `onPinChange` / `onDutyChange`            | MCU 引脚翻转或 PWM 占空比更新                     | 驱动物理状态机或同步 Canvas 渲染                          |
| **物理步进**   | `step?(dtSeconds, simTimeMs)`             | 主引擎驱动的物理微分步进                          | 模拟二阶运动学、热传导微分方程                            |
| **快照持久化** | `serializeState()` / `deserializeState()` | 保存与加载工程快照                                | 序列化内部 FIFO 与 Schema 版本迁移                        |
| **销毁清理**   | `onDestroy()` / `onPowerOff()`            | 外设删除或仿真停止                                | 显式调用 `this.ctx.releasePin(pin)` 释放管脚              |

#### 工业级传感器（含全生命周期 Hook）标准编写范例

```typescript
export class Mpu6050Plugin
  extends BaseSimulationPlugin<MpuState, MpuProps>
  implements UnisimPluginABI
{
  readonly manifest = mpu6050Manifest;
  private fifoBuffer: Uint8Array[] = [];
  private isPllLocked = false;

  /** 1. 异步预热钩子：模拟传感器上电 10ms PLL 锁定 */
  async onReady(): Promise<void> {
    await this.ctx?.deferUs(10_000n); // 模拟 10ms 物理上电延迟
    this.isPllLocked = true;
  }

  /** 2. 系统热复位钩子：MCU 复位时清空物理状态 */
  onReset(): void {
    this._echoGeneration = 0;
    this.fifoBuffer = []; // 清空硬件 FIFO 缓冲区
    this.isPllLocked = true; // 热复位无需重新预热
    this.ctx?.releaseAllPins(); // 重置所有驱动电平为 Hi-Z
    this.publishInitialStates(); // 重置 StateChannel 默认值
  }

  /** 3. 属性变更钩子：响应面板修改量程 */
  onPropertyChange(key: string, _oldVal: unknown, newVal: unknown): void {
    if (key === 'gyroFsRange') {
      this.updateGyroScale(newVal as number);
    }
  }

  /** 4. 销毁钩子：释放资源 */
  onDestroy(): void {
    this.ctx?.releaseAllPins();
  }
}
```

---

## 🧪 5. 物理域算法 (src/physics/) 与真实世界 Edge-Cases 建模规范

外设插件目录下的 `src/physics/` 是**纯物理域算法模块 (Pure Physical Domain)**。它不依赖 DOM 或前端组件，负责**物理 ➔ 电气**与**电气 ➔ 物理**的双向公式换算，并专门负责**模拟真实世界的物理噪声、触点抖动及边缘 Case**，用以强力验证 MCU 固件侧 DAL/HAL 层的防抖、重试与容错算法。

```text
peripherals/builtin/button/1.0.0/src/
├── physics/
│   ├── button-bounce.ts          # 按键触点抖动 (Glitch/Bouncing) 算法
│   └── ntc-temperature.ts        # NTC B值曲线与高斯白噪声算法
└── simulation.ts                 # 仅处理事件绑定与 injectWaveform / publish 调度
```

---

### 5.1 边缘 Case 建模 1：按键机械触点抖动算法 (`button-bounce.ts`)

#### 物理背景

真实世界的机械触点在按下或释放瞬间，弹片由于机械弹性会有 3ms ~ 15ms 的高速弹跳（Bouncing Glitch）。若 MCU 固件没有做 10ms 软件防抖（Debounce）或者消抖算法写错，就会单次按压误触发多次外部中断。

#### 物理-电气算法代码 (`src/physics/button-bounce.ts`)

```typescript
export interface ButtonBounceOptions {
  bounceDurationUs?: bigint; // 抖动持续总时长 (默认 10,000us = 10ms)
  bounceCount?: number; // 抖动脉冲次数 (默认 4 次)
}

/**
 * 生成包含真实机械触点抖动 (Glitch) 的波形边沿数组
 * @param startUs 动作发起的虚拟绝对时刻
 * @param targetLevel 最终稳定目标电平 (0=LOW/按下, 1=HIGH/释放)
 */
export function generateButtonBounceEdges(
  startUs: bigint,
  targetLevel: 0 | 1,
  options: ButtonBounceOptions = {},
): WaveformEdge[] {
  const durationUs = options.bounceDurationUs ?? 10_000n; // 10ms 抖动
  const count = options.bounceCount ?? 4;
  const edges: WaveformEdge[] = [];

  const stepUs = durationUs / BigInt(count * 2);
  let currentLevel: 0 | 1 = targetLevel === 0 ? 1 : 0;

  // 生成 N 次高速高低翻转的触点抖动毛刺
  for (let i = 0; i < count * 2 - 1; i++) {
    edges.push({
      tUs: startUs + BigInt(i) * stepUs,
      level: currentLevel,
    });
    currentLevel = currentLevel === 0 ? 1 : 0;
  }

  // 最终达到稳定目标电平
  edges.push({
    tUs: startUs + durationUs,
    level: targetLevel,
  });

  return edges;
}
```

#### 在 `simulation.ts` 中消费物理抖动

```typescript
// 当用户点击 UI 按键触发 PRESS 事件时：
PRESS(): void {
  const now = this.ctx.nowUs();
  // 注入带有 10ms 真实触点抖动的波形，强力验证 MCU 固件防抖算法
  const bounceEdges = generateButtonBounceEdges(now, 0, { bounceDurationUs: 10_000n, bounceCount: 4 });
  this.ctx.injectWaveform('1.l', {
    edges: bounceEdges,
  });
}
```

---

### 5.2 边缘 Case 建模 2：NTC 温度传感器高斯白噪声算法 (`ntc-temperature.ts`)

#### 物理背景

真实温度传感器在采集中由于电源纹波与环境电磁干扰，采样电压存在 $\pm 0.5\%$ 的微小高斯波动。

```typescript
/**
 * 计算 NTC 归一化电压 [0.0, 1.0] 并叠加高斯白噪声
 */
export function calcNtcNormalizedVoltageWithNoise(
  tempCelsius: number,
  r25 = 10_000,
  bValue = 3950,
  noiseStdDev = 0.002, // 0.2% 电源噪声
): number {
  const tKelvin = tempCelsius + 273.15;
  const t25Kelvin = 298.15;
  const rNtc = r25 * Math.exp(bValue * (1 / tKelvin - 1 / t25Kelvin));

  // 分压电路：10k 上拉电阻
  const rPullUp = 10_000;
  const normVolt = rNtc / (rNtc + rPullUp);

  // Box-Muller 变换生成高斯白噪声
  const noise =
    Math.sqrt(-2 * Math.log(Math.random())) * Math.cos(2 * Math.PI * Math.random()) * noiseStdDev;
  return Math.max(0, Math.min(1, normVolt + noise));
}
```

---

### 5.3 边缘 Case 建模 3：超声波声学吸收与超程无回声 (`ultrasonic-edge-cases.ts`)

#### 物理背景

若前方障碍物吸音（如软海绵）或超出 400cm 测距极限，超声波传感器硬件回声脚 ECHO 将永远无法接收到回声，或者会在 38ms 超时后被芯片强制拉低。

```typescript
export function calcUltrasonicFlightWithEdgeCases(
  distanceCm: number,
  obstacleType: 'hard_wall' | 'soft_sponge',
): { isTimeout: boolean; echoPulseUs: bigint } {
  // 物理边缘 Case：超出 400cm 或遇到吸音海绵 ➔ 触发 38ms 硬件强制拉低超时
  if (distanceCm > 400 || obstacleType === 'soft_sponge') {
    return { isTimeout: true, echoPulseUs: 38_000n };
  }
  const echoUs = BigInt(Math.round((distanceCm * 20_000) / 343));
  return { isTimeout: false, echoPulseUs: echoUs };
}
```

---

## ⚠️ 6. 外设插件开发避坑指南与硬约束 (Developer Caveats)

在编写外设插件时，开发者必须严格遵守以下底层避坑规则：

1. **防 WASM Asyncify 睡眠态重入崩溃**：
   当用户在 UI 前端拖动滑块（如改变超声波距离 `SET_DISTANCE_CM`）时，插件内部只需更新 `this.distanceCm` 状态，**严禁在此回调中强行同步重入调用 WASM 导出函数**。C WASM 可能正处于 Asyncify 睡眠等待状态，强行重入调用会破坏 C 堆栈导致 `unreachable` 崩溃。
2. **严禁使用宿主机原生定时器**：
   **绝对禁止**在插件代码中使用 JavaScript 原生的 `setTimeout` 或 `setInterval`！物理时间流逝必须完全依赖 `this.ctx.nowUs()` 和 `this.ctx.deferUs()`，否则会导致全系统确定性 Replay 失效。
3. **避免 PinArbiter 边沿双重推送**：
   所有高保真波形只能通过 `injectWaveform` 唯一渠道推给 C WASM，切勿在插件中手动调用 `writePin` 重复发送相同的下降沿，防止底层队列出现残留电平错位。
4. **插件资源清理**：
   在插件 `onDestroy()` 生命周期中，务必显式调用 `this.ctx.releasePin(pinName)` 或 `releaseAllPins()`，释放注册的电平仲裁管脚，避免内存与事件泄漏。
