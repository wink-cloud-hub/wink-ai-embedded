# 07. 错误模型、故障注入与安全降级规范

嵌入式系统的专业性不只体现在正常路径能运行，更体现在异常路径可识别、可恢复、可测试、可追踪。Wink-AI 平台需要将错误模型作为 App、BAL、DAL、PAL、仿真器和真机部署的统一契约，避免使用隐式返回值、魔法数字或不可控异常。

---

## 1. 设计目标

1. **统一错误语义**：所有 DAL/PAL API 使用标准状态码表达错误。
2. **故障可注入**：仿真环境可主动制造断线、超时、噪声、越界等异常。
3. **安全优先**：发生危险状态时优先进入安全姿态，而不是继续执行不可信控制逻辑。
4. **AI 生成可约束**：AI 生成 App 时必须显式检查错误码。
5. **虚实一致可验证**：仿真错误、真机错误和 Golden Trace 使用同一套状态字段。

---

## 2. 统一状态码

```c
#ifndef WINK_STATUS_H
#define WINK_STATUS_H

#include <stdint.h>

/* 阻塞 API 强隔离标记（ADR-0017） */
#define WINK_BLOCKING \
    __attribute__((deprecated("Blocking API forbidden in cooperative runtime; use non-blocking variant")))

typedef enum {
    WINK_OK = 0,

    /* 通用可恢复错误（负数，对齐 Linux/POSIX 惯例） */
    WINK_ERR_INVALID_ARG        = -1,
    WINK_ERR_TIMEOUT            = -2,
    WINK_ERR_DISCONNECTED       = -3,
    WINK_ERR_OUT_OF_RANGE       = -4,
    WINK_ERR_IO                 = -5,
    WINK_ERR_BUSY               = -6,
    WINK_ERR_UNSUPPORTED        = -7,
    WINK_ERR_CHECKSUM           = -8,
    WINK_ERR_PERMISSION         = -9,
    WINK_ERR_RESOURCE_EXHAUSTED = -10,
    WINK_ERR_NOT_INITIALIZED    = -11,
    WINK_ERR_HARDWARE           = -12,   /* 硬件/驱动返回非 OK（如 ESP-IDF esp_err_t） */
    WINK_ERR_NO_MEM             = -13,   /* 内存分配失败 / 内存不足 */
    WINK_ERR_EMPTY              = -14,   /* 容器 / 队列空（正常 Poll 轮询返回） */
    WINK_ERR_FULL               = -15,   /* 容器 / 队列已满 */
    WINK_ERR_INVALID_STATE      = -16,   /* 状态机迁移非法 */
    WINK_ERR_LOCKED             = -17,   /* 资源锁定（启动安全锁 / 闪存锁） */
    WINK_ERR_NOT_FOUND          = -18,   /* 查找目标不在 Registry 中 */
    WINK_ERR_CANCELED           = -19,   /* 并发良性取消 */

    /* 功能安全相关（区分可恢复 / 致命） */
    WINK_ERR_OVERCURRENT        = -20,   /* 过流（可恢复：限流重试） */
    WINK_ERR_OVERTEMPERATURE    = -21,   /* 过温（可恢复：降频） */
    WINK_ERR_ALREADY_INITIALIZED = -22,  /* 重复 init（调用序 bug，不隐式 deinit） */
    WINK_ERR_WATCHDOG           = -30,   /* 看门狗超时（致命：复位） */
    WINK_ERR_OVERFLOW           = -40,   /* 数值溢出 / 计算 UB（致命） */

    /* 可恢复降级（ADR-0005）：未完全成功，但系统已安全降级、应继续运行（非 halt） */
    WINK_ERR_CONFIG_CORRUPT_DEGRADED = -50,   /* NVS/配置损坏 → 用安全默认值继续 */
    WINK_ERR_FAILED_INIT             = -51,   /* 器件 init 失败 → 器件隔离，系统继续 */
    WINK_ERR_PANIC              = -99,   /* 不可恢复，需 halt */
} wink_status_t;

#endif
```

