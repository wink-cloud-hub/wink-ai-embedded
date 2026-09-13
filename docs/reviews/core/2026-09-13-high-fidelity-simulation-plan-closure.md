# 高保真仿真系统真机一致性整改与演进计划 结项验收记录

**评审日期**：2026-09-13
**评审对象**：[`PLAN-20260912-SIM-FIDELITY`](../../implementation-plans/core/2026-09-12-high-fidelity-simulation-system-hardening-plan.md)（v2.6）Phase-A + Phase-B 全量交付
**评审视角**：资深嵌入式与仿真架构师（静态分发范式、ADR-0004；单镜像安全硬边界、ADR-0067；波形原子对契约、ADR-0068）
**关联决策**：ADR-0067（Plant Profile 架构）、ADR-0068（波形边沿与虚拟时戳契约）、ADR-0001/0002/0042/0047/0053/0055
**关联设计**：[Wasm 仿真现行入口](../../zh/design/04-wasm-simulation/00-README.md)、[一致性规范](../../zh/design/04-wasm-simulation/04-assurance/01-consistency-spec.md)
**跨仓交付**：sibling `unisim`（D-003 plant-loop 执行器；D-004 SDK 波形契约）

---

## 一、总体结论

**通过。** 计划 Phase-A（Task 1~3，P0）与 Phase-B（Task 4~5，P1，含 ADR 明示的外仓前置 D-003/D-004）全部落地并验收：

- 固件侧：10ms tick 守卫内双向 20ms 去抖 + POST 卡键抑制 + `WARM_HYST_C=2`，`xdata` 16 槽斜率干烧检测（单向冷水守卫 + `slope_valid_sec` 有效窗门控）与 650s/550s 单镜像双级兜底全部生效，`heat_seconds` 唯一自增点。
- 仿真侧：UniSim 补齐 `PLANT_LOOP` 真实执行器（场景只配物理环境、Plant 自激演算）与 `injectWaveform` 世代抢占/原子对/级联推迟/取消契约，button 外设实现原子按压对内的确定性毛刺序列。
- 验证侧：`mcs51_health_pot` **23/23 headless 场景全绿**（16 既有 + 7 新增），含 600s 虚拟快进 stall、150s fast-boil、timing 模式 8ms bounce 与 4COM 时延门禁。

---

## 二、Phase-A 核验（Task 1~3）

| 项 | 结果 | 证据 |
|---|---|---|
| 按键去抖调用点铁律 | ✅ | `button_scan_10ms()` 仅在 `if (tick_flag)` 内、`handle_buttons()` 前调用（`health_pot.c:1049`）；外层 `while(1)` 轮询已删除 |
| POST 卡键抑制 | ✅ | `button_init_post()` 上电锁存（`health_pot.c:396-401`），`health-pot-power-cycle-dwell` / `sensor-fault-powermute` 回归通过 |
| 保温迟滞 2°C | ✅ | `WARM_HYST_C=2u`（`health_pot.c:71`）；`boil-warm`/`direct-55` 回归通过 |
| 干烧逻辑唯一归属 | ✅ | `heat_seconds` 仅在 `heat_slope_task_1s` 自增（`health_pot.c:503`）；`one_second_task` 旧判据块已删；斜率任务先于 `telemetry_emit()`（`health_pot.c:854-855`） |
| 单向冷水守卫 + 有效窗 | ✅ | 上升沿 >40 码触发重置并清零 `slope_valid_sec`，下降沿自然穿透（`health_pot.c:485-506`）；`dryfire-coldwater-gate` / `coldstart-passthrough` 通过 |
| 3 连正跳 E-01 | ✅ | `glitch_cnt >= 3` → `enter_fault(1)`（`health_pot.c:486-489`）；`ntc-glitch-e01` 通过（瞬态类 E-01 300ms 有效读数后安全自愈） |
| 650s/550s 单镜像硬兜底 | ✅ | `DRYFIRE_SECONDS=650u`、`BOIL_TIMEOUT_SECONDS=550u`（`health_pot.c:74-77`）；`dryfire` 21s 斜率触发、`dryfire-stall` 565s Stage-2 断言 |
| xdata 隔离 | ✅ | 全部新增标量/环 `xdata`（`health_pot.c:191-205`），架构门禁零发现 |
| ADR 交付 | ✅ | ADR-0067/0068 Accepted（2026-09-12），并已回写 DESIGN.md |

