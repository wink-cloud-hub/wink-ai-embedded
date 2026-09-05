# Catalog / 外设元数据 SSOT 收敛 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `subagent-driven-development` or inline phase execution. Steps use checkbox (`- [ ]`) syntax. Each Phase ends with `npm test` + `npm run typecheck` + `npm run build` in `../../../../wink-ai/packages/embedded-frontend/`.

**Goal:** 让「新增/修改一个电路域器件」只需改 **一处 `definition.ts`**，catalog、绑定校验、画布、资产库全部从该 SSOT 派生；消除引脚双写、stub 散点、legacy-adapter 间接路径。

**Architecture:** `peripherals/` 与 `world-assets/` 为声明层 SSOT；`device-catalog.ts` 瘦身为只读聚合 facade；`manifest.bindings` 仍为项目级 binding 实例 SSOT。成熟形态下，**凡参与 `connections` 的器件必须是可上画布的 peripheral 插件**（`CanvasGlyph` 必选）；`stub` 仅为开发阶段命名，不是永久架构分类。

**Tech Stack:** Vue 3.5 + Vite + TypeScript + Vitest；仅 `../../../../wink-ai/packages/embedded-frontend/` 变更。

**Spec:**
- [外设插件注册计划](./2026-07-10-peripheral-plugin-registry-plan.md)（P0–P3 已完成，本计划补齐 P2 残留双写）
- [W2 绑定模型](../../design/05-frontend-workbench/03-dual-viewport-phased-design/02-phase-w2-binding-model.md)
- [如何新增外设](../../design/05-frontend-workbench/04-adding-a-peripheral.md)

## Global Constraints

- 不修改 Manifest schema（`schemaVersion: 2` 不变）
- 不修改 Worker 仿真协议（binding 桥属 W3c，本计划不实现）
- 不提交 `.env`、secrets
- 每 Phase 结束：`npm test` + `npm run typecheck` + `npm run build` 全绿
- Commit message 英文；仅在用户要求时 commit
- 新代码禁止在 `device-catalog.ts` 手写 `category: 'peripheral' | 'stub'` 条目（board 除外）

---

## 1. 元数据表

| 字段 | 内容 |
|------|------|
| **计划编号** | `PLAN-20260711-CATALOG-SSOT` |
| **创建日期** | 2026-07-11 |
| **目标平台** | `embedded-frontend`（浏览器 Wasm 仿真工作台） |
| **计划状态** | ✅ 已完成 |
| **优先级** | 🟡 P1（W3c 前完成，避免 stub/catalog 继续扩散） |
| **计划版本** | v1.0 |
| **预估工期** | ~3–4 天 |
| **关联技术设计** | 无，已并入本计划 |
| **关联设计规范** | [04-adding-a-peripheral.md](../../design/05-frontend-workbench/04-adding-a-peripheral.md)、[02-phase-w2-binding-model.md](../../design/05-frontend-workbench/03-dual-viewport-phased-design/02-phase-w2-binding-model.md) |
| **关联 ADR** | 无 |
| **前置依赖计划** | [2026-07-10-peripheral-plugin-registry-plan.md](./2026-07-10-peripheral-plugin-registry-plan.md) P0–P3 ✅ |
| **所需子代理技能** | `subagent-driven-development` |

---

## 2. 背景与目标

### 2.1 问题陈述

P0–P3 已建立 `peripherals/*/definition.ts` 插件模型，但维护路径仍未统一：

1. **引脚双写**：`catalog.pins`（校验用）与 `pins`（画布用）结构不同、内容重复
2. **`worldCoupling` 双写**：`catalog` 与 `simulation` 各一份
3. **stub 外设散点**：`motor_driver_stub` 等只在 `device-catalog.ts` 的 `STATIC_DEVICES`，与 `peripherals/` 路径不一致
4. **机械/环境模型散点**：`MECHANICAL_MODELS` / `ENVIRONMENT_MODELS` 硬编码在 catalog
5. **legacy-adapter 仍被 5+ call site 依赖**：间接 SSOT，增加认知负担
6. **无 CI 约束**：`allowedSensorMappings` 可与 `mapping-registry` 漂移

若不收敛，W3c 加 Worker binding 桥、新增 buzzer/motor 时会继续在 catalog 手写条目，SSOT 持续恶化。

### 2.2 技术/业务目标

