# DAL 渐进式配置披露（Progressive Config Disclosure）设计与实施计划评审报告

> **实施后验收（复评）：** 见 [`2026-07-17-dal-progressive-config-disclosure-acceptance.md`](./2026-07-17-dal-progressive-config-disclosure-acceptance.md)（Accept with deferred HIL）。本文为设计期只读快照，正文不改写。
>
> **评审对象：**
> - 技术设计：[`docs/tech-designs/core/2026-07-16-dal-progressive-config-disclosure.md`](../../tech-designs/core/2026-07-16-dal-progressive-config-disclosure.md)
> - 实施计划：[`docs/implementation-plans/core/2026-07-16-dal-progressive-config-disclosure-plan.md`](../../implementation-plans/core/2026-07-16-dal-progressive-config-disclosure-plan.md)
>
> **评审日期：** 2026-07-17
> **评审人：** 资深 Code Reviewer（嵌入式运行时 + Binary SDK ABI + 多 target 仿真视角）
> **评审基线：** 以当前工作树、[`ADR-0028`](../../decisions/core/0028-host-binary-abi-toolchain-contract.md) 与 `ADR-0012` 为准
> **文档状态：** Draft（设计期）；收口见上方 acceptance 链接
> **变更范围风险：** High —— 覆盖 DAL 公共 API/ABI、PWM 共享硬件 timer 资源模型、三 target（host/wasm/ESP32）语义、Flash wire、codegen 与发布回滚流程
> **评审性质：** 只读评审。未修改任何源码/文档，未运行构建或测试。

---

## 0. 评审框架与总体结论

按 **"设计契约 → 现有代码事实 → 实施步骤可执行性 → 测试与回滚闭环"** 四层审查，并将结论区分为：

- **P0 阻断项（Blocker）** —— 未修复不得进入实现；
- **P1 重要修订项（Should-fix）** —— 影响契约完整性、可测试性或发布安全性；
- **优化建议 / 保留项** —— 值得继续保留的方向 + 可选改进。

不把计划文字本身当作事实，全部结论均以现有代码 / ADR / 规范交叉核验。

### 0.1 总体结论

> **建议：Major Revision（重大修订）**
>
> 当前版本**不应**直接将 `ADR-0034` 标记为 **Accepted**，也**不应**按现计划开始实现。

"AUTO=0 + 默认不向 AI 暴露 + 专家可选 escape hatch" 这一总体方向是正确的，也符合本仓库的静态分发、低代码简洁性与双 target 约束。但当前设计还有多项会导致 **真实硬件行为错误**、**Binary SDK ABI 破坏** 或 **仿真假阳性** 的未闭环问题，必须先修正下述四个核心问题再重新评审：

1. **PWM timer profile 共享模型**（见 P0-1）；
2. **Binary SDK ABI / 版本策略**（见 P0-2）；
3. **FIXED / resolution / floating input 的确定契约**（见 P0-3、P0-4）；
4. **Registry / schema / codegen / Layer ① 的 SSOT 闭环**（见 P1-2、P1-6）。

计划必须同时证明三件事，缺一不宜实施：

- App 无法绕过 L1 API；
- 驱动仍能完整访问配置；
- 旧生成物不会静默错配。

---

## 一、阻断项（P0）

### P0-1：PWM Router 的资源模型错误

**问题**

实施计划仍按 `freq_hz` 共享 timer，只在每个 channel 缓存 resolution：

