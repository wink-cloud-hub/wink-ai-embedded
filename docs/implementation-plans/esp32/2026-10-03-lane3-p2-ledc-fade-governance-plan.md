<!-- SPDX-License-Identifier: Apache-2.0 -->
# 实施计划：Lane 3 P2 标杆 `peripherals/ledc/ledc_fade` 官方示例仿真治理闭环

> 遵循 `governance-sop-esp` 规范、`ADR-0001` ~ `ADR-0092` 架构决策，推进 Lane 3（定时脉冲与电机控制）LEDC 硬件平滑渐变与计数信号量回调治理闭环。

| 字段 | 内容 |
|---|---|
| 计划编号 | PLAN-20261003-LANE3-P2-LEDC-FADE-GOVERNANCE-v1.0 |
| 任务目标 | 达成 `peripherals/ledc/ledc_fade`（Display ID: 49）零修改镜像治理与 LEDC 硬件平滑渐变（Fade Up / Fade Down / Direct Update）、FreeRTOS 计数信号量同步高保真确定性仿真认证 |
| 泳道与优先级 | **Lane 3（定时脉冲与电机/PWM控制） / P2 进阶标杆** |
| 状态 | 🟢 **Complete (Verified & Signed-off)** |
| 日期 | 2026-10-03 |
| 上游路径 | `examples/peripherals/ledc/ledc_fade` |
| 目标载体目录 | `wink-micro-app/vendor/esp_idfv61/peripherals/ledc_ledc_fade/` |
| 场景路径 | `peripherals/ledc_ledc_fade/unisim-scenarios/peripherals_ledc_ledc_fade.scenario.json` |
| 依赖能力 | `cap.pulse.ledc_fade`, `cap.core.fiber_task`, `cap.core.sync_tokens` |
| 交付工件 | 原厂零修改代码镜像、UniSim 确定性场景、仿真执行报告、Gate 1 合规签署与看板派生 |

---

## 一、 战略总目标与标杆意义

### 1.1 攻坚背景
在 Lane 3（定时脉冲与电机/PWM控制）中，我们已成功交付：
- `#047` `peripherals/ledc/ledc_basic`（定点 PWM 占空比输出与动态更新）
- `#083` `peripherals/timer_group/gptimer`（通用高精度硬件定时器闹钟与中断）

**`peripherals/ledc/ledc_fade`（Display ID: 49）是 LEDC 控制器中最核心的进阶标杆**：
- 演示多通道（4 通道：HS CH0/CH1 + LS CH2/CH3）定时器绑定与差分占空比配置；
- 演示平滑渐变中断驱动服务安装（`ledc_fade_func_install`）；
- 演示中断上下文回调注册（`ledc_cb_register` + `cb_ledc_fade_end_event`）；
- 演示与 FreeRTOS 计数信号量（`xSemaphoreCreateCounting(4, 0)`）的协同同步机制（多通道渐变全部完成后唤醒控制任务）；
- 演示硬件平滑渐变（`ledc_set_fade_with_time` / `ledc_fade_start`）与瞬态占空比直接写入（`ledc_set_duty` / `ledc_update_duty`）的生命周期切换。

### 1.2 战略收益
1. **完善 LEDC 控制器高级驱动因果闭环**：验证从渐变启动、完成事件中断派发、FreeRTOS 信号量唤醒到应用任务推进的确定性时序。
2. **打通多通道 PWM 硬件与输出反转支持**：支持低速通道 `output_invert` 硬件标记与多通道统一管理。
3. **扩展实证交付基线至 25 项**：强化 Lane 3（定时脉冲与电机/PWM控制）治理深度。

---

## 二、 技术与架构方案

### 2.1 零修改镜像契约 (Layer C)
- 权威原厂路径：`D:\software\embedded-tools\esp-idf\.espressif\v6.1\esp-idf\examples\peripherals\ledc\ledc_fade`
- 镜像文件与锁定 SHA-256：
  - `main/ledc_fade_example_main.c` -> `wink-micro-app/vendor/esp_idfv61/peripherals/ledc_ledc_fade/ledc_fade_example_main.c`:  
    `e05ce10c2f1bed04bdf4c85883674124e3b61a4d56dc3e4ce438febb58447984`
- 严禁修改任何原厂源码，所有适配均通过 `CMakeLists.txt`、`sdkconfig.h`、`wink-app.json` 完成。