- ✅ 改一个外设引脚/耦合策略只动 `peripherals/<name>/definition.ts` 一处
- ✅ 所有电路域器件（含原 stub）均可拖上画布、连线、过 B-10 校验
- ✅ `device-catalog.ts` 仅做 merge + query，不含手写 peripheral/stub 条目
- ✅ 删除 `legacy-adapter.ts`，call site 直连 `registry`
- ✅ CI 单测防止 catalog ↔ mapping-registry 漂移

### 2.3 成功指标

| 指标 | 通过标准 | 验证方法 |
|------|----------|----------|
| 引脚 SSOT | `grep catalog.pins peripherals/` 为 0 | ripgrep |
| worldCoupling SSOT | `grep simulation.worldCoupling peripherals/` 为 0 | ripgrep |
| stub 收敛 | `STATIC_DEVICES` 仅含 board | 读 `device-catalog.ts` |
| adapter 删除 | `grep peripheralConfigsAdapter embedded-frontend/` 为 0 | ripgrep |
| 绑定回归 | B-01~B-10 单测全绿 | Vitest |
| 构建 | 0 error | `npm run build` |

---

## 3. SSOT 目标架构

### 3.1 归属表（重构后）

| 数据域 | SSOT 位置 | 禁止出现的位置 |
|--------|-----------|----------------|
| 外设引脚（逻辑类型 + 画布布局） | `definition.pins[]` 统一结构 | ~~`catalog.pins` 手写~~ |
| worldCoupling / allowedMappings | `definition.catalog` | ~~`simulation.worldCoupling`~~、`STATIC_DEVICES` 里的 peripheral/stub |
| 画布 type ↔ catalog modelId | `definition.catalog.id` + `definition.type` 派生 | ~~手写 CANVAS_TYPE_TO_MODEL 条目~~ |
| 板型 GPIO 列表 | `boards/esp32-devkit-v1/definition.ts` | ~~`BOARDS` + `STATIC_DEVICES` 双份~~ |
| 机械/环境 modelId | `world-assets/*/definition.ts` | ~~`MECHANICAL_MODELS` 常量数组~~ |
| 映射类型 schema | `types/mapping-registry.ts` | — |
| 用户 binding 实例 | `manifest.bindings` | 任何 definition 文件 |

### 3.2 成熟形态：电路域 vs 世界域

| 层 | 职责 | 画布 | 例子 |
|----|------|------|------|
| `peripherals/` | 电路域器件（有引脚、连 GPIO） | **必选 CanvasGlyph** | HC-SR04、Motor Driver、DHT22、Buzzer |
| `world-assets/` | 纯机械/环境（无电路引脚） | 世界视窗 | chassis、wheel、wall |
| `boards/` | 板型 GPIO 能力 | 固定 Board 区域 | ESP32 DevKit |
| `manifest.bindings` | 语义桥接 | — | radar → mount_ultrasonic |

**架构决策（已定稿）：** 原 `motor_driver_stub` / `dht22_stub` / `buzzer_stub` 迁入 `peripherals/`，**必须包含 `CanvasGlyph.vue`**。`_stub` 后缀仅为过渡期 catalog.id 兼容，成熟后更名为真实型号（如 `motor_driver` / `l298n`）。Phase S1 不要求 `WorldWidget` 或 Wasm 仿真行为。

### 3.3 架构红线

1. **禁止**在 `device-catalog.ts` 新增手写 `category: 'peripheral' | 'stub'` 条目
2. **禁止**新 call site import `legacy-adapter`（S3 完成后删除该文件）
3. **禁止**在 business 代码手写 `from`/`to` 双格式分支（沿用 `connection-normalize.ts`）
4. Manifest `bindings` 实例不得写入 definition 文件

---

## 4. Phase 地图

| Phase | 主题 | 工期 | Checkpoint |
|-------|------|------|------------|
| **S0** | 引脚 + catalog 字段 SSOT 合并 | ~1d | ultrasonic 引脚只改一处 |
| **S1** | stub → peripherals + CanvasGlyph 必选 | ~1d | 避障模板 B-09 回归 |
| **S2** | `world-assets/` 机械/环境注册 | ~1d | 无 MECHANICAL_MODELS 硬编码 |
| **S3** | 删除 legacy-adapter + boards SSOT | ~1d | 无 adapter import |
| **S4** | 校验护栏 + 文档回写 | ~0.5d | mapping 一致性单测 |

S1 可与 S2 并行（不同目录），但 S0 必须先于 S1。

---

## 5. Phase S0 — 引脚与 Catalog 字段 SSOT