- [实施计划:86-88](../../implementation-plans/core/2026-07-16-dal-progressive-config-disclosure-plan.md#L86-L88)
- [实施计划:311-314](../../implementation-plans/core/2026-07-16-dal-progressive-config-disclosure-plan.md#L311-L314)

而当前 Router 的 timer 身份**确实只有频率**：

- [`pal_pwm_router.c:9-17`](../../../../wink-micro-os/pal/src/pal_pwm_router.c#L9-L17)
- [`pal_pwm_router.c:31-43`](../../../../wink-micro-os/pal/src/pal_pwm_router.c#L31-L43)

ESP32 的 `frequency`、`resolution`、`clock` 都是 **LEDC timer 级属性**，不是 channel 属性：

- [`pal_hal_pwm_esp32.c:37-44`](../../../../wink-micro-os/targets/esp32/pal_hal_pwm_esp32.c#L37-L44)

**故障场景**

两个通道都是 50 Hz：

- channel 0：10-bit
- channel 1：13-bit

Router 会让它们复用同一 timer。channel 1 初始化时将 timer 重配为 13-bit，channel 0 的**硬件分辨率被静默改变**，但 channel 0 的 `s_ch_bits` 仍保存 10；此后 duty 换算错误，输出错误占空比。**这是可复现的真实硬件行为错误。**

**必须修改**

Router 的资源身份至少应是规范化后的**完整 effective profile**：

```c
typedef struct {
    uint32_t              freq_hz;
    uint8_t               effective_resolution_bits;
    pal_pwm_clk_source_t  effective_clk_source;
} pal_pwm_timer_profile_t;
```

规则应锁定为：

1. **完整 effective profile 相同**才可共享 timer；
2. 同 channel、同 profile → 幂等成功；
3. 同 channel、不同 profile → `WINK_ERR_BUSY`；
4. 不同 channel、不同 profile → 分配另一个 timer；
5. 无空闲 timer → `WINK_ERR_RESOURCE_EXHAUSTED`；
6. **必须先由 target 把 AUTO 解析为 effective profile，再交给 Router**。

> 仅增加 per-channel bits 数组**不足以**解决问题。

---

### P0-2：公共 POD 增字段是 Binary SDK ABI 破坏，不是"非破坏扩展"

**问题**

实施计划将 Button/Servo 结构体扩展描述为"尾部增加字段，0=旧行为，非破坏"：

- [实施计划:125-132](../../implementation-plans/core/2026-07-16-dal-progressive-config-disclosure-plan.md#L125-L132)

但这两个都是 Binary SDK 对外发布的公共 POD：

- [`dal_button.h:44-48`](../../../../wink-micro-os/dal/include/input/dal_button.h#L44-L48)
- [`dal_servo.h:12-18`](../../../wink-micro-os/dal/include/actuator/dal_servo.h#L12-L18)

而 [`ADR-0028`](../../decisions/core/0028-host-binary-abi-toolchain-contract.md) 已明确规定：

> **公共 POD 增、删、重排字段 → MAJOR + ABI++**

—— 见 [ADR-0028:39-52](../../decisions/core/0028-host-binary-abi-toolchain-contract.md#L39-L52)。

当前版本：`0.1.0`，`ABI=1`（见 [`wink-micro-os/VERSION`](../../../../wink-micro-os/VERSION#L1-L2)）。

**"零值兼容"不能写成无条件保证**

指定初始化器（designated initializer）确实会将省略字段补零：

```c
dal_servo_config_t cfg = {
    .owner = "...",
    /* ... */
};
```

但**不适用于**外部消费者手动逐字段赋值且未先 `{0}` 清零的情况：

```c
dal_servo_config_t cfg;  /* 未清零 */
cfg.owner = "...";
cfg.pwm_channel = 0;
/* 新增字段为垃圾值 */
```

**必须修改**

在变更公共头之前，先增加"ABI 决策"任务：

1. 明确本次变更是**源码兼容但二进制不兼容**；
2. 按 `ADR-0028` 更新 SemVer 与 `ABI=`；
3. **新头文件必须与新 `.a` 成对发布**；
4. 增加 Binary SDK host/wasm 打包与消费冒烟；
5. 禁止新头搭配旧 archive。

> 如果项目希望在 pre-1.0 阶段允许破坏性 minor 变更，需要**先修订 `ADR-0028`**，不能在本计划里默默绕过。

---

### P0-3：FIXED 时钟策略尚未形成可实现契约

**问题**

当前设计中的措辞包括"偏好稳定源"、"如 `REF_TICK`"、"不可用则 AUTO + warn"、"host/wasm 忽略或可选 warn"：

- [技术设计:118-127](../../tech-designs/core/2026-07-16-dal-progressive-config-disclosure.md#L118-L127)
- [技术设计:168-176](../../tech-designs/core/2026-07-16-dal-progressive-config-disclosure.md#L168-L176)
- [实施计划:317-326](../../implementation-plans/core/2026-07-16-dal-progressive-config-disclosure-plan.md#L317-L326)

这不符合 `ADR-0012` **契约诚实原则**：契约必须诚实，不允许含"或 / 可选 / 如"等未决措辞。

**建议锁定为**

```c
typedef enum {
    PAL_PWM_CLK_AUTO             = 0,
    PAL_PWM_CLK_STABLE_REQUIRED  = 1,
} pal_pwm_clk_policy_t;
```

语义矩阵：

| Target | AUTO             | STABLE_REQUIRED                                    |
| ------ | ---------------- | -------------------------------------------------- |
| ESP32  | 平台自动选择     | 映射到明确锁定的稳定源；不可满足则返回负数错误     |
| host   | 接受并记录 AUTO  | `WINK_ERR_UNSUPPORTED`                             |
| wasm   | 接受并记录 AUTO  | `WINK_ERR_UNSUPPORTED`                             |

如果确实需要 best-effort，再**独立**增加：

```c
PAL_PWM_CLK_STABLE_PREFERRED
```

并规定**统一、强制、可观测**的降级告警，不能写成"可选 warn"。

同时需要冻结的一并事项：

- ESP-IDF 版本下究竟使用哪个 clock source；
- clock 不可用时返回什么错误；
- `(freq, bits, clock)` 不可实现时返回 `INVALID_ARG` 还是 `UNSUPPORTED`；
- 允许的 resolution 是 1–20、8–14，还是由 target capability 决定。

当前计划一处写"1–20"，另一处写"只映射 8–14"，**仍然自相矛盾**，必须先统一口径。

---

### P0-4：`pull=none` 的 host/wasm 语义会产生虚假按下事件

**问题**

技术设计**没有定义**"无拉且没有外部电平注入"的可观察结果：

- [技术设计:168-175](../../tech-designs/core/2026-07-16-dal-progressive-config-disclosure.md#L168-L175)

host/wasm 对普通 `INPUT` 默认读出 `false`，而 Button 的 active-low 逻辑会把 `false` 解释为**按下**：

- [`dal_button.c:19-22`](../../../../wink-micro-os/dal/src/input/dal_button.c#L19-L22)

因此：

```
pull=none + active_low=true + 未注入外部电平
```

可能在数次 poll 后产生**虚假的 PRESS 事件**。

**推荐契约**

优先方案：

- 未驱动的无拉输入返回明确的 `disconnected / floating` 状态；
- 或**要求**仿真配置同时提供外部 idle 电平；
- 未提供时**拒绝或告警**，而不是默认成 LOW。

至少必须补充下列测试用例：

```
NONE × active_low × 未注入外部电平
NONE × active_low × 注入 HIGH
NONE × active_low × 注入 LOW
```

> 只读回 GPIO mode 不能证明 Button 行为正确。

---

### P0-5：DAL 公共头不应直接泄漏 PAL 类型

**问题**

技术设计考虑把：

```c
pal_pwm_clk_policy_t clk_policy;
```

直接放进 `dal_servo_config_t`：

- [技术设计:137-147](../../tech-designs/core/2026-07-16-dal-progressive-config-disclosure.md#L137-L147)

这会迫使 `dal_servo.h` 依赖 PAL 硬件契约，**破坏 DAL 信息隐藏**，也会让 App 代码被迫 include `pal_hal.h`。

**建议二选一：**

1. DAL 定义自己的**语义**枚举，由 `dal_servo.c` 内部映射到 PAL；
2. 真正的 clock override 放到 **board / target-owned** 配置中，Servo DAL 只表达舵机语义。

如果保留 DAL escape hatch，建议使用：

```c
typedef enum {
    DAL_SERVO_PWM_CLOCK_AUTO = 0,
    DAL_SERVO_PWM_CLOCK_STABLE_REQUIRED,
} dal_servo_pwm_clock_t;
```

> **不要**在 DAL public API 中直接出现 `pal_*` 类型。

---

## 二、重要修订项（P1）

### P1-1：只保留一种 JSON 规范表示

设计同时允许：

```json
"pull": "none"
```

与：

```json
"advanced": {
  "pull": "none"
}
```

这会扩大 AI 可见面、冲突处理和迁移矩阵。

**建议只接受 `advanced.pull`。**

如果顶层 `pull` 已有历史消费者，才将其作为 **deprecated alias**；无论两者值是否相同，只要**双写就报错**。当前计划一处说"双写报错"，示例逻辑却只在值不同时报错，需统一。

同时必须校验：

- `advanced` 必须为 object；
- 未知 `advanced` 子键报错；
- `pull` 必须为 string；
- 大小写严格；
- `bool` 不能被当作 integer resolution。

---

### P1-2：重新定义配置 SSOT

技术设计称默认值在 C 头和 init 中是 SSOT：

- [技术设计:46](../../tech-designs/core/2026-07-16-dal-progressive-config-disclosure.md#L46)

但 Device Model Registry 又宣称自己是统一 SSOT，且其 Button API 已经过时：

- [Device Model Registry:1-13](../../design/07-platform-governance/01-device-model-registry.md#L1-L13)
- [Device Model Registry:344-356](../../design/07-platform-governance/01-device-model-registry.md#L344-L356)

**建议明确所有权：**

| 层                 | 所有权                                             |
| ------------------ | -------------------------------------------------- |
| Registry / schema  | 字段名、类型、范围、L1/L2 可见性、迁移规则         |
| Python plugin      | 输入校验、alias 处理、C 枚举映射                   |
| C DAL / PAL        | AUTO 的运行期解析和防御性校验                      |
| target             | effective resolution / clock 选择及硬件可实现性    |

> Registry 更新**不能再列为"可选"**。

此外，当前真正的 `wink-app.json` 规范主要在 [`03-app-codegen/01-app-business-logic.md`](../../design/03-app-codegen/01-app-business-logic.md)，**不是**计划引用的未来 Project Manifest 文档。Task 6 的目标文档需要修正。

---

### P1-3：Flash wire 可以现在就定论，不必延后

Servo override 是显式 **9-byte wire**，不依赖 `sizeof(dal_servo_config_t)`：

- [`dal_servo.c:66-90`](../../../wink-micro-os/dal/src/actuator/dal_servo.c#L66-L90)
- [`dal_servo.h:84-94`](../../../wink-micro-os/dal/include/actuator/dal_servo.h#L84-L94)

因此本次应明确：

- wire v1 继续保持 **9 bytes**；
- **advanced 字段不参与 Flash override**；
- override 后 advanced 保持原值 / AUTO；
- **不需要 wire version bump**；
- Button 当前无对应 override，记为 N/A。

> 注意：wire 兼容 **不等于** Binary SDK POD ABI 兼容，二者必须分开描述。

---

### P1-4：host/wasm 必须继续使用百分比 duty 语义

当前跨 target API 是：

```c
pal_pwm_set_duty(channel, percent);
```

host 记录百分比，wasm JS bridge 也传百分比：

- [`pal_hal_host.c:498-508`](../../../../wink-micro-os/targets/host/pal_hal_host.c#L498-L508)
- [`pal_hal_wasm.c:136-148`](../../../wink-micro-os/targets/wasm/pal_hal_wasm.c#L136-L148)
- [`test_host_pal.c:35-43`](../../../../wink-micro-os/test/unit/pal/test_host_pal.c#L35-L43)

因此：

- **只有 ESP32** 将百分比转换成 raw duty；
- host/wasm 继续观察 `7.5f` 等百分比；
- raw full-scale 测试应通过纯转换 helper 或 test-only hook；
- **不要改变** `sim_last_pwm_duty()` 与 `js_pal_pwm_set_duty()` 的语义。

---

### P1-5：测试范围和测试注册不完整

计划新增 `test_pal_pwm_config.c`，但遗漏：

- [`test/CMakeLists.txt`](../../../../CMakeLists.txt)
- [`host_test_ctrl.h`](../../../../wink-micro-os/test/stubs/host_test_ctrl.h)
- 实际记录 PWM 状态的 `pal_osal_host.c`
- `test_golden.py`
- **Binary SDK package / consumer 测试**

现有 golden 只做文本比较，并**不编译** advanced fixture 的生成 C：

- [`test_golden.py:40-80`](../../../../wink-tools/tools/codegen/tests/test_golden.py#L40-L80)

而 `wink.py test` 只直接运行 `test_golden.py`，不会自动发现新增 validation 文件：

- [`wink.py:627-646`](../../../../wink-tools/wink.py#L627-L646)

**建议改成 `unittest discover`，并增加 advanced fixture 生成 C 的编译门禁。**

---

### P1-6：ADR Accepted 后不能等到 Task 6 才回写 Layer ①

仓库规则要求 ADR Accepted 后**立即**回写活规范：

- [`.claude/rules/docs-adr.md:86-88`](../../../../.claude/rules/docs-adr.md#L86-L88)

因此应将：

```
ADR Accepted + Layer ① backport
```

合并成**同一个任务 / 变更批次**，而不是先 Accepted、实现完成后再补文档。

---

### P1-7：回滚方案需要区分"发布前"和"ABI 发布后"

计划中的 `WINK_PWM_LEGACY_DUTY` 当前并不存在，也没有实现和测试：

- [实施计划:523-535](../../implementation-plans/core/2026-07-16-dal-progressive-config-disclosure-plan.md#L523-L535)

**建议：**

**发布前** —— 整体 revert：

- API
- Router
- target
- codegen
- golden
- VERSION / ABI
- 文档

**ABI 发布后** —— 不能再删除公共字段或 `pal_pwm_init_ex`：

- 保留新增符号和结构体布局；
- 暂停 codegen 发射 advanced；
- advanced 请求返回明确错误；
- `pal_pwm_init` 保持 legacy 默认；
- 以新的 patch / minor archive 发布兼容修复。

---

## 三、建议后的任务顺序

将当前 Phase 0–8 调整为如下顺序：

1. **基线记录** —— host full、golden、Wasm/ESP32 样板基线。
2. **冻结设计口径** —— timer profile、clock、resolution、错误码、JSON 唯一表示、仿真 floating 语义。
3. **ABI 与 wire 决策** —— SemVer / ABI bump；wire v1 保持 9-byte。
4. **ADR Accepted + Layer ① 同步回写** —— 同一批完成。
5. **测试脚手架** —— CMake 注册、host hooks、codegen discover、失败测试。
6. **PAL API + profile-aware Router** —— `init_ex`、完整 profile、percent-to-raw helper。
7. **三 target** —— host/wasm 保持 percent；ESP32 做真实 raw 转换和失败回滚。
8. **Button DAL** —— 可与 PWM 主线并行。
9. **Servo DAL** —— 使用 DAL 自有策略类型并映射 PAL。
10. **Codegen** —— 默认 golden 零 diff；新增 advanced fixture。
11. **Binary SDK 发布兼容验证** —— VERSION/ABI、host/wasm package + consumer smoke。
12. **最终验收与 Review Record**。

---

## 四、最终验收门槛建议

至少要求：

1. `python wink-tools/wink.py test --full`
2. 所有 codegen `test_*.py` 被 discover
3. 默认 golden **字节级不变**
4. advanced fixture 生成的 C 能实际编译
5. Wasm：
   - Button App
   - Servo / PWM App
6. ESP32：
   - Button App build
   - `avoidance_car` Servo / PWM build
7. Binary SDK：
   - host package + consumer smoke
   - wasm package + consumer smoke
8. PWM Router：
   - 同频同 profile 复用
   - 同频不同 bits **不错误复用**
   - clock profile 冲突
   - init 失败**完整回滚**
   - deinit 后可重新分配
9. Button：
   - AUTO / UP / DOWN / NONE × polarity
   - floating 未注入行为
   - **非法枚举在 claim 前失败**
10. HIL 可选；未执行时必须明确记录未验证，**不得写"全部验证完成"**。

---

## 五、值得保留的设计优点

以下方向应继续保留：

- `AUTO=0` 适合静态聚合初始化和 codegen 默认省略；
- 保留 `pal_pwm_init()` 作为兼容薄包装；
- 不把 APB / XTAL / system clock 或外拉阻值暴露给 App；
- 坚持 **POD + 命名 API**，无 vtable / `container_of`；
- App 默认 JSON 不增加高级字段；
- 仿真明确不承诺时钟树和电阻强度保真；
- 未配置 advanced 时现有 App 行为不变，作为强回归门槛。

---

## 六、可直接落到两份文档的修订方案

以下是可以直接改到 **技术设计** 与 **实施计划** 的具体建议。目的：**把模糊处改成可实现、可测试的设计锁**。

### 6.1 建议先修改技术设计

#### 6.1.1 重写 §2 分层模型

只保留一种 JSON 高级配置形态：

```
L1  wink-app.json
    button: pin / active_low / event_drive
    servo:  pwm_channel / min_pulse_ms / max_pulse_ms
             │
             │ advanced 可选，默认不生成
             ▼
L2  advanced
    button.advanced.pull
    servo.advanced.resolution_bits
    servo.advanced.clock_requirement
             │
             │ codegen 映射
             ▼
    DAL config POD
    DAL 自有语义枚举，不引用 pal_* 类型
             │
             │ DAL 显式映射
             ▼
    PAL PWM config
    target-independent requested config
             │
             │ target 解析 AUTO
             ▼
    PAL effective timer profile
    freq + effective bits + effective clock source
```

删除顶层 `button.pull` 支持，避免两套表达。如果确实要保留顶层 alias，应写成：

> 顶层 `pull` 仅作为一个明确版本周期内的 **deprecated** 输入。任何同时出现 `pull` 与 `advanced.pull` 的配置均报错，**不比较二者值是否相同**。Codegen 永不生成顶层形式。

#### 6.1.2 修订 SSOT 定义

原文"默认值定义在 C 头文件枚举与 init 解析函数中"不够准确。建议替换为：

**配置所有权**

- **Device Model Registry / wink-app schema**：负责字段名称、数据类型、取值范围、L1/L2 可见性和迁移规则。
- **Codegen driver plugin**：负责输入结构校验、字符串到 C 枚举的确定性映射及条件发射。
- **DAL**：负责器件语义配置到 PAL 配置的映射，**不暴露 PAL 类型**。
- **PAL target**：负责将 AUTO 解析为 effective config，并权威校验 frequency / resolution / clock 的硬件可实现性。
- **C 中的 AUTO=0**：是**运行期默认行为的最终权威定义**；文档和 Registry 必须与其一致。

同时把 Device Model Registry 更新从"可选"改成**必做**（Registry 的 Button API 已经落后于真实代码）。

#### 6.1.3 Button 配置建议

**DAL 类型**

普通 C `enum` 往往占 4 字节，直接追加在 `bool` 后会引入 padding。建议固定存储宽度：

```c
typedef uint8_t dal_button_pull_t;

enum {
    DAL_BUTTON_PULL_AUTO = 0,
    DAL_BUTTON_PULL_UP   = 1,
    DAL_BUTTON_PULL_DOWN = 2,
    DAL_BUTTON_PULL_NONE = 3,
};

typedef struct {
    const char        *owner;
    uint16_t           pin;
    bool               active_low;
    dal_button_pull_t  pull;
} dal_button_config_t;
```

这仍然是 ABI 变更，但**布局更明确**。

**初始化契约**

必须写清：

> **非法 `pull` 必须在 `pal_resource_claim()` 之前返回 `WINK_ERR_INVALID_ARG`。**

否则错误配置可能短暂占用资源，增加失败回滚复杂度。

**NONE 的仿真契约**

```
host/wasm 中，PAL_GPIO_INPUT 表示输入端未提供内部驱动。
若 simulator 尚未向该 pin 注入外部电平，读取不得默认为逻辑 LOW；
应返回 WINK_ERR_DISCONNECTED（或项目现有等价错误）。
一旦注入外部电平，读取该电平。
```

因此 `pull=NONE` **不隐含**任何 idle level。

若当前 PAL 接口暂时无法表达 floating，最低限度也要：

- 增加"是否有外部电平注入"状态；
- 未注入时 Button poll 不得推进去抖状态机；
- 记录明确 warning / error。

#### 6.1.4 PWM API 建议

**Requested config**

```c
typedef enum {
    PAL_PWM_CLOCK_AUTO             = 0,
    PAL_PWM_CLOCK_STABLE_REQUIRED  = 1,
} pal_pwm_clock_requirement_t;

typedef struct {
    uint32_t                     freq_hz;
    uint8_t                      resolution_bits;
    pal_pwm_clock_requirement_t  clock_requirement;
} pal_pwm_config_t;

WINK_WARN_UNUSED_RESULT
wink_status_t pal_pwm_init_ex(uint8_t channel,
                              const pal_pwm_config_t *cfg);
```

其中：`resolution_bits == 0 → target 自动选择`。

**是否保留 FIXED**

不建议继续使用 "FIXED" 这个名字，它没有说明是"偏好"还是"必须兑现的保证"。建议从下面两种中明确选择：

**方案 A：保证型（推荐）**

```
PAL_PWM_CLOCK_STABLE_REQUIRED
```

- ESP32 无法满足 → `WINK_ERR_UNSUPPORTED` 或 `WINK_ERR_HARDWARE`；
- host/wasm → `WINK_ERR_UNSUPPORTED`。

优点是**契约最诚实**。

**方案 B：偏好型**

```
PAL_PWM_CLOCK_STABLE_PREFERRED
```

- ESP32 尽量使用稳定源；
- 不可满足时允许降级；
- 降级必须发**统一、可计数** warning；
- host/wasm 接受配置但必须发 warning。

> 如果选择方案 B，**不能继续叫 FIXED**。

#### 6.1.5 增加 effective timer profile

这是技术设计中**当前最重要的缺失结构**：

```c
typedef struct {
    uint32_t  freq_hz;
    uint8_t   resolution_bits;
    uint8_t   clock_source;
} pal_pwm_timer_profile_t;
```

其中 `clock_source` 是 **target 解析后**的 effective source，而不是用户传入的 policy。

Router 规则：

1. target 先解析 requested config → effective timer profile；
2. Router 只允许**完整 profile 相同**的通道共享 timer；
3. 同 channel + 同 profile：幂等；
4. 同 channel + 不同 profile：`BUSY`；
5. 不同 channel + 不同 profile：使用不同 timer；
6. timer 耗尽：`RESOURCE_EXHAUSTED`；
7. 硬件配置失败：**撤销 Router acquire**，不留下半初始化状态。

Router API 建议：

```c
WINK_WARN_UNUSED_RESULT
wink_status_t pal_pwm_router_acquire(
    uint8_t channel,
    const pal_pwm_timer_profile_t *profile,
    uint8_t *out_timer_num);
```

#### 6.1.6 Duty 语义必须按 target 分开描述

建议技术设计明确写：

> `pal_pwm_set_duty(channel, duty_percent)` 的跨 target 公共语义**始终**是百分比。
>
> - **ESP32**：根据该 channel 所属 timer 的 effective resolution 将百分比换算为 LEDC raw duty。
> - **host**：继续记录百分比，不将仿真观测值改为 raw count。
> - **wasm**：继续向 JS bridge 和虚拟 Servo 模型传递百分比。
> - raw duty 换算通过纯内部 helper 或测试专用 hook 验证，**不属于** host/wasm 仿真公开 ABI。

推荐**整数安全转换**：

```c
static uint32_t pwm_percent_to_raw(float percent, uint8_t bits)
{
    if (percent < 0.0f)   { percent = 0.0f; }
    if (percent > 100.0f) { percent = 100.0f; }

    uint32_t max_duty = (UINT32_C(1) << bits) - UINT32_C(1);
    return (uint32_t)((percent * (float)max_duty) / 100.0f);
}
```

> 还需要防止不安全移位：`bits` 必须在执行 `1u << bits` **前**完成合法性校验。

#### 6.1.7 Resolution 校验分两层

取消 "Codegen 认定 1–20 全部可用" 的说法，改为：

**Codegen（target-independent 校验）：**

- 必须为整数；
- `bool` 不接受；
- `0` 只允许通过省略或 `"auto"` 表达；
- 显式值应落在平台格式允许的宽泛边界内。

**Target（权威校验）：**

- `(freq_hz, requested bits, selected clock source)` 是否可实现。

> 如果构建时已知 board capability，Codegen 可以提前报错，但 PAL 仍**必须保留防御性校验**。

#### 6.1.8 DAL / PAL 类型边界

Servo public header 建议使用 DAL 自有类型：

```c
typedef uint8_t dal_servo_clock_requirement_t;

enum {
    DAL_SERVO_CLOCK_AUTO             = 0,
    DAL_SERVO_CLOCK_STABLE_REQUIRED  = 1,
};

typedef struct {
    const char                     *owner;
    uint8_t                         pwm_channel;
    uint8_t                         resolution_bits;
    dal_servo_clock_requirement_t   clock_requirement;
    float                           min_pulse_ms;
    float                           max_pulse_ms;
} dal_servo_config_t;
```

`dal_servo.c` 内部转换：

```c
static wink_status_t servo_map_pwm_config(
    const dal_servo_config_t *servo_cfg,
    pal_pwm_config_t         *out_pwm_cfg);
```

这样 App / device-tree 不需要包含 `pal_hal.h`。

> 字段排序需要根据最终 ABI 布局重新设计，**不能机械地"全部尾加"**。无论如何都应按 `ADR-0028` bump ABI。

#### 6.1.9 兼容性矩阵改写

| 场景                                       | 保证                                                        |
| ------------------------------------------ | ----------------------------------------------------------- |
| 旧 JSON 未含 advanced                      | 行为兼容，不需要迁移                                        |
| 旧源代码使用 zero / designated initializer | 重新编译后新字段为 AUTO                                     |
| 旧源代码逐字段赋值且未清零                 | 不保证；迁移要求先 `{0}` 初始化                             |
| 已编译 Binary SDK 消费者                   | POD 布局不兼容；必须 ABI++ 并成对升级头与 archive           |
| 旧 `pal_pwm_init(ch, freq)`                | 继续得到 13-bit / AUTO 的历史行为                           |
| host / wasm duty 观测                      | 保持百分比                                                  |
| Servo Flash wire v1                        | 继续固定 9 bytes；advanced 不可 Flash override              |
| Button Flash wire                          | N/A，当前无消费者                                           |
| `pull=NONE` 未注入电平                     | 明确返回 floating / disconnected，不假定 LOW                |
| stable clock 在不支持 target               | 按确定策略返回 `UNSUPPORTED`，或强制发降级 warning          |

---

### 6.2 建议重构实施计划

#### Task 0：基线和决策冻结

**不要**一开始就把 ADR 标为 Accepted。先完成：

- [ ] 冻结**唯一** JSON 形式：仅 `advanced.*`
- [ ] 冻结 clock guarantee / preference 语义
- [ ] 冻结 ESP32 effective clock source
- [ ] 冻结 Router profile 身份
- [ ] 冻结 resolution 校验分层
- [ ] 冻结 floating input 语义
- [ ] 冻结 ABI / SemVer 升级策略
- [ ] 确认 Flash wire v1 不扩展

并记录以下基线结果：

- host full；
- codegen golden；
- Wasm Button App；
- Wasm Servo App；
- ESP32 Button App；
- ESP32 Servo App；
- Binary SDK consumer smoke。

#### Task 1：ADR Accepted 与 Layer ① **同时**更新

修改：

- `decisions/0034-*.md`
- `02-wink-micro-os/01-dal-device-abstraction.md`
- `02-wink-micro-os/02-pal-platform-abstraction.md`
- `03-app-codegen/01-app-business-logic.md`
- `07-platform-governance/01-device-model-registry.md`
- 本技术设计
- 本实施计划

> `02-project-manifest-schema.md` 只有在未来 Project Manifest 也要承载 advanced 时才修改。

#### Task 2：ABI 与发布契约

- [ ] 更新 `wink-micro-os/VERSION`
- [ ] ABI generation bump
- [ ] 记录 public POD size / layout 变化
- [ ] Source SDK smoke
- [ ] Binary host SDK package + consumer smoke
- [ ] Binary wasm SDK package + consumer smoke
- [ ] 验证新头不能搭配旧 archive

另外应单独登记现有治理缺陷：

> `ADR-0028` 声称 `pal_hal.h` 不进入 Binary SDK，但当前 packer 会自动扫描 PAL include 树。应**单独修复或明确修订 ADR**，不能让本功能顺便扩大未定义的公开 PAL ABI。

#### Task 3：测试脚手架（先增加失败测试再实现）

- `test/CMakeLists.txt`
- `test_pal_pwm_config.c`
- `test_pal_pwm_router.c`
- `test_dal_button.c`
- `test_dal_servo.c`
- `host_test_ctrl.h`
- Codegen `unittest discover`
- advanced positive / negative fixtures
- generated-C compile test

#### Task 4：Profile-aware PWM Router

先实现 target-independent 部分：

- requested / effective profile；
- 完整 profile 比较；
- acquire / release / reset；
- percent-to-raw helper；
- 失败回滚。

> 这一步完成后再动 ESP32 target。

#### Task 5：三 target PWM

**Host / Wasm**

- 保存 requested / effective profile；
- 保留百分比 duty；
- 对 stable requirement 执行锁定的失败 / 降级规则；
- **不模拟不存在的高精度时钟**。

**ESP32**

- 解析 effective bits；
- 解析 effective clock source；
- Router acquire；
- LEDC timer 配置；
- LEDC channel 配置；
- 任一步失败都**完整回滚**；
- init 成功后才发布 channel effective state。

**建议状态发布顺序：**

```
validate
→ resolve effective profile
→ router acquire
→ configure timer
→ configure channel
→ commit channel state
```

**不能在硬件成功前写 `s_ch_bits[channel]`。**

#### Task 6：Button DAL

必须验证：

- 非法 enum 在 claim 前失败；
- AUTO × 两种 polarity；
- UP / DOWN / NONE × 两种 polarity；
- NONE 未注入；
- NONE 注入 HIGH / LOW；
- init 失败无资源泄漏；
- debounce / events / IRQ 回归。

#### Task 7：Servo DAL

必须验证：

- DAL 类型不泄漏 PAL；
- 默认 50Hz / 13-bit / AUTO 行为不变；
- explicit resolution；
- stable clock 错误传播；
- deinit 清理 effective state；
- Flash wire 仍为 9 bytes；
- override 不覆盖 advanced 字段。

#### Task 8：Codegen

建议只支持：

```json
"advanced": {
  "pull": "none"
}
```

以及：

```json
"advanced": {
  "resolution_bits": 10,
  "clock_requirement": "stable_required"
}
```

负测试至少包括：

- `advanced` 非 object；
- 未知键；
- `pull` 非字符串；
- 大小写错误；
- resolution 为 `bool`；
- resolution 为 float / string；
- resolution 越界；
- clock 非字符串；
- clock 未知值；
- deprecated alias 与 advanced 双写。

> 默认 golden 必须**字节级不变**；高级 fixture 应单独新增，**不修改默认 fixture**。

#### Task 9：完整验收

把当前"至少一个 ESP32 App"改成**分别覆盖两条路径**：

**Host**

```
python wink-tools/wink.py test --full
```

**Codegen**

```
python -m unittest discover \
  -s wink-micro-os/tools/codegen/tests \
  -p "test_*.py"
```

**Wasm**

```
oled_dashboard / 其他 Button App
avoidance_car / Servo App
```

**ESP32**

```
devkitc_smoke / 其他 Button App
avoidance_car / Servo App
```

**Binary SDK**

```
host package + external consumer smoke
wasm package + external consumer smoke
```

> HIL 可以保持可选，但报告中必须写"未执行 HIL"，**不能以构建通过代替硬件行为验证**。

---

## 七、评审签字条件（ADR-0034 Accepted 前置）

`ADR-0034` 只有满足以下条件才建议 **Accepted**：

- [ ] PWM Router 按**完整 effective profile** 管理 timer
- [ ] Clock policy 不再含"或、可选、如"等未决措辞
- [ ] `pull=NONE` 仿真 floating 语义明确
- [ ] DAL public API **不引用** PAL 类型
- [ ] JSON 只有**一种**规范表达
- [ ] Binary SDK ABI bump 已确定
- [ ] Flash wire 与 POD ABI 已明确区分
- [ ] Registry、真实 wink-app 规范和 Python plugin 的所有权闭环
- [ ] Layer ① 与 ADR 同步更新
- [ ] 测试矩阵覆盖 host / wasm / ESP32 / source / binary

> 达到这些条件后，本方案会从"方向正确但存在硬件风险"，变成一份真正可执行的专业级实施方案。

---

## 八、Safety Review 记录

- **Risk level：** High
- **Scope：** API / ABI、共享硬件 timer、资源生命周期、三 target 语义、Flash wire、codegen 和发布回滚
- **Findings：** 上述 P0 / P1
- **Fixed：** 无，仅评审
- **Assumptions：** 以当前工作树和 `ADR-0028` / `ADR-0012` 为准
- **Commands run：** 仅文件与代码事实检查，**无状态变更命令**

---

## 九、附录：证据索引

**代码事实（现有实现）：**

- Router：[`pal_pwm_router.c`](../../../../wink-micro-os/pal/src/pal_pwm_router.c)
- ESP32 PWM：[`pal_hal_pwm_esp32.c`](../../../../wink-micro-os/targets/esp32/pal_hal_pwm_esp32.c)
- Host PWM：[`pal_hal_host.c:498-508`](../../../../wink-micro-os/targets/host/pal_hal_host.c#L498-L508)
- Wasm PWM：[`pal_hal_wasm.c:136-148`](../../../wink-micro-os/targets/wasm/pal_hal_wasm.c#L136-L148)
- Button DAL：[`dal_button.h`](../../../../wink-micro-os/dal/include/input/dal_button.h)、[`dal_button.c`](../../../../wink-micro-os/dal/src/input/dal_button.c)
- Servo DAL：[`dal_servo.h`](../../../wink-micro-os/dal/include/actuator/dal_servo.h)、[`dal_servo.c`](../../../wink-micro-os/dal/src/actuator/dal_servo.c)
- 版本：[`wink-micro-os/VERSION`](../../../../wink-micro-os/VERSION)
- 测试驱动：[`wink.py:627-646`](../../../../wink-tools/wink.py#L627-L646)、[`test_golden.py`](../../../../wink-tools/tools/codegen/tests/test_golden.py)

**规范 / ADR：**

- [`ADR-0028` Host Binary ABI Toolchain Contract](../../decisions/core/0028-host-binary-abi-toolchain-contract.md)
- [`ADR-0012` Contract Honesty over Silent Degradation](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)
- [`ADR-0001` 负数错误码约定](../../decisions/core/0001-error-code-sign-convention.md)
- [`ADR-0004` 静态分发](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md)
- [`.claude/rules/docs-adr.md`](../../../../.claude/rules/docs-adr.md)
- [`03-app-codegen/01-app-business-logic.md`](../../design/03-app-codegen/01-app-business-logic.md)
- [`07-platform-governance/01-device-model-registry.md`](../../design/07-platform-governance/01-device-model-registry.md)

---

> **备注**：本 Review 由 Claude Code 会话（`2026-07-17-091719-*.txt`）整理归档；对话曾因平台限流中断，最终结论已整合完毕。

