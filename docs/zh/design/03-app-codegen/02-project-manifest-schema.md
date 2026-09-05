# 13. Embedded Project Manifest 与 Registry Lock 规范

本文定义 Wink-AI 嵌入式项目的统一项目文件格式。Project Manifest 是前端画布、AI 生成、Wasm 仿真、设备树生成、云端编译、烧录和 Golden Trace 的共同契约，也是未来与 Wink-AI 主项目集成时的项目边界。

---

## 1. 设计目标

1. **单一项目入口**：一个 Manifest 描述嵌入式项目的目标板、器件、连线、业务逻辑、仿真配置、构建配置和验证状态。
2. **可复现**：通过 Registry Lock 锁定模型版本和 hash，保证历史项目可重新仿真、编译和对比 trace。
3. **可迁移**：通过 schemaVersion、projectVersion 和 migration 记录管理格式演进。
4. **可审计**：所有 AI 生成、代码生成、构建产物和安全门禁都有明确来源和 hash。
5. **主项目友好**：主项目只需保存 Manifest 和摘要 metadata，不需要理解嵌入式内部表结构。

---

## 2. 文件组成

推荐一个嵌入式项目目录包含：

```text
embedded-project/
├── wink-project.json              # Project Manifest
├── device-registry.lock.json      # Device Model / Board Model 锁文件
├── src/
│   ├── app.dsl.json               # 推荐：AI/低代码生成的 DSL
│   ├── app_main.c                 # 生成后的 App C，可查看可导出
│   ├── app_config.h
│   ├── device_tree.c
│   └── device_tree.h
├── traces/
│   ├── normal.golden.json
│   └── fault-timeout.golden.json
├── builds/
│   └── build-manifest.json
└── assets/
    └── thumbnail.png
```

`src/app_main.c`、`device_tree.c` 和 `device_tree.h` 是派生产物，可以重新生成。权威输入是 `wink-project.json`、`device-registry.lock.json` 和 `src/app.dsl.json`。

---

## 3. Project Manifest 顶层结构

```json
{
  "schemaVersion": 1,
  "projectVersion": "0.1.0",
  "projectType": "wink-embedded",
  "id": "proj_distance_alarm",
  "name": "Distance Alarm",
  "description": "ESP32 ultrasonic distance alarm demo",
  "createdAt": "2026-06-22T00:00:00Z",
  "updatedAt": "2026-06-22T00:00:00Z",
  "target": {
    "boardId": "esp32-devkit-v1",
    "boardVersion": "1.0.0",
    "palTarget": "targets/esp32",
    "toolchain": "esp-idf"
  },
  "devices": [],
  "connections": [],
  "logic": {},
  "simulation": {},
  "safety": {},
  "build": {},
  "trace": {},
  "metadata": {}
}
```

---

## 4. Target 定义

```json
{
  "target": {
    "boardId": "esp32-devkit-v1",
    "boardVersion": "1.0.0",
    "boardModelHash": "sha256:...",
    "palTarget": "targets/esp32",
    "runtimeVersion": "0.1.0",
    "toolchain": "esp-idf",
    "flashProtocol": "webserial-esptool"
  }
}
```

约束：

1. `boardModelHash` 必须来自 `device-registry.lock.json`。
2. `palTarget` 必须由 Board Model 声明，不允许前端任意拼接。
3. `flashProtocol` 决定 Build & Flash 向导可用能力。

---

## 5. Devices 定义

```json
{
  "devices": [
    {
      "componentId": "front_radar",
      "modelId": "hc-sr04",
      "modelVersion": "1.0.0",
      "modelHash": "sha256:...",
      "displayName": "Front Radar",
      "category": "sensor",
      "position": { "x": 320, "y": 180 },
      "rotation": 0,
      "properties": {
        "maxDistanceCm": 400,
        "sampleIntervalMs": 60
      },
      "codegen": {
        "instanceName": "front_radar",
        "dalType": "dal_ultrasonic_t",
        "role": "distance_sensor"
      }
    }
  ]
}
```

约束：

1. `componentId` 是项目内稳定 ID，用于 trace、fault、连接和 UI 状态。
2. `instanceName` 是 C identifier，必须经过 sanitization。
3. 用户重命名显示名不应改变 `componentId`，避免 trace 无法对齐。
4. `codegen.role` 用于在 C 生成层指定抽象角色（Role Interface）接口，可选（若缺省则自动 fallback 为驱动声明的默认角色）。

---

## 6. Connections 定义

```json
{
  "connections": [
    {
      "id": "conn_001",
      "from": {
        "componentId": "front_radar",
        "pin": "TRIG"
      },
      "to": {
        "componentId": "esp32-devkit-v1",
        "pin": "GPIO4"
      },
      "netId": "net_trig",
      "validation": {
        "status": "passed",
        "warnings": []
      }
    }
  ]
}
```

连接校验结果可以缓存，但每次打开项目、切换目标板或升级 Registry 后必须重新计算。

---

## 7. Logic 定义

推荐以 DSL 作为权威逻辑输入：

```json
{
  "logic": {
    "sourceType": "dsl",
    "dslPath": "src/app.dsl.json",
    "generatedCPath": "src/app_main.c",
    "codegenVersion": "0.1.0",
    "sourceHash": "sha256:...",
    "generatedHash": "sha256:...",
    "entrypoints": {
      "init": "app_init",
      "loop": "app_loop",
      "fault": "app_on_fault"
    },
    "generator": {
      "type": "ai",
      "model": "configured-by-host",
      "promptHash": "sha256:..."
    }
  }
}
```

