# DAL / BAL 分层架构评审

**评审日期**：2026-07-28
**评审对象**：
- `docs/design/02-wink-micro-os/01-dal-device-abstraction.md`（DAL 设计规范）
- `docs/design/02-wink-micro-os/06-bal-layer.md`（BAL 设计规范）
- `docs/design/02-wink-micro-os/03-directory-architecture.md`（内核目录架构）
- 实际代码：`wink-micro-os/{dal,bal,pal}/`

**评审视角**：资深嵌入式架构师，静态分发范式（ADR-0004）
**评审方法**：逐条将设计规范与现有代码库落地做核对，区分「已落地健康部分」与「尚未落地的风险方向」
**关联决策**：ADR-0004（静态分发）、ADR-0017（阻塞 API 隔离）、ADR-0023/0032/0037/0038（BAL 分层与命名）、ADR-0024（Deinit 铁律）、ADR-0026（FOC 电机分层，roadmap）

---

## 一、总体评价

这套分层是嵌入式低代码平台里少有的、纪律性极强的架构：五层模型（App/BAL/DAL/PAL/targets）清晰、依赖单向趋稳、静态分发 + Codegen 代偿运行期多态、CI 门禁可机械验证。

**核心判断**：当前**已落地部分（LED / button / servo / ultrasonic / chassis / closed_loop_motor 等）非常健康**，分层纪律执行到位。真正的风险几乎全部集中在**尚未落地的电机 / FOC 方向**（ADR-0026 / ADR-0037 的交汇处），那里存在三处设计模糊：ISR 分层归属、仿真物理引擎落点、control 闭环测试代偿。

建议：在正式开工 FOC / 闭环电机之前，先用 ADR 把这三点钉死，避免在错误地基上施工再返工。

---

## 二、做得非常好的地方（值得保留）

1. **依赖方向与仓库边界干净**
   `pal(INTERFACE) ← dal ← runtime/trace ← BAL/App`，且 BAL 独立仓库、只 link 公共头面。这让「预编译 `.a` + 每项目只重编 BAL/app」的云端策略成立，是真金白银的价值。

2. **静态分发替代 vtable（ADR-0004）**
   对 AI 代码生成友好、消除 Wasm `call_indirect` 开销、断点调试直观。代偿分析（OCP→Codegen 设备树、容器遍历→静态展开、热插拔→Non-goal）诚实且到位。

3. **`actuator` vs `control` 判定口诀**
   「单器件开环/便利增强 → actuator；跟目标、用反馈、跨器件 → control」非常清晰，杜绝了「helper / controller 满天飞」的常见腐化。

4. **阻塞 API 三层硬隔离（ADR-0017）**
   `WINK_BLOCKING` 警告 + `#ifndef WINK_STRICT_NONBLOCKING` 符号剔除 + 运行期 assert，是对「AI 会绕过编译警告」的精准防御，红线写得对。

5. **Deinit 10 项清场铁律 + Bus-Owner 静态总线模型（ADR-0024）**
   把「SSD1306 销毁总线导致 EEPROM 崩」这类真实血泪坑，用拓扑序生命周期根治了。

---

## 三、补充建议（按优先级）

### 🔴 P0-1：BAL/control 的 host 单测缺 DAL 测试替身（test double）

**问题**：`wink_closed_loop_motor` / `wink_chassis` 依赖真实 `dal_motor_t` / `dal_encoder_t`。math 层有纯算单测，但**闭环控制逻辑（fail-safe、实测 dt、anti-windup）的验证路径不清晰**。运行期多态天然可注入 mock，静态分发丢了这个能力却没看到对应代偿。

**性质**：不是「静态分发的锅」，而是**测试策略缺口**。math 层已做纯算单测代偿，唯独 control 闭环没有。

**建议**：在 `test/stubs/` 提供**可编译期替换的 DAL fake**（host 变体 `dal_motor` 记录 duty、`dal_encoder` 可注入 count），让 control 闭环能在 host 上做「阶跃响应 / 反馈超时 → 制动」的确定性测试。思路与现有 `test/stubs/host_test_ctrl.c` 一脉相承。否则 BAL §4.2 的 fail-safe 契约只是纸面承诺。

**代价/收益**：代价小、收益大——fail-safe / anti-windup 恰恰是最容易写错、最难靠仿真复现的逻辑。

### 🔴 P0-2：FOC ISR 的分层归属存在真实张力

