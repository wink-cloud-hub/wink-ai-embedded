# 外设插件新手 15 分钟快速上手指南 (Quickstart Guide)

> **目标**：在 15 分钟内，学会从选模板、创建工程、编写仿真与 UI，到本地编译自检的全流程。  
> **面向对象**：第一次开发 UniSim 3.0 外设插件的开发者。  
> **入口索引**：[README.md](./README.md)

---

## ⏱️ 时间预估与阶段产出

| 阶段                       | 预计耗时 | 完成标准与产出                                             |
| :------------------------- | :------: | :--------------------------------------------------------- |
| **步骤 1：模板选择**       |  2 分钟  | 根据硬件引脚与信号类型，选中对应的官方参考外设。           |
| **步骤 2：创建外设工程**   |  3 分钟  | 在 `peripherals/builtin/{type}/1.0.0/` 下建立标准目录。    |
| **步骤 3：编写仿真与 UI**  |  7 分钟  | 完成 `manifest.json`、`simulation.ts` 与 `definition.ts`。 |
| **步骤 4：编译自检与验证** |  3 分钟  | 运行 `build-peripherals.bat`，在 Workbench 调测成功。      |

---

## 🎯 步骤 1：选择外设模板 (Template Selection)

根据您要开发的外设的 **主信号 PinType**，选择最匹配的参考模板：

| 主 PinType 分类                  | 硬件特性与信号形态             | 你的外设像…              | 参考内置外设  | 继承基类 (Base Class)  |
| :------------------------------- | :----------------------------- | :----------------------- | :------------ | :--------------------- |
| **`digital_in`**                 | MCU 输出驱动外设；外设读取电平 | 单色 LED 灯、蜂鸣器      | `led`         | `BaseSimulationPlugin` |
| **`digital_out`**                | 外设驱动 MCU；UI/手势触发输入  | 物理按键、微动开关       | `button`      | `BaseSimulationPlugin` |
| **`pwm`**                        | 占空比驱动；角度/速度执行器    | SG90 舵机、电机调速      | `rc_servo`    | `BaseSimulationPlugin` |
| **`digital_in` + `digital_out`** | 触发/应答双数字脚；微秒脉冲    | 超声波测距 (HC-SR04)     | `ultrasonic`  | `BaseSimulationPlugin` |
| **`i2c_sda` + `i2c_scl`**        | I2C 从设备；寄存器与帧缓冲     | 单色 OLED 屏幕 (SSD1306) | `mono_oled`   | `BaseSimulationPlugin` |
| **`adc`**                        | 连续模拟电压输出 `[0V, 3.3V]`  | 旋钮电位器、摇杆         | `analog_knob` | `BaseSimulationPlugin` |

---

## 📁 步骤 2：建立外设工程目录

在 `peripherals/builtin/` 目录下建立您的外设文件夹（命名推荐：`小写下划线/{版本号}`）：

```text
peripherals/builtin/my_sensor/1.0.0/
├── manifest.json                 # 外设硬件契约与元数据
├── vite.config.sim.ts            # 仿真逻辑编译配置
├── vite.config.ui.ts             # 前端 UI 视图编译配置
└── src/
    ├── simulation.ts             # 仿真逻辑入口 (继承 BaseSimulationPlugin)
    └── definition.ts             # 前端 UI 控件入口 (定义 Vue 组件)
```

---

## 💻 步骤 3：编写外设代码 (核心 3 个文件)

### 3.1 编写 `manifest.json`（硬件契约定义）

声明外设的引脚、可配置属性、运行态通道与交互事件：

```json
{
  "type": "my_sensor",
  "version": "1.0.0",
  "displayName": "My Custom Sensor",
  "category": "sensor",
  "description": "Custom 2-pin digital sensor",
  "pins": [
    { "name": "SIG", "pinType": "digital_out", "required": true },
    { "name": "VCC", "pinType": "vcc", "required": false },
    { "name": "GND", "pinType": "gnd", "required": false }
  ],
  "properties": {
    "sensitivity": { "type": "number", "default": 50, "min": 0, "max": 100 }
  },
  "stateChannels": {
    "active": { "type": "boolean", "default": false }
  },
  "events": {
    "SET_ACTIVE": {
      "description": "Set active state from UI slider/toggle",
      "params": { "active": { "type": "boolean", "required": true } }
    }
  }
}
```

### 3.2 编写 `src/simulation.ts`（仿真逻辑）

```typescript
import { BaseSimulationPlugin, type PluginContext, LogicStates } from '@unisim/plugin';

export interface MySensorState {
  active: boolean;
}

export interface MySensorProps {
  sensitivity: number;
}

export class MySensorPlugin extends BaseSimulationPlugin<MySensorState, MySensorProps> {
  private active = false;

  // 1. 初始化绑定
  protected onBound(
    _ctx: PluginContext<MySensorState>,
    _pinMapping: Record<string, number>,
    _props: MySensorProps,
  ): Partial<MySensorState> {
    return { active: false };
  }

  // 2. 响应 UI 触发事件 (事件名 SET_ACTIVE 对应 _setActive 方法)
  _setActive(active: boolean): void {
    this.active = active;
    this.ctx?.publish('active', active);

    // 向 SIG 引脚输出电平信号
    this.ctx?.writePin('SIG', active);
  }

  // 3. 销毁时清理驱动
  onDestroy(): void {
    this.ctx?.releasePin('SIG');
  }
}

export default {
  PluginClass: MySensorPlugin,
};
```

### 3.3 编写 `src/definition.ts`（前端 UI 控件）

```typescript
import { defineComponent, h } from 'vue';

export const MySensorUI = defineComponent({
  name: 'MySensorUI',
  props: {
    state: { type: Object, required: true },
  },
  emits: ['emit-event'],
  setup(props, { emit }) {
    const toggleActive = () => {
      emit('emit-event', {
        event: 'SET_ACTIVE',
        params: { active: !props.state.active },
      });
    };

    return () =>
      h('div', { class: 'my-sensor-widget', onClick: toggleActive }, [
        h('span', `Sensor Status: ${props.state.active ? 'ACTIVE' : 'IDLE'}`),
      ]);
  },
});

export default MySensorUI;
```

---

## 🛠️ 步骤 4：编译打包与本地验证

### 4.1 执行一键构建

在 `peripherals/` 目录下运行构建脚本：

- **Windows**: `build-peripherals.bat`
- **macOS / Linux**: `./build-peripherals.ps1`
- 或在项目根目录运行 `bun run build:peripherals`

构建成功后，在 `peripherals/builtin/my_sensor/1.0.0/dist/` 下将自动生成：

- `simulation.js`（仿真逻辑 Bundle）
- `index.js`（UI 视图 Bundle）

### 4.2 Workbench 本地调试

1. 启动项目调试服务器：`bun run dev`
2. 打开仿真 Workbench 页面，在右侧 **“添加外设”** 列表中搜索 `my_sensor`。
3. 将外设挂载至画布，拖动控件或点击按钮，确认：
   - UI 状态（`active`）实时改变；
   - 引脚信号（`SIG`）与 MCU 连线正确输出高低电平。

---

## ⏭️ 下一步

完成新手入门后，建议进一步阅读：

- **[02-MANIFEST_AND_METADATA_SPEC.md](./02-MANIFEST_AND_METADATA_SPEC.md)**：深入了解 Manifest 完整引脚类型与契约定义。
- **[03-HIGH_FIDELITY_WAVEFORM_GUIDE.md](./03-HIGH_FIDELITY_WAVEFORM_GUIDE.md)**：学习如何使用 `injectWaveform` 实现微秒级高保真时序。