> ✅ **已采纳 —— [ADR-0001](../../decisions/core/0001-error-code-sign-convention.md)（状态：Accepted，2026-06-23 拍板）**
>
> 错误码符号约定已由**正数**改为**负数**（对齐同工作区 `CLAUDE.md`「返回 `int`：0=成功、负数=错误」与 Linux/POSIX `-EINVAL` 惯例），并补齐功能安全码。要点：
> 1. `if (status)` 现在语义正确（0=成功=假，负数=错误=真）；PAL 底层 errno 可直接透传，移除 DAL 符号翻译层。
> 2. **码段分区**：`-1..-11` 通用可恢复 / `-20s` 安全可恢复 / `-50s` 可恢复降级（ADR-0005）/ `-30s·-40s·-99` 致命，供 fail-safe 分类（见 §7）。
> 3. 原 `WINK_ERR_INTERNAL(255)` 废弃，其「不可恢复内部错误」语义由 `WINK_ERR_PANIC(-99)` 承载。
>
> 历史正数值（`1..11, 255`）于 2026-06-23 废弃；新代码必须使用负数语义。既有依赖正数值的代码需按 ADR-0001 Consequences 迁移。

> 🔁 **扩展 —— [ADR-0005](../../decisions/core/0005-degraded-status-segment.md)（Accepted，2026-06-23）**：新增 `-50s`「可恢复降级」段（`WINK_ERR_CONFIG_CORRUPT_DEGRADED = -50`、`WINK_ERR_FAILED_INIT = -51`）。**保持负数**，`if (status < 0)` 对降级状态依然正确捕获；BAL 按码值特判走保守降级。**无正数 warning 段**，统一 `ERR_*` 前缀、废止 `WARN_*`。

> ➕ **补充 —— `WINK_ERR_HARDWARE = -12`（2026-06-26）**：通用可恢复段延伸至 `-12`，承载「底层硬件/驱动返回非 OK」语义（如 ESP-IDF `esp_err_t != ESP_OK` 的统一映射，取代原先散落的 `WINK_ERR_IO` 折叠）。属 `-1..-12` 通用可恢复段，DAL 透传后 BAL 按通用重试/限幅策略处理。**不引入 `WINK_ERR_UNKNOWN` 泛码**——本规范 §2.5 要求精确表达错误原因，未归类驱动错误统一归 `WINK_ERR_HARDWARE`。

状态码约束：

1. `WINK_OK` 必须为 0，便于 C 语言条件判断。
2. DAL/PAL 失败型 API 不使用 `-1.0f`、`NULL`、`false` 等隐式错误语义承载复杂故障。**PAL HAL/OSAL 失败型 API（`pal_gpio_init` / `pal_gpio_enable_interrupt` / `pal_gpio_disable_interrupt` / `pal_pwm_init` / `pal_pwm_set_duty` / `pal_i2c_transfer` / `pal_mutex_lock` / `pal_mutex_unlock`）已由 `bool` 统一迁移为 `wink_status_t`（ADR-0001 / review Phase 3，2026-06-25）**：DAL 透传精确 PAL 错误、不再折叠成 `WINK_ERR_IO`；读取型 `pal_gpio_write(void)` / `pal_gpio_read(bool)` 无失败语义，保持现状。
3. 传感器读数通过输出参数返回。
4. 执行器控制通过状态码表达是否成功写入目标值。
5. `WINK_ERR_PANIC` 仅用于不可恢复、需 halt 的内部错误，不能滥用；可恢复的内部错误应使用对应的 `WINK_ERR_*` 精确表达原因。
6. **WDT / 复位与 fail-safe（Phase 5）**：`WINK_ERR_WATCHDOG`(-30) 为致命复位码。复位后 `pal_get_reset_reason()` 返回 `PAL_RESET_REASON_WATCHDOG`/`PANIC` 时，runtime 经 boot safe-lock（`wink_actuator_safe_off_all` + `WINK_FAULT_BOOT_AFTER_RESET` trace）先关断执行器再 `init`。硬件 WDT 是真挂死/CPU 卡死的最后兜底；软件 fault 路径（`wink_runtime_fault`）不替代 WDT。详见 [04-runtime-and-trace.md](../02-wink-micro-os/04-runtime-and-trace.md) §3。