**问题**：DAL §8.1 说 SimpleFOC 的 10kHz+ 硬中断计算「归入 BAL 级别」，但：
- BAL 公共头禁 `pal_*`（除 `pal_log.h`）
- BAL 定位是「可复用纯业务逻辑」

而 10kHz ISR + 硬件定时器触发**本质是平台强相关的**，塞进 BAL 会同时违反上述两条既定红线。

**本质**：FOC 有两块，文档没把这刀切下去：
- **实时环**（Clarke / Park / SVPWM，硬中断上下文）→ 平台强相关，应属 DAL，甚至需 PAL 暴露 `pal_hwtimer` + ISR 注册原语
- **参数环**（目标速度 / 位置，50Hz 协作循环）→ 才是 BAL/control

**注意**：FOC 目前是 roadmap（ADR-0026 未落地），**不是现存漏洞**，而是「真正做 FOC 前必须先补的边界决策」。现在不写代码是对的。

**建议**：动 FOC 前先补一条 ADR，钉死前后台切分落点，别留「归入 BAL 级别」这种模糊措辞。

### 🟡 P1-1：DAL 层内 `#ifdef SIMULATION` 与「同源」承诺的持续侵蚀风险

**问题**：`dal_ultrasonic.c` 是「trigger + echo 脉宽」最低层旁路的教科书级正面样板。但 DAL §8.3 的电机动力学差分方程（`dal_motor_physics_update`）直接写在 `#ifdef SIMULATION` 里——**这是仿真专属物理引擎，不是「旁路最低物理信号」**，范围明显比超声波大。

**风险**：一旦这块照抄进 `dal_motor.c` 的 `#ifdef SIMULATION`，就开了「DAL 里塞仿真物理引擎」的口子，bypass 范围会随器件复杂度单调膨胀，最终 DAL 变成「两套代码」。

**建议**：趁电机仿真尚未落地，立规矩——仿真物理模型收敛到 `targets/common/wink_sim_physical.c`（已存在），DAL 只留一个 `pal_*` 级别的信号注入点。

### 🟡 P1-2：`communication` / `storage` 分类命名不一致

**问题**：DAL 用全称 `communication/`，BAL 用缩写 `comm/`。跨层 grep 与 codegen 模板会踩坑。

**建议**：统一命名（倾向都用 `comm`，与 BAL 现状一致、改动面小）。属卫生问题，下次碰这些目录时随手收掉即可，不值得单独开工。

### 🟢 P2：Capability 别名映射（DAL §8.2）缺编译期唯一性校验

**问题**：`#define left_wheel_set_speed(...)` 这类宏别名，若 codegen 对同一逻辑名生成两次、或 capability 与实际驱动 API 不匹配，是**宏展开静默错误**，比类型错误更难查。

**建议**：在生成的 `device_tree.h` 里加 `_Static_assert` 校验 capability ↔ 驱动类型一致性。

---

## 四、需要 Owner 确认的设计事实问题

按 CLAUDE.md 规则，评审中发现一处**业务事实不够完善**，需 Owner 拍板：

**PAL 目前没有通用硬件定时器 / ISR 注册契约**——只有 `pal_gpio_pulse_in`（`pal/include/hal/pal_hal.h:276`）和 ESP32 专用 `pal_rmt`。但 DAL §8 的 FOC 场景明确需要「硬件定时器触发的硬中断」。

这意味着二选一，且**当前该边界是空的**：
- **(a)** 补 PAL `pal_hwtimer_*` 契约（进公共契约面）
- **(b)** FOC ISR 走 target 私有代码、不进 PAL

建议与 P0-2 的 ADR 一并裁决。

---

## 五、优先级汇总

| 编号 | 建议 | 严重度 | 时机 |
|------|------|--------|------|
| P0-1 | control 层 DAL 测试替身 + 闭环 fail-safe 单测 | 🔴 | 尽快，独立可做 |
| P0-2 | FOC ISR 分层边界 ADR（含 PAL 定时器契约） | 🔴 | 开工 FOC 前必做 |
| P1-1 | 仿真物理引擎移出 DAL，收敛到 targets/common | 🟡 | 电机仿真落地前立规矩 |
| P1-2 | 统一 comm / communication 命名 | 🟡 | 卫生项，随手收 |
| P2   | Capability 别名映射加 `_Static_assert` | 🟢 | codegen 演进时 |

**一句话结论**：地基健康，风险集中在未落地的 FOC/电机方向。三处模糊（ISR 归属、仿真物理落点、闭环测试代偿）应在正式开工前用 ADR 钉死。
