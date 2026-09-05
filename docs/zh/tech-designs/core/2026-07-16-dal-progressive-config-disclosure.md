# DAL/PAL 渐进披露配置 — 技术设计

| 项 | 内容 |
|----|------|
| 创建日期 | 2026-07-16 |
| 修订 | 2026-07-17（并入评审 Major Revision 的 P0/P1 设计锁） |
| 关联 ADR | [ADR-0034](../../decisions/core/0034-dal-progressive-config-disclosure.md)（**Accepted**） |
| 关联评审 | [reviews/2026-07-17-dal-progressive-config-disclosure-review.md](../../reviews/core/2026-07-17-dal-progressive-config-disclosure-review.md) |
| 关联实施计划 | [implementation-plans/2026-07-16-dal-progressive-config-disclosure-plan.md](../../implementation-plans/core/2026-07-16-dal-progressive-config-disclosure-plan.md) |
| 关联设计规范 | `02-wink-micro-os/01-dal-device-abstraction.md`、`02-wink-micro-os/02-pal-platform-abstraction.md`、`03-app-codegen/01-app-business-logic.md`、`07-platform-governance/01-device-model-registry.md` |
| 关联 ABI/契约 ADR | [ADR-0028](../../decisions/core/0028-host-binary-abi-toolchain-contract.md)、[ADR-0012](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)、[ADR-0008](../../decisions/core/0008-dynamic-device-tree-config-flash.md) |

---

## 1. 设计目标

在**不改变**现有 L1 App / 默认板级行为的前提下：

1. DAL/PAL 能表达专业级电气/PWM 细节（escape hatch）。
2. 默认值 = 今日行为，对 AI/codegen **默认不暴露**。
3. 双 target 诚实：仿真不承诺时钟树/片内电阻强度保真，无法保真处明确 `UNSUPPORTED` 或统一告警。
4. 公共 POD 变更按 ABI 契约（ADR-0028）成对升级头与 archive；不静默错配旧生成物。

---

## 2. 分层模型

```text
L1  wink-app.json（默认可见）
    button: pin / active_low / event_drive
    servo:  pwm_channel / min_pulse_ms / max_pulse_ms
             │  advanced 可选，默认不生成
             ▼
L2  advanced（唯一 L2 表示）
    button.advanced.pull
    servo.advanced.resolution_bits
    servo.advanced.clock_requirement
             │  codegen 映射（字符串 → C 枚举，条件发射）
             ▼
    DAL config POD
    DAL 自有语义枚举，不引用 pal_* 类型
             │  DAL 显式映射
             ▼
    PAL PWM requested config（target-independent）
             │  target 解析 AUTO
             ▼
    PAL effective timer profile
    freq + effective bits + effective clock source
```

- **只保留一种 L2 表达：`advanced.*`。** 删除顶层 `button.pull` 双写。若为历史兼容保留顶层 `pull`，仅作明确版本周期内的 deprecated alias：任何同时出现 `pull` 与 `advanced.pull` 的配置**一律报错，不比较取值是否相同**；codegen 永不生成顶层形式。

### 2.1 配置所有权（SSOT）

原「默认值仅在 C 头/init」表述不足。改为按层分权：

| 层 | 所有权 |
|----|--------|
| Device Model Registry / wink-app schema | 字段名称、数据类型、取值范围、L1/L2 可见性、迁移规则 |
| Codegen driver plugin | 输入结构校验、alias 处理、字符串 → C 枚举确定性映射、条件发射 |
| DAL | 器件语义配置 → PAL 配置映射，**不暴露 PAL 类型** |
| PAL target | AUTO → effective config 解析，frequency/resolution/clock 硬件可实现性**权威校验** |
| C 中 `AUTO=0` | 运行期默认行为的最终权威定义；文档与 Registry 必须与其一致 |

> Device Model Registry 更新为**必做**（其 Button API 已落后于真实代码）。真正的 `wink-app.json` 规范主要在 `03-app-codegen/01-app-business-logic.md`，不是 `02-project-manifest-schema.md`。

