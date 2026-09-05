# 14. AI DSL、状态机 AST 与 App Safe Codegen 管线规范

本文定义 Wink-AI 嵌入式平台中 AI/低代码到 App C 代码的安全生成链路。平台不应让 AI 直接自由生成可烧录 C 代码，而应让 AI 先生成受约束的 DSL 或状态机 AST，再由确定性 codegen 输出 App C，从而提升安全性、可解释性、可回放性和可视化编辑能力。

> **DAL 驱动 / Role 描述 SSOT（ADR-0046 机制 + ADR-0051 路径）**：  
> - **目标态**：可扫描扩展根（默认 `wink-micro-os/codegen/drivers/*.yaml` + `roles/*.yaml`；可选 App / CMake cache `WINK_CODEGEN_PATHS`）为机读描述 SSOT；`wink-tools` 引擎只读扫描、校验、**沙箱化**渲染、`list_drivers` 发射 CMake。顺序：**内置 → OS → env → App**。  
> - **机制保留**：`list_drivers.py` 生成数据型 CMake；`app_codegen` 动态枚举 `WINK_USE_*`；禁止手改多处驱动表。  
> - **迁移期**：仍可读旧 `tools/codegen/drivers/*.py`，直至 tech-design 退出标准达成。  
> 决策：[ADR-0051](../../decisions/tools/0051-scannable-codegen-extension-roots.md)；设计：[scannable-codegen-extension-roots-design](../../tech-designs/tools/2026-07-28-scannable-codegen-extension-roots-design.md)。操作指南见 [`adding-peripheral.md`](../../../wink-micro-os/docs/dal-development-guide/adding-peripheral.md)；手册索引 [`dal-development-guide/`](../../../wink-micro-os/docs/implementation-plans/scripts/README.md)。

---

## 1. 核心原则

1. **AI 生成意图，不直接拥有最终 C 代码控制权**。
2. **DSL/AST 是权威业务逻辑输入**，App C 是可审计、可导出、可重新生成的派生产物。
3. **所有硬件访问必须通过 Device Model 声明的 DAL API**。
4. **所有返回 `wink_status_t` 的调用必须显式检查**。
5. **生成结果必须能映射回画布、状态机节点和用户自然语言需求**。

---

## 2. 总体管线

```text
Natural Language / Blockly / State Machine UI
        │
        ▼
Intent Parser / AI Tool
        │
        ▼
Embedded App DSL / State Machine AST
        │
        ▼
Schema Validation
        │
        ▼
Device Model Constraint Check
        │
        ▼
Safety Rule Check
        │
        ▼
Deterministic App C Codegen
        │
        ▼
Static C Check
        │
        ▼
Wasm Simulation
        │
        ▼
Build / Flash Gate
```

---

## 3. DSL 顶层结构

```json
{
  "dslVersion": 1,
  "kind": "wink-app-state-machine",
  "name": "DistanceAlarmLogic",
  "devices": {
    "front_radar": {
      "modelId": "hc-sr04",
      "dalType": "dal_ultrasonic_t"
    },
    "status_led": {
      "modelId": "led",
      "dalType": "dal_led_t"
    }
  },
  "constants": [],
  "stateMachine": {},
  "faultPolicy": {},
  "loop": {}
}
```

---

## 4. Constants 定义

```json
{
  "constants": [
    {
      "name": "OBSTACLE_THRESHOLD_CM",
      "type": "float",
      "value": 20.0,
      "min": 2.0,
      "max": 400.0,
      "description": "Distance threshold for obstacle detection"
    },
    {
      "name": "MAX_SENSOR_ERROR_COUNT",
      "type": "int",
      "value": 3,
      "min": 1,
      "max": 10
    }
  ]
}
```

约束：

1. 常量名称必须是合法 C identifier。
2. 数值必须符合 Device Model 和业务约束。
3. 用户可在属性面板中修改常量，修改后重新生成 App。

---

## 5. 状态机定义

