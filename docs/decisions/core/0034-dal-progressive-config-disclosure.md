# ADR-0034：DAL/PAL 渐进披露配置（完整能力 + 通用默认）

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳）** |
| 日期 | 2026-07-16（Accepted 2026-07-17） |
| 触发 | Button 上下拉与 `active_low` 绑死、PWM 固定 13-bit/`LEDC_AUTO_CLK` 且无 escape hatch；需在专业可扩展与低代码/AI 简洁性之间取得平衡 |
| 影响范围 | PAL GPIO/PWM API 与 **Binary SDK ABI**；PWM 共享 timer 资源模型；DAL button/servo config；三 target（host/wasm/ESP32）语义；codegen schema（可选 advanced）；设计规范 `02-wink-micro-os` / `03-app-codegen` / `07-platform-governance` |
| 决策者 | 项目 Owner（2026-07-17 确认执行 Task 1；ABI 锁定 `0.2.0` / `ABI=2`） |
| 关联 ADR | [ADR-0002](../unisim/0002-dual-target-compilation.md)、[ADR-0003](../unisim/0003-simulation-fidelity-boundary.md)、[ADR-0004](0004-static-dispatch-vs-runtime-ops.md)、[ADR-0008](0008-dynamic-device-tree-config-flash.md)、[ADR-0012](0012-contract-honesty-over-silent-degradation.md)、[ADR-0028](0028-host-binary-abi-toolchain-contract.md)、[ADR-0031](0031-button-event-drive-config.md) |
| 关联技术设计 | [tech-designs/2026-07-16-dal-progressive-config-disclosure.md](../../zh/tech-designs/core/2026-07-16-dal-progressive-config-disclosure.md) |
| 关联实施计划 | [implementation-plans/2026-07-16-dal-progressive-config-disclosure-plan.md](../../implementation-plans/core/2026-07-16-dal-progressive-config-disclosure-plan.md) |
| 关联评审记录 | [reviews/2026-07-17-dal-progressive-config-disclosure-review.md](../../reviews/core/2026-07-17-dal-progressive-config-disclosure-review.md)（Major Revision） |
| 关联设计规范（Accepted 后回写） | `02-wink-micro-os/01-dal-device-abstraction.md`、`02-wink-micro-os/02-pal-platform-abstraction.md`、`03-app-codegen/01-app-business-logic.md`、`07-platform-governance/01-device-model-registry.md` |

---

## 背景（Context）

当前实现：

1. **Button**：`active_low == true` → `PAL_GPIO_INPUT_PULLUP`，否则 `PULLDOWN`。无法表达「外置上拉 + 禁片内上拉」或「浮空输入」。
2. **PWM**：`pal_pwm_init(ch, freq_hz)` + ESP32 固定 `LEDC_TIMER_13_BIT` + `LEDC_AUTO_CLK`。对 50Hz 舵机够用，但无法按器件/场景选择分辨率或时钟策略；占空比换算写死 8191。

产品定位是 AI 低代码 + 双 target，**不应**把 APB/XTAL Hz、外拉阻值等电气细节摊进默认 `wink-app.json`。同时作为平台，又需要**可 override 的完整能力**，避免特殊板卡旁路 DAL。

---

## 方案比选（Options）

### 方案 A：渐进披露（采纳）

- C/PAL/DAL：**完整可选字段**，零值 / `AUTO` 哨兵 = 今日行为。
- JSON/L1：**默认不生成、不展示**高级字段；仅在显式书写时 codegen 烘焙。
- **禁止**把系统时钟 Hz、外拉阻值 Ω 放进 App 配置面。

### 方案 B：维持绑死默认，文档说明限制

短期零成本；长期特殊硬件会 fork / `#ifdef`，否决作为终态。

### 方案 C：JSON 一等暴露全部细节

专业表面完整，但 AI 易乱填、双 target 仿真契约虚化、测试矩阵爆炸——否决。

---

## 决策结论（Decision）

**采纳方案 A 的总体方向**（AUTO=0 + 默认不向 AI 暴露 + 专家 escape hatch）。经 2026-07-17 评审（Major Revision），**在 Accepted 前**追加锁定以下硬约束，避免真实硬件行为错误、Binary SDK ABI 破坏与仿真假阳性：

1. **分层**  
   - L1（低代码/AI）：语义字段 only（button: `pin`/`active_low`；servo: `pwm_channel`/`min_pulse_ms`/`max_pulse_ms`）。  
   - L2（专家 escape）：**唯一**规范表示为 `advanced.*` 对象；**取消顶层 `pull` 双写**。若为历史兼容保留顶层 `pull`，仅作一个版本周期的 deprecated alias，且**只要同时出现 `pull` 与 `advanced.pull` 即报错（不比较取值是否相同）**，codegen 永不生成顶层形式。  
   - PAL/DAL C API：始终可表达完整配置。

