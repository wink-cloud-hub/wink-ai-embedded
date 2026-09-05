# 避障小车 Phase 1 — 舵机观测闭环 专家评审报告

> **评审对象：** `docs/implementation-plans/core/2026-07-11-avoidance-car-phase1-servo-observe-plan.md` 及其已实现代码
> **评审日期：** 2026-07-12
> **评审人：** Qoder（资深嵌入式仿真设计视角）
> **计划状态：** ✅ 已完成（commit `5818d50`）

---

## 1. 总体评价

**综合评分：8.5 / 10 — 优秀**

本计划在已完成的 Phase 1 中建立了一套**双层执行器观测模型**（Raw `ActuatorOutputBatch` → Semantic `ActuatorObservation`），实现了从 Wasm 固件 PWM 占空比到前端舵机角度显示的完整闭环。架构分层清晰、类型契约严谨、扩展性预留充分，是一份高质量的嵌入式仿真基础设施设计。

| 维度 | 评分 | 说明 |
|------|------|------|
| 架构设计 | 9/10 | 双层模型 + 三输入契约，扩展性好 |
| 计划完整度 | 9/10 | 引脚 SSOT、因果链、风险表齐全 |
| 实现忠实度 | 9/10 | 与计划高度一致，converter 独立文件是合理偏差 |
| 测试覆盖 | 7/10 | 正常路径覆盖，边界/异常路径不足 |
| 未来演进 | 8/10 | Phase 2 预留充分，value 类型和 converter props 读取需补强 |

---

## 2. 架构设计评审

### 2.1 双层执行器模型 — 核心亮点

```text
┌─ Raw 层 (Worker) ─────────────────────┐
│  ActuatorOutputBatch { pwm, gpio }     │  ← 传输层，与硬件通道一一对应
└──────────────┬────────────────────────┘
               │ STATE_UPDATE.actuatorOutputs
┌──────────────▼────────────────────────┐
│  Mapper (主线程)                       │  ← 语义层，消化 PWM/FOC/VESC 差异
│  batch + sources + components          │
│  → ActuatorObservation[]               │
└──────────────┬────────────────────────┘
               │
┌──────────────▼────────────────────────┐
│  UI / 3D ActuatorMirror (只读)         │  ← 消费层，零硬件知识
└───────────────────────────────────────┘
```

**评价：** 这是本计划最核心的设计决策，非常正确。

- `ActuatorOutputBatch` 对齐硬件传输通道（PWM channel、GPIO pin），Worker 采集时不需要理解外设语义，职责单一。
- `ActuatorObservation` 是统一物理量表达，UI/3D 层只消费此结构，不感知底层是 PWM 还是 FOC 还是 VESC。
- 这个分层在工业仿真框架（如 MATLAB/Simulink 的 Signal Bus、AUTOSAR 的 Sender-Receiver）中是标准做法，符合嵌入式仿真领域的最佳实践。
- Phase 2 扩展 motor_driver 时，只需新增 converter 注册 + binding 分支，UI 层零改动。

### 2.2 Mapper 三输入契约 — 正确的约束

```typescript
mapActuatorOutputs(
  batch: ActuatorOutputBatch,       // Raw 数据
  actuatorSources: ActuatorObserveSource[],  // 外设声明的采集源
  components: CircuitComponentInstance[],    // 画布实例（含 props）
): ActuatorObservation[]
```

**评价：** 强制三类输入同时传入，避免了仅靠 `components[]` 推断 transport 的隐式耦合。

- `batch` 提供 raw 数据，来源明确（Worker 采集）
- `actuatorSources` 提供 transport 映射关系，来源明确（外设 `simulation.observe` 声明）
- `components` 提供 props（如 `minPulseMs`/`maxPulseMs`），来源明确（画布实例）

三者各司其职，任何一类缺失都会导致映射不完整。这是面向多执行器类型扩展的正确约束。

### 2.3 方案 A（嵌入 STATE_UPDATE） — 务实选择

在 `STATE_UPDATE.payload.actuatorOutputs` 中嵌入 PWM 数据，而非新增独立 `actuatorOutput` 事件。

**评价：** Phase 1 数据量极小（1 个 PWM channel + 少量 GPIO），嵌入 STATE_UPDATE 减少 Worker 消息类型膨胀。W3b 文档已回写 Spike 结论，协议统一。