```json
{
  "stateMachine": {
    "name": "SystemState",
    "initialState": "INIT",
    "states": [
      {
        "id": "INIT",
        "onEnter": [
          { "action": "setLed", "device": "status_led", "value": "off" },
          { "action": "transition", "to": "RUNNING" }
        ]
      },
      {
        "id": "RUNNING",
        "transitions": [
          {
            "when": {
              "op": "lt",
              "left": { "var": "front_distance_cm" },
              "right": { "const": "OBSTACLE_THRESHOLD_CM" }
            },
            "to": "ALARM",
            "actions": [
              { "action": "setLed", "device": "status_led", "value": "on" }
            ]
          }
        ]
      },
      {
        "id": "ALARM",
        "transitions": [
          {
            "when": {
              "op": "gte",
              "left": { "var": "front_distance_cm" },
              "right": { "const": "OBSTACLE_THRESHOLD_CM" }
            },
            "to": "RUNNING",
            "actions": [
              { "action": "setLed", "device": "status_led", "value": "off" }
            ]
          }
        ]
      }
    ]
  }
}
```

约束：

1. 状态数量 MVP 建议不超过 16。
2. 每个 transition 必须有确定条件。
3. 禁止递归状态动作。
4. `onEnter` 和 transition actions 必须有限且可静态展开。
5. Codegen 必须将状态机翻译为扁平 `state_var + switch-case` 循环控制模式，禁止将 transition 翻译为函数直接重入（如 `state_A() { ...; state_B(); }`）。（P-stack 栈安全）
6. 单个 action chain 最大深度 ≤ 4 层。DSL validator 必须拒绝超出的结构。（P-stack 栈安全）
7. 表达式 AST 最大嵌套深度 ≤ 8 层，防止 codegen 生成过深的 if-else 嵌套。（P-stack 栈安全）

---

## 6. Loop 定义

```json
{
  "loop": {
    "periodMs": 50,
    "steps": [
      {
        "id": "read_front_distance",
        "action": "dalRead",
        "device": "front_radar",
        "api": "dal_ultrasonic_read",
        "outputs": {
          "distance_cm": "front_distance_cm"
        },
        "onError": {
          "increment": "front_radar_error_count",
          "ifGte": {
            "left": "front_radar_error_count",
            "right": "MAX_SENSOR_ERROR_COUNT",
            "then": [
              {
                "action": "fault",
                "code": "FAULT_FRONT_RADAR_UNAVAILABLE"
              }
            ]
          },
          "returnLoop": true
        }
      },
      {
        "id": "run_state_machine",
        "action": "evaluateStateMachine"
      },
      {
        "id": "delay_tick",
        "action": "delay",
        "ms": { "const": "APP_TICK_RATE_MS" }
      }
    ]
  }
}
```

`delay` 在 App 层不应直接暴露为 `pal_delay_ms`。推荐由 WinkMicroOS 调度器控制 loop period；若 MVP 仍使用 delay，应通过 `wink_app_delay_ms()` 包装，避免 App 直接 include PAL。

---

## 7. Fault Policy 定义

```json
{
  "faultPolicy": {
    "faultCodes": [
      {
        "name": "FAULT_FRONT_RADAR_UNAVAILABLE",
        "value": 1001,
        "severity": "error"
      }
    ],
    "onFault": [
      {
        "action": "setLed",
        "device": "status_led",
        "value": "flash"
      },
      {
        "action": "traceFault",
        "code": { "arg": "fault_code" }
      }
    ]
  }
}
```

约束：

1. `onFault` 只允许执行 fail-safe 动作和 trace，不允许复杂业务状态迁移。
2. 执行器 fail-safe 姿态应优先来自 Device Model。
3. AI 生成 DSL 时必须为传感器错误提供 fault path。

---

## 8. Action 白名单

MVP 支持以下动作：

| Action | 说明 | Codegen 目标 |
|---|---|---|
| `dalRead` | 调用传感器 DAL API | `wink_status_t status = dal_xxx_read(...)` |
| `dalWrite` | 调用执行器 DAL API | `wink_status_t status = dal_xxx_set(...)` |
| `setLed` | LED 语义动作 | `dal_led_set_state(...)` |
| `setServoAngle` | 舵机语义动作 | `dal_rc_servo_set_angle(...)` |
| `displayText` | OLED 显示文本 | `dal_oled_draw_text(...)` |
| `transition` | 状态切换 | `current_state = ...` |
| `fault` | 触发 fault | `app_on_fault(...)` |
| `traceFault` | 记录故障 | `wink_trace_fault(...)` |
| `evaluateStateMachine` | 执行状态机 | switch dispatch |
| `delay` | 周期延时 | `wink_app_delay_ms(...)` 或调度器 |