### 5.1 类型设计

**Modify:** `../../../../wink-ai/packages/embedded-frontend/src/peripherals/types.ts`

```typescript
/** 统一引脚 SSOT — 画布 + catalog + pin-resolver 共用 */
export interface UnifiedPinDef {
  name: string;
  /** catalog / B-06 用语义类型 */
  catalogType: 'pwm' | 'gpio' | 'digital_in' | 'digital_out' | 'i2c' | 'power';
  description?: string;
  required?: boolean;
  /** 画布 */
  signalType: 'digital' | 'i2c' | 'power' | 'custom';
  defaultConnection?: PinConnectionValue;
  relX?: number;
  relY?: number;
}

export interface PeripheralDefinition {
  pins: UnifiedPinDef[];           // 唯一引脚数组
  catalog: {
    id: string;
    description?: string;
    worldCoupling: WorldCoupling;  // 必填
    allowedActuatorMappings?: string[];
    allowedSensorMappings?: string[];
    // 删除 pins[] — 由 derive 生成
  };
  simulation?: {
    observe?: ObserveFn;
    // 删除 worldCoupling
  };
  // ... 其余字段不变
}
```

**Create:** `../../../../wink-ai/packages/embedded-frontend/src/catalog/derive-catalog-entry.ts`

- `pinsToCatalogPins(pins: UnifiedPinDef[]): CatalogPinDef[]`
- `definitionToCatalogEntry(def: PeripheralDefinition): DeviceCatalogEntry`

从 `device-catalog.ts` 移出 `definitionToCatalogEntry`。

### 5.2 Tasks

- [ ] **S0.1** 新增 `UnifiedPinDef`；创建 `derive-catalog-entry.ts` + 单测（ultrasonic TRIG/ECHO 往返）
- [ ] **S0.2** 迁移 `ultrasonic/definition.ts`：删除 `catalog.pins`；合并为单一 `pins[]`
- [ ] **S0.3** 迁移 `led` / `button` / `oled` / `_template`
- [ ] **S0.4** 删除所有 `simulation.worldCoupling`；derive 只读 `catalog.worldCoupling`
- [ ] **S0.5** 更新 `legacy-adapter.ts` 映射（仍读统一 `pins`，S3 前过渡）
- [ ] **S0.6** golden 单测：`definitionToCatalogEntry` snapshot

### 5.3 验收

- [ ] A1: `grep -r "catalog\.pins" ../../../../wink-ai/packages/embedded-frontend/src/peripherals` 为 0
- [ ] A2: `grep -r "simulation\.worldCoupling" ../../../../wink-ai/packages/embedded-frontend/src/peripherals` 为 0
- [ ] A3: binding-validation + pin-resolver 单测全绿
- [ ] A4: `npm run build` 通过

**Checkpoint S0.**

---

## 6. Phase S1 — Stub 迁入 peripherals（CanvasGlyph 必选）

### 6.1 目标包结构

```
peripherals/
  motor_driver_stub/
    definition.ts
    CanvasGlyph.vue      # 最小占位：矩形 + 标签 + 引脚锚点
    index.ts
  dht22_stub/
    definition.ts
    CanvasGlyph.vue
    index.ts
  buzzer_stub/
    definition.ts
    CanvasGlyph.vue
    index.ts
```

### 6.2 每个包最低交付

| 文件 | 要求 |
|------|------|
| `definition.ts` | S0 统一 `pins[]`；`catalog.id` 保持现有 modelId（`motor_driver_stub` 等）以兼容 Manifest/模板；`worldCoupling` + `allowed*Mappings` |
| `CanvasGlyph.vue` | 可拖拽；引脚位置与 `pins[].relX/relY` 一致；走线颜色 `wireColor` |
| `index.ts` | `registry.register(...)` |
| `WorldWidget.vue` | **S1 不要求**（W3c 绑 3D 时再加） |
| `simulation.observe` | **S1 不要求**（W3c binding 桥为主路径） |

### 6.3 从 catalog 删除

**Modify:** `../../../../wink-ai/packages/embedded-frontend/src/catalog/device-catalog.ts`

- 从 `STATIC_DEVICES` 删除 `motor_driver_stub`、`dht22_stub`、`buzzer_stub`
- `STATIC_DEVICES` 重构后仅保留 board 条目（board 迁出见 S3）

### 6.4 Tasks