**潜在风险：** 当执行器数量增多（如 6 舵机 + 2 电机），`actuatorOutputs` payload 可能变大。但由于 STATE_UPDATE 本身已有帧率控制（仿真步进），不会造成额外带宽压力。Phase 2 如果 payload 超过 1KB，建议评估是否需要独立事件 + 降频采样。

### 2.4 `deviceComponentId` SSOT — 关键决策

统一为 `CircuitComponentInstance.id`（= Manifest `devices[].componentId`），消除了画布 ID vs manifest componentId 的二义性。

**评价：** 这是避免后续 3D 镜像层对接时出 bug 的关键决策。在 Phase 2 引入 3D ActuatorMirror 时，3D 场景中的 mesh 需要与 `deviceComponentId` 关联消费 `ActuatorObservation`。如果此处有二义性，3D 层会出现"找不到对应执行器"或"映射到错误执行器"的问题。

---

## 3. 实现质量审查

### 3.1 计划 vs 实现对照表

| Task | 计划要求 | 实现文件 | 状态 | 备注 |
|------|---------|---------|------|------|
| T0 | `pal_wasm_get_pwm_duty_percent` export | `wasm_dev_servo.c:36-41` | ✅ | 含 bounds check |
| T1 | 构建链指向 `wink-micro-app/` | `build-wasm.mjs:28` | ✅ | `microAppDir` fallback |
| T2 | servo 外设插件 | `peripherals/servo/{definition,index,CanvasGlyph}.ts` | ✅ | 完整 |
| T2 | converter 注册 | `servo/index.ts:8-23` | ✅ | 独立文件 `actuator-converter-registry.ts` |
| T3 | 类型 SSOT | `types/actuator-observation.ts` | ✅ | 与计划完全一致 |
| T3 | Mapper 三输入 | `services/actuator-observation.mapper.ts` | ✅ | 含 error handling |
| T3 | observe-builder 扩展 | `peripherals/observe-builder.ts:15,52-54` | ✅ | `watchActuatorSource` |
| T3 | Worker 采集 | `workers/wasm-simulation.worker.ts:276-296` | ✅ | 含 export 存在性检查 |
| T3 | 方案 A client mapper | `services/simulation-client.ts:108-111` | ✅ | mapper 在 client 层 |
| T3 | runtime 数据面 | `services/simulation-runtime.ts:25-26` | ✅ | `shallowRef` |
| T4 | 模板扩展 neck_servo | `templates/avoidance-car-w2-minimal.ts:94-104` | ✅ | props 含 pwmChannel/minPulseMs/maxPulseMs |
| T4 | 模板连线 | 同上 `:108-125` | ✅ | SIG→GPIO2, VCC, GND |
| T5 | SimActuatorPanel | `components/workbench/SimActuatorPanel.vue` | ✅ | 只读 `actuatorObservations` |
| T6 | 架构护栏 | `__tests__/sim-actuator-panel.arch.test.ts` | ✅ | 禁止 raw 引用 |
| T7 | W3b Spike 回写 | `04-phase-w3b-physics-actuators.md:38-55` | ✅ | 方案 A 结论 |
| — | App 零改动 | `wink-micro-app/avoidance_car/*.c` | ✅ | 约束遵守 |

### 3.2 实现中的合理偏差

1. **`actuator-converter-registry.ts` 独立文件** — 计划中将 converter registry 定义在 `actuator-observation.mapper.ts` 内。实现将其拆为独立文件，避免了 `servo/index.ts` import mapper 时产生的循环依赖（servo → mapper → peripherals/registry → servo）。这是正确的工程决策。

2. **Mapper 中的 error handling** — 计划中未明确要求 try/catch，但实现在 converter 调用处加了 `try/catch` 并 `console.error`。这是合理的防御性编程，因为 converter 是外设插件注册的，可能有 bug。

3. **Worker 中的 export 存在性检查** — `hasEmscriptenExport(realModule, 'pal_wasm_get_pwm_duty_percent')` 在采集前检查 export 是否存在。这是正确的防御，因为旧版 Wasm 可能没有此 export。

### 3.3 Converter 数学验证

**C 侧** (`wasm_dev_servo.c:54-64`):
```c
pulse_us = duty_cycle_percent * 200.0f;  // 50Hz → 20ms period → duty% * 200 = us
if (pulse_us <= 500)  angle = 0;
else if (pulse_us >= 2500) angle = 180;
else angle = (pulse_us - 500) * 180 / 2000;
```

**JS 侧** (`servo/index.ts:15`):
```typescript
rawAngle = (duty - 2.5) * 18;
```

