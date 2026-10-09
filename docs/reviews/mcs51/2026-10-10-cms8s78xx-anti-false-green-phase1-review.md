# CMS8S78xx Anti-False-Green Phase 1 评审与验收报告

- **评审对象**: CMS8S78xx 标杆外设防假绿改造（Phase 1: `timer2_compare_mode` 与 `temperture_sensor`）
- **依据规范**: [TECH-20261010-CMS8S78XX-AFG-PHASE1](../../zh/tech-designs/mcs51/2026-10-10-cms8s78xx-anti-false-green-phase1-design.md)
- **执行计划**: [PLAN-20261010-MCS51-ANTI-FALSE-GREEN-PHASE1](../../implementation-plans/mcs51/2026-10-10-cms8s78xx-anti-false-green-phase1-plan.md)
- **评审结论**: **PASSED (合格交付)**
- **评审日期**: 2026-10-10

---

## 1. 交付目标回顾与验收矩阵

本阶段针对早期审查报告中的两大弱断言 / 假绿隐患（F5 与 F4）实施物理行为闭环与双向变异防线验证：

| 标杆用例 | 原始假绿缺陷 | 改造后真实物理闭环 | 阳性用例状态 | 反向变异拦截验证 (`.fail.scenario.json`) | 验收结论 |
|---|---|---|:---:|:---:|:---:|
| `timer2_compare_mode` | 仅断言 P3.2 定时器溢出方波，未验证 CC0 比较匹配硬件波形 (F5) | 补齐 `T2CM=0` 硬件翻转逻辑，通过 `js_pal_gpio_write(0, ...)` 驱动 CC0 (P0.0)，断言 500Hz 精确方波 | 🟢 PASS (500Hz) | 🔴 FAIL (预期 2000Hz 真实拦截，退出码 1) | **PASSED** |
| `temperture_sensor` | 仅断言 P3.2 输出 0（弱断言点灯），温感 ADC 未解算物理输入 (F4) | 补齐 `ADC_CH_63_TS` 对物理归一化刺激 `js_pal_adc_read_norm(63)` 与单元注入的动态解算，增加时序因果断言 | 🟢 PASS (4 步因果) | 🔴 FAIL (预期 100ms 仍为 1 真实拦截，退出码 1) | **PASSED** |

---

## 2. 关键设计与实现细节

### 2.1 Timer2 Compare Mode 硬件翻转闭环
- **底层驱动**: 在 [wink-micro-os/frameworks/mcs51/chips/cms8s78xx/src/cms8s_timer.cpp](../../../wink-micro-os/frameworks/mcs51/chips/cms8s78xx/src/cms8s_timer.cpp) 的 `on_timer2_compare_match` 中，实现了 Mode 0 (`T2CM == 0`) 硬件电平自动翻转：
  - 映射 CC0 -> P0.0 (pin 0)、CC1 -> P0.1 (pin 1)、CC2 -> P1.5 (pin 13)、CC3 -> P1.4 (pin 12)；
  - 每次匹配事件切换引脚电平并通过 `js_pal_gpio_write()` 通知 UniSim 虚拟总线；
- **强因果断言**:
  - `timer2_compare_mode.scenario.json`: 增加 `ASSERT_WAVEFORM` on pin 0 (500Hz 方波)；
  - `timer2_compare_mode.fail.scenario.json`: 设定变异预期 2000Hz，验证拦截器有效性。

### 2.2 片内温度传感器 (TS) 动态物理激励与时序闭环
- **底层驱动**: 在 [wink-micro-os/frameworks/mcs51/chips/cms8s78xx/src/cms8s_adc.cpp](../../../wink-micro-os/frameworks/mcs51/chips/cms8s78xx/src/cms8s_adc.cpp) 中，重构 `ADC_CH_63_TS` 逻辑：
  - 支持 `ctx->adc_inject_flag[63]` 单测注入与 `js_pal_adc_read_norm(63)` 场景物理注入；
  - 根据输入电压 $V_{ts}$ 动态生成基准 ADC 读数，并在 `TS_REG` 微调寄存器驱动下进行 16 档扫描匹配；
- **时序因果断言**:
  - `temperture_sensor.scenario.json`: 在 0us 注入标称 25℃ 物理电压 (0.333252 Norm)；在 10ms (微调未完成) 断言 P3.2 为高电平 1；在 100ms (微调收敛) 断言 P3.2 为低电平 0；
  - `temperture_sensor.fail.scenario.json`: 预期 100ms 仍为高电平 1，验证断言拦截力。

---

## 3. 全局质量门禁验证

1. **原厂固件零篡改校验 (Upstream Content Lock)**:
   - `python wink-micro-os/frameworks/mcs51/tools/audit_vendor_mirror.py`
   - **结果**: 148 文件全部校验通过，findings: 0，原厂 C 代码 0 侵入。
2. **单元测试回归**:
   - `python -m unittest wink-micro-os/frameworks/mcs51/test/core/test_audit_vendor_mirror.py wink-micro-os/frameworks/mcs51/test/core/test_run_cms8s78xx_baseline.py`
   - **结果**: 71 tests in 2.568s, 100% PASS。
3. **分层与 API 门禁**:
   - `python wink-ai/packages/wink-tools/wink.py lint --pack layering --pack api`
   - **结果**: 0 findings, 100% Clean。
4. **开源许可证地图门禁 (ADR-0083/0084)**:
   - `python .github/scripts/check_license_map.py`
   - **结果**: Satisfied (LGPL-3.0-only / Apache-2.0 / GPL-3.0-only 分层合规)。

---

## 4. 后续建议 (Next Phase)

本标杆验证已成功确立“真实物理闭环 + 双向变异防御”的防假绿规范模式。建议下一阶段（Phase 2）扩展至：
1. `epwm_*` 复杂互补 PWM、死区配置及硬件故障刹车的波形级断言闭环；
2. `wdt` 看门狗硬件复位因果闭环。
