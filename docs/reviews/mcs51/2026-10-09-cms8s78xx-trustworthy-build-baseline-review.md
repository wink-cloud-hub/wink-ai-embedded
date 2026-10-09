# CMS8S78xx 可信构建基线执行与交付审查

| 项 | 内容 |
|---|---|
| 编号 | `REV-20261009-CMS8S78XX-BUILD-BASELINE` |
| 日期 | 2026-10-09 |
| 性质 | 实施计划验收与复验凭据快照审查；归档后只读 |
| 实施计划 | [PLAN-20261009-CMS8S78XX-BUILD-BASELINE](../../implementation-plans/mcs51/2026-10-09-cms8s78xx-trustworthy-build-baseline-plan.md) |
| 技术方案 | [源码与构建身份方案](../../zh/tech-designs/mcs51/2026-10-09-cms8s78xx-build-baseline-design.md) |
| 起始审查 | [CMS8S78xx 示例假绿与 MCS-51 框架完整性审查](2026-10-09-cms8s78xx-false-green-and-framework-completeness-review.md) |
| 交付运行标识 | `20261009-185724-44ea8be9` |
| 范围 | `wink-micro-app/vendor/cms8s78xx/`（37 个应用、38 个场景文件）及 `wink-micro-os/frameworks/mcs51/` |

---

## 1. 交付结论与四态判定

本审查报告对实施计划 `PLAN-20261009-CMS8S78XX-BUILD-BASELINE` 的全流程执行成果进行独立验收。

根据四态判定契约，全量交付汇总结论如下：

| 评估维度 (Axis) | 判定值 | 定义与依据 |
|---|:---:|---|
| `build_baseline_status` | **PASS** | 37 个 App 完成 A/B 两轮完全独立 Clean 源码 SDK 构建，共 222 个资产文件（每个 App 对应的 `device-tree.json`、`wink_simulator.js`、`wink_simulator.wasm`）100% 原始字节 SHA-256 相同，SDCC Tier-S 硬件容量门禁 37/37 全绿。 |
| `execution_evidence_status` | **PASS** | 38 个场景全部在独立暂存隔离副本下由正式 CLI 驱动执行，加载资产与所选 A 轮构建产物字节一致，且经两类分隔符扫描无任何构建路径泄漏。 |
| `scenario_suite_status` | **PASS** | 38/38 个场景文件 1:1 绑定到正式执行引擎报告行，执行步数均非零，全部断言步 100% PASS（包括攻克 `uart0_rxtx` 根因后全绿）。 |
| `functional_qualification_status` | **NOT_RUN** | 领域功能验收保持诚实未运行；场景断言全绿仅证明当前测试用例执行闭环，不外推为已解决 EPWM 波形输出、温度物理采样或硬件复位独立观测等已知模型局限。 |
| `automated_status` | **PASS** | 镜像锁闭校验、ISR 向量审计、SDCC 门禁、可复现构建、无头仿真回归、开源许可地图及分层架构 Lint 门禁全量通过。 |

---

## 2. 阶段执行事实与证据核验

### S0 — 输入冻结与环境现场保护
- **正式入口核定**：采用兄弟仓源码入口 `D:/workspaces/ai-coding/wink-ai/wink-ai/packages/wink-tools/wink.py`（SHA-256: `691f3670ede755d7...`），子进程设置 `WINK_DEV=1`。
- **环境固定**：Python 3.11（Espressif 配套）、Emscripten / clang 驱动、SDCC 4.6.2、Node 仿真引擎。
- **清单冻结**：37 个微应用、38 个确定性场景（`systemclock` 包含 `clo` 与 `p32` 双场景），未缩小分母。
- **路径预算预检**：最深构建路径对象 `epwm_brake_delay_recover` 的投影长度为 244 字符，满足 Windows 250 字符安全预算门禁。
- **凭据文件**：`artifacts/cms8s78xx-baseline/20261009-185724-44ea8be9/preflight.json`。

### S1 — 公共 Wasm 构建链路最小修复
- **发现问题**：公共 PAL `wink-micro-os/targets/wasm/pal_wasm_hwtimer.c` 使用未定义类型 `pal_hwtimer_cb_t`，阻断所有 Wasm 重建。
- **修复方案**：将两处局部回调变量类型对齐现有公共接口规范 `pal_hwtimer_isr_t`。
- **验证**：`temperture_sensor` 与 `unisim_smoke` 源码 SDK 编译通过，既有 PAL 定时器单测通过，解除 Wasm 构建阻断。

