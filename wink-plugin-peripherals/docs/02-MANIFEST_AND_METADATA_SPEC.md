# Manifest 元数据与引脚规范手册 (Manifest & Metadata Spec)

> **目标**：全面掌握 `PeripheralManifest` 权威规范，包括 PinType 全集、属性定义、状态通道与 `mapEventToMethod` 事件绑定契约。  
> **面向对象**：需定义硬件接口、连接连线与元数据的开发者。  
> **入口索引**：[README.md](./README.md)

---

## 📘 1. Manifest 核心结构概览

在 UniSim 4.0 体系中，`PeripheralManifest` 是外设与仿真内核、嵌入式前端 UI 及 C 固件之间的**唯一真相源契约 (Single Source of Truth Contract)**。它决定了外设的电气引脚、可配置参数、运行态观察通道以及支持的操作事件。

```typescript
export interface PeripheralManifest {
  type: string; // 抽象驱动类型标识（如 "ultrasonic", "rc_servo"）
  version: string; // 语义化版本号，如 "1.0.0"
  displayName: string; // UI 显示名称，如 "HC-SR04 Ultrasonic Sensor"
  category: 'input' | 'actuator' | 'sensor' | 'display' | 'other'; // 设备分类
  description?: string; // 设备功能描述
  timingModel: 'event-driven' | 'step-lock' | 'hybrid'; // 物理时序模型
  pins: PeripheralManifestPinInput[]; // 引脚定义列表
  properties: Record<string, PropertySchema>; // 静态配置属性 (如 maxDistanceCm)
  stateChannels: Record<string, StateChannelSchema>; // 运行态观察通道 (如 distanceCm)
  events: Record<string, EventSchema>; // 可被 UI / 外部触发的事件 (如 SET_DISTANCE_CM)
}
```

---

## 🔌 2. 引脚 PinType 权威类型全集与 Authoring DX

为了极大简化外设 Manifest 撰写体验，开发者在定义引脚时可直接使用高层级的强类型 `pinType`，底层 `normalizeManifest()` 会自动补齐电气方向（`direction`）、信号类型（`signal`）及目录分类：

| `pinType` | 方向 (`direction`) | 信号 (`signal`) | 说明与典型应用 | 连线校验与仲裁规则 |
| :--- | :--- | :--- | :--- | :--- |
| **`digital_in`** | `sink` (外设输入) | `digital` | 外设读取电平（LED 阳极、蜂鸣器 SIG） | 允许连接 MCU 的 GPIO 输出引脚 |
| **`digital_out`** | `source` (外设输出) | `digital` | 外设驱动电平（按键 OUT、超声波 ECHO） | 允许连接 MCU 的 GPIO 输入引脚 |
| **`pwm`** | `source` (外设输入) | `analog` | 占空比信号（舵机 PWM 引脚、电机调速） | 必须连接支持 PWM 输出的 MCU 引脚 |
| **`adc`** | `sink` (外设输出) | `analog` | 连续模拟电压输出（旋钮、电位器、光敏） | 必须连接支持 ADC 采样的 MCU 引脚 |
| **`i2c_sda`** | `bidirectional` (双向) | `digital` | I2C 数据总线（OLED SDA、传感器 SDA） | 自动挂载至 I2C SDA 总线仲裁器 |
| **`i2c_scl`** | `sink` (外设输入) | `digital` | I2C 时钟总线（OLED SCL、传感器 SCL） | 自动挂载至 I2C SCL 时钟监听 |
| **`uart_tx`** | `source` (外设输出) | `digital` | 串口发送引脚（如 GPS TX） | 必须连接 MCU 的 RX 串口引脚 |
| **`uart_rx`** | `sink` (外设输入) | `digital` | 串口接收引脚（如 GPS RX） | 必须连接 MCU 的 TX 串口引脚 |
| **`vcc`** | `power` (电源正极) | `power` | 供电管脚（3.3V / 5V） | 用于连线吸附，不参与数字信号仲裁 |
| **`gnd`** | `ground` (接地端) | `power` | 物理共地脚 | 用于连线吸附，不参与数字信号仲裁 |

### 2.1 极简撰写三大法则 (Authoring DX)