任何不在白名单内的动作必须拒绝。

---

## 9. 表达式子集

条件表达式使用结构化 JSON，不允许直接注入 C 表达式。

```json
{
  "op": "and",
  "args": [
    {
      "op": "gt",
      "left": { "var": "front_distance_cm" },
      "right": { "literal": 0.0 }
    },
    {
      "op": "lt",
      "left": { "var": "front_distance_cm" },
      "right": { "const": "OBSTACLE_THRESHOLD_CM" }
    }
  ]
}
```

允许操作符：

```text
eq, neq, lt, lte, gt, gte, and, or, not, add, sub, mul, div, clamp
```

约束：

1. 禁止任意 C 代码片段。
2. 除法必须检查除数不为 0。
3. 浮点比较生成代码时可按规则加入容差。

---

## 10. Codegen 输出规范

DSL 生成 App C 时必须输出：

```text
src/
├── app_config.h
├── app_main.c
└── app_generated_meta.json
```

`app_generated_meta.json` 记录：

```json
{
  "dslHash": "sha256:...",
  "codegenVersion": "0.1.0",
  "generatedFiles": [
    {
      "path": "src/app_main.c",
      "hash": "sha256:..."
    }
  ],
  "sourceMap": [
    {
      "dslNodeId": "read_front_distance",
      "file": "src/app_main.c",
      "startLine": 42,
      "endLine": 58
    }
  ]
}
```

sourceMap 用于：

1. 静态检查错误回定位到状态机节点。
2. AI 修复时只修改 DSL 节点。
3. 用户点击 C 代码时高亮对应画布块。

---

### 10.1 MCS-51 拦截层板级 codegen（`mcs51_board_config.h`）

对 8051/Keil C51 **零侵入仿真**（[ADR-0070](../decisions/core/0070-mcs51-zero-code-simulation-interception-layer.md)，见 `02-wink-micro-os/07-mcs51-simulation-interception.md`），不生成 App C，而是生成**固件期静态板描述头**：

- 生成器 `wink-tools/tools/codegen/generators/mcs51_board_config.py`（模板 `templates/mcs51_board_config.h.j2`，板图 `boards/mcs51_devboard.json`）；输入 `wink-app.json`（`board` + `devices`），输出 `mcs51_board_config.h`。
- 引脚引用经板 headers 解析：`"$board.headers.P2.0"` → 线性 index = `port*8 + bit`（P0.0…P3.7 → 0…31；SFR 口地址 `0x80 + port*0x10`）。
- **只**固化固件静态常量：ADC0832 的 CS/CLK/DI/DO port+bit、通道、VREF；`thermal_heater_plate` 的 drive port+bit、NTC 通道、设定点（`SETPOINT_C_X100` 定点）。热动力学参数（tau/watts/beta/R25）属运行期 device-tree.properties，**不**编入固件（spike-S3 C4）。
- 消费缝：`frameworks/mcs51/src/mcs51_bridge.cpp` 编译期 `#if __has_include("mcs51_board_config.h")` → 定义 `MCS51_HAS_ADC0832` 并在 framework init 以头文件常量调 `mcs51_adc0832_init(...)` 自动绑定，零运行期 JSON。头文件目录须在编译 bridge.cpp 的 **`wink_mcs51_compat` 库** include 路径上（CMake 以生成器 `EXISTS` 夹具门控，缺失则跳过 iron_ntc 测试）。
- 闭环样例 `iron_ntc`（NTC 温控 + 开路/短路安全态）验证该缝：e2e 驱动不调 `mcs51_adc0832_init`，仅经 post-init hook 注入码值。

---

## 11. 生成 C 代码示例

