# 4.2 UniSim 虚拟电路规范与 SchemaForm 数据驱动配置

为了实现低代码平台的可视化拖拽、多板电路连线、外设动态属性配置（与 AI 生成完全同步），Wink-AI 平台提出并设计了 **UniSim (Unified Simulation Project) 虚拟外设与项目电路拓扑规范**。

---

## 1. 项目拓扑与电路图存储规范 (`sim-project.json`)

电路网表与画布配置采用平铺式对象模型组织，能够完美记录多开发板、外设元件的坐标、物理参数以及引脚之间的电气导线连接关系。

### 1.1 电路描述 Schema 范例

```json
{
  "$schema": "https://unisim-spec.org/v1/sim-project.schema.json",
  "version": 1,
  "projectName": "Multi-Board IoT Gateway",
  "boards": [
    {
      "id": "gateway_esp32",
      "type": "board-esp32-s3",
      "x": 100,
      "y": 150,
      "sourceDir": "src/gateway-esp32",
      "settings": {
        "baudRate": 115200,
        "flashSize": 8388608
      }
    },
    {
      "id": "node_nano",
      "type": "board-arduino-nano",
      "x": 500,
      "y": 150,
      "sourceDir": "src/node-nano"
    }
  ],
  "components": [
    {
      "id": "led_status",
      "type": "generic-led",
      "x": 350,
      "y": 280,
      "rotation": 90,
      "properties": {
        "color": "#ff0000",
        "currentLimitResistor": 220
      }
    }
  ],
  "connections": [
    {
      "id": "wire_1",
      "from": "node_nano:D13",
      "to": "led_status:Anode",
      "color": "red",
      "signalType": "digital",
      "routing": {
        "mode": "orthogonal",
        "path": ["v15", "h-30", "*"]
      }
    },
    {
      "id": "wire_2",
      "from": "gateway_esp32:TX0",
      "to": "node_nano:RX",
      "color": "blue",
      "signalType": "uart",
      "routing": {
        "mode": "custom",
        "points": [
          { "x": 180, "y": 190 },
          { "x": 340, "y": 190 },
          { "x": 480, "y": 170 }
        ]
      }
    }
  ]
}
```

---

## 2. 基于 SchemaForm 的外设元数据表单设计

为了减少手动编写属性配置表单的工作量，外设的元数据规范与项目前端原生的 **SchemaForm** 规范（基于 `@yo-cloud/yo-ux-vue`）完全保持一致。

### 2.1 外设定义规范示例 (`peripheral-definition.json`)
在外设定义中，`properties` 字段声明为 `DynamicItemSchemaType[]` 类型的配置数组。当用户在画布中选中该外设时，属性面板直接将此配置数组传给 `<SchemaForm>` 进行零转换动态渲染。

```json
{
  "$schema": "https://unisim-spec.org/v1/peripheral-definition.schema.json",
  "id": "generic-led",
  "tagName": "wokwi-led",
  "name": {
    "en": "Light Emitting Diode",
    "zh": "发光二极管 (LED)"
  },
  "category": "output",
  "visual": {
    "thumbnail": "<svg width=\"64\" height=\"64\">...</svg>",
    "dimensions": { "width": 24, "height": 36 }
  },
  "pins": [
    { "name": "Anode", "label": "A", "type": "digital_io", "description": "阳极" },
    { "name": "Cathode", "label": "C", "type": "gnd", "description": "阴极" }
  ],
  "properties": [
    {
      "prop": "color",
      "label": "LED颜色",
      "compType": "Select",
      "compProps": {
        "placeholder": "请选择发光颜色",
        "options": [
          { "label": "红色", "value": "red" },
          { "label": "绿色", "value": "green" },
          { "label": "黄色", "value": "yellow" },
          { "label": "蓝色", "value": "blue" }
        ]
      },
      "defaultValue": "red",
      "rules": [{ "required": true, "message": "必须选择颜色", "trigger": "change" }]
    },
    {
      "prop": "currentLimitResistor",
      "label": "限流电阻值 (Ω)",
      "compType": "Slider",
      "compProps": {
        "min": 0,
        "max": 10000,
        "step": 10
      },
      "defaultValue": 220
    }
  ]
}
```