1. **常规引脚（95% 场景）**：只需声明 `name: 'TRIG'` + `pinType: 'digital_in'`。系统靠大小写不敏感自动匹配 `wink-app.json` 中的 `trig_pin`，**完全不需要写 `role` 与 `aliases`**。
2. **1 对 1 单脚别名映射**：若引脚在特定芯片上有不同叫法，只需写 `aliases: ['anode']`。
3. **多脚归一（如 4 脚按键）**：多个物理引脚连接到同一内部电路节点时，手写 `role: 'signal'`（`role` 允许同名共享分组，而 `aliases` 必须唯一）。

---

## ⏰ 3. 时序模型 (Timing Model)

Manifest 中的 `timingModel` 告知仿真引擎该外设在时间轴上的驱动节奏：

| 时序模型 | 引擎调度行为 | 典型适用场景 |
| :--- | :--- | :--- |
| **`event-driven`** | 仅当引脚电平跳变 (`onPinChange`) 或收到外部事件时被调用；步进周期不消耗 CPU | **输入传感器**（超声波、按键、霍尔编码器） |
| **`step-lock`** | 每个仿真时钟 Tick (`dtUs`) 必须固定调用 `step()` 推进微分方程 | **连续微分物理模型**（电机角速度积分、热力学扩散） |
| **`hybrid`** | 既监听引脚异步事件，又在每个周期推进内部状态平滑过渡 | **执行器状态平滑**（舵机插值阻尼转动） |

---

## ⚙️ 4. 静态属性与运行态观察通道

### 4.1 `properties`（静态配置属性）
- **定义**：设备创建或初始化时的参数，通常在面板中配置或由 `wink-app.json` 提供。
- **示例**：
  ```typescript
  properties: {
    maxDistanceCm: { type: 'number', default: 400, min: 2, max: 400, unit: 'cm' },
    speedOfSoundMps: { type: 'number', default: 343, min: 300, max: 400, unit: 'm/s' },
  }
  ```

### 4.2 `stateChannels`（运行态观察通道）
- **定义**：随仿真微秒时钟动态演进的物理遥测数据，通过 `this.ctx.publish(key, val)` 广播，驱动 2D 画布组件、曲线仪表盘及 E2E 断言更新。
- **示例**：
  ```typescript
  stateChannels: {
    distanceCm: { type: 'number', default: 100, unit: 'cm', description: 'Current distance' },
    echoUs: { type: 'number', default: 0, unit: 'us', description: 'Echo pulse width' },
  }
  ```

---

## ⚡ 5. 事件声明与处理函数命名契约 (`mapEventToMethod`)

`events` 定义了该外设支持的外部领域语义激励（如点击按键、拖动滑块）。

### 5.1 事件声明格式

在 Manifest 中声明事件名及其入参 Schema：

```typescript
events: {
  SET_DISTANCE_CM: {
    description: 'Set simulated target distance',
    params: {
      cm: { type: 'number', required: true, min: 2, max: 400, default: 100 },
    },
  },
  PRESS: {
    description: 'Press momentary button',
    params: {},
  },
}
```

### 5.2 核心规则：`mapEventToMethod` 自动方法映射

UniSim 内核根据统一转换规则将事件名映射到插件类的方法名：

$$\text{方法名} = \text{'_'} + \text{剔除 \textbf{set\_} 前缀} + \text{驼峰转换}$$

| Manifest 事件名 | 插件类方法名 (`simulation.ts`) | 入参传递规则 |
| :--- | :--- | :--- |
| **`SET_DISTANCE_CM`** | **`_distanceCm(cm: number)`** | 自动剔除 `SET_`，单参数直接展开传入 |
| **`SET_ACTIVE`** | **`_active(active: boolean)`** | 自动剔除 `SET_`，单参数直接展开传入 |
| **`SET_ANGLE`** | **`_angle(angle: number)`** | 自动剔除 `SET_`，单参数直接展开传入 |
| **`SET_PWM`** | **`_pwm(duty: number)`** | 自动剔除 `SET_`，单参数直接展开传入 |
| **`PRESS`** | **`_press()`** | 无 `SET_` 前缀，直接转小写加下划线 |
| **`RELEASE`** | **`_release()`** | 无 `SET_` 前缀，直接转小写加下划线 |
| **`STOP`** | **`_stop()`** | 无参数事件 |
| **`RESET_SENSOR`** | **`_resetSensor()`** | 无 `SET_` 前缀，转驼峰加下划线 |