2. **Button `pull`**  
   - 存储 `typedef uint8_t dal_button_pull_t`（固定宽度，布局明确）；取值 `AUTO=0 | UP=1 | DOWN=2 | NONE=3`。  
   - `AUTO` 推导：`active_low` → UP，否则 DOWN（**保持今日行为**）。`active_low` 仅表示逻辑极性，与电气上下拉解耦。  
   - **非法 `pull` 值必须在 `pal_resource_claim()` 之前返回 `WINK_ERR_INVALID_ARG`**（避免半占用资源）。  
   - **`pull=NONE` 不隐含任何 idle 电平**：host/wasm 未注入外部电平时读取**不得**默认判为 LOW；须返回明确的 disconnected/floating 语义，或 Button `poll` 不推进去抖并告警。

3. **PWM**  
   - 保留 `pal_pwm_init(channel, freq_hz)` 为薄包装（向后兼容，得到 13-bit + AUTO 历史行为）。  
   - 新增 `pal_pwm_init_ex(channel, const pal_pwm_config_t *)`：`freq_hz` + `resolution_bits`（0=AUTO）+ `clock_requirement`（`AUTO=0` | `STABLE_REQUIRED=1`）。**弃用含糊的 `FIXED` 命名**。  
   - **时钟契约诚实（ADR-0012）**：`STABLE_REQUIRED` 在 ESP32（classic）映射为 **`LEDC_USE_REF_TICK`**（DFS-stable，1 MHz）；不可映射/越界 → `WINK_ERR_INVALID_ARG`；`ledc_timer_config` 失败 → `WINK_ERR_HARDWARE` 并撤销 router；host/wasm 一律 `WINK_ERR_UNSUPPORTED`。禁止「偏好/可选/如 REF_TICK」类未决措辞。Light-sleep keep-alive（`RC_FAST`+`KEEP_ALIVE`）为本 ADR **Non-goal**。如确需 best-effort，另立 `STABLE_PREFERRED` 并规定统一、可计数、强制的降级告警。  
   - **PWM timer 资源模型按完整 effective profile**：`freq_hz` + effective `resolution_bits` + effective `clock_source` 三者组成 timer 身份；**由 target 先把 AUTO 解析为 effective profile 再交给 Router**。仅完整 profile 相同才可共享 timer；同 channel 不同 profile → `WINK_ERR_BUSY`；无空闲 timer → `WINK_ERR_RESOURCE_EXHAUSTED`；硬件配置失败须**完整撤销 Router acquire**，不留半初始化态。仅缓存 per-channel bits **不足以**避免同频不同 bits 复用同一 timer 导致的占空比错误。  
   - 占空比公共语义**始终为百分比**：仅 **ESP32** 按该通道 timer 的 effective resolution 换算为 raw duty（换算前必须已校验 `bits`，防不安全移位）；**host/wasm 保持百分比观测**（不改 `sim_last_pwm_duty` / JS bridge 语义）。禁止写死 8191。  
   - resolution 两层校验：codegen 做 target-independent 结构/类型/宽泛边界校验；**target 保留权威校验** `(freq, bits, clock)` 硬件可实现性。  
   - **不**暴露 `system_clock_hz` / APB / XTAL / LEDC speed mode / 外拉阻值 到 JSON。

4. **DAL 不泄漏 PAL 类型**  
   - `dal_servo_config_t` 不得出现 `pal_*` 类型；DAL 定义自有语义枚举（如 `dal_servo_clock_requirement_t`），由 `dal_servo.c` 内部映射到 PAL。App / device_tree 不因此被迫 include `pal_hal.h`。

5. **Binary SDK ABI（ADR-0028）**  
   - Button/Servo 公共 POD 增字段是**源码兼容但二进制不兼容**：必须按 ADR-0028 bump SemVer 与 `ABI=`。本波次 Owner 锁定：**`0.2.0` / `ABI=2`**（pre-1.0：布局破坏用 0.x MINOR + ABI++；字面 MAJOR=`1.0.0` 对本波次过重）。新头文件与新 `.a` 成对发布，禁止新头搭配旧 archive。  
   - 「零值兼容」仅对指定初始化器成立；对外部消费者未 `{0}` 清零的逐字段赋值**不保证**——迁移须要求先 `{0}`。

6. **Flash wire 与 POD ABI 分离**  
   - Servo override 是显式 **9-byte wire**（`dal_servo.c`），不依赖 `sizeof(config)`：本次 wire v1 保持 9 bytes，**advanced 字段不参与 Flash override**，**无需 wire version bump**；Button 当前无 override，记为 N/A。wire 兼容 ≠ POD ABI 兼容，二者分开描述。

