# 09. 虚实一致性验证、Golden Trace 与可复现实验规范

Wink-AI 平台的核心承诺是“同一份业务逻辑先在浏览器仿真验证，再部署到真实硬件”。要让该承诺具备工程可信度，必须建立可量化、可回放、可比较的虚实一致性验证体系，而不是只依赖用户肉眼观察仿真效果。

---

## 1. 一致性目标

1. **输入一致**：仿真和真机接收同一组逻辑输入事件。
2. **状态一致**：App 状态机在关键节点产生相同状态迁移。
3. **输出一致**：DAL 执行器命令在容忍时间窗口内一致。
4. **错误一致**：故障、超时、断线等错误码语义一致。
5. **可复现**：任意一次仿真或真机运行都能导出 trace 并回放。

---

## 2. Golden Trace 概念

Golden Trace 是平台记录的一组结构化运行事件。它不记录底层所有电平波形，而是记录与业务逻辑相关的语义事件。

```text
Golden Trace
├── Input Events       用户输入、传感器物理量、故障注入
├── App Events         app_init、app_loop、状态迁移、fault
├── DAL Read Events    传感器读取值与状态码
├── DAL Write Events   执行器控制命令与状态码
├── PAL Events         关键总线事务摘要
└── Runtime Events     watchdog、memory、worker lifecycle
```

---

## 3. Trace 事件结构

```json
{
  "traceVersion": 1,
  "runId": "run_20260622_000001",
  "environment": "simulation",
  "target": "wasm32-emscripten",
  "projectHash": "sha256:...",
  "deviceTreeHash": "sha256:...",
  "startedAt": "2026-06-22T00:00:00Z",
  "events": [
    {
      "seq": 1,
      "timeMs": 0,
      "type": "runtime.start",
      "payload": { "runtime": "wink-micro-os", "version": "0.1.0" }
    },
    {
      "seq": 2,
      "timeMs": 10,
      "type": "dal.read",
      "componentId": "front_radar",
      "api": "dal_ultrasonic_read",
      "status": "WINK_OK",
      "payload": { "distanceCm": 35.2 }
    },
    {
      "seq": 3,
      "timeMs": 12,
      "type": "bal.state_change",
      "payload": { "from": "RUNNING", "to": "AVOIDING" }
    },
    {
      "seq": 4,
      "timeMs": 13,
      "type": "dal.write",
      "componentId": "neck_servo",
      "api": "dal_rc_servo_set_angle",
      "status": "WINK_OK",
      "payload": { "angleDeg": 180 }
    }
  ]
}
```

---

## 4. 事件类型规范

| 类型 | 含义 | 必填字段 |
|---|---|---|
| `runtime.start` | 运行开始 | runtime, version |
| `runtime.stop` | 运行停止 | reason |
| `runtime.watchdog` | watchdog 事件 | loopTimeMs, action |
| `input.sensor` | 传感器输入 | componentId, value |
| `input.user` | 用户交互 | componentId, action |
| `fault.inject` | 故障注入 | componentId, faultType |
| `bal.init` | app_init 调用 | status |
| `bal.loop` | app_loop 周期 | durationMs |
| `bal.state_change` | 状态机迁移 | from, to |
| `bal.fault` | app_on_fault 调用 | faultCode |
| `dal.read` | DAL 读取 | componentId, api, status, value |
| `dal.write` | DAL 写入 | componentId, api, status, command |
| `pal.transfer` | 协议事务 | bus, port, size, status |

---

## 5. 仿真 Trace 采集

Wasm 仿真环境采集 trace 的位置：

1. WinkMicroOS 调用 `app_init()` 前后。
2. 每次 `app_loop()` 开始和结束。
3. DAL API 入口与出口。
4. PAL 总线事务摘要。
5. JS 侧传感器输入与 UI 事件。
6. 故障注入器触发时。
7. Worker watchdog 事件。

C 侧接口：

```c
void wink_trace_dal_read(uint32_t component_id, uint16_t api_id, wink_status_t status, float value);
void wink_trace_dal_write(uint32_t component_id, uint16_t api_id, wink_status_t status, float command);
void wink_trace_fault(uint32_t fault_code);
void wink_trace_state_change(uint16_t from_state, uint16_t to_state);
```

Wasm 环境中，这些接口通过 JS import 写入 Worker trace buffer。

---

## 6. 真机 Trace 采集

真机上不应输出过高频 trace，以免影响实时性。建议采用：