**等价性验证：**
- `(duty * 200 - 500) * 180 / 2000`
- `= (duty * 200 - 500) * 0.09`
- `= duty * 18 - 45`
- `= (duty - 2.5) * 18` ✅

**边界验证：**
| duty% | pulse_us | C angle | JS angle | 一致 |
|-------|----------|---------|----------|------|
| 2.5 | 500 | 0° | 0° | ✅ |
| 7.5 | 1500 | 90° | 90° | ✅ |
| 12.5 | 2500 | 180° | 180° | ✅ |
| 0.0 | 0 | 0° (clamped) | -45° → 0° (clamped) | ✅ |
| 15.0 | 3000 | 180° (clamped) | 225° → 180° (clamped) | ✅ |

---

## 4. 发现的问题与建议

### 4.1 [P1 — 应修复] Converter 未读取 props 中的 pulse range

**文件：** `../../../../wink-ai/packages/embedded-frontend/src/peripherals/servo/index.ts:8-23`

**问题：** `sg90_from_duty` converter 硬编码了 0.5ms/2.5ms 的 pulse range（对应 duty 2.5%/12.5%），但 `ActuatorObserveProfile` 的 converter 签名中已包含 `ctx.props`。模板中配置了 `minPulseMs: 0.5, maxPulseMs: 2.5`，但如果用户修改这些 props（例如使用不同规格的舵机），converter 不会响应。

**当前代码：**
```typescript
actuatorConverterRegistry.register('sg90_from_duty', (duty, ctx) => {
  const rawAngle = (duty - 2.5) * 18;  // 硬编码 2.5% 起点、10% 范围
  const value = Math.max(0, Math.min(180, rawAngle));
  // ...
});
```

**建议修改：**
```typescript
actuatorConverterRegistry.register('sg90_from_duty', (duty, ctx) => {
  const minPulseMs = (ctx.props?.minPulseMs as number) ?? 0.5;
  const maxPulseMs = (ctx.props?.maxPulseMs as number) ?? 2.5;
  const periodMs = 20;  // 50Hz
  const minDuty = (minPulseMs / periodMs) * 100;  // 2.5
  const maxDuty = (maxPulseMs / periodMs) * 100;  // 12.5
  const dutyRange = maxDuty - minDuty;             // 10

  const rawAngle = dutyRange > 0 ? ((duty - minDuty) / dutyRange) * 180 : 0;
  const value = Math.max(0, Math.min(180, rawAngle));
  return { quantity: 'angular_position', value, unit: 'deg', role: 'command' };
});
```

**影响：** Phase 1 默认值下行为正确，但 props 配置形同虚设。Phase 2 引入不同规格舵机时会暴露此问题。

### 4.2 [P2 — 建议补强] `lastActuatorSources` 模板切换生命周期

**文件：** `../../../../wink-ai/packages/embedded-frontend/src/services/simulation-runtime.ts:25-26`, `simulation-client.ts:212`

**问题：** `lastActuatorSources` 在 `observePins()` 调用时更新（`:212`），在 reset 时是否清空需确认。如果用户执行以下操作序列：

1. 加载避障模板 → `observePins()` → `lastActuatorSources = [neck_servo pwm_ch0]`
2. 切换到空模板（无舵机）→ `observePins()` → `lastActuatorSources = []`
3. 但如果切换过程中 `observePins()` 未被重新调用 → `lastActuatorSources` 残留旧值

**风险：** 残留的旧 `actuatorSources` 会导致 Mapper 用旧的 `deviceComponentId` 在新 `components[]` 中查找，由于 `comp` 找不到而 `continue`，最终产出空数组。功能上不会出错（Mapper 有 `if (!comp) continue` 保护），但会产生不必要的计算和潜在的日志噪声。

**建议：** 确认模板切换时 `observePins()` 是否必然重新调用。如果是，则无需修改；如果不是，在 `simulation-runtime.ts` 的 reset 逻辑中同步清空 `lastActuatorSources`。

### 4.3 [P2 — 建议补强] 测试覆盖不足

**文件：** `../../../../wink-ai/packages/embedded-frontend/src/services/__tests__/actuator-observation.mapper.test.ts`

**当前覆盖：** 仅 1 个 test case（正常路径：2 个 servo，duty 7.5% → 90°，duty 12.5% → 180°）。

**建议补充的 test cases：**