7. **SSOT 所有权闭环**（取消「Registry 更新可选」）  
   | 层 | 所有权 |
   |----|--------|
   | Device Model Registry / wink-app schema | 字段名、类型、范围、L1/L2 可见性、迁移规则 |
   | Codegen Python plugin | 输入校验、alias 处理、字符串→C 枚举映射、条件发射 |
   | DAL | 器件语义配置 → PAL 配置映射，不暴露 PAL 类型 |
   | PAL target | AUTO → effective config 解析、frequency/resolution/clock 硬件可实现性权威校验 |
   | C 中 `AUTO=0` | 运行期默认行为的最终权威定义；文档/Registry 必须与其一致 |
   - 真正的 `wink-app.json` 规范主要在 `03-app-codegen/01-app-business-logic.md`（非 `02-project-manifest-schema.md`）。

8. **仿真诚实（ADR-0003 / 0012）**  
   - host/wasm 对 `pull` / `clock_requirement` / `resolution` 以语义或可观测 stub 实现；无法保真的细节明确 `UNSUPPORTED` 或统一告警，**不得静默假装**已生效。

9. **非目标（本 ADR）**  
   - 外拉阻值建模、LEDC high-speed mode、任意 SoC 时钟树、把 advanced 做成前端一等可视化控件（可后置）。

---

## 后果与约束（Consequences & Constraints）

- 现有 App / **默认 golden 字节级不变**（未写 advanced 时）。  
- `dal_button_config_t` / `dal_servo_config_t` / `pal_pwm_config_t` 扩展 = ABI 变更，须按 ADR-0028 bump（见决策 §5）。  
- Router 签名从 `(channel, freq_hz, out_timer)` 改为按 profile：`pal_pwm_router_acquire(channel, const pal_pwm_timer_profile_t*, out_timer)`。  
- Codegen：省略字段不发射 C 指定初始化中的 advanced 行（依赖 C 零初始化 / 显式 `= AUTO`）。  
- 治理缺口登记：ADR-0028 声称 `pal_hal.h` 不进 Binary SDK，但 `wink-micro-os/tools/pack_sdk_binary.py` 当前 auto-scan `pal/include`，会打包 `pal_hal.h`——须单独修复 packer 白名单或修订 ADR-0028，不得借本功能扩大未定义的公开 PAL ABI。

---

## 遵循与后续（Compliance & Follow-up）

1. **ADR Accepted 与 Layer ① 回写为同一批变更**（docs-adr 规则，不得先 Accepted 后补文档）。  
2. 按实施计划执行；**host full 全绿 + 默认 golden 字节级不变 + Binary SDK 成对升级**为合入门槛。  
3. 前端 workbench 对 `advanced` 的 UI 暴露列为可选后续，不阻塞本 ADR。

### Accepted 前置（设计锁定 vs 实现跟踪）

**设计已锁定（Task 0/1，可 Accepted）：**

- [x] Clock policy 无「或/可选/如」未决措辞（`STABLE_REQUIRED` → ESP32 `LEDC_USE_REF_TICK`；host/wasm `UNSUPPORTED`）  
- [x] `pull=NONE` 仿真 floating 语义明确（`WINK_ERR_DISCONNECTED`）  
- [x] DAL public API **不引用** PAL 类型（设计契约）  
- [x] JSON 只有**一种**规范表达（`advanced.*`）  
- [x] Binary SDK ABI bump 已确定（**`0.2.0` / `ABI=2`**；VERSION 文件在实施计划 Task 2 落地）  
- [x] Flash wire 与 POD ABI 已明确区分（wire v1=9B，advanced 不进 Flash）  
- [x] Registry / 真实 wink-app 规范 / Python plugin 的所有权闭环（见 Layer ① 回写）  
- [x] Layer ① 与 ADR 同步更新（本 Accepted 同批）

**实现跟踪（实施计划 Task 2–9，合入门槛；不阻塞本 ADR Accepted）：**

- [ ] PWM Router 按**完整 effective profile** 管理 timer（Task 4–5）  
- [ ] 测试矩阵覆盖 host / wasm / ESP32 / source / binary（Task 9）  

---

*本 ADR 状态变更请在此记录：*
- 2026-07-16：Proposed（会话结论：渐进披露优于绑死默认与全量 JSON 暴露）
- 2026-07-17：Major Revision（评审 [`reviews/2026-07-17-*`](../../reviews/core/2026-07-17-dal-progressive-config-disclosure-review.md)）——追加 P0/P1 硬约束
- 2026-07-17：Task 0 冻结——ESP32 `STABLE_REQUIRED`=`LEDC_USE_REF_TICK`；floating=`WINK_ERR_DISCONNECTED`；ABI 预留 `0.2.0`/`ABI=2`
- 2026-07-17：Accepted（Owner 确认执行 Task 1；ABI 锁定 `0.2.0`/`ABI=2`；Layer ① 同批回写；实现跟踪见实施计划）