---

## 3. DAL API 错误返回规范

推荐模式：

```c
wink_status_t dal_ultrasonic_read(dal_ultrasonic_t *dev, float *distance_cm);
wink_status_t dal_rc_servo_set_angle(dal_rc_servo_t *dev, float angle_deg);
wink_status_t dal_oled_draw_buffer(dal_oled_t *dev, const uint8_t *buffer, uint32_t len);
```

不推荐模式：

```c
float dal_ultrasonic_get_distance(dal_ultrasonic_t *dev);
bool dal_rc_servo_set_angle(dal_rc_servo_t *dev, float angle_deg);
```

原因：

| 不推荐方式 | 问题 |
|---|---|
| `float -1.0f` | 无法区分断线、超时、越界、未初始化 |
| `bool false` | 无法表达失败原因 |
| `NULL` | 只适合指针，不适合传感器数值 |

---

## 4. 标准故障类型

| 故障 | 状态码 | 仿真来源 | 真机来源 | 默认策略 |
|---|---|---|---|---|
| 参数非法 | `WINK_ERR_INVALID_ARG` | AI 生成错误 | 调用传参错误 | 阻止继续执行 |
| 超时 | `WINK_ERR_TIMEOUT` | 延迟响应注入 | 总线/传感器无响应 | 使用上次有效值或进入保护 |
| 断线 | `WINK_ERR_DISCONNECTED` | 删除连线/断开引脚 | GPIO/I2C 检测失败 | 进入安全状态 |
| 越界 | `WINK_ERR_OUT_OF_RANGE` | 属性滑块超范围 | 传感器读数异常 | 限幅或报警 |
| 忙碌 | `WINK_ERR_BUSY` | 总线占用 | I2C/SPI/UART 未释放 | 延迟重试 |
| 不支持 | `WINK_ERR_UNSUPPORTED` | 目标平台缺能力 | PAL target 缺实现 | 编译前阻止 |
| 校验失败 | `WINK_ERR_CHECKSUM` | 虚拟包损坏 | 总线 CRC 错误 | 重试或报警 |
| 资源不足 | `WINK_ERR_RESOURCE_EXHAUSTED` | PWM 通道不足 | 内存/句柄不足 | 阻止部署 |

---

## 5. BAL 错误处理约束

AI 或低代码生成的 App 必须遵守：

1. 调用返回 `wink_status_t` 的 DAL API 后必须检查状态码。
2. 传感器读取失败不得使用未初始化输出值。
3. 执行器控制失败必须进入可观测错误路径。
4. 连续 N 次失败必须触发 `app_on_fault()`。
5. `app_on_fault()` 不得执行复杂业务逻辑，只执行安全动作和状态记录。

示例：

```c
void app_loop(void) {
    float distance_cm = 0.0f;
    wink_status_t status = dal_ultrasonic_read(&front_radar, &distance_cm);

    if (status != WINK_OK) {
        sensor_error_count++;
        if (sensor_error_count >= MAX_SENSOR_ERROR_COUNT) {
            app_on_fault(FAULT_FRONT_RADAR_UNAVAILABLE);
        }
        wink_app_delay_ms(APP_TICK_RATE_MS);
        return;
    }

    sensor_error_count = 0;

    if (distance_cm > 0.0f && distance_cm < OBSTACLE_THRESHOLD_CM) {
        wink_status_t servo_status = dal_rc_servo_set_angle(&neck_servo, 180.0f);
        if (servo_status != WINK_OK) {
            app_on_fault(FAULT_SERVO_CONTROL_FAILED);
            return;
        }
    }

    wink_app_delay_ms(APP_TICK_RATE_MS);
}
```

---

## 6. 故障注入模型

仿真器必须支持按器件实例注入故障：