### 2.2 Layer A 底座对齐与 LEDC 渐变回调支持
- 审查 `wink-micro-os/frameworks/esp_idf/src/drivers/esp_ledc.c`：
  - `ledc_timer_config` / `ledc_channel_config`：已有完备配置逻辑，并对接 `pal_pwm_init_ex`；
  - `ledc_fade_func_install` / `ledc_fade_func_uninstall`：控制全局渐变引擎状态；
  - `ledc_set_fade_with_time`：配置目标渐变占空比；
  - `ledc_cb_register`：保存 `fade_cb` 回调指针与 `user_arg`（计数信号量）；
  - `ledc_fade_start`：触发占空比跃迁并正确构造 `ledc_cb_param_t`（`event = LEDC_FADE_END_EVT`）触发回调，释放信号量；
  - `ledc_set_duty` 与 `ledc_update_duty`：定点万分比（basis points）无浮点定点计算更新。
- 遵循 ADR-0066 定点 PWM 规范与 LGPL-3.0 许可。

### 2.3 Layer S 确定性因果时序与断言设计
- 业务因果链路：
  - 供电断言：`power:VCC_3V3` 维持 3.3V；
  - 步骤 1：断言阶段 1 渐变启动日志 `"1. LEDC fade up to duty = 4000"` 打印；
  - 步骤 2：断言渐变完成并进入阶段 2 `"2. LEDC fade down to duty = 0"`；
  - 步骤 3：断言进入阶段 3 直接占空比配置 `"3. LEDC set duty = 4000 without fade"`；
  - 步骤 4：断言阶段 3 中 `pwm:0` 目标占空比由 0 跃升至 48.83%（4883bp）；
  - 步骤 5：断言进入阶段 4 直接清零配置 `"4. LEDC set duty = 0 without fade"`；
  - 步骤 6：断言阶段 4 中 `pwm:0` 占空比直接清零为 0%；
- Canary 变异缺陷敏感性检验：
  - 变异断言日志（例如 `"LEDC set duty = 9999 without fade"`），验证在窗口期内 100% 击杀。

---

## 三、 执行计划与任务拆解

### Phase 1：Layer C 应用载体与原厂镜像落盘
- [x] **Task 1.1**：创建目录 `wink-micro-app/vendor/esp_idfv61/peripherals/ledc_ledc_fade/`。
- [x] **Task 1.2**：镜像 `ledc_fade_example_main.c`，固化 SHA-256。
- [x] **Task 1.3**：创建 `CMakeLists.txt`、`include/sdkconfig.h` 与 `wink-app.json`。
- [x] **Task 1.4**：在 `run_esp32_headless_evidence.ps1` 登记 `ledc_ledc_fade` carrier。

### Phase 2：Layer A 底座对齐与驱动验证
- [x] **Task 2.1**：核查 `esp_ledc.c` 对 `ledc_fade_start` 及回调参数的精准映射。
- [x] **Task 2.2**：编译并构建 Wasm 目标。

### Phase 3：Layer S 高保真因果场景编撰
- [x] **Task 3.1**：编撰 `unisim-scenarios/peripherals_ledc_ledc_fade.scenario.json` 业务因果断言集合（涵盖 7 步断言：3.3V 供电、4 大阶段日志、48.83% 及 0% 物理 PWM 输出）。
- [x] **Task 3.2**：核验 `checklist.data.json` 中该项元数据（Display ID: 49, scenario_path 等）。

### Phase 4：正向仿真基线通过
- [x] **Task 4.1**：运行 `run_esp32_headless_evidence.ps1 -App ledc_ledc_fade`，7/7 步骤 100% 通过（Virtual: 3000000µs, Wall-clock: 89ms）。

### Phase 5：Canary 变异缺陷敏感性检验
- [x] **Task 5.1**：注入非等价业务变异，验证指定断言在窗口期内精准失败并被捕获（Canary Kill 100%）。

### Phase 6：凭据归档与治理看板派生
- [x] **Task 6.1**：执行带 `-WriteEvidence` 正式录入证据。
- [x] **Task 6.2**：更新 `checklist.data.json` 中的 audit 签署（`loop_sop_daemon`）。
- [x] **Task 6.3**：运行 `generate_checklist_v1_1.py` 重新生成 `CHECKLIST.md`。

### Phase 7：全量门禁与零回归复验
- [x] **Task 7.1**：运行 Gate 1（12 项规则全绿通过）。
- [x] **Task 7.2**：运行许可证检查（100% 合规）。
- [x] **Task 7.3**：运行 `evidence_verifier.py --verify-all`（25/25 项全部通过）。

### Phase 8：原子化 Git 提交
- [ ] **Task 8.1**：分块提交 Carrier/场景与治理凭据。