允许专业开发者进入 `manual-c` 模式，但必须受到 App Safe Codegen 静态规则约束。

```json
{
  "logic": {
    "sourceType": "manual-c",
    "sourcePath": "src/app_main.c",
    "sourceHash": "sha256:..."
  }
}
```

---

## 8. Simulation 定义

```json
{
  "simulation": {
    "preferredRouting": "dal-value-bypass",
    "wasmTarget": "wasm32-emscripten",
    "asyncify": true,
    "workerLimits": {
      "initialMemoryMb": 16,
      "maxMemoryMb": 64,
      "heartbeatMs": 100,
      "loopTimeSliceMs": 20,
      "maxTraceEvents": 10000
    },
    "faultScenarios": [
      {
        "id": "sensor_timeout",
        "name": "Sensor Timeout",
        "events": [
          {
            "timeMs": 3000,
            "componentId": "front_radar",
            "type": "timeout",
            "durationMs": 1000
          }
        ]
      }
    ]
  }
}
```

---

## 9. Safety 定义

```json
{
  "safety": {
    "level": "S2",
    "staticCheck": {
      "status": "passed",
      "checkedAt": "2026-06-22T00:00:00Z",
      "diagnostics": [],
      "rulesetVersion": "0.1.0"
    },
    "simulationGate": {
      "normalRunPassed": true,
      "faultTestsPassed": true,
      "lastRunId": "run_001"
    },
    "flashAllowed": false
  }
}
```

`flashAllowed` 只能由安全门禁计算得出，不允许用户手动编辑。

---

## 10. Build 定义

```json
{
  "build": {
    "lastBuildId": "build_001",
    "status": "success",
    "buildProfile": "debug-trace",
    "artifact": {
      "name": "firmware.bin",
      "size": 1048576,
      "sha256": "sha256:..."
    },
    "manifestPath": "builds/build-manifest.json"
  }
}
```

构建结果只作为最近状态缓存，权威构建记录由 Build Manifest 保存。

---

## 11. Trace 定义

```json
{
  "trace": {
    "consistencyLevel": "C2",
    "lastSimulationRunId": "run_sim_001",
    "lastHardwareRunId": null,
    "goldenTraces": [
      {
        "id": "normal",
        "path": "traces/normal.golden.json",
        "hash": "sha256:..."
      }
    ]
  }
}
```

---

## 12. Registry Lock

`device-registry.lock.json` 用于锁定项目依赖的 Board、Peripheral、Bus、DAL API、Codegen Template 和 Simulation Model。

```json
{
  "lockVersion": 1,
  "registryVersion": "0.1.0",
  "registryHash": "sha256:...",
  "generatedAt": "2026-06-22T00:00:00Z",
  "models": [
    {
      "type": "board",
      "id": "esp32-devkit-v1",
      "version": "1.0.0",
      "hash": "sha256:..."
    },
    {
      "type": "peripheral",
      "id": "hc-sr04",
      "version": "1.0.0",
      "hash": "sha256:..."
    }
  ],
  "templates": [
    {
      "id": "device-tree/hc-sr04.c.mustache",
      "version": "1.0.0",
      "hash": "sha256:..."
    }
  ],
  "migrations": []
}
```

约束：

1. 打开项目时，Registry 当前 hash 与 lock 不一致必须提示用户。
2. 自动 migration 不得静默改变引脚、电压、DAL API 语义或默认安全策略。
3. 构建和 trace 必须记录使用的 lock hash。

---

## 13. 摘要 Metadata

供主项目项目列表、搜索和卡片展示使用：

```json
{
  "metadata": {
    "thumbnail": "assets/thumbnail.png",
    "tags": ["esp32", "sensor", "education"],
    "summary": {
      "deviceCount": 3,
      "connectionCount": 6,
      "target": "ESP32 DevKit V1",
      "safetyLevel": "S2",
      "consistencyLevel": "C2",
      "lastBuildStatus": "success"
    }
  }
}
```

主项目应优先读取 `metadata.summary`，避免解析完整嵌入式拓扑。

---

## 14. Manifest 校验规则

保存、仿真、编译前必须执行：

1. schemaVersion 支持检查。
2. projectType 必须为 `wink-embedded`。
3. target board 存在且 hash 匹配 lock。
4. 所有 device model 存在且 hash 匹配 lock。
5. 所有 connection 通过 Board/Peripheral 约束校验。
6. logic sourceHash 与实际文件一致。
7. generatedHash 与当前 codegen 输出一致。
8. safety level 不可高于实际门禁结果。
9. build artifact hash 必须匹配 Build Manifest。

---

## 15. 版本迁移策略

Manifest 迁移流程：

```text
load manifest
  ↓
check schemaVersion
  ↓
load registry lock
  ↓
run manifest migration if needed
  ↓
run registry migration if needed
  ↓
revalidate devices/connections/logic
  ↓
write migration report
```

迁移报告示例：

```json
{
  "fromSchemaVersion": 1,
  "toSchemaVersion": 2,
  "changes": [
    {
      "type": "addField",
      "path": "simulation.workerLimits.maxTraceEvents",
      "value": 10000
    }
  ],
  "requiresUserConfirmation": false
}
```

---

## 16. MVP 最小字段

MVP-0 只要求实现：

1. `schemaVersion`
2. `projectType`
3. `id/name`
4. `target`
5. `devices`
6. `connections`
7. `logic.sourceType`
8. `simulation.workerLimits`
9. `safety.level/staticCheck`
10. `trace.consistencyLevel`
11. `device-registry.lock.json`

Build、Flash、Hardware Trace 可以在后续阶段补齐。