### 2.2 前端渲染集成代码
```vue
<template>
  <el-card class="property-editor-card" shadow="never">
    <template #header>
      <div class="header-title">
        <span>外设属性配置 ({{ activeComponent.id }})</span>
      </div>
    </template>
    
    <!-- 动态表单组件 -->
    <SchemaForm
      :schemas="activeComponentMeta.properties"
      v-model:data="activeComponent.properties"
      :form-props="formProps"
      :default-item-span="24"
    />
  </el-card>
</template>

<script setup lang="ts">
import { ref } from 'vue';
import { SchemaForm, type DynamicItemSchemaType } from "@yo-cloud/yo-ux-vue";

const formProps = {
  labelPosition: 'top',
  size: 'default'
};

interface ComponentInstance {
  id: string;
  type: string;
  properties: Record<string, any>;
}

const activeComponent = ref<ComponentInstance>({
  id: "led_status",
  type: "generic-led",
  properties: {
    color: "red",
    currentLimitResistor: 220
  }
});

const activeComponentMeta = ref({
  properties: [
    {
      prop: "color",
      label: "LED颜色",
      compType: "Select",
      compProps: {
        options: [
          { label: "红色", value: "red" },
          { label: "绿色", value: "green" },
          { label: "黄色", value: "yellow" },
          { label: "蓝色", value: "blue" }
        ]
      }
    },
    {
      prop: "currentLimitResistor",
      label: "限流电阻值 (Ω)",
      compType: "Slider",
      compProps: { min: 0, max: 1000, step: 10 }
    }
  ] as DynamicItemSchemaType[]
});
</script>
```

---

## 3. 自适应导线路由设计 (Adaptive Routing)

在拖动元器件时，导线需要能自动转折。连线引擎提供两种互补模式：
1. **Orthogonal (正交自动寻路)**：默认在此模式下，导线为横平竖直的正交线段。引擎使用相对命令记录导线转折，并在元器件位置发生偏转时由渲染器实时重算：
   *   `v[N]`：垂直方向走线 N 像素。
   *   `h[N]`：水平方向走线 N 像素。
   *   `*`：源侧与目标侧走线路径的交汇对齐分界符。
2. **Custom (自定义绝对拐点)**：当用户手动在画布上拖动导线的转折点（添加 Handle）时，导线类型自动退化为 `custom` 模式，存储为一组绝对的二维坐标列表 `points: {x, y}[]`，防止自动路由重算破坏用户的排线美化布局。

---

## 4. 虚拟外设驱动注册表设计 (WasmPeripheralRegistry)

为了将 Web 端 DOM (如 Web Component `<wokwi-led>`) 的视觉状态与 Wasm 仿真线程的逻辑引脚电平同步，我们定义了 JS/TS 层的虚拟外设注册表规范。引入了 **4值逻辑状态** 与 **3级驱动强度** 仲裁体系以解决引脚冲突与线与逻辑。