| 场景 | 预期行为 |
|------|---------|
| `transport: 'gpio_pin'` | 从 `batch.gpio[transportKey]` 读取 |
| converter 未注册 | 该 source 被 skip，不报错 |
| `comp` 未找到（`deviceComponentId` 不在 `components` 中） | 该 source 被 skip |
| 空 `actuatorSources` | 返回空数组 |
| 空 `batch.pwm` | 对应 source rawValue = 0 |
| duty 越界（< 0 或 > 100） | clamp 到 0~180 |
| `def.actuatorObserve` 不存在（非执行器外设） | 该 source 被 skip |
| converter 抛异常 | 该 source 被 skip，console.error |

**补充测试示例：**

```typescript
it('skips sources with unregistered converter', () => {
  const batch = { simTimeUs: '0', pwm: { 0: 7.5 }, gpio: {} };
  const sources = [{ deviceComponentId: 'x', transport: 'pwm_channel', transportKey: 0 }];
  const components = [{ id: 'x', type: 'unknown_type', props: {}, pinConnections: {}, position: { x: 0, y: 0 } }];
  expect(mapActuatorOutputs(batch, sources, components)).toEqual([]);
});

it('clamps angle to 0-180 for out-of-range duty', () => {
  const batch = { simTimeUs: '0', pwm: { 0: -5, 1: 200 }, gpio: {} };
  const sources = [
    { deviceComponentId: 'a', transport: 'pwm_channel', transportKey: 0 },
    { deviceComponentId: 'b', transport: 'pwm_channel', transportKey: 1 },
  ];
  const components = [
    { id: 'a', type: 'servo', props: { pwmChannel: 0 }, pinConnections: {}, position: { x: 0, y: 0 } },
    { id: 'b', type: 'servo', props: { pwmChannel: 1 }, pinConnections: {}, position: { x: 0, y: 0 } },
  ];
  const obs = mapActuatorOutputs(batch, sources, components);
  expect(obs[0].value).toBe(0);
  expect(obs[1].value).toBe(180);
});
```

### 4.4 [P3 — Phase 2 前] `ActuatorObservation.value` 类型过于宽松

**文件：** `../../../../wink-ai/packages/embedded-frontend/src/types/actuator-observation.ts:18`

```typescript
value: number | string | any[];
```

`any[]` 是 escape hatch，在 `pixel_colors`（灯带）场景下会丢失类型信息。建议 Phase 2 前收敛为：

```typescript
// Option A: discriminated union
type ActuatorValue = number | string | number[] | string[];

// Option B: generic (more strict but more complex)
interface ActuatorObservation<T = number> {
  value: T;
  // ...
}
```

Phase 1 只用到 `number`，暂无实际问题。但建议在类型文件中加 `// TODO(phase2): tighten value type — see review 2026-07-12 §4.4` 注释标记。

### 4.5 [P3 — Phase 2 前] 缺少 `simTimeUs` 单调递增校验

**问题：** Mapper 未校验 `batch.simTimeUs` 是否单调递增。Phase 2 动力学平滑需要 `deltaUs = current.simTimeUs - prev.simTimeUs`，如果 Worker 因某种原因重发旧帧（如网络抖动导致 STATE_UPDATE 乱序），Mapper 会静默接受，导致平滑计算出错。

**建议：** Phase 2 在 Mapper 或 `simulation-client.ts` 的 STATE_UPDATE handler 中加 monotonically-ascending guard：

```typescript
if (BigInt(payload.actuatorOutputs.simTimeUs) <= BigInt(lastSimTimeUs)) {
  return;  // drop stale frame
}
```

Phase 1 无需实现，但 `ActuatorObservation.quality?` 字段已预留，为 Phase 2 留了扩展点。

### 4.6 [P3 — 可选] `SimActuatorPanel.vue` 显示精度

**文件：** `../../../../wink-ai/packages/embedded-frontend/src/components/workbench/SimActuatorPanel.vue:36`

```vue
{{ typeof obs.value === 'number' ? Math.round(obs.value) : obs.value }}
```

验收标准 A4 要求 ±5° 容差，`Math.round` 满足。但 Phase 2 动力学平滑后角度可能非整数（如 `89.7°`），`Math.round` 会丢失精度信息，不利于调试。

**建议：** 改为 `toFixed(1)`，显示 1 位小数：

```vue
{{ typeof obs.value === 'number' ? obs.value.toFixed(1) : obs.value }}
```

或保留当前实现但增加 DevTools 友好属性（如 `data-raw-value`）。

---