- [ ] **S1.1** 创建 `motor_driver_stub` 包（definition + CanvasGlyph + register）
- [ ] **S1.2** 创建 `dht22_stub` 包
- [ ] **S1.3** 创建 `buzzer_stub` 包
- [ ] **S1.4** 更新 `peripherals/index.ts` side-effect imports
- [ ] **S1.5** 更新 `avoidance-car-w2-minimal.ts` 及相关单测（modelId 不变）
- [ ] **S1.6** 契约测试：`deviceCatalog.getDevice('motor_driver_stub')` 来自 registry；资产库 Peripherals 分区可见
- [ ] **S1.7** 手动冒烟：拖 motor_driver_stub 上画布 → 连 PWM 引脚 → static-check 通过

### 6.5 验收

- [ ] B1: `STATIC_DEVICES` 无 stub 条目
- [ ] B2: B-09 对无 binding 的 `hc-sr04` / `motor_driver_stub` 仍阻塞 simulate
- [ ] B3: 三个 stub 均可从资产库拖上画布并连线
- [ ] B4: `npm test` 全绿

**Checkpoint S1.**

---

## 7. Phase S2 — World Assets 注册表

### 7.1 目标结构

**Create:**

```
../../../../wink-ai/packages/embedded-frontend/src/world-assets/
  types.ts
  registry.ts
  ultrasonic_mount_v1/definition.ts
  diff_drive_chassis_v1/definition.ts
  drive_wheel_v1/definition.ts
  env_wall_segment/definition.ts
  env_heat_source/definition.ts
  sensor_enclosure_v1/definition.ts   # 模板前置清单预留
  index.ts
```

```typescript
export interface MechanicalAssetDef {
  id: string;
  displayName: string;
  category: 'mount' | 'chassis' | 'wheel' | 'enclosure';
  defaultTransform?: Transform3D;
}

export interface EnvironmentAssetDef {
  id: string;
  displayName: string;
  category: 'obstacle' | 'field' | 'prop';
}
```

**Modify:** `device-catalog.ts` — `listMechanicalModels()` / `listEnvironmentModels()` 改读 `worldRegistry`；删除 `MECHANICAL_MODELS` / `ENVIRONMENT_MODELS` 常量。

### 7.2 Tasks

- [ ] **S2.1** 建 `world-assets/` 骨架 + registry 单测
- [ ] **S2.2** 迁移现有 modelId 定义（与 W2 §5.5 模板前置清单一一致）
- [ ] **S2.3** 更新 `binding-suggest.service.ts` / `LayeredAssetLibrary.vue`
- [ ] **S2.4** B-03/B-04 binding 校验单测回归

### 7.3 验收

- [ ] C1: `device-catalog.ts` 无 `MECHANICAL_MODELS` / `ENVIRONMENT_MODELS` 硬编码
- [ ] C2: 新增机械零件 = 新建 `world-assets/xxx/` + index import
- [ ] C3: BindingsInspector 自动建议仍能找到 `ultrasonic_mount_v1`

**Checkpoint S2.**

---

## 8. Phase S3 — Legacy Adapter 删除 + Boards SSOT

### 8.1 Call site 迁移

| 文件 | 当前 | 目标 |
|------|------|------|
| `WorkbenchPropertyInspector.vue` | `peripheralConfigsAdapter[type]` | `registry.get(type)?.pins/props` |
| `useCanvasLayout.ts` | adapter.size | `registry.getSize(type)` |
| `useWireRendering.ts` | adapter.pins | `registry.get(type)?.pins` |
| `static-check.service.ts` | adapter.pins | `registry.get(type)?.pins` |

**Create:** `../../../../wink-ai/packages/embedded-frontend/src/boards/esp32-devkit-v1/definition.ts` + `boards/registry.ts`

- 合并 `BOARDS` 与 `STATIC_DEVICES` 中的 board 条目
- `binding-pin-resolver` 继续通过 `deviceCatalog.getBoard()` 查询

**Delete:** `../../../../wink-ai/packages/embedded-frontend/src/peripherals/legacy-adapter.ts`（及仅测 adapter 的用例，改测 registry 直连）

### 8.2 Tasks

- [ ] **S3.1** 迁移 4 个 call site 到 registry API
- [ ] **S3.2** 建 `boards/` registry；更新 `device-catalog.ts`
- [ ] **S3.3** 删除 `legacy-adapter.ts`；更新 `peripherals/index.ts` exports
- [ ] **S3.4** 清理 `types/peripheral-pins.ts` 中与 `peripherals/types.ts` 重复的 `PeripheralPinDef`（re-export 或删除）
- [ ] **S3.5** 手动冒烟：属性面板 / 走线 / 拖放

