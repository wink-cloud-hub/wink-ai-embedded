# CMS8S78xx 示例防假绿与外设物理行为闭环实施计划（第一阶段）

| 项 | 内容 |
|---|---|
| 编号 | `PLAN-20261010-MCS51-ANTI-FALSE-GREEN-PHASE1` |
| 日期 / 状态 | 2026-10-10 / Completed (已完成并通过验收) |
| 版本 | v1.0：聚焦温度传感器与 Timer2 比较器两项标杆假绿治理 |
| 技术方案 | [防假绿技术方案（第一阶段）](../../zh/tech-designs/mcs51/2026-10-10-cms8s78xx-anti-false-green-phase1-design.md) |
| 审查依据 | [假绿与完整性审查](../../reviews/mcs51/2026-10-09-cms8s78xx-false-green-and-framework-completeness-review.md)（F4、F5） |
| 交付报告 | [防假绿一期评审报告](../../reviews/mcs51/2026-10-10-cms8s78xx-anti-false-green-phase1-review.md) |
| 前序基线 | [可信构建基线执行审查](../../reviews/mcs51/2026-10-09-cms8s78xx-trustworthy-build-baseline-review.md)（`REV-20261009-CMS8S78XX-BUILD-BASELINE`） |
| 核心目标 | 补齐 Timer2 比较输出引脚驱动与温度传感器物理模拟，升级为强因果断言，并建立反向变异红灯防御测试 |

---

## 1. 阶段目标与准出门禁

| 阶段 | 治理对象 | 核心操作 | 验收出口门禁 | 状态 |
|---|---|---|---|:---:|
| **S0** 准备与基线核验 | 环境与目标清单 | 冻结当前 Git 状态，核验工具链环境，保存待治理 App 快照 | 两个目标 App（`timer2_compare_mode`, `temperture_sensor`）输入身份明确 | **[x] 已完成** |
| **S1** Timer2 Compare 闭环 | `timer2_compare_mode` | 模型补齐 CC0~CC3 引脚翻转，场景升级为断言 P0.0（CC0）硬件方波，加入比较器失效变异测试 | P0.0 500Hz 方波断言通过；变异测试（预期 2000Hz）真实报错退出码 1 | **[x] 已完成** |
| **S2** 温度传感器物理闭环 | `temperture_sensor` | 模型实现动态温度转换与 Trim 结合计算，场景实现温度注入与因果断言，加入温度异常变异测试 | 物理温度解算链路闭环；变异测试（预期未收敛）真实报错退出码 1 | **[x] 已完成** |
| **S3** 汇总回归与交付 | 全局回归与凭据 | 运行基线门禁、单测回归、许可检查与架构 Lint，归档审查记录 | 71/71 单测通过、无 Lint 违规、正反用例凭据齐备 | **[x] 已完成** |

---

## 2. 详细任务拆分

### S0 — 准备与基线核验
- [x] 核验前序基线 `artifacts/cms8s78xx-baseline/20261009-185724-44ea8be9/` 凭据完备性。
- [x] 确认工作目录状态，确保仅针对本次治理目标开展最小必要修改。
- [x] 确认工具链：Python 3.11、Emscripten、SDCC 4.6.2、Wasm 仿真运行时可用。

### S1 — Timer2 Compare 模型物理行为与强因果断言闭环 (解决 F5)
- [x] **模型增强**：在 `wink-micro-os/frameworks/mcs51/chips/cms8s78xx/src/cms8s_timer.cpp` 的 `on_timer2_compare_match()` 中补齐硬件引脚驱动逻辑：
  - 检查通道模式是否为比较输出（`TMR2_MODE_COMPARE`）；
  - 检查 IO 复用状态（`P00CFG == GPIO_P00_MUX_CC0` 等）；
  - 触发对应物理引脚（P0.0 / CC0, P0.1 / CC1, P1.5 / CC2, P1.4 / CC3）电平翻转，同步至 UniSim 事件流。
- [x] **断言升级**：重构 `wink-micro-app/vendor/cms8s78xx/timer2_compare_mode/unisim-scenarios/timer2_compare_mode.scenario.json`：
  - 增加对物理引脚 `pin: 0`（P0.0 / CC0）的 `ASSERT_WAVEFORM` 断言，验证其在 500µs 比较点产生的精确 500Hz 硬件方波；
  - 彻底解耦对 P32 溢出方波的寄生依赖。
- [x] **真实构建与正向回归**：使用 `wink.py build sim` 重新构建 assets 并执行 `wink.py sim run --mode headless`，验证新场景 100% 通过。
- [x] **反向变异测试 (Anti-Defense)**：编写并运行变异测试（`timer2_compare_mode.fail.scenario.json` 预期 2000Hz），验证场景可靠拦截并退出码 1 报错。

### S2 — 温度传感器物理温度注入与模型闭环 (解决 F4)
- [x] **模型增强**：在 `wink-micro-os/frameworks/mcs51/chips/cms8s78xx/src/cms8s_adc.cpp` 中升级 `ADC_CH_63_TS` 通道：
  - 接入动态物理温度变量与场景注入支持（`js_pal_adc_read_norm(63)` 与 `ctx->adc_injected[63]`）；
  - 按照标称转换公式 $V_{ts} = 0.909 + T \times 0.0035 + (trim - 8) \times 0.0051$ 动态合成 ADC 采样值。
- [x] **断言升级**：重构 `wink-micro-app/vendor/cms8s78xx/temperture_sensor/unisim-scenarios/temperture_sensor.scenario.json`：
  - 增加标称 25℃ 物理电压注入阶跃；
  - 增加强因果断言，验证校准前状态（10ms P3.2=1）、校准收敛行为（100ms P3.2=0）与持续采样的动态响应。
- [x] **真实构建与正向回归**：使用 `wink.py build sim` 重新构建 assets 并执行 `wink.py sim run --mode headless`，验证新场景 100% 通过。
- [x] **反向变异测试 (Anti-Defense)**：编写并运行变异测试（`temperture_sensor.fail.scenario.json` 预期 100ms 未收敛），验证场景可靠拦截并退出码 1 报错。

### S3 — 汇总回归与交付
- [x] 运行 MCS-51 框架工具单元测试（71/71 项通过）。
- [x] 运行开源许可地图门禁：`python .github/scripts/check_license_map.py`。
- [x] 运行分层架构门禁：`python wink-tools/wink.py lint --pack layering --pack api`。
- [x] 编写并归档交付审查报告：`docs/reviews/mcs51/2026-10-10-cms8s78xx-anti-false-green-phase1-review.md`。
- [x] 按原子提交规范完成代码与文档提交。

---

## 3. 风险与控制准则

1. **原厂代码零篡改原则**：严禁为了让断言通过而修改 vendor demo 中的任何 C 代码（`main.c`, `demo_*.c`, `isr.c`）。
2. **变异击穿硬指标**：任何被称为“已消除假绿”的用例，必须提供至少 1 组能将该场景击穿变红的变异反例。
3. **向后兼容**：底座外设模型更新不得破坏全量 37 App 的 SDCC 硬件门禁和既有其他 36 个场景的回归。