```json
{
  "componentId": "front_radar",
  "faults": [
    {
      "type": "timeout",
      "enabled": true,
      "startAtMs": 5000,
      "durationMs": 3000
    },
    {
      "type": "noise",
      "enabled": true,
      "stddev": 1.2
    },
    {
      "type": "disconnect",
      "enabled": false
    }
  ]
}
```

故障注入入口：

| 入口 | 使用场景 |
|---|---|
| 属性面板 | 用户手动测试异常 |
| 自动测试用例 | CI 验证 App 安全逻辑 |
| AI 测试生成 | AI 根据需求生成异常场景 |
| Golden Trace | 复现实验输入序列 |

---

## 7. 安全降级策略

每个执行器应定义 fail-safe 姿态。降级动作按错误码码段分类（码段定义见 §2，源自 [ADR-0001](../../decisions/core/0001-error-code-sign-convention.md) 方案 C）：

| 码段 | 类别 | 降级原则 |
|---|---|---|
| `WINK_OK(0)` | 正常 | 继续执行 |
| `-1..-11` 通用 | 可恢复 | 重试 / 使用上次有效值 / 限幅；记录事件，不必 halt |
| `-50s`（配置损坏降级 / init 失败） | 可恢复降级 | 器件隔离或回退安全默认值，**系统继续运行**；BAL 按码值（`-50/-51`）特判走保守逻辑（非 halt）。见器件 health 模型 |
| `-20s`（过流 / 过温） | 安全可恢复 | 限流、降频、降功耗后可继续；持续触发则升级为致命处理 |
| `-30s`（watchdog） | 致命 | 触发系统复位 |
| `-40s`（溢出 / UB） | 致命 | 停止不可信计算，进入 halt |
| `-99`（panic） | 不可恢复 | 立即 halt 并记录，等待外部复位 |

可恢复码段下执行器按需限幅 / 重试即可；致命 / 不可恢复码段下，各执行器应进入下列 fail-safe 姿态：

| 执行器 | 安全姿态 |
|---|---|
| LED | 打开红灯或闪烁报警 |
| Servo | 回到中位或保持当前位置 |
| Motor | 停止输出 PWM |
| Relay | 断开输出 |
| Heater | 关闭加热 |
| Pump | 停止运行 |

示例：

```c
void app_on_fault(uint32_t fault_code) {
    dal_motor_stop(&left_motor);
    dal_motor_stop(&right_motor);
    dal_rc_servo_set_angle(&neck_servo, 90.0f);
    dal_led_set_state(&status_led, LED_STATE_FLASHING);
    wink_trace_fault(fault_code);
}
```

---

## 8. 错误可观测性

运行时必须记录错误事件：

```c
typedef struct {
    uint64_t timestamp_ms;
    uint32_t component_id;
    uint16_t api_id;
    wink_status_t status;
    uint32_t fault_code;
} wink_error_event_t;
```

事件输出通道：

| 环境 | 输出方式 |
|---|---|
| Web 仿真 | Worker postMessage 到控制台 |
| 真机调试 | UART trace 输出 |
| CI 测试 | Golden Trace 文件 |
| 云端分析 | 上传结构化错误日志 |

---

## 9. 编译期静态检查

编译前应检查：

1. App 是否忽略了 `wink_status_t` 返回值。
2. 设备树是否存在不可满足的能力约束。
3. 目标平台是否缺少所需 PAL API。
4. 引脚电压是否存在危险连接。
5. 采样频率是否违反器件最小间隔。

AI 生成代码中，以下模式必须拒绝或警告：

```c
dal_ultrasonic_read(&front_radar, &distance_cm);
```

必须改为：

```c
wink_status_t status = dal_ultrasonic_read(&front_radar, &distance_cm);
if (status != WINK_OK) {
    app_on_fault(FAULT_FRONT_RADAR_UNAVAILABLE);
    return;
}
```

---

## 10. 产品体验建议

仿真控制台应提供“异常测试”面板：

1. 断开某根线。
2. 让传感器持续超时。
3. 为 ADC 加入噪声。
4. 模拟 I2C 地址冲突。
5. 模拟 Servo 卡死。
6. 观察 App 是否进入安全状态。