1. 环形缓冲区存储关键事件。
2. 串口低频批量输出。
3. 可通过编译选项关闭 trace。
4. 只记录语义事件，不记录全部 GPIO 电平。

串口输出格式使用 JSON Lines：

```jsonl
{"seq":1,"timeMs":0,"type":"runtime.start","target":"esp32"}
{"seq":2,"timeMs":10,"type":"dal.read","componentId":"front_radar","status":"WINK_OK","distanceCm":35.1}
{"seq":3,"timeMs":13,"type":"dal.write","componentId":"neck_servo","status":"WINK_OK","angleDeg":180}
```

---

## 7. Trace 对比规则

虚实一致性不是要求毫秒级完全相同，而是要求在容忍窗口内语义一致。

| 对比项 | 规则 |
|---|---|
| 事件顺序 | 关键因果顺序必须一致 |
| 时间 | 允许目标相关容差，例如 ±50ms |
| 浮点值 | 允许绝对/相对误差，例如 ±1cm |
| 状态迁移 | 必须完全一致 |
| 错误码 | 必须完全一致 |
| 执行器命令 | 最终目标值必须一致 |

> **错误码覆盖范围**：trace 的 `status` 字段必须能承载 [ADR-0001](../../decisions/core/0001-error-code-sign-convention.md) 全部码位，包括新增的功能安全码（`WINK_ERR_OVERCURRENT` / `OVERTEMPERATURE` / `WATCHDOG` / `OVERFLOW` / `PANIC`）。异常测试（C2）与真机一致性对比（C3）应专门纳入这些码位，验证仿真与真机对过流 / 过温 / 看门狗 / 溢出等故障的分类与降级是否一致（降级分类见 [`02-error-fault-model.md` §7](./02-error-fault-model.md)）。

对比结果：

```json
{
  "matched": false,
  "summary": {
    "totalEvents": 120,
    "matchedEvents": 116,
    "mismatches": 4
  },
  "mismatches": [
    {
      "type": "value_delta_exceeded",
      "simulationEventSeq": 42,
      "hardwareEventSeq": 41,
      "field": "distanceCm",
      "simulationValue": 18.2,
      "hardwareValue": 23.8,
      "tolerance": 1.0
    }
  ]
}
```

---

## 8. 输入回放机制

仿真器必须能从 trace 中提取输入事件并回放：

```json
{
  "replayVersion": 1,
  "events": [
    {
      "timeMs": 1000,
      "type": "input.sensor",
      "componentId": "front_radar",
      "payload": { "distanceCm": 30 }
    },
    {
      "timeMs": 2000,
      "type": "input.sensor",
      "componentId": "front_radar",
      "payload": { "distanceCm": 12 }
    },
    {
      "timeMs": 3000,
      "type": "fault.inject",
      "componentId": "front_radar",
      "payload": { "faultType": "timeout", "durationMs": 1000 }
    }
  ]
}
```

用途：

1. 复现用户 bug。
2. 生成回归测试。
3. 对比不同 DAL/PAL 实现。
4. 证明 AI 修改没有破坏原有行为。

---

## 9. 一致性等级

| 等级 | 含义 | 要求 |
|---|---|---|
| C0 | 未验证 | 只运行过代码 |
| C1 | 仿真通过 | Wasm trace 无错误 |
| C2 | 仿真异常测试通过 | 故障注入后进入安全状态 |
| C3 | 真机 trace 匹配 | 真机与仿真关键事件一致 |
| C4 | 回归测试稳定 | 多次运行 trace 差异在容忍范围内 |

产品可以在项目页面显示一致性徽章：

```text
Consistency: C2 Simulation Fault-Tested
Consistency: C3 Hardware Trace Matched
```

---

## 10. 与 CI/CD 集成

每个官方示例项目应包含：

```text
examples/obstacle-car/
├── project.json
├── app_main.c
├── traces/
│   ├── normal-run.golden.json
│   ├── obstacle-detected.golden.json
│   └── sensor-timeout.golden.json
└── tests/
    └── trace-compare.test.ts
```

CI 执行：

1. 编译 Wasm。
2. 使用 replay 输入运行仿真。
3. 导出 actual trace。
4. 与 golden trace 对比。
5. 失败时输出差异报告。

---

## 11. MVP 落地范围

第一阶段只要求记录：

1. `runtime.start/stop`
2. `bal.state_change`
3. `bal.fault`
4. `dal.read`
5. `dal.write`
6. `fault.inject`

暂不记录完整 PAL transfer 波形，只记录 I2C/OLED 等事务摘要。