---

## 3. Button 配置

### 3.1 DAL C 类型（固定宽度，布局明确）

```c
typedef uint8_t dal_button_pull_t;

enum {
    DAL_BUTTON_PULL_AUTO = 0,   /* active_low → UP，否则 DOWN */
    DAL_BUTTON_PULL_UP   = 1,
    DAL_BUTTON_PULL_DOWN = 2,
    DAL_BUTTON_PULL_NONE = 3,
};

typedef struct {
    const char        *owner;
    uint16_t           pin;
    bool               active_low;
    dal_button_pull_t  pull;   /* 新增；0 = AUTO */
} dal_button_config_t;
```

> 用 `uint8_t` 而非普通 `enum`（后者常占 4B，追加在 `bool` 后引入 padding）。**这仍是 ABI 变更**（见 §7），布局只是更明确。

### 3.2 解析（`dal_button_init`）

| pull | active_low | pal_gpio_mode |
|------|------------|---------------|
| AUTO | true | INPUT_PULLUP |
| AUTO | false | INPUT_PULLDOWN |
| UP | * | INPUT_PULLUP |
| DOWN | * | INPUT_PULLDOWN |
| NONE | * | INPUT（无内部上下拉） |

`active_low` 仅影响 `button_raw_pressed` 逻辑，不再单独决定电气模式（除 AUTO 推导外）。

**初始化契约：非法 `pull` 必须在 `pal_resource_claim()` 之前返回 `WINK_ERR_INVALID_ARG`**，否则错误配置会短暂占用资源，增加失败回滚复杂度。

### 3.3 `pull=NONE` 的仿真契约（P0-4）

host/wasm 对普通 `INPUT` 默认读 `false`，而 active-low 逻辑把 `false` 解释为**按下** → `NONE` + 未注入外部电平会产生**虚假 PRESS**。

因此锁定（Task 0，2026-07-17）：

```
host/wasm 中，PAL_GPIO_INPUT（pull=NONE）表示输入端未提供内部驱动。
若 simulator 尚未向该 pin 注入外部电平（无 sim_set_gpio_ideal / 等价注入）：
  - pal_gpio_read 返回 WINK_ERR_DISCONNECTED（已有错误码 -3），*out_level 不作为有效电平；
  - dal_button_poll 遇到 DISCONNECTED：不推进去抖状态机，不派发 PRESS/RELEASE，向上返回该错误（或 WINK_OK 且零边沿——锁定为：返回 WINK_ERR_DISCONNECTED）；
一旦注入外部电平，读取该电平并恢复正常去抖。
```

`pull=NONE` **不隐含**任何 idle level。今日 `AUTO`+`active_low`→`PULLUP` 路径不受影响（idle=HIGH）。

### 3.4 JSON（唯一 L2 表示）

默认：

```json
"boot_button": { "type": "button", "pin": 0, "active_low": true }
```

专家：

```json
"boot_button": {
  "type": "button",
  "pin": 0,
  "active_low": true,
  "advanced": { "pull": "none" }
}
```

Codegen 校验：`advanced` 必须为 object；未知子键报错；`pull ∈ {auto,up,down,none}`（string、大小写严格）；违规 → `SystemExit(2)`。缺省不发射 `.pull` 行（C 侧 0 = AUTO）。

---

## 4. PWM 配置

### 4.1 Requested config（target-independent）

```c
typedef enum {
    PAL_PWM_CLOCK_AUTO            = 0,  /* 平台自动选择（ESP32: LEDC_AUTO_CLK） */
    PAL_PWM_CLOCK_STABLE_REQUIRED = 1,  /* 必须兑现的稳定源保证；不可满足 → 负数错误 */
} pal_pwm_clock_requirement_t;

typedef struct {
    uint32_t                    freq_hz;          /* 必填，>0 */
    uint8_t                     resolution_bits;  /* 0 = AUTO → target 自动选择 */
    pal_pwm_clock_requirement_t clock_requirement;
} pal_pwm_config_t;

WINK_WARN_UNUSED_RESULT
wink_status_t pal_pwm_init(uint8_t channel, uint32_t frequency_hz);
/* 薄包装：init_ex(channel, &(pal_pwm_config_t){ .freq_hz = frequency_hz }); → 13-bit + AUTO */

WINK_WARN_UNUSED_RESULT
wink_status_t pal_pwm_init_ex(uint8_t channel, const pal_pwm_config_t *cfg);
```