```typescript
/** 4值逻辑状态 */
export type LogicState = 0 | 1 | 'Z' | 'X';

/** 驱动强度等级 */
export enum DriveStrength {
  SUPPLY = 3, // 电源直连（如 VCC/GND, 推挽输出 push-pull）
  PULL   = 2, // 电阻拉低/拉高（如 I2C 外部上拉, 内部 pull-up/down）
  WEAK   = 1, // 弱电平/悬空（漏极开路 open-drain 释放时、高阻输入）
}

export interface PeripheralLifecycle {
  /** 
   * 绑定的物理电源轨/电源域 (如 '3V3_SYS', '5V_PERIPHERAL')
   * 当板载主控断电或电源轨因保护关闭时，会自动切断该外设的供电
   */
  powerDomain: string;

  /** 
   * 模拟电源从 0V 升至稳定工作电压的时序爬坡延迟 (单位: 微秒)
   * 在此延迟时间段内，外设对总线/引脚读写无响应，必须返回 WINK_ERR_BUSY
   */
  powerUpDelayUs?: number;

  /** 上电复位调用 */
  onPowerOn?: () => Promise<void>;

  /** 断电/热插拔移除调用 */
  onPowerOff?: () => void;

  /** 软复位 */
  onReset?: () => void;

  /** 属性动态更改 */
  onPropertyChange?: (key: string, oldValue: any, newValue: any) => void;
}

/**
 * PinArbiter 引脚仲裁接口（基于驱动强度的 4 值逻辑）
 * 支持：
 * - 开漏 / 线与行为（I2C、OneWire）
 * - 总线冲突检测
 * - 高阻态（Hi-Z）状态处理
 * - 模拟组件电压估算
 */
export interface PinArbiter {
  readPin(pin: number): LogicState; // 0 | 1 | 'Z' | 'X'
  getResolvedVoltage(pin: number): number;
  onPinChange(pin: number, callback: (pin: number, state: LogicState) => void): () => void;
  setDriver(pin: number, driver: PinDriver): void;
  removeDriver(pin: number, driverId: string): void;
}

/**
 * 引脚驱动定义
 */
export interface PinDriver {
  /** 唯一驱动 ID */
  id: string;
  /** 当前驱动逻辑状态 */
  state: LogicState;
  /** 驱动强度 */
  strength: DriveStrength;
}

export interface PeripheralSimulationLogic extends PeripheralLifecycle {
  /**
   * 当连接到该外设的引脚电平状态改变时被调用
   */
  onPinStateChange?: (pinName: string, state: LogicState) => void;

  /**
   * 当仿真开始时调用，用于绑定 UI 事件并将事件传导回 Wasm 中
   * @param element 外设的 DOM 元素
   * @param pinArbiter 引脚电平与仲裁管理器
   * @param getMappedPin 获取某外设引脚绑定的 Wasm 逻辑 GPIO 端口号
   * @param componentId 实例唯一 ID
   */
  attachEvents?: (
    element: HTMLElement,
    pinArbiter: PinArbiter,
    getMappedPin: (partPinName: string) => number | null,
    componentId: string
  ) => () => void; // 返回 cleanup 回收函数
}

class PeripheralRegistry {
  private registry = new Map<string, PeripheralSimulationLogic>();

  register(type: string, logic: PeripheralSimulationLogic) {
    this.registry.set(type, logic);
  }

  get(type: string) {
    return this.registry.get(type);
  }
}

export const WasmPeripheralRegistry = new PeripheralRegistry();
```

### 4.1 四大典型虚拟驱动实现

#### 1. LED 指示灯（数字输出型）
```typescript
WasmPeripheralRegistry.register('generic-led', {
  powerDomain: '3V3_SYS',
  powerUpDelayUs: 0, // LED 瞬时就绪

  attachEvents: (element, pinManager, getMappedPin, componentId) => {
    const anodePin = getMappedPin('Anode'); // 阳极引脚
    const cathodePin = getMappedPin('Cathode'); // 阴极引脚
    const driverId = `${componentId}:led_drv`;

    // 1. 注册该组件在引脚上的阻抗与驱动属性 (高阻输入模式)
    if (anodePin !== null) {
      pinManager.setDriver(anodePin, driverId, 'Z', DriveStrength.WEAK);
    }
    if (cathodePin !== null) {
      pinManager.setDriver(cathodePin, driverId, 'Z', DriveStrength.WEAK);
    }

    const updateLed = () => {
      // 2. 使用仲裁器解析后的电压差计算 LED 亮度与点亮状态
      const anodeVoltage = anodePin !== null ? pinManager.getResolvedVoltage(anodePin) : 0;
      const cathodeVoltage = cathodePin !== null ? pinManager.getResolvedVoltage(cathodePin) : 0;
      
      const voltageAcrossLed = Math.max(0, anodeVoltage - cathodeVoltage - 1.8); // 考虑 1.8V 正向压降
      const brightness = Math.min(1, voltageAcrossLed / 1.5);
      
      (element as any).value = brightness > 0.1;
      (element as any).brightness = brightness;
    };

    let unsubAnode = () => {};
    let unsubCathode = () => {};
    if (anodePin !== null) {
      unsubAnode = pinManager.onPinChange(anodePin, updateLed);
    }
    if (cathodePin !== null) {
      unsubCathode = pinManager.onPinChange(cathodePin, updateLed);
    }

    return () => {
      unsubAnode();
      unsubCathode();
      if (anodePin !== null) pinManager.removeDriver(anodePin, driverId);
      if (cathodePin !== null) pinManager.removeDriver(cathodePin, driverId);
    };
  }
});
```

