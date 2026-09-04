# 外设物理算法与无头单测验证指南 (Physics & Headless Testing)

> **目标**：指导开发者如何为外设编写独立的物理数学算法与仿真逻辑无头单测，实现毫秒级验证与自动化 CI 回归。  
> **面向对象**：编写高精度传感器、执行器算法、总线协议以及需保证确定性的外设开发者。  
> **入口索引**：[README.md](./README.md)

---

## 🧪 1. 为什么必须为外设编写无头单测？

在传统的嵌入式前端开发中，开发者往往修改了算法后，必须启动前端 UI、在电路画布上连线、拖动滑块甚至烧录固件来肉眼观察。这种方式存在三大痛点：
1. **反馈周期极长**：每次改动需经历完整的构建、热刷新与页面交互（数秒至数十秒）；
2. **边缘场景难以复现**：如浮点精度临界值、零距离触发、声速极端温度补偿等边缘 Case，难以通过鼠标手工滑动精确构造；
3. **无法防止代码腐化 (Regression)**：后续重构或依赖升级时，缺乏自动化用例保障。

UniSim 4.0 遵循 **“物理域算法解耦 + 无头单测驱动 (Decoupled Physics & Headless-First)”** 理念：将数学/物理公式抽离为纯函数（Pure Functions），在无浏览器、无 WASM 的纯 TS 环境下通过 `bun test` 获得**毫秒级的秒级反馈**。

---

## 📁 2. 测试工程目录规范

在外设工程中，推荐采用以下标准测试组织结构：

```text
builtin/{type}/1.0.0/src/
├── physics/
│   ├── distance-echo-us.ts       # 纯数学物理计算函数 (无 DOM/UI 依赖)
│   └── __tests__/
│       └── distance-echo-us.test.ts  # 物理公式单测
├── simulation.ts                 # 仿真插件类
└── __tests__/
    └── simulation.test.ts        # 插件生命周期与状态机单测
```

---

## 📐 3. 第一部分：纯物理域算法单测 (Pure Physics Testing)

物理算法应当只依赖标准数学运算，不引入任何 Vue、Canvas 或硬件上下文。

### 3.1 物理公式源码示例 (`src/physics/distance-echo-us.ts`)

```typescript
/**
 * 根据物理距离 (cm) 与环境声速 (m/s) 计算超声波往返所需微秒时间 (us)
 * 物理公式: t = (2 * s) / v
 */
export function distanceCmToEchoUs(distanceCm: number, speedOfSoundMps = 343): number {
  if (distanceCm <= 0 || speedOfSoundMps <= 0) return 0;
  // 距离换算为米: distanceCm / 100
  // 往返双程: 2 * (distanceCm / 100)
  // 时间(秒): (2 * distanceCm / 100) / speedOfSoundMps
  // 微秒换算: * 1_000_000
  const echoUs = ((2 * distanceCm) / 100 / speedOfSoundMps) * 1_000_000;
  return Math.round(echoUs);
}
```

### 3.2 编写物理单测 (`src/physics/__tests__/distance-echo-us.test.ts`)

使用 Bun 内置的高性能测试套件 `bun:test`：

```typescript
import { describe, it, expect } from 'bun:test';
import { distanceCmToEchoUs } from '../distance-echo-us';

describe('Ultrasonic Physics Model', () => {
  it('calculates standard 100cm echo pulse width at 343m/s', () => {
    // 1米往返: 2米 / 343m/s ≈ 0.0058309s ≈ 5831us
    const echoUs = distanceCmToEchoUs(100, 343);
    expect(echoUs).toBe(5831);
  });

  it('handles minimum valid distance (2cm)', () => {
    // 2cm: 0.04m / 343m/s ≈ 116.6us -> 117us
    const echoUs = distanceCmToEchoUs(2, 343);
    expect(echoUs).toBe(117);
  });

  it('clamps invalid zero or negative inputs', () => {
    expect(distanceCmToEchoUs(0)).toBe(0);
    expect(distanceCmToEchoUs(-10)).toBe(0);
  });
});
```

---

## ⚡ 4. 第二部分：插件仿真逻辑单测 (Plugin State Machine Testing)

通过 Mock 轻量级的 `PluginContext`，可对外设插件的生命周期（`onBound`）、事件处理（`_distanceCm`）与引脚读写逻辑进行端到端隔离测试。

### 4.1 仿真插件单测示例 (`src/__tests__/simulation.test.ts`)

```typescript
import { describe, it, expect, beforeEach } from 'bun:test';
import { UltrasonicPlugin, createUltrasonicManifest } from '../simulation';
import type { PluginContext } from '@wink-ai/unisim';

describe('UltrasonicPlugin Headless Lifecycle', () => {
  let plugin: UltrasonicPlugin;
  let publishedChannels: Record<string, unknown>;
  let writtenPins: Record<string, boolean>;

  // 创建轻量级仿真上下文 Mock
  function createMockContext(): PluginContext<any> {
    publishedChannels = {};
    writtenPins = {};
    return {
      publish: (key: string, val: unknown) => {
        publishedChannels[key] = val;
      },
      writePin: (pinName: string, level: boolean | number) => {
        writtenPins[pinName] = Boolean(level);
      },
      releasePin: () => {},
      nowUs: () => 1_000_000n,
    } as unknown as PluginContext<any>;
  }

  beforeEach(() => {
    plugin = new UltrasonicPlugin();
  });

  it('initializes default state and channels on onBound', () => {
    const ctx = createMockContext();
    const pinMapping = { TRIG: 5, ECHO: 18 };
    const props = { maxDistanceCm: 400, minDistanceCm: 2, speedOfSoundMps: 343 };

    (plugin as any).onBound(ctx, pinMapping, props);

    // 验证初始状态发布
    expect(publishedChannels['distanceCm']).toBe(100);
    expect(publishedChannels['echoUs']).toBe(0);
  });

  it('updates distance and publishes channel when _distanceCm event is invoked', () => {
    const ctx = createMockContext();
    (plugin as any).ctx = ctx;

    // 模拟前端拖动滑块触发 SET_DISTANCE_CM 事件方法
    plugin._distanceCm(42);

    expect(publishedChannels['distanceCm']).toBe(42);
  });
});
```

---

## 🚀 5. 执行测试指令

在 `wink-plugin-peripherals` 仓库根目录下执行：

```bash
# 1. 运行全量外设单元测试
bun test

# 2. 仅运行指定外设的测试用例
bun test builtin/ultrasonic

# 3. 监听模式 (保存文件自动重新跑测试)
bun test --watch
```

> **💡 建议标准**：在提交或发布外设前，保持所有 `*.test.ts` 100% 通过（绿灯），确保物理模型与事件状态机坚固可靠。