**弃用 `FIXED`**（未说明是「偏好」还是「必须」）。默认采用**保证型** `STABLE_REQUIRED`。

#### 4.1.1 时钟语义矩阵（Task 0 已冻结，2026-07-17）

| Target | AUTO | STABLE_REQUIRED |
|--------|------|-----------------|
| ESP32（本仓库 DevKitC / classic） | `LEDC_AUTO_CLK` | **`LEDC_USE_REF_TICK`**（1 MHz，**DFS-stable**） |
| host | 接受并记录 AUTO | `WINK_ERR_UNSUPPORTED` |
| wasm | 接受并记录 AUTO | `WINK_ERR_UNSUPPORTED` |

**语义边界（诚实契约）：**

- `STABLE_REQUIRED` = **APB/DFS 下频率不漂移**（REF_TICK 特性），**不**等同于 Light-sleep keep-alive PWM。
- Light-sleep 持续输出需要 `RC_FAST` + `LEDC_SLEEP_MODE_KEEP_ALIVE` —— **本波次 Non-goal**；不得静默把 STABLE_REQUIRED 假装成 sleep keep-alive。
- best-effort 降级另立 `PAL_PWM_CLOCK_STABLE_PREFERRED`（本波次不做）。

**错误码冻结：**

| 条件 | 返回 |
|------|------|
| host/wasm 收到 `STABLE_REQUIRED` | `WINK_ERR_UNSUPPORTED` |
| `resolution_bits` 越界 / 无法映射到 `ledc_timer_bit_t` | `WINK_ERR_INVALID_ARG`（在 router acquire / 硬件配置之前） |
| `(freq, bits, REF_TICK)` 合法入参但 `ledc_timer_config` 失败 | `WINK_ERR_HARDWARE`，并**完整撤销** router acquire |
| `freq_hz == 0` / `cfg == NULL` | `WINK_ERR_INVALID_ARG` |

**AUTO resolution：** ESP32 默认仍为 **13-bit**（兼容今日 `pal_pwm_init`）。

**Effective `clock_source` 编码（写入 `pal_pwm_timer_profile_t.clock_source`）：**

```c
enum {
    PAL_PWM_EFF_CLK_PLATFORM_AUTO = 0, /* AUTO → LEDC_AUTO_CLK 已接受 */
    PAL_PWM_EFF_CLK_REF_TICK      = 1, /* STABLE_REQUIRED → LEDC_USE_REF_TICK */
};
```

### 4.2 Effective timer profile + profile-aware Router（P0-1，最关键）

ESP32 的 frequency / resolution / clock 是 **LEDC timer 级属性**，不是 channel 属性。仅缓存 per-channel bits 无法避免「同频不同 bits 复用同一 timer」导致的静默占空比错误。

```c
typedef struct {
    uint32_t freq_hz;
    uint8_t  resolution_bits;  /* effective（AUTO 已解析） */
    uint8_t  clock_source;     /* target 解析后的 effective source，非用户 policy */
} pal_pwm_timer_profile_t;

WINK_WARN_UNUSED_RESULT
wink_status_t pal_pwm_router_acquire(uint8_t channel,
                                     const pal_pwm_timer_profile_t *profile,
                                     uint8_t *out_timer_num);
```

Router 规则（锁定）：

1. **target 先解析** requested config → effective timer profile；
2. 仅**完整 profile 相同**的通道可共享 timer；
3. 同 channel + 同 profile → 幂等成功；
4. 同 channel + 不同 profile → `WINK_ERR_BUSY`；
5. 不同 channel + 不同 profile → 分配另一 timer；
6. timer 耗尽 → `WINK_ERR_RESOURCE_EXHAUSTED`；
7. 硬件配置失败 → **撤销 Router acquire**，不留半初始化态。