> [!CAUTION]
> **严禁在方法名中保留 `Set`！**
> 例如声明了 `SET_DISTANCE_CM`，方法名必须是 **`_distanceCm`**，**严禁写成 `_setDistanceCm`**！
> 声明了 `SET_ACTIVE`，方法名必须是 **`_active`**，**严禁写成 `_setActive`**！
> 若命名错误，`SimulationPluginHost.dispatchEvent` 运行时将直接抛出致命错误：
> `[PluginHost] missing handler for event 'SET_DISTANCE_CM' (expected method '_distanceCm')`。

---

## 🏭 6. 动态引脚变体工厂 (`ManifestFactory`)

当同一个外设存在多种硬件封装时（如按键有 2 脚与 4 脚版本，OLED 有 I2C 与 SPI 接口），推荐使用 `ManifestFactory` 模式：

```typescript
export type ButtonVariant = 'default' | 'two_pin';

export const BUTTON_PIN_VARIANTS: Record<ButtonVariant, { displayName: string; pins: PeripheralManifestPinInput[] }> = {
  default: {
    displayName: '4-Pin Push Button',
    pins: [
      { name: '1.l', pinType: 'digital_out', role: 'signal' },
      { name: '2.l', pinType: 'gnd' },
      { name: '1.r', pinType: 'digital_out', role: 'signal' },
      { name: '2.r', pinType: 'gnd' },
    ],
  },
  two_pin: {
    displayName: '2-Pin Push Button',
    pins: [
      { name: 'SIG', pinType: 'digital_out', role: 'signal' },
      { name: 'GND', pinType: 'gnd' },
    ],
  },
};

export const buttonManifestFactory: ManifestFactory = (variant: string) =>
  createButtonManifest(resolveVariant(variant));
```

---

## 📐 7. 画布引脚布局热区规范 (`pinsOverlay`)

在 `src/definition.ts` 中，外设必须定义 `pinsOverlay`，以便前端布线引擎（Auto Router）吸附连线与自动接线：

```typescript
pinsOverlay: {
  SIG: { relX: -5, relY: 20, wireNet: 'primary' },
  VCC: { relX: -5, relY: 35, wireNet: 'vcc', defaultConnection: 'VCC' },
  GND: { relX: -5, relY: 50, wireNet: 'gnd', defaultConnection: 'GND' },
}
```

### 7.1 坐标基准与定位技巧 (Positioning Guidelines)
1. **坐标系原点**：组件边界框的 **左上角为 `(0, 0)`**，水平向右为 `+X`，垂直向下为 `+Y`。
2. **左侧引出点外凸原则**：若引脚在元件左侧，推荐设置 `relX: -5`，此时连线端子恰好贴合在元件左边框边缘；若在右侧，则设置为 `relX: width + 5`。
3. **`wireNet` 电气网络分类**：
   - `'primary'`：主信号线（默认蓝色/天蓝色高亮）；
   - `'secondary'`：辅助信号线（如 I2C SCL、SPI CLK，默认绿色/淡青色）；
   - `'vcc'`：电源正极网络（红色）；
   - `'gnd'`：地网络（黑色/深灰色）。
4. **`defaultConnection` 自动接线**：
   - 设置为 `'VCC'`：拖入画布时，前端布线器自动拉一根线连向开发板的 VCC 供电轨；
   - 设置为 `'GND'`：自动拉一根线连向开发板的 GND 地轨；
   - 设置为具体引脚号（如 `12`）：自动连接到指定的 MCU GPIO。

---

## 🛠️ 8. 编译生成与静态检查

在当前仓根目录下运行：

```bash
# 全量构建
bun run build

# 静态类型校验
bun run typecheck

# 单元测试
bun test
```

打包工具会自动调用 `normalizeManifest` 对引脚与元数据进行完整性校验，并在外设各自的 `dist/` 目录下生成 `manifest.json` 与 `schema.json`。