#### 2. 物理按键（数字输入与中断触发）
```typescript
WasmPeripheralRegistry.register('pushbutton', {
  powerDomain: '3V3_SYS',

  attachEvents: (element, pinManager, getMappedPin, componentId) => {
    const gpioPin = getMappedPin('1.l') ?? getMappedPin('2.l');
    if (gpioPin === null) return () => {};
    const driverId = `${componentId}:btn_drv`;

    // 未按下时默认为高阻态，交由板载/外部上拉电阻决定逻辑电平 (Active Low 连线)
    pinManager.setDriver(gpioPin, driverId, 'Z', DriveStrength.WEAK);

    const onPress = () => {
      // 按下时将引脚直接接地 (Supply 强度的低电平 0)
      pinManager.setDriver(gpioPin, driverId, 0, DriveStrength.SUPPLY);
      (element as any).pressed = true;
    };
    const onRelease = () => {
      // 释放后恢复为弱高阻态
      pinManager.setDriver(gpioPin, driverId, 'Z', DriveStrength.WEAK);
      (element as any).pressed = false;
    };

    element.addEventListener('button-press', onPress);
    element.addEventListener('button-release', onRelease);

    return () => {
      element.removeEventListener('button-press', onPress);
      element.removeEventListener('button-release', onRelease);
      pinManager.removeDriver(gpioPin, driverId);
    };
  }
});
```

#### 3. 电位器旋钮（ADC 模拟输入）
```typescript
WasmPeripheralRegistry.register('potentiometer', {
  powerDomain: '3V3_SYS',

  attachEvents: (element, pinManager, getMappedPin, componentId) => {
    const adcPin = getMappedPin('SIG');
    if (adcPin === null) return () => {};
    const driverId = `${componentId}:pot_drv`;

    const onValueChange = (e: Event) => {
      const percent = (e.target as any).value; // 滑动百分比 0.0 ~ 1.0
      const simulatedVoltage = percent * 3.3; // 转换为 3.3V ADC 参考电平
      
      // 模拟电位器阻抗驱动输出，这里简化为以 SUPPLY 强度写入
      pinManager.setAnalogVoltage?.(adcPin, driverId, simulatedVoltage);
    };

    element.addEventListener('input', onValueChange);
    return () => {
      element.removeEventListener('input', onValueChange);
      pinManager.removeDriver?.(adcPin, driverId);
    };
  }
});
```