### 4.3 Duty 语义按 target 分开（P1-4）

`pal_pwm_set_duty(channel, duty_percent)` 的跨 target 公共语义**始终是百分比**：

- **ESP32**：按该 channel 所属 timer 的 effective resolution 换算为 LEDC raw duty；
- **host**：继续记录百分比，不改为 raw count；
- **wasm**：继续向 JS bridge 与虚拟 servo 模型传百分比；
- raw 换算仅通过纯 helper / 测试专用 hook 验证，**不属于** host/wasm 仿真公开 ABI（不改 `sim_last_pwm_duty()` / `js_pal_pwm_set_duty()`）。

整数安全转换（`bits` 须在 `1u << bits` **之前**完成合法性校验，防不安全移位）：

```c
static uint32_t pwm_percent_to_raw(float percent, uint8_t bits)
{
    if (percent < 0.0f)   { percent = 0.0f; }
    if (percent > 100.0f) { percent = 100.0f; }
    uint32_t max_duty = (UINT32_C(1) << bits) - UINT32_C(1);
    return (uint32_t)((percent * (float)max_duty) / 100.0f);
}
```

### 4.4 Resolution 两层校验（P0-3 口径统一）

取消「codegen 认定 1–20 全部可用」。分两层：

- **Codegen（target-independent）**：必须为整数；拒绝 `bool`；`0` 只能通过省略或 `"auto"` 表达；落在平台格式宽泛边界内。
- **Target（权威）**：`(freq_hz, requested bits, selected clock source)` 硬件是否可实现；PAL **必须**保留防御性校验，即使构建期已知 board capability。

状态发布顺序（ESP32，硬件成功前**不**写 `s_ch_bits[channel]`）：

```
validate → resolve effective profile → router acquire
→ configure timer → configure channel → commit channel state
```

### 4.5 Servo DAL（DAL 不泄漏 PAL 类型，P0-5）

```c
typedef uint8_t dal_servo_clock_requirement_t;

enum {
    DAL_SERVO_CLOCK_AUTO            = 0,
    DAL_SERVO_CLOCK_STABLE_REQUIRED = 1,
};

typedef struct {
    const char                    *owner;
    uint8_t                        pwm_channel;
    uint8_t                        resolution_bits;   /* 0 = AUTO */
    dal_servo_clock_requirement_t  clock_requirement; /* 0 = AUTO */
    float                          min_pulse_ms;
    float                          max_pulse_ms;
} dal_servo_config_t;
```

`dal_servo.c` 内部映射，App / device_tree 无需 include `pal_hal.h`：

```c
static wink_status_t servo_map_pwm_config(const dal_servo_config_t *servo_cfg,
                                          pal_pwm_config_t *out_pwm_cfg);
```

默认路径仍 50Hz；`resolution_bits`/`clock_requirement` 为 0 时行为等价旧 `pal_pwm_init(ch, 50)`。字段顺序按最终 ABI 布局设计，**不机械尾加**；无论如何按 ADR-0028 bump ABI。

JSON L2：`advanced.resolution_bits`（整数）、`advanced.clock_requirement`（`auto`|`stable_required`）。

### 4.6 明确不进入 JSON 的项

| 项 | 原因 |
|----|------|
| `system_clock_hz` / APB / XTAL | SoC 专属，由 target + IDF 掌握 |
| LEDC speed mode HS/LS | 平台策略，非 App 语义 |
| 外拉电阻 Ω | 原理图域，非固件配置 |

---

## 5. Codegen / AI 护栏

1. **默认生成**：不发射 `pull` / `resolution_bits` / `clock_requirement` 行（C 侧 0 = AUTO）。
2. **仅当 JSON `advanced.*` 显式出现**才写入指定初始化。
3. 只接受 `advanced.*`；顶层 alias（若保留）与 `advanced` 双写一律报错。
4. Schema 文档将 advanced 标为「专家可选」；设备目录默认示例不含 advanced。
5. 默认 golden **字节级不变**；高级 fixture 单独新增，不改默认 fixture；新增「生成 C 能实际编译」门禁。
6. （可选后续）workbench UI 折叠「高级」；本设计不阻塞。