这会把平台从“能跑 demo”提升为“能验证真实嵌入式风险”的专业工具。

---

## 11. AI Codegen 错误码语义详表

> **本节 = 单一事实来源（SSOT）**：错误码语义、典型触发场景、推荐恢复策略、是否可用作 `WINK_PT_EXIT` 退出条件，全部在此定义。任何新增/调整 `wink_status_t` 值必须先改本节（并更新 `wink_status.h` 内联 doxygen），再改代码；两侧要求 **bit-for-bit 一致**（值、名称、简介）。
>
> **为什么需要这一节**：§2 定义了枚举值与码段，§7 定义了码段级降级策略，但每个具体 `WINK_ERR_*` 码「触发时应当做什么」在 AI Codegen 场景下仍是模糊的（例如 `WINK_ERR_TIMEOUT` 与 `WINK_ERR_BUSY` 都属 "-1..-11 通用"，但一个应当上限次数重试后 fault、另一个几乎总是 yield 到下一 tick 即可）。本节把「码 → 应用侧动作」显式化，供 AI 生成 App 时作 few-shot 提示。
>
> **"WINK_PT_EXIT 条件"**：`WINK_PT_EXIT(pt)` 语义是让当前 protothread **永久停止**（`line=0xFFFF`，不会重启）。表格中 ✅ = 该错误一旦出现，本 PT 已无恢复可能（例如器件从未成功 init），继续跑等于死循环，应 `WINK_PT_EXIT`；❌ = 应交给 fault 逻辑或重试路径，不宜由本 PT 自决退出。

### 11.1 通用可恢复段（`-1..-17`）