#### 4. 三维机械臂/舵机关节（PWM 输出与 WebGL 联动）
```typescript
WasmPeripheralRegistry.register('servo-motor', {
  powerDomain: '5V_PERIPHERAL',
  powerUpDelayUs: 5000, // 模拟舵机上电电容充电稳定需 5ms

  attachEvents: (element, pinManager, getMappedPin, componentId) => {
    const pwmChannel = getMappedPin('PWM');
    if (pwmChannel === null) return () => {};

    const unsubPwm = pinManager.onPwmChange(pwmChannel, (dutyCycle) => {
      // 0.5ms - 2.5ms 高电平占空比，换算为 0° - 180° 旋转角度
      const minAngle = 0;
      const maxAngle = 180;
      const targetAngle = minAngle + (dutyCycle / 100) * (maxAngle - minAngle);

      // 分发事件供外层 Three.js 画布获取，刷新 3D 网格矩阵
      window.dispatchEvent(new CustomEvent('servo-rotate', {
        detail: { componentId, angle: targetAngle }
      }));
    });

    return () => {
      unsubPwm();
    };
  }
});
```

并在 Vue 3 的 3D WebGL 视口中监听事件并旋转关节：
```javascript
window.addEventListener('servo-rotate', (e) => {
  const { componentId, angle } = e.detail;
  const joint = robot3DModel.findJointById(componentId);
  if (joint) {
     joint.rotation.y = THREE.MathUtils.degToRad(angle); // 弧度旋转
  }
});
```

### 4.2 引脚仲裁架构（阶段 0）

为了精确模拟真实电路行为，包括开漏总线、上下拉电阻和总线冲突，UniSim 使用 **基于驱动强度的 4 值逻辑仲裁系统**。

#### 4.2.1 核心概念

**4 值逻辑状态：**

| 状态 | 含义 | 电压 |
|-------|---------|---------|
| `0` | 逻辑低电平 | 0.0V |
| `1` | 逻辑高电平 | 3.3V |
| `'Z'` | 高阻态 / 悬空 | 0.0V（默认，组件可自定义处理） |
| `'X'` | 冲突 / 未知 | 1.65V（中点） |

**驱动强度等级：**

| 等级 | 值 | 使用场景 |
|-------|-------|----------|
| `SUPPLY` | 3 | 推挽输出 GPIO、VCC/GND 直连 |
| `PULL` | 2 | 外部 I2C 上下拉电阻（4.7kΩ） |
| `WEAK` | 1 | MCU 内部上拉、开漏释放状态 |

#### 4.2.2 仲裁算法

1. 所有状态为 `'Z'` 的驱动器被忽略（高阻态不驱动）
2. 在剩余激活驱动器中寻找最大强度
3. 如果所有最大强度驱动器状态一致 → 该状态获胜
4. 如果最大强度驱动器状态不一致 → `'X'`（冲突，记录警告）
5. 如果无激活驱动器 → `'Z'`（悬空）

#### 4.2.3 I2C 线与示例

```typescript
// I2C 总线带外部上拉电阻
pinArbiter.setDriver(6, {
  id: 'board:i2c-pullup-sda',
  state: 1,
  strength: DriveStrength.PULL
});

// MCU SDA 开漏模式
pinArbiter.setDriver(6, {
  id: 'mcu:sda',
  state: 0, // MCU 拉低
  strength: DriveStrength.SUPPLY
});

pinArbiter.readPin(6); // 返回 0（线与：低电平获胜）

// MCU 释放总线
pinArbiter.setDriver(6, {
  id: 'mcu:sda',
  state: 'Z', // 高阻释放
  strength: DriveStrength.SUPPLY
});

pinArbiter.readPin(6); // 返回 1（上拉获胜）
```

#### 4.2.4 驱动开发示例

所有驱动直接使用 `PinArbiter` 4 值逻辑接口开发：

```typescript
const updateLed = () => {
  // 直接获取电压，支持模拟亮度计算
  const voltage = pinArbiter.getResolvedVoltage(anodePin); // 0.0-3.3V
  element.brightness = Math.min(1, Math.max(0, voltage / 3.3)); // 模拟亮度！
  
  // 也可以检查逻辑状态
  const state = pinArbiter.readPin(anodePin); // 0 | 1 | 'Z' | 'X'
  if (state === 'X') {
    console.warn('引脚冲突检测');
  }
};
```
