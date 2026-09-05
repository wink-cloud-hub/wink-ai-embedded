# 04. 跨仓组件依赖、契约协议与商业机密隔离规范 (Cross-Repository Boundary & Confidentiality Contract)

> **文档定位**：本文定义 `wink-ai-embedded`（内核与工具链仓）与 `wink-ai` 主 monorepo 兄弟包（`embedded-frontend`、`unisim`）之间的跨仓物理依赖架构、数据契约协议以及**商业机密隔离原则**。

---

## 1. 商业机密与黑盒隔离原则 (Black-Box Insulation Rule)

为了保障商业核心资产与代码权限隔离，跨仓架构文档遵循以下**黑盒隔离铁律**：

1. **黑盒边界**：归属于 `wink-ai` 主仓的外部组件（如 `embedded-frontend` UI 编辑器、`unisim` 仿真引擎等），本仓设计文档**仅定义其功能作用 (Function)、使用场景 (Usage)、公开 API / DTO / CLI 契约以及输入输出产物**。
2. **禁止泄露内部实现**：严禁在本仓文档中记录主仓私有渲染优化、商业版编辑器业务逻辑、后端鉴权与云端调度算法细节。
3. **接口契约即真相**：两仓交互完全依赖机读契约文件（`wink-app.json` Schema、`SimTraceSpecV2`、`wasm_bridge.h` ABI），只要接口契约不变，两仓均可独立迭代演进。

---

## 2. 现行物理包结构与跨仓映射

在当前的 Wink-AI Monorepo 体系中，跨仓物理分工如下：

```text
Wink-AI 跨仓组件分布与黑盒契约边界:

[ 外部主仓组件 ] (黑盒依赖，仅依赖公开 API / Manifest / ABI 契约)
├── embedded-frontend                       # 跨仓组件 1：嵌入式 Web 工作台 UI (黑盒包)
│   └── 契约接口：wink-app.json Manifest, Dual-Viewport State Sync DTO
└── unisim                                  # 跨仓组件 2：UniSim Wasm 仿真引擎 (黑盒包)
    └── 契约接口：wasm_bridge.h C-ABI, SimTraceSpecV2 Spec

[ 本仓组件: wink-ai-embedded ] (包含内核 Code-Mapping)
├── wink-tools/                             # 跨仓组件 3：统一 CLI 工具链 (wink CLI)
├── wink-micro-os/                          # 跨仓组件 4：C 语言 SDK 内核 (PAL/DAL/BAL，Code-Mapping SSOT)
└── wink-micro-app/                         # 跨仓组件 5：嵌入式应用工程规范
```

---

## 3. 跨仓三大机读契约协议

### 3.1 契约一：项目单一事实源 (`wink-app.json` Manifest)

`wink-app.json` 是 `embedded-frontend`、`wink-tools` 与 `wink-micro-os` 三者间唯一交换的项目定义文档：

```json
{
  "schemaVersion": 2,
  "name": "distance_alarm",
  "target_board": "esp32_devkitc",
  "tick_ms": 10,
  "devices": [
    {
      "id": "radar_1",
      "model": "hc_sr04",
      "pin_map": { "trig": 12, "echo": 13 }
    }
  ]
}
```

- **`embedded-frontend` 职责**：负责可视化编辑、属性配置并序列化导出 `wink-app.json`。
- **`wink-tools` 职责**：读取 `wink-app.json` 并调用 `wink gen` 输出 `app_main.c` 和 `device_tree.c`。
- **`wink-micro-os` 职责**：编译并运行导出的 C 代码。

### 3.2 契约二：Wasm 仿真桥接 C-ABI (`wasm_bridge.h`)

`wink-micro-os` 编译为 `wasm32` 目标时，暴露固定的 C-ABI 供 `unisim` 在 Web Worker 中装载：

- **导出入口**：`wink_wasm_init()`、`wink_wasm_step(microseconds)`、`wink_wasm_get_trace_buffer()`。
- **导入桩**：由 `unisim` 注入虚拟外设读写挂钩 (`unisim_gpio_write` / `unisim_i2c_transfer`)。

### 3.3 契约三：仿真与真机事件追溯协议 (`SimTraceSpecV2`)

用于 Headless 自动化测试、前端时间线渲染以及虚实一致性比对：

- **格式**：JSONL / JSON 结构化 Envelope，包含微秒级时间戳、器件 ID、事件类型（`GPIO_SET` / `I2C_TRANSFER` / `FAULT_INJECT`）及状态负载。
- **消费方**：`embedded-frontend` Trace 控制台展示，`wink test` CLI 用于 CI 断言。

---

## 4. 依赖安全与门禁策略

1. **单向依赖**：`wink-micro-os` C 内核绝对不依赖任何 Node.js/TS 包；`unisim` 与 `embedded-frontend` 仅依赖 `wink-micro-os` 导出的 Wasm 产物与公共 C 结构体头。
2. **构建隔离**：`wink build` 运行于独立的沙箱容器中，编译脚本不得直接调用主仓私有 API。
3. **版本锁定**：Manifest `schemaVersion` 与 `SimTraceSpecV2` 必须保持前向兼容与版本升级迁移校验 (`manifest-migration.ts`)。