---

## 6. Binary SDK ABI 与 Flash wire（分开描述）

### 6.1 POD ABI（ADR-0028）

Button/Servo 公共 POD 增字段 = **源码兼容、二进制不兼容**：

- 按 ADR-0028 bump SemVer 与 `ABI=`（本波次锁定 **`0.2.0` / `ABI=2`**；VERSION 文件在实施计划 Task 2 落地）；
- 新头文件与新 `.a` **成对发布**，禁止新头搭旧 archive；
- 「零值兼容」仅对指定初始化器成立；外部消费者未 `{0}` 清零的逐字段赋值不保证，迁移须先 `{0}`；
- 治理缺口：ADR-0028 称 `pal_hal.h` 不进 Binary SDK，但 `wink-tools/tools/pack/binary.py` 当前 auto-scan `pal/include`，会打包 `pal_hal.h`——单独修复 packer 白名单或修订 ADR-0028，不借本功能扩大未定义 PAL ABI。

### 6.2 Flash wire（ADR-0008）

Servo override 是显式 **9-byte wire**（`dal_servo.c`：byte0 `pwm_channel`、byte1–4 `min_pulse_ms`、byte5–8 `max_pulse_ms`），不依赖 `sizeof(config)`：

- wire v1 保持 **9 bytes**；
- **advanced 字段不参与 Flash override**；override 后 advanced 保持原值 / AUTO；
- **无需 wire version bump**；
- Button 当前无 override，记 N/A。

> wire 兼容 **不等于** POD ABI 兼容。

---

## 7. 兼容性矩阵

| 场景 | 保证 |
|------|------|
| 旧 JSON 未含 advanced | 行为兼容，无需迁移 |
| 旧源码用 zero / designated initializer | 重新编译后新字段 = AUTO |
| 旧源码逐字段赋值且未清零 | **不保证**；迁移要求先 `{0}` |
| 已编译 Binary SDK 消费者 | POD 布局不兼容；必须 ABI++ 并成对升级头与 archive |
| 旧 `pal_pwm_init(ch, freq)` | 继续得到 13-bit / AUTO 历史行为 |
| host / wasm duty 观测 | 保持百分比 |
| Servo Flash wire v1 | 固定 9 bytes；advanced 不可 Flash override |
| Button Flash wire | N/A，当前无消费者 |
| `pull=NONE` 未注入电平 | 明确返回 floating / disconnected，不假定 LOW |
| stable clock 在不支持 target | 按确定策略返回 `UNSUPPORTED` |
| PWM 同频不同 bits | **不复用同一 timer**；同 channel 冲突 → `BUSY` |

---

## 8. 测试要点

- **Button**：AUTO/UP/DOWN/NONE × active_low 真值表；NONE × 未注入 / 注入 HIGH / 注入 LOW；非法枚举在 claim 前失败；debounce/events/IRQ 回归。
- **PWM Router**：同频同 profile 复用；同频不同 bits 不错误复用；clock profile 冲突；init 失败完整回滚；deinit 后可重分配。
- **PWM duty**：host/wasm 保持百分比；`pwm_percent_to_raw` 纯函数 full-scale；ESP32 raw 换算按 effective bits。
- **Codegen**：默认 golden 字节级不变；advanced positive/negative fixtures；生成 C 能编译；alias 双写报错。
- **Binary SDK**：VERSION/ABI 升级；host/wasm package + consumer smoke。
- **App 样板**：`oled_dashboard` / `avoidance_car` 零 JSON 变更，e2e 回归。

---

## 9. 方案比选摘要

见 [ADR-0034](../../decisions/core/0034-dal-progressive-config-disclosure.md)；本技术设计固化 API 形状、Router profile 模型、契约与分层，不重复否决理由。

