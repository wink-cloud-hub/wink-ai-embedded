# Manifest 元数据与引脚规范手册 (Manifest & Metadata Spec)

> **目标**：全面掌握 `manifest.json` 权威规范，包括引脚 PinType 全集、属性定义、状态通道与事件绑定契约。  
> **面向对象**：需定义硬件接口、连接连线与元数据的开发者。  
> **入口索引**：[README.md](./README.md)

---

## 📘 1. Manifest 核心结构概览

`manifest.json` 是外设与 UniSim 仿真内核、嵌入式前端 UI 之间的**唯一真相源契约 (Single Source of Truth Contract)**。它决定了外设能连什么引脚、有哪些可调参数、在 UI 上显示什么数据以及能接收什么操作事件。

```typescript
export interface PeripheralManifest {
  type: string; // 唯一设备类型标识，如 "ultrasonic"
  version: string; // 语义化版本号，如 "1.0.0"
  displayName: string; // 显示名称，如 "HC-SR04 Ultrasonic Sensor"
  category: string; // 设备分类：sensor | actuator | display | input
  description?: string; // 设备功能描述
  timingModel?: string; // 物理时序模型：event-driven | periodic | continuous
  pins: PeripheralManifestPin[]; // 引脚定义列表
  properties: Record<string, PropertySchema>; // 静态配置属性 (如 maxDistance)
  stateChannels: Record<string, StateChannelSchema>; // 运行态观察通道 (如 distanceCm)
  events: Record<string, EventSchema>; // 可被 UI / 外部触发的事件 (如 SET_DISTANCE_CM)
}
```

---

## 🔌 2. 引脚 PinType 权威类型全集

UniSim 3.0 定义了标准的电气引脚类型，前端电路布线引擎（Wire Routing Engine）与仿真仲裁器（`PinArbiter`）根据 `pinType` 执行连线校验与电气仲裁：

| `pinType`         | 方向 / 电气属性                       | 说明与典型应用                             | 连线校验规则                       |
| :---------------- | :------------------------------------ | :----------------------------------------- | :--------------------------------- |
| **`digital_in`**  | MCU 输出 $\rightarrow$ 外设输入       | 外设读取电平（如 LED 阳极、蜂鸣器 SIG）。  | 允许连接 MCU 的 GPIO 输出引脚。    |
| **`digital_out`** | 外设输出 $\rightarrow$ MCU 输入       | 外设驱动电平（如 按键 OUT、超声波 ECHO）。 | 允许连接 MCU 的 GPIO 输入引脚。    |
| **`pwm`**         | 占空比 / 方波输入                     | 外设接收 PWM 信号（如 舵机 PWM 引脚）。    | 必须连接支持 PWM 输出的 MCU 引脚。 |
| **`adc`**         | 模拟电压 $[0.0\text{V}, 3.3\text{V}]$ | 连续模拟量输出/输入（如 旋钮 OUT）。       | 必须连接支持 ADC 采样的 MCU 引脚。 |
| **`i2c_sda`**     | 双向数据线 (I2C)                      | I2C 数据总线引脚（如 OLED SDA）。          | 必须连接 I2C SDA 总线。            |
| **`i2c_scl`**     | 时钟输入线 (I2C)                      | I2C 时钟总线引脚（如 OLED SCL）。          | 必须连接 I2C SCL 总线。            |
| **`uart_tx`**     | 外设发送 $\rightarrow$ MCU 接收       | 串口发送引脚（如 GPS TX）。                | 必须连接 MCU 的 RX 引脚。          |
| **`uart_rx`**     | MCU 发送 $\rightarrow$ 外设接收       | 串口接收引脚（如 GPS RX）。                | 必须连接 MCU 的 TX 引脚。          |
| **`vcc`**         | 电源正极                              | 逻辑供电脚（如 3.3V / 5V）。               | 电源管脚，不参与数字信号仲裁。     |
| **`gnd`**         | 接地端                                | 物理共地脚。                               | 共地管脚，不参与数字信号仲裁。     |

---

## ⚙️ 3. 静态属性与运行态观察通道规范

必须区分 **`properties`（静态属性）** 与 **`stateChannels`（运行态观察通道）**：

### 3.1 `properties`（静态配置属性）

- **定义**：设备创建时或初始化时的固定参数，一般不频繁改变。
- **支持类型**：`number` | `boolean` | `string`
- **示例**：
  ```json
  "properties": {
    "maxDistanceCm": { "type": "number", "default": 400, "min": 2, "max": 400, "unit": "cm" },
    "speedOfSoundMps": { "type": "number", "default": 343, "unit": "m/s" }
  }
  ```

### 3.2 `stateChannels`（运行态观察通道）

- **定义**：随仿真运行动态改变的数据，供 UI 观察控件、仪表盘、图表实时渲染。
- **更新方式**：在 `simulation.ts` 中通过 `this.ctx.publish('channelName', value)` 触发更新。
- **示例**：
  ```json
  "stateChannels": {
    "distanceCm": { "type": "number", "default": 100, "unit": "cm", "description": "Current distance" },
    "echoUs": { "type": "number", "default": 0, "unit": "us", "description": "Echo pulse width" }
  }
  ```

---

## ⚡ 4. 事件声明与处理函数命名契约

`events` 定义了该外设支持的交互动作（如拖动控制滑块、点击按键）。

### 4.1 事件声明格式

在 Manifest 中声明事件名及其参数 Schema：

```json
"events": {
  "SET_DISTANCE_CM": {
    "description": "Set the simulated target distance",
    "params": {
      "cm": { "type": "number", "required": true, "min": 2, "max": 400, "default": 100 }
    }
  }
}
```

### 4.2 处理函数自动绑定规则

在 `simulation.ts` 中，事件名采用 **`_驼峰命名`** 规则自动绑定处理函数：

| Manifest 事件名   | `simulation.ts` 处理函数名    | 参数传递                               |
| :---------------- | :---------------------------- | :------------------------------------- |
| `SET_DISTANCE_CM` | `_setDistanceCm(cm: number)`  | 展开 `params` 对象，按顺序作为参数传入 |
| `SET_ACTIVE`      | `_setActive(active: boolean)` | 自动解构传入 `active`                  |
| `PRESS`           | `_press()`                    | 无参数事件                             |

> **⚠️ 避坑提醒**：如果在 Manifest 中声明了事件，但在插件类中遗漏了对应的 `_methodName` 函数，在 UI 触发该事件时控制台会抛出未找到处理函数的错误。

---

## 🛠️ 5. 代码自检与工具校验

外设打包工具链会自动使用 `normalizeManifest` 对 `manifest.json` 进行类型补全与合规性校验。

您可以运行以下命令对 Manifest 进行静态检查：

```bash
cd peripherals
bun run build:peripherals
```