## 5. 安全与鲁棒性

### 5.1 Worker 侧防御

- `pal_wasm_get_pwm_duty_percent` C 函数有 `channel >= MAX_PWM_CHANNELS` bounds check ✅
- Worker 有 `hasEmscriptenExport` 存在性检查 ✅
- Mapper 有 `if (!comp) continue` / `if (!profile) continue` / `if (!converter) continue` 三级保护 ✅
- Converter 调用有 try/catch ✅

### 5.2 类型安全

- `ActuatorObserveSource.transport` 是 union type，Mapper 中用 `typeof transportKey === 'number'` 做 type narrowing ✅
- `ActuatorQuantity` 和 `unit` 是封闭 union，新增类型需显式扩展 ✅

### 5.3 潜在风险

- `actuatorConverterRegistry` 是全局 Map，如果两个外设插件注册同名 converter，后者会覆盖前者。建议 Phase 2 加 `console.warn` 在 `register` 时检测重复。
- `ObserveBuilderImpl.build()` 使用 spread `...this.params`，如果 `setParam` 设置了与 `pins`/`oled` 同名的 key，会覆盖内置字段。建议 Phase 2 将 params 放在独立 namespace 下。

---

## 6. 与后续 Phase 的衔接评估

### 6.1 Phase 1 交付物可复用性

| 交付物 | Phase 2 复用方式 | 评估 |
|--------|-----------------|------|
| `ActuatorOutputBatch` | 新增 `uart`/`i2c` 字段用于 serial actuators | ✅ 直接扩展 |
| `ActuatorObservation` | 新增 `angular_velocity` 等 quantity | ✅ enum 扩展 |
| `mapActuatorOutputs` | Phase 2 加 binding 分支 | ✅ 三输入不变 |
| `actuatorConverterRegistry` | 注册 `pwm_to_angular_velocity` 等 | ✅ 直接注册 |
| `watchActuatorSource` | motor_driver 插件声明 `pwm_channel` | ✅ 直接复用 |
| `SimActuatorPanel` | 展示 motor RPM 等 | ✅ 只读 observation |
| `pal_wasm_get_pwm_duty_percent` | 保留作 fallback | ✅ |

### 6.2 Phase 2 需要新增的基础设施

1. **`ActuatorOutputBatch.semantic[]`** — DAL Wasm export 直填语义快照，Mapper 优先消费
2. **Binding 分支** — Mapper 中 `if (manifest.bindings.actuators[...])` 优先用 binding 公式
3. **动力学平滑** — Mapper 或 Virtual Device 层做角度/速度积分
4. **3D ActuatorMirror** — 消费 `actuatorObservations` 驱动 3D mesh
5. **`simTimeUs` 单调校验** — 防止乱序帧

### 6.3 FOC / VESC 演进路径

计划中已明确：扩展 `ActuatorOutputBatch.semantic[]` 或 transport enum，Mapper 优先 semantic、fallback pwm/gpio。路径清晰，Phase 1 的 `semantic?: ActuatorObservation[]` 字段已预留。

---

## 7. 总结与建议优先级

| 优先级 | 编号 | 问题 | 建议 | 时机 |
|--------|------|------|------|------|
| **P1** | §4.1 | Converter 未读取 props pulse range | 从 `ctx.props` 读 minPulseMs/maxPulseMs | 尽快修复 |
| **P2** | §4.2 | `lastActuatorSources` 模板切换 | 确认 observePins 重调时机 | 验证后决定 |
| **P2** | §4.3 | 测试覆盖不足 | 补充 6-8 个边界/异常 test cases | 尽快补强 |
| P3 | §4.4 | `value` 类型宽松 | 收敛为 `number \| string \| number[] \| string[]` | Phase 2 前 |
| P3 | §4.5 | `simTimeUs` 单调校验 | 防乱序帧 | Phase 2 |
| P3 | §4.6 | 显示精度 `Math.round` | 改 `toFixed(1)` | 可选 |

**最需立即修复的 1 项：** §4.1 — `sg90_from_duty` converter 应读取 `ctx.props.minPulseMs/maxPulseMs`，否则模板中配置的 pulse range 不生效。

**最需 Phase 2 前补强的 1 项：** §4.3 — Mapper 单测边界覆盖，确保 skip/error 路径被锁定。

---

*评审完成。整体而言，这是一份设计严谨、实现忠实的 Phase 1 交付。上述建议均为增量改进，不影响当前演示闭环的正确性。*