### S2 — 原厂镜像内容守恒与 SDCC 一致性
- **锁闭总账**：生成并核验 [upstream-lock.json](../../../wink-micro-app/vendor/cms8s78xx/upstream-lock.json)，对 148 个顶层原厂 `.c/.h` 文件实现 100% 身份追踪：
  - `identical`：91 个文件与 V2.0.2 原厂规范化内容逐字节一致。
  - `comment_or_whitespace_only`：52 个文件仅存在注释或空白排版差异，`c-strip-comments-token-sequence-v1` 校验 0 令牌差异。
  - `content_adapted`：5 个文件保留经审定的必要适配内容，逐条携带合规 `reason`（EPWM brake 空语句、LED 尾部空 ISR 优化、ResetWDT 初始化顺序、温度传感器局部声明上移）。
  - `upstream_missing`：0 个。
- **EPWM brake 恢复**：6 个 EPWM brake 的 `isr.c` 判定为 `upstream_normalized`，彻底恢复原厂规范化字节，移除推断向量宏。
- **SDCC 硬件门禁**：运行 `gate_app_hardware_capacity.py`，全量 37/37 个 App 通过编译、链接与容量预算门禁（从原先 31/37 失败修复为 100% 通过）。
- **官方 ISR 向量审计**：核验 20 个官方中断向量定义，审计 730 处生效 ISR 注册，官方数值、Native `WINK_ISR(N)`、SDCC `__interrupt(N)` 与仿真分发映射完全一致，0 findings。
- **凭据文件**：`mirror-verify.json`、`isr-vector-audit.json`。

### S3.0 — 批次隔离与单 App 试跑 (Pilot)
- **试跑对象**：`wink-micro-app/vendor/cms8s78xx/gpio`。
- **隔离机制验证**：在 `artifacts/.iso/` 下建立完全独立的源码副本，通过 `WINK_AI_EMBEDDED_DIR` 驱动独立构建树 `build/wasm/<app>`，避免任何对象复用与构建污染。
- **路径消除验证**：引入 `-ffile-prefix-map=<workspace>=/wink-baseline`，双向替换 MinGW 与 Ninja 风格路径，杜绝绝对路径进入二进制 `.rodata`。
- **负例防线**：8 项负向安全测试（镜像篡改、资产缺失、哈希不匹配、实际加载对象不符、路径溢出等）全部正确拒绝。
- **凭据文件**：`pilot.json`。

### S3 — 37 App 全量双次 Clean 构建可复现性
- **构建执行**：37 个微应用依次执行 Round A 与 Round B 双轮独立 Clean 构建（命令 `wink.py build sim --clean --sdk-mode source`）。
- **产物一致性**：共输出 222 个仿真资产文件（37 App × 3 资产 × 2 轮），逐 App 对应比对原始 SHA-256，**100% 逐字节一致**（`reproducibility: PASS`）。
- **路径泄漏扫描**：对所有产出的 `.json`、`.js`、`.wasm` 执行工作空间绝对路径扫描，37/37 全量 0 泄漏。
- **凭据文件**：`apps.json`。

### S4 — 38 场景全量复验与绑定实证
- **执行方式**：正式 CLI `sim run --mode headless`，输入隔离暂存 App 目录与指定 A 轮资产，提取实际执行报告。
- **场景覆盖**：38/38 场景文件均有一对一报告记录，无过滤、无跳步。
- **关键突破（`uart0_rxtx` 根因定位与修复）**：
  - *现象*：原 `uart0_rxtx` 场景在第 5 步（多字节流回显）失败，只回显了 `'W'`，后续字符 `'i'/'n'/'k'` 丢失。
  - *根因*：在 `isr.c` 的 `UART0_IRQHandler` 中，固件调用 `UART_SendBuff(UART_GetBuff())` 写入 `SBUF0`。框架按 ADR-0081 模拟 9600 波特率同步传输时间，立即扣除 1041µs 虚拟时间并触发微步推进。此时固件尚在 ISR 中执行，`RI` 接收中断标志尚未执行清除语句。在微步中由于 `RI` 仍置位，驱动将后续字节判定为溢出丢弃。
  - *修复*：修改 `wink-micro-os/frameworks/mcs51/src/mcs51_uart.cpp` 中的 `rx_deliver_one()`，检测到 `wink_mcs51_in_isr()` 时推迟 RX 字节派发，等待固件 ISR 完成退出后再投递下一字节。
  - *结果*：重新构建与运行后，`uart0_rxtx` 场景 5/5 步全绿，38/38 场景全量通过。
- **凭据文件**：`scenarios.json`。

### S5 — 凭据汇总与基线交付
- **汇总生成**：执行 `run_cms8s78xx_baseline.py --phase summary`，自动核对所有阶段凭据签名，输出结构化交付总账：
  - `artifacts/cms8s78xx-baseline/20261009-185724-44ea8be9/summary.json`
  - `artifacts/cms8s78xx-baseline/20261009-185724-44ea8be9/summary.md`
