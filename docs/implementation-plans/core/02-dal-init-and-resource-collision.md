# Phase 2: DAL Init 补全与资源冲突校验框架

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.
>
> **核验状态（2026-06-24）：** 已对照 `dal_servo.h/.c`、`dal_ultrasonic.h/.c`、`wink_status.h` 确认。
>
> **执行序位置（见 00-README）：** `0/1 → 3 → 2 → 4 → 5 → 6`。**硬前置：Phase 3**——本阶段资源冲突治理依赖 Phase 3 已完成的 `pal_gpio_init`/`pal_pwm_init` status 签名（`bool` 无法表达 `WINK_ERR_BUSY`）。

**Goal:** 补齐 P0-3：DAL 显式 `init`、幂等生命周期、servo 一次性 PWM init、ultrasonic GPIO 方向配置，以及 host/debug target 的多维资源冲突检测。

**Architecture:**
- DAL 器件保持 POD + 命名 API（ADR-0004）。新增 `dal_xxx_init` 一次性配置硬件并置 `initialized`；运行态 API 只执行业务动作，前置 `initialized` 检查。
- 资源冲突用**静态表**（零动态分配）在 host/debug target 先闭环；真机（esp32）的等价治理随 P2-6 ROADMAP 推进。
- 资源占用经 Phase 3 的 status 签名返回 `WINK_ERR_BUSY`（已被占用）/ `WINK_ERR_RESOURCE_EXHAUSTED`（表满）。

**Tech Stack:** C99, CMake, Unity, host target

## Global Constraints
- 零动态分配：资源表为静态数组
- 不引入 vtable / ops / container_of（ADR-0004）
- 新增失败 API 返回 `wink_status_t`；判定用 `wink_status_is_error`
- 资源已占用 → `WINK_ERR_BUSY`；资源表满 → `WINK_ERR_RESOURCE_EXHAUSTED`（均已在 `wink_status.h` 存在：-6/-10 ✅）

## Sequencing
- **前置：Phase 3**（status 签名）必须先完成
- 后续：Phase 4（ultrasonic 非阻塞）需本阶段的 `dal_ultrasonic_init` + `initialized` 字段；Phase 5（servo safe_off）需 `dal_servo_init`
- Task 内部：2-3（资源表基础设施）可最先做；2-1/2-2（servo/ultrasonic init）依赖 2-3 的 guard；2-4（device_tree 收敛）最后

---

### Task 2-1: `dal_servo_init` 与 servo 一次性 PWM init

**Files:**
- Modify: `wink-micro-os/dal/include/dal_servo.h`
- Modify: `wink-micro-os/dal/src/dal_servo.c`
- Modify: `wink-micro-os/test/test_dal_servo.c`
- Modify: `wink-micro-os/samples/avoidance_car/app_main.c`
- Modify: `wink-micro-os/samples/avoidance_car/device_tree.c`

**Source-of-truth check:** 当前 `dal_servo_t`（`dal_servo.h:11-16`）含 `pwm_channel/current_angle/min_pulse_ms/max_pulse_ms`，**无 `initialized`**。`dal_servo.c:17` 每次 `set_angle` 调 `pal_pwm_init`（Phase 0 已补 `{}`，Phase 3 已 status 化）。

**Interfaces:**
```c
#include <stdbool.h>

typedef struct {
    uint8_t pwm_channel;
    float min_pulse_ms;
    float max_pulse_ms;
} dal_servo_config_t;

typedef struct {
    uint8_t pwm_channel;
    float current_angle;
    float min_pulse_ms;
    float max_pulse_ms;
    bool initialized;
} dal_servo_t;

WINK_WARN_UNUSED_RESULT
wink_status_t dal_servo_init(dal_servo_t *dev, const dal_servo_config_t *cfg);
```

> ⚠️ **config vs device 字段冗余说明**：`min_pulse_ms/max_pulse_ms` 在 `config_t`（输入）与 `dal_servo_t`（解析后状态）中重复。这是有意的：config 是构造期输入，device 持有运行期解析值。未来若引入 270° 舵机，`max_angle` 应同样作为 config 传入而非硬编码（呼应 Phase 0 Task 0-4 把角度常量留在 `.c` 的决策）。

**Implementation rules:**
- `dal_servo_init(NULL, ...)` → `WINK_ERR_INVALID_ARG`
- `min_pulse_ms <= 0.0f` 或 `max_pulse_ms <= min_pulse_ms` → `WINK_ERR_INVALID_ARG`
- `pal_pwm_init` 失败（Phase 3 status）→ 透传精确错误（`WINK_ERR_IO`/`WINK_ERR_INVALID_ARG`/`WINK_ERR_BUSY`/`WINK_ERR_RESOURCE_EXHAUSTED`）
- 成功置 `dev->initialized = true`
- `dal_servo_set_angle` 在 `!initialized` 时 → `WINK_ERR_NOT_INITIALIZED`
- `dal_servo_set_angle` **不再**调 `pal_pwm_init`（只 `pal_pwm_set_duty`）
- **继承 Phase 0**：`set_angle` 重写时保留 `SERVO_*` 派生常量与 `{}` 风格

**Tests:**
```c
void test_init_null_returns_invalid_arg(void);
void test_init_rejects_invalid_pulse_range(void);
void test_set_angle_before_init_returns_not_initialized(void);
void test_init_then_set_angle_updates_duty(void);
```

---

### Task 2-2: `dal_ultrasonic_init` 与 GPIO 方向配置