| 码值 | 名称 | 语义 | 典型触发场景 | 推荐恢复策略 | 可作 `WINK_PT_EXIT` 条件 |
|------|------|------|-------------|-------------|-----------------------|
| 0 | `WINK_OK` | 操作成功 | 正常路径 | 继续执行 | ❌ 否 |
| -1 | `WINK_ERR_INVALID_ARG` | 参数校验失败 | NULL 指针、越界枚举、非法配置组合 | **caller bug**：修调用方；不重试；`WINK_PT_DEBUG` 下可 assert 快速暴露 | ❌ 否（是 bug 不是 recover） |
| -2 | `WINK_ERR_TIMEOUT` | 操作超时 | I2C ACK 未到、GPIO wait_level 超时、RMT 采样超期 | 短期抖动（1~2 tick）内自动重试 ≤ N 次；连续超阈值 → `app_on_fault()` | ❌ 否（应重试后 fault，非 exit） |
| -3 | `WINK_ERR_DISCONNECTED` | 器件断线 | 上电探测 NACK、传感器长期无响应、拔线 | 立即进入 fail-safe 姿态；停止对该器件的后续 API 调用 | ✅ 是（本 PT 依赖的物理器件已消失） |
| -4 | `WINK_ERR_OUT_OF_RANGE` | 数值越界 | ADC 读数超量程、几何参数超行程 | 限幅到合法边界继续；若为控制目标越界则 `app_on_fault()` | ❌ 否（限幅重跑本 tick） |
| -5 | `WINK_ERR_IO` | 通用 I/O 错误 | 底层 bus 返错、CRC 前置校验失败但未归类 | 短期重试 ≤ 2 次；仍失败 → 视同 `DISCONNECTED` | ❌ 否 |
| -6 | `WINK_ERR_BUSY` | 资源被占用 / 未就绪 | I2C 总线仲裁失败、PT `WAIT_UNTIL` 条件未满足（**语义复用**：这也是 `WINK_PT_YIELD/WAIT_UNTIL` 内部返回值） | **绝大多数场景 = yield**：直接 `WINK_PT_YIELD` 让出到下一 tick；不作 fault | ❌ 否（是 yield 信号非错误） |
| -7 | `WINK_ERR_UNSUPPORTED` | 当前 target/构建缺失能力 | wasm 上调 ESP32-only API、`WINK_STRICT_NONBLOCKING` 下调 blocking API | **编译期就应拦截**；运行期出现 = 生成器 bug；日志 + fault | ✅ 是（本 PT 在此 target 永远跑不通） |
| -8 | `WINK_ERR_CHECKSUM` | 校验失败 | I2C/SPI CRC 错、EEPROM 数据完整性校验错 | 重试 ≤ 2 次；仍失败 → 视同 `WINK_ERR_HARDWARE` | ❌ 否 |
| -9 | `WINK_ERR_PERMISSION` | 权限拒绝 | 沙箱越权、Flash 保护区写入被拒 | 立即停止越权操作；`app_on_fault()` | ✅ 是（沙箱策略拒绝，不会自愈） |
| -10 | `WINK_ERR_RESOURCE_EXHAUSTED` | 资源池耗尽 | `pal_resource_claim` 表满、PWM 通道用尽、软定时器耗尽 | **部署期 bug**：设备树容量校验应前置拦截；运行期出现 → `app_on_fault()` | ✅ 是（当前编译产物容量不足） |
| -11 | `WINK_ERR_NOT_INITIALIZED` | 器件未 init 就被调用 | `dal_*_init` 未跑或返错就调 read/write | **调用序 bug**：修 caller；不重试 | ✅ 是（依赖的器件从未 init 成功） |
| -12 | `WINK_ERR_HARDWARE` | 底层驱动返错（含 ESP-IDF `esp_err_t != ESP_OK` 的统一映射） | GPIO ISR 服务安装失败、DMA 通道申请失败 | 重试 ≤ 2 次；仍失败 → `app_on_fault()` | ❌ 否（首次不 exit，交给 fault 决策） |
| -13 | `WINK_ERR_NO_MEM` | 内存不足 | `pvPortMalloc` 返 NULL、静态池耗尽 | **禁止在 runtime path 分配动态内存**；出现即视为部署配置错，`app_on_fault()` | ✅ 是（无法恢复） |
| -14 | `WINK_ERR_EMPTY` | 容器/队列空 | 无待处理事件、trace 环形空 | 通常是 poll-style API 的正常返回；yield 到下一 tick | ❌ 否（多为正常路径信号） |
| -15 | `WINK_ERR_FULL` | 容器/队列满 | trace 环形满（覆盖旧记录）、event queue 满 | 若为 lossless 队列 → 提高消费速率或扩容；lossy 场景视需求丢头/丢尾 | ❌ 否 |
| -16 | `WINK_ERR_INVALID_STATE` | 状态机非法转移 | `pal_gpio_read` 未 claim 的 pin、resource 双向 release、驱动被销毁后调用 | **调用序 bug**：修 caller；不重试 | ✅ 是（约束已破坏，重试无意义） |
| -17 | `WINK_ERR_LOCKED` | 资源被锁 | 引导安全锁触发（[ADR-0010](../../decisions/core/0010-boot-safe-lock-recovery-threshold.md)）、配置 flash 被外部锁定 | 遵循 boot safe-lock 流程；不由 PT 自行解锁 | ✅ 是（本 PT 无权解除锁） |

### 11.2 安全可恢复段（`-20..-22`）

| 码值 | 名称 | 语义 | 典型触发场景 | 推荐恢复策略 | 可作 `WINK_PT_EXIT` 条件 |
|------|------|------|-------------|-------------|-----------------------|
| -20 | `WINK_ERR_OVERCURRENT` | 过流 | 电机堵转、短路检测 | 立即限流或关断；短期尝试恢复；持续 → 升级为致命 | ❌ 否（先降级重试） |
| -21 | `WINK_ERR_OVERTEMPERATURE` | 过温 | 芯片 / MOSFET 温度阈值 | 降频、降占空比；持续 → 关闭输出并 fault | ❌ 否 |
| -22 | `WINK_ERR_ALREADY_INITIALIZED` | 对已初始化器件重复 `init` | 同一 `dal_xxx_t` 未 deinit 即再次 init（DAL-L-004 fail-fast） | **调用序 bug**：修 caller；不隐式 deinit、不重试 | ✅ 是（约束已破坏，重试无意义） |