### 8.3 验收

- [ ] D1: `grep peripheralConfigsAdapter embedded-frontend/` 为 0
- [ ] D2: `grep legacy-adapter embedded-frontend/` 为 0
- [ ] D3: `npm test` + `npm run build` 全绿

**Checkpoint S3.**

---

## 9. Phase S4 — 校验护栏与文档

### 9.1 Catalog ↔ Mapping 交叉校验

**Create:** `../../../../wink-ai/packages/embedded-frontend/src/catalog/__tests__/catalog-mapping-consistency.test.ts`

```typescript
for (const device of deviceCatalog.listDevices()) {
  for (const m of device.simulation?.allowedSensorMappings ?? []) {
    expect KNOWN_SENSOR_MAPPING_TYPES.has(m)).toBe(true);
  }
  for (const m of device.simulation?.allowedActuatorMappings ?? []) {
    expect KNOWN_ACTUATOR_MAPPING_TYPES.has(m)).toBe(true);
  }
}
```

### 9.2 文档回写

- [ ] **S4.1** 更新 [04-adding-a-peripheral.md](../../design/05-frontend-workbench/04-adding-a-peripheral.md)：
  - SSOT 归属表
  - stub 迁移为正式 peripheral 的路径
  - 仿真接入：binding 桥（主路径）vs `simulation.observe`（OLED 等过渡）
- [ ] **S4.2** 更新 `_template/definition.ts` 头部注释（UnifiedPinDef、catalog 必填字段）
- [ ] **S4.3** 在 [01-device-model-registry.md](../../design/07-platform-governance/01-device-model-registry.md) 增补「前端 catalog SSOT 归属」一节（≤1 页）
- [ ] **S4.4** 本计划状态更新为 ✅ 已完成

### 9.3 验收

- [ ] E1: mapping 一致性单测绿灯
- [ ] E2: 文档与代码结构一致

**Checkpoint S4 — 计划完成.**

---

## 10. 依赖与风险

### 10.1 前置依赖

| 依赖 | 状态 |
|------|------|
| 外设插件 registry P0–P3 | ✅ 已完成 |
| W2 binding-validation B-01~B-10 | ✅ 已完成 |

### 10.2 风险矩阵

| 风险 | 级别 | 缓解 |
|------|------|------|
| `catalogType` ↔ `signalType` 映射错误 | 高 | golden 单测 + B-06/B-10 回归 |
| stub CanvasGlyph 引脚锚点偏移导致走线异常 | 中 | 对照 ultrasonic 引脚布局；useWireRendering 单测 |
| 删除 adapter 遗漏 call site | 中 | ripgrep CI + 更新 call-sites 契约测试 |
| catalog.id 改名破坏现有 Manifest | 高 | **S1 保持 `motor_driver_stub` 等 id 不变** |
| world-assets 与 W3c GLB 路径不一致 | 低 | definition 只声明 id；GLB W3c 对齐 |

---

## 11. 非目标

- Worker 侧 binding 桥实现（W3c）
- 删除 `simulation.observe`（W3c 接通后另计划评估）
- Manifest schema 变更
- `import.meta.glob` 自动发现（保持显式 import）
- 全量 Prettier 格式化

---

## 12. 新增器件 Checklist（重构后）

```
□ peripherals/<name>/definition.ts     ← 唯一手写入口
  □ pins[] 含 catalogType + 画布字段
  □ catalog.id / worldCoupling / allowed*Mappings（无 catalog.pins）
  □ CanvasGlyph.vue（有引脚即必选）
  □ world / simulation.observe（按阶段可选）
□ peripherals/<name>/index.ts → registry.register
□ peripherals/index.ts 加 import
□ world-assets/<name>/（若绑定需要新机械/环境零件）
□ types/mapping-registry.ts（若为新 mapping 类型）
□ catalog-mapping-consistency 单测
□ binding-validation 相关单测
```

**禁止：** 在 `device-catalog.ts` 手写 peripheral/stub 条目。

---

## 13. 文档变更记录

- 2026-07-11：v1.0 初版。定稿：成熟形态下电路域器件必须可上画布；S1 stub 迁移含 CanvasGlyph 必选。
- 2026-07-11：v1.1 执行完成——S0–S4 全绿（230 tests）；boards SSOT + 文档回写。