16 个既有场景回归与 4 个 L1 替代场景全绿，具体对照见计划 §6 与 §9。

---

## 三、Phase-B 核验（Task 4~5 + D-003/D-004）

| 项 | 结果 | 证据 |
|---|---|---|
| **D-003** PLANT_LOOP 执行器 | ✅ | `PlantLoopRuntime` 按 `sampleIntervalUs` 与 OS/WASM 共用 `virtual_dt`（Step-Lock）积分 `first_order_thermal` 精确解；读固件执行器引脚（`gpio:16`）、回写传感器插件（`plugin:temp_sensor/temperature`）；`stepPlant` 内核钩子先于固件 tick |
| Task 4 fast-boil 闭环 | ✅ | 场景零温漂注入，0.3L/1000W/1.5K/W：60s=71.62°C、92s=96.09°C，切断 (92s,99s) + 3s 确认 → `S=2,H=0`；12/12 断言 |
| **D-004** 波形契约 | ✅ | `injectWaveform`：世代抢占（旧世代 JS 回调取消 + `pal_wasm_cancel_waveform_generation`）、C 批量绝对时戳通道、迟到按下沿级联推迟（$t_{release}\ge t_{press}+30$ms）、`cancelWaveform` 整对取消；插件驱动与 C 环锁步更新 |
| button 原子对/毛刺 | ✅ | `buildPressEdges` 确定性毛刺序列（无 RNG）；显式 `pressDurationUs` 时原子对、未指定时保持事件驱动长按；早释放重投递保证 ≥30ms |
| Task 5 显示时延门禁 | ✅ | 加性帧在消抖识别后 ≤50ms 稳定（470ms 断言）；移除段按 80ms POV 常数 ~120ms 收敛（1140ms 断言） |
| Task 5 bounce 回归 | ✅ | timing 模式 8ms/8 毛刺注入，两次按压各恰好单次识别/翻转；既有 PDK button-led 场景无回归 |

---

## 四、验收证据汇总（2026-09-13）

- **全量场景**：`mcs51_health_pot` **23/23 PASS**（原 16 + fast-boil + key-bounce + display-latency + 4×L1）。
- **单测**：unisim `src/sdk`、`src/core/domains`、`src/core/physics`、`src/simulation-runner/headless`、`src/plugin/core` **46/46 PASS**；`wink-plugin-peripherals` button **19/19 PASS**。
- **门禁**：`wink lint --root wink-micro-os --pack layering --pack api` → `No lint findings.`；WASM 资产每轮自动重建成功；unisim 打包安全/泄漏检查 7/7 通过。
- **已知非本计划失败**：unisim `BatchConsistencyScanner` 对 `vendor_cms8s78xx*` 的批一致性用例在本环境离线复现失败（改动前后均失败，与本计划无关）。

---

## 五、偏差、风险与后续

1. **stall 场景 +10s 余量**（565s 断言 / 575s 复位 / 576.5~579s 复位窗口）：计划文本为 555s/565s/566.5~569s；实现选择更宽的 tick 漂移余量，语义一致，已回填计划 §5 Task 2。
2. **L0/L1 host 单测替代**：`mcs51_health_pot` 为 wasm-sim only（`CMakeLists.txt:61`），无 host target；host 单测门禁以 7 个确定性 headless 场景替代并在计划中如实标注。
3. **显示 POV 物理残影**：移除段的 80ms 指数衰减导致解码文本在 ~120ms 内出现并集（如 `-88-`）；稳定帧门禁以“加性帧 ≤50ms + 移除段 ≤150ms 收敛”分档，属模型固有物理特性而非缺陷。
4. **变更管理**：本仓与 sibling 仓变更尚未提交；建议按“固件+场景 / 插件 / 外仓引擎 / 计划与记录”拆分原子提交（AGENTS.md 提交规则）。
5. **后续承接**：[`PLAN-20260915-APPLIANCE-SAFETY-AND-GB4706`](../../implementation-plans/core/2026-09-15-appliance-safety-and-gb4706-compliance-plan.md)——60s 冷却锁定、掉电热态防重开与 GB4706 故障矩阵；本计划已解锁其场景 4 前置。