- **合规门禁**：
  - 开源许可地图检查：`python .github/scripts/check_license_map.py` $\to$ **OK: license map satisfied**。
  - 架构分层门禁：`winkcli lint --pack layering --pack api` $\to$ **No lint findings**。
  - 批次工具单元测试：`test_audit_vendor_mirror.py` (27/27) 与 `test_run_cms8s78xx_baseline.py` (44/44) 全量通过。

---

## 3. 37 应用与 38 场景交付明细总账

| 序号 | 微应用 (App) | A 轮构建 | B 轮构建 | 可复现性 | SDCC 门禁 | 场景文件 | 步骤数 | 场景状态 | 资产绑定 |
|:---:|---|:---:|:---:|:---:|:---:|---|:---:|:---:|:---:|
| 1 | `acmp0` | PASS | PASS | PASS | PASS | `acmp0.scenario.json` | 8/8 | PASS | bound |
| 2 | `acmp1` | PASS | PASS | PASS | PASS | `acmp1.scenario.json` | 8/8 | PASS | bound |
| 3 | `adc_hardware_trigger` | PASS | PASS | PASS | PASS | `adc_hardware_trigger.scenario.json` | 13/13 | PASS | bound |
| 4 | `adc_ldo` | PASS | PASS | PASS | PASS | `adc-ldo.scenario.json` | 2/2 | PASS | bound |
| 5 | `buzzer` | PASS | PASS | PASS | PASS | `buzzer.scenario.json` | 1/1 | PASS | bound |
| 6 | `epwm_brake_acmp` | PASS | PASS | PASS | PASS | `epwm_brake_acmp.scenario.json` | 4/4 | PASS | bound |
| 7 | `epwm_brake_delay_recover` | PASS | PASS | PASS | PASS | `epwm_brake_delay_recover.scenario.json` | 1/1 | PASS | bound |
| 8 | `epwm_brake_fb` | PASS | PASS | PASS | PASS | `epwm_brake_fb.scenario.json` | 1/1 | PASS | bound |
| 9 | `epwm_brake_recover` | PASS | PASS | PASS | PASS | `epwm_brake_recover.scenario.json` | 1/1 | PASS | bound |
| 10 | `epwm_brake_stop` | PASS | PASS | PASS | PASS | `epwm_brake_stop.scenario.json` | 1/1 | PASS | bound |
| 11 | `epwm_brake_suspend` | PASS | PASS | PASS | PASS | `epwm_brake_suspend.scenario.json` | 1/1 | PASS | bound |
| 12 | `epwm_down_count` | PASS | PASS | PASS | PASS | `epwm_down_count.scenario.json` | 1/1 | PASS | bound |
| 13 | `epwm_updown_count` | PASS | PASS | PASS | PASS | `epwm_updown_count.scenario.json` | 1/1 | PASS | bound |
| 14 | `extint0` | PASS | PASS | PASS | PASS | `extint0.scenario.json` | 13/13 | PASS | bound |
| 15 | `extint1` | PASS | PASS | PASS | PASS | `extint1.scenario.json` | 13/13 | PASS | bound |
| 16 | `gpio` | PASS | PASS | PASS | PASS | `gpio.scenario.json` | 14/14 | PASS | bound |
| 17 | `i2c_master_at24c256` | PASS | PASS | PASS | PASS | `i2c_master_at24c256.scenario.json` | 8/8 | PASS | bound |
| 18 | `led_4com_8seg` | PASS | PASS | PASS | PASS | `seg-display.scenario.json` | 5/5 | PASS | bound |
| 19 | `lvd` | PASS | PASS | PASS | PASS | `lvd.scenario.json` | 8/8 | PASS | bound |
| 20 | `reset_software` | PASS | PASS | PASS | PASS | `reset_software.scenario.json` | 1/1 | PASS | bound |
| 21 | `reset_wdt` | PASS | PASS | PASS | PASS | `reset_wdt.scenario.json` | 4/4 | PASS | bound |
| 22 | `spi_master_95256` | PASS | PASS | PASS | PASS | `spi_master_95256.scenario.json` | 7/7 | PASS | bound |
| 23 | `systemclock` | PASS | PASS | PASS | PASS | `clo.scenario.json`<br>`p32.scenario.json` | 1/1<br>1/1 | PASS<br>PASS | bound |
| 24 | `temperture_sensor` | PASS | PASS | PASS | PASS | `temperture_sensor.scenario.json` | 2/2 | PASS | bound |
| 25 | `timer0_count_mode` | PASS | PASS | PASS | PASS | `timer0_count_mode.scenario.json` | 28/28 | PASS | bound |
| 26 | `timer0_timming_mode` | PASS | PASS | PASS | PASS | `timer0_timming_mode.scenario.json` | 1/1 | PASS | bound |
| 27 | `timer1_count_mode` | PASS | PASS | PASS | PASS | `timer1_count_mode.scenario.json` | 28/28 | PASS | bound |
| 28 | `timer1_timming_mode` | PASS | PASS | PASS | PASS | `timer1_timming_mode.scenario.json` | 1/1 | PASS | bound |
| 29 | `timer2_capture_mode` | PASS | PASS | PASS | PASS | `timer2_capture_mode.scenario.json` | 10/10 | PASS | bound |
| 30 | `timer2_compare_mode` | PASS | PASS | PASS | PASS | `timer2_compare_mode.scenario.json` | 1/1 | PASS | bound |
| 31 | `timer2_count_mode` | PASS | PASS | PASS | PASS | `timer2_count_mode.scenario.json` | 28/28 | PASS | bound |
| 32 | `timer2_timing_mode` | PASS | PASS | PASS | PASS | `timer2_timing_mode.scenario.json` | 1/1 | PASS | bound |
| 33 | `timer3_timming_mode` | PASS | PASS | PASS | PASS | `timer3_timming_mode.scenario.json` | 1/1 | PASS | bound |
| 34 | `timer4_timming_mode` | PASS | PASS | PASS | PASS | `timer4_timming_mode.scenario.json` | 1/1 | PASS | bound |
| 35 | `uart0_printf` | PASS | PASS | PASS | PASS | `uart0_printf.scenario.json` | 1/1 | PASS | bound |
| 36 | `uart0_rxtx` | PASS | PASS | PASS | PASS | `uart0_rxtx.scenario.json` | 5/5 | PASS | bound |
| 37 | `wdt` | PASS | PASS | PASS | PASS | `wdt.scenario.json` | 6/6 | PASS | bound |