```c
#include "device_tree.h"
#include "app_config.h"
#include "wink_app.h"
#include "wink_trace.h"

static system_state_t current_state = SYSTEM_STATE_INIT;
static uint8_t front_radar_error_count = 0;

void app_init(void) {
    wink_status_t led_status = dal_led_set_state(&status_led, LED_STATE_OFF);
    if (led_status != WINK_OK) {
        app_on_fault(FAULT_STATUS_LED_UNAVAILABLE);
        return;
    }
    current_state = SYSTEM_STATE_RUNNING;
}

void app_loop(void) {
    float front_distance_cm = 0.0f;
    wink_status_t radar_status = dal_ultrasonic_read(&front_radar, &front_distance_cm);
    if (radar_status != WINK_OK) {
        front_radar_error_count++;
        if (front_radar_error_count >= MAX_SENSOR_ERROR_COUNT) {
            app_on_fault(FAULT_FRONT_RADAR_UNAVAILABLE);
        }
        wink_app_delay_ms(APP_TICK_RATE_MS);
        return;
    }

    front_radar_error_count = 0;

    switch (current_state) {
        case SYSTEM_STATE_RUNNING:
            if (front_distance_cm > 0.0f && front_distance_cm < OBSTACLE_THRESHOLD_CM) {
                current_state = SYSTEM_STATE_ALARM;
                wink_trace_state_change(SYSTEM_STATE_RUNNING, SYSTEM_STATE_ALARM);
                wink_status_t led_status = dal_led_set_state(&status_led, LED_STATE_ON);
                if (led_status != WINK_OK) {
                    app_on_fault(FAULT_STATUS_LED_UNAVAILABLE);
                    return;
                }
            }
            break;
        default:
            break;
    }

    wink_app_delay_ms(APP_TICK_RATE_MS);
}
```

---

## 12. 静态检查规则

DSL 层检查：

1. schema 校验。
2. device 引用存在。
3. DAL API 存在且参数匹配。
4. 状态机可达性检查。
5. fault path 存在。
6. loop steps 有限。
7. 表达式无未定义变量。
8. 采样周期不小于 Device Model 最小间隔。

C 层检查：

1. 禁止 include PAL。
2. 禁止 malloc/free。
3. 禁止递归（自动化执行：`.clang-tidy` `misc-no-recursion` 门禁）。
4. 禁止用户 while(1)。
5. `wink_status_t` 返回值必须检查。
6. app_loop 必须有限时间返回。
7. 编译使用 `-Wstack-usage=1536 -Werror=stack-usage`，单个函数栈帧超过 1536 字节的代码禁止部署。（P-stack 栈安全）
8. 禁止调用标准库 `alloca`、VLA（变长数组）。Codegen 禁止产生 VLA 语法（`int arr[n];`），必须使用固定大小数组。自动化执行：`.clang-tidy` `clang-analyzer-security.insecureAPI.alloca` 门禁。（P-stack 栈安全）

---

## 13. AI 修复流程

```text
diagnostic
  ↓
map to source DSL node
  ↓
AI proposes DSL patch
  ↓
schema validate patch
  ↓
constraint check
  ↓
regenerate C
  ↓
static check
  ↓
simulation replay
```

AI 修复输出格式：

```json
{
  "patchType": "dsl-json-patch",
  "reason": "front_radar timeout was not handled",
  "patch": [
    {
      "op": "add",
      "path": "/loop/steps/0/onError",
      "value": {
        "action": "fault",
        "code": "FAULT_FRONT_RADAR_UNAVAILABLE"
      }
    }
  ]
}
```

AI 不应直接返回自由文本 C 补丁作为默认修复路径。

---

## 14. 与 Blockly/可视化编辑的关系

DSL 应同时服务 AI 和低代码编辑器：

```text
Blockly Block / State Node
        ⇄
App DSL Node
        ⇄
Generated C SourceMap
```

这样用户可以：

1. 用 AI 生成初稿。
2. 在状态机画布中查看和修改。
3. 查看生成 C 代码。
4. 错误时定位到具体节点。
5. 重新生成且不丢失结构化语义。

---

## 15. MVP 落地范围

MVP-0 支持：

1. `dalRead`
2. `setLed`
3. `transition`
4. `fault`
5. 两到三个状态。
6. Button LED / Distance Alarm 示例。
7. DSL -> C codegen。
8. C sourceMap。
9. `wink_status_t` 检查。

MVP-1 再加入 Servo、OLED、复杂表达式和 AI 自动修复。