### 11.3 可恢复降级段（`-50..-51`，[ADR-0005](../../decisions/core/0005-degraded-status-segment.md)）

| 码值 | 名称 | 语义 | 典型触发场景 | 推荐恢复策略 | 可作 `WINK_PT_EXIT` 条件 |
|------|------|------|-------------|-------------|-----------------------|
| -50 | `WINK_ERR_CONFIG_CORRUPT_DEGRADED` | 配置损坏 → 使用安全默认值继续 | NVS/Flash 配置 CRC 错、schema 版本不兼容 | **不停机**：加载安全默认值，标记 degraded；BAL 走保守分支 | ❌ 否（系统继续，交给 BAL） |
| -51 | `WINK_ERR_FAILED_INIT` | 器件 init 失败 → 器件隔离，系统继续 | 单个 DAL init 失败但系统其他器件仍可用 | 该器件的所有后续 API 直接返 `WINK_ERR_NOT_INITIALIZED`；系统整体继续 | ✅ 是（本 PT 若强依赖该器件） |

### 11.4 致命段（`-30..-40`，`-99`）

| 码值 | 名称 | 语义 | 典型触发场景 | 推荐恢复策略 | 可作 `WINK_PT_EXIT` 条件 |
|------|------|------|-------------|-------------|-----------------------|
| -30 | `WINK_ERR_WATCHDOG` | 看门狗超时 | 硬件 WDT 已经或即将触发复位 | **复位**：runtime + boot safe-lock 处理（[04-runtime-and-trace.md](../02-wink-micro-os/04-runtime-and-trace.md) §3） | ❌ 否（本 PT 已不会再调度） |
| -40 | `WINK_ERR_OVERFLOW` | 数值溢出 / 计算 UB | 整数溢出、栈溢出保护触发 | 停止不可信计算；`wink_trace_fault + halt` | ❌ 否（应 fault 而非 PT exit） |
| -99 | `WINK_ERR_PANIC` | 不可恢复内部错误 | INVARIANT 断言、WINK_ASSERT_NONBLOCKING 触发（[ADR-0017](../../decisions/core/0017-blocking-api-hard-isolation.md)） | 立即 `wink_trace_fault` + halt，等待外部复位 | ❌ 否（本 PT 已不会再调度） |

### 11.5 生成器约束

AI 代码生成器（`wink-tools/tools/codegen/pt_state.py` 等）在处理 DAL/PAL 调用时必须：

1. **命中即检查**：调用返回 `wink_status_t` 的 API 后立即分支；禁止 `WINK_IGNORE_UNUSED` 除非注释说明理由；
2. **码值分档**：不同码段走不同分支 —— **通用可恢复**走重试计数；**降级**走保守路径；**致命**直接 `app_on_fault()`；
3. **`WINK_PT_EXIT` 收敛**：仅当本表 ✅ 列命中且业务语义确认本 PT 已无跑通可能时才用；其他一律走 fault 或 yield；
4. **`WINK_ERR_BUSY` 特例**：出现在 protothread yield 路径时（`WINK_PT_YIELD` / `WINK_PT_WAIT_UNTIL` 返回值），**不视为错误**；`wink_status_is_error(WINK_ERR_BUSY) == true` 但生成器应用 `if (status < 0 && status != WINK_ERR_BUSY)` 判定；
5. **未列入本表的负值码 = 生成器 bug**：代码中若出现本表未定义的 `wink_status_t` 负值，视为符号漂移，应先补本节 + 补 `wink_status.h`，再改代码。

> **同步义务**：本节新增/调整任何一行时，**必须同步**：
> 1. `wink-micro-os/pal/include/wink_status.h` 对应 enum 的内联 `/**< brief */`（bit-for-bit 一致）；
> 2. §2 码段总览（若涉及码段变化）；
> 3. §7 降级策略表（若涉及码段级动作变化）。