**Files:**
- Modify: `wink-micro-os/dal/include/dal_ultrasonic.h`
- Modify: `wink-micro-os/dal/src/dal_ultrasonic.c`
- Modify: `wink-micro-os/test/test_dal_ultrasonic.c`
- Modify: `wink-micro-os/test/test_dal_ultrasonic_sim.c`
- Modify: `wink-micro-os/samples/avoidance_car/app_main.c`
- Modify: `wink-micro-os/samples/avoidance_car/device_tree.c`

**Source-of-truth check:** 当前 `dal_ultrasonic_t`（`dal_ultrasonic.h:11-15`）含 `trig_pin/echo_pin/last_distance`，**无 `initialized`**，真机分支直接 `pal_gpio_write/read` 而**未先 `pal_gpio_init` 配方向**（`dal_ultrasonic.c:41-43`）——正是 P0-3 隐患。

**Interfaces:**
- Produces: `wink_status_t dal_ultrasonic_init(dal_ultrasonic_t *dev, uint16_t trig_pin, uint16_t echo_pin)`、`dal_ultrasonic_t.initialized`

**Implementation rules:**
- `dal_ultrasonic_init(NULL, ...)` → `WINK_ERR_INVALID_ARG`
- `trig_pin == echo_pin` → `WINK_ERR_INVALID_ARG`
- 真机分支：`pal_gpio_init(trig_pin, PAL_GPIO_OUTPUT_PUSH_PULL)` + `pal_gpio_init(echo_pin, PAL_GPIO_INPUT)`（Phase 3 status）→ 失败透传
- 仿真分支：跳过物理 GPIO 配置，但须初始化结构状态（`initialized=true`）
- `dal_ultrasonic_read` 在 `!initialized` → `WINK_ERR_NOT_INITIALIZED`

**Tests:**
```c
void test_ultrasonic_init_null_returns_invalid_arg(void);
void test_ultrasonic_init_rejects_same_pin(void);
void test_ultrasonic_read_before_init_returns_not_initialized(void);
void test_ultrasonic_init_then_read_real_measure_pulse(void);
```

---

### Task 2-3: host resource conflict guard

**Files:**
- Create: `wink-micro-os/pal/include/pal_resource.h`
- Create: `wink-micro-os/targets/host/pal_resource_host.c`
- Modify: `wink-micro-os/targets/host/CMakeLists.txt`
- Modify: `wink-micro-os/targets/host/pal_hal_host.c`
- Create: `wink-micro-os/test/test_pal_resource.c`
- Modify: `wink-micro-os/test/CMakeLists.txt`

**Interfaces:**
```c
typedef enum {
    PAL_RESOURCE_GPIO_PIN = 1,
    PAL_RESOURCE_PWM_CHANNEL = 2,
    PAL_RESOURCE_I2C_PORT = 3,
} pal_resource_type_t;

WINK_WARN_UNUSED_RESULT
wink_status_t pal_resource_claim(pal_resource_type_t type, uint32_t id, const char *owner);

void pal_resource_reset(void);
```

**Implementation rules:**
- 静态表容量 `PAL_RESOURCE_MAX_CLAIMS`，默认 `32`
- 同 `(type, id)` 同 owner → `WINK_OK`（幂等）
- 同 `(type, id)` 不同 owner → `WINK_ERR_BUSY`
- 表满 → `WINK_ERR_RESOURCE_EXHAUSTED`
- `pal_gpio_init`（host）claim `PAL_RESOURCE_GPIO_PIN`；`pal_pwm_init`（host）claim `PAL_RESOURCE_PWM_CHANNEL`

> ⚠️ **`const char *owner` 生命周期契约**：静态表**持有指针**（不拷贝字符串）。`owner` 必须指向生命周期 ≥ 资源占用期的静态存储——实践中仅接受**字符串字面量**或 device_tree 中的静态名。文档须明示此约束，否则 caller 传栈上/临时字符串会产生悬垂指针。若需放宽，应在 claim 时 `strncpy` 到表内固定缓冲（增加每项体积，权衡）。

> ⚠️ **host-only 边界**：本 guard 仅 host/debug target。esp32 等真机的等价资源治理随 P2-6 ROADMAP 推进——`pal_resource_*` 在真机 target 可暂为空实现或编译期排除，但不得假装已治理。

**Tests:**
```c
void test_resource_claim_same_owner_idempotent(void);
void test_resource_claim_conflict_returns_busy(void);
void test_resource_claim_table_full_returns_exhausted(void);
```

---

### Task 2-4: device_tree 初始化路径收敛

**Files:**
- Modify: `wink-micro-os/samples/avoidance_car/device_tree.c`
- Modify: `wink-micro-os/samples/avoidance_car/app_main.c`
- Modify: `wink-micro-os/test/test_app_e2e.c`

**Interfaces:**
- sample app `init` 阶段必须调用 `dal_servo_init` 与 `dal_ultrasonic_init`
- 任一 init 失败必须 `wink_trace_fault` 或进入安全降级路径，**禁止 `(void)status` 吞错**（呼应 review P2-2）

**Verification Gate:**
```powershell
cd wink-micro-os
python wink-tools/wink.py test --clean
```
→ `100% tests passed`，含新增 `test_pal_resource`。

## 出口验收
- [ ] `python wink-tools/wink.py test --clean` 全绿（含资源冲突三测试）
- [ ] servo/ultrasonic `set_angle`/`read` 在未 init 时返回 `WINK_ERR_NOT_INITIALIZED`
- [ ] `owner` 生命周期契约已写入 `pal_resource.h` 注释
- [ ] 整改跟踪表 P0-3 标"host 完成；esp32 随 P2-6"
- [ ] 完成后方可启动 Phase 4/5