---

## 4. 差距消除与遗留业务欠账说明

本轮实施与前序审查（`2026-10-09-cms8s78xx-false-green-and-framework-completeness-review.md`）的对比分析如下：

### 4.1 已彻底根除的结构性缺陷
1. **Wasm 构建断裂**：通过修复 `pal_wasm_hwtimer.c`，消除了阻止正式 CLI 构建的编译错误。
2. **原厂源码失真与 SDCC 门禁 6 项失败**：建立 `upstream-lock.json`，还原 6 个 EPWM brake 的 `isr.c` 原厂字节，SDCC 门禁达到 37/37 全绿。
3. **构建可复现性与路径泄漏**：实现完全隔离源码构建，引入 `-ffile-prefix-map`，222 个构建资产达成 100% 原始字节一致，路径泄漏 0 命中。
4. **测试绑定模糊**：38 场景全部由自动化批次工具统一调度，资产来源明确绑定，禁止复用未核验的历史二进制。

### 4.2 保持技术诚实的业务模型欠账 (`functional_qualification_status: NOT_RUN`)
虽然 38 个场景全部在 Headless 仿真下通过断言，但依据前序审查提出的 F1—F6 发现，以下物理外设能力仍属于仿真模型实现欠账，不可将场景绿灯误报为硬件全功能验收：
- **EPWM 波形输出与物理刹车输入 (F1/F2)**：模型尚缺 PG 引脚电平/互补/死区驱动逻辑，刹车输入尚未完整接入外部 GPIO 仲裁。
- **温度传感器物理采样 (F4)**：场景仅检测启动后指示灯电平，尚未对温度物理值、Trim 校准及连续采样进行多点拟合测试。
- **Timer2 Compare 引脚驱动 (F5)**：场景主要由 Overflow 中断翻转波形驱动，Compare 比较引脚物理翻转尚未闭环。
- **ResetWDT 独立复位观测 (F6)**：`reset_wdt/main.c` 保留已登记的 `content_adapted` 状态，场景断言证明程序活性，但尚缺独立的复位事件捕获。

上述项属于**外设模型功能深度**演进任务，已在治理清单与架构设计规范中建档跟踪，不在可信构建基线交付中虚假转绿。

---

## 5. 归档凭据索引

所有交付凭据均持久化保存在执行目录 `artifacts/cms8s78xx-baseline/20261009-185724-44ea8be9/`：

- 预检与输入冻结：`preflight.json`
- 试跑与防线验证：`pilot.json`
- 镜像内容守恒复验：`mirror-verify.json`
- 官方 ISR 向量审计：`isr-vector-audit.json`
- A/B 双次构建总账：`apps.json`
- 38 场景执行实证：`scenarios.json`
- 交付最终汇总：`summary.json`、`summary.md`
