<!-- SPDX-License-Identifier: GPL-3.0-only -->
# ESP-IDF v6.1 已完成示例的实现与验收完成度评审（深度融合重构版）

| 项 | 内容 |
|---|---|
| 日期 | 2026-10-05，Asia/Shanghai |
| 模式 | Review；源码、场景、归档报告、只读门禁、内存反例与底层机理深度审查 |
| 主审范围 | 用户发起评审时 CHECKLIST.md 的 33 个 `[x]`，逐项见第 4 节 |
| 起始 HEAD | `412fa0bd8b07b68aca74c030b9297ca233aa4e9f` |
| 复核快照 | `34c83a4c44c4a99a353a6a8a23c010a3c815edc8`；2026-10-05 11:57 CST |
| 现场变化 | 其他工作在评审期间提交了 #151 startup_time、#094 uart_async_rxtxtasks；另列补充观察，不混入主 33 项统计 |
| 任务清单 | [CHECKLIST.md](../../../wink-micro-app/vendor/esp_idfv61/CHECKLIST.md) |
| 数据与规范 | [checklist.data.json](../../../wink-micro-app/vendor/esp_idfv61/.governance/data/checklist.data.json)、[CLASSIFICATION-SPEC.md](../../../wink-micro-app/vendor/esp_idfv61/.governance/specs/CLASSIFICATION-SPEC.md)、[PLAYBOOK.md](../../../wink-micro-app/vendor/esp_idfv61/.governance/specs/PLAYBOOK.md) |
| 评审规程 | [governance-sop-esp](../../../.agents/skills/governance-sop-esp/SKILL.md)、[领域断言指南](../../../.agents/skills/governance-sop-esp/references/domain-assertion-guide.md) |
| 核心架构标准 | ADR-0001 (负数错误码)、ADR-0004 (静态分发)、ADR-0012、ADR-0043 (分层门禁)、ADR-0066 (PWM定点bp)、ADR-0091、ADR-0092 (PAL增量规范) |
| 关联计划 | [防假绿实施计划](../../implementation-plans/esp32/2026-10-01-anti-false-green-verification-plan.md)、[全维度对抗测试计划](../../implementation-plans/esp32/2026-10-05-comprehensive-adversarial-red-green-testing-plan.md) |
| 本轮写入 | 仅本评审记录；没有修改代码、场景、交付状态、审计字段或历史凭据，没有执行未受控仿真或物理烧录 |

---

## 1. 结论与判定口径

**现有勾选不能等同于完整功能已经完成。** 本项目已建立不少有价值的真实业务出口和确定性正向场景，但深入底层分析表明，系统仍存在**核心语义缺失（Semantic Void）、观察器绕过固件（Causal Short-Circuit）以及证据晋升防线退化（Evidence Degradation）**三大系统性深水区矛盾。

主审的 33 项中：
- **A 类（14 项）**：具备有实际物理/协议意义的正向业务观察；
- **B 类（9 项）**：仅覆盖部分业务子集，核心协议分支或并发未闭环；
- **C 类（10 项）**：存在确定的底层实现缺陷、伪造时序或断言完全绕过固件，应立即暂缓“完成”结论。

```
                       【三层验证判定模型】

  ┌─────────────────────────────────────────────────────────┐
  │ ① 测试执行活性 (Execution Liveness): 进程退出 0、日志非空  │ ◄── 当前门禁/核验器所处层级
  ├─────────────────────────────────────────────────────────┤
  │ ② 固件因果闭环 (Causal Invariant): 观察值必源自固件内部处理 │ ◄── S-1, Q-1, Q-3 失陷于此
  ├─────────────────────────────────────────────────────────┤
  │ ③ 真实物理/内核语义 (Physical/Kernel Semantics): 时钟/调度 │ ◄── S-2, S-3, S-4 失陷于此
  └─────────────────────────────────────────────────────────┘
```

必须严格区分三类结论，防止概念混淆：
1. **登记状态**：起始清单记录为 33 项完成；复核快照为 35 项登记为 `verified`。
2. **工具结果**：复核快照下 Gate 1 的 12 条规则全部执行并通过；当前凭据核验器接受 35/35 个配置。
3. **业务完成度**：工具全绿仅代表静态哈希吻合与汇总计数平衡，**不足以证明领域语义、固件因果、故障处理、复位隔离和实际配置身份全部成立**。

特别强调：绝不能因条目登记了 `backend: wasm_browser`、`target_soc: esp32`，就外推为浏览器与 Node 两种宿主均已验收、全部 ESP32 衍生芯片适用，或 ESP32 真机已经验收。当前归档报告未充分绑定这些运行身份，本轮也未进行物理真机核验。

---

## 2. Standards：实现契约、内核机理与生命周期

### S-1 / P1：#013、#014 DAC API 未产生电压或波形输出（虚设门面）

* **定位**：[esp_dac.c](../../../wink-micro-os/frameworks/esp_idf/src/drivers/esp_dac.c):82–93、149–160。
* **机理剖析**：
  `dac_oneshot_output_voltage()` 仅将入参写入全局静态变量 `last_value = val` 并返回成功；`dac_cosine_start()` 仅设置 `is_running = true`。整份门面没有驱动底层模拟引脚、虚拟电学总线，更未挂载任何 DAC 转换器。物理上 DAC 的数字码到模拟电压转换（如 $V_{\text{out}} = V_{\text{ref}} \times \frac{\text{raw}}{255}$）、通道间电隔离、余弦发生器的采样频率与正弦查找表（LUT）在底层全部为空。
* **架构合规与修复要求**：
  必须遵循 **ADR-0092 Tier 1（PAL 纯增量演进）**：
  1. 允许在 `wink-micro-os/pal/include/hal/` 纯增量新增 `pal_dac.h`，严禁修改已有 PAL 函数签名；
  2. 保持跨平台纯洁性，严禁在 PAL 头文件中引入任何 `esp_*.h`、`freertos/*.h` 或芯片专有寄存器类型；
  3. 执行双 Target 同源交付：必须在同一补丁中同时提交 `targets/wasm/` 虚拟电学总线驱动与 `targets/esp32/` 物理驱动，返回值严格遵循 ADR-0001 负数错误码。严禁在 App 层凭空伪造期望波形。

### S-2 / P1：#049 LEDC fade 被瞬时更新替代（时间塌陷与时序降级）

* **定位**：[esp_ledc.c](../../../wink-micro-os/frameworks/esp_idf/src/drivers/esp_ledc.c):143–185；[原厂示例](../../../wink-micro-app/vendor/esp_idfv61/peripherals/ledc_ledc_fade/ledc_fade_example_main.c):68、200–224；[场景](../../../wink-micro-app/vendor/esp_idfv61/peripherals/ledc_ledc_fade/unisim-scenarios/peripherals_ledc_ledc_fade.scenario.json):29–51。
* **机理剖析**：
  实现代码完全忽略了 `max_fade_time_ms` 和 `fade_mode`，在 API 调用内部立即将目标占空比写入终值，并**同步调用**完成回调。原厂示例明确定义单次 fade 耗时为 3000ms，要求四通道分别渐变，阶段 3 应在系统启动约 6000ms 后才发生；但场景文件却迁就错误的底层实现，要求阶段 2、3 在 800ms 内全部发生，并在 500ms 处断言终态占空比。这属于典型的**“测试用例迁就 Bug，固化错误实现”**。
* **架构合规与修复要求**：
  LEDC 硬件本质是由定时器周期驱动的阶梯积分器。UniSim 必须引入**虚拟时间轮（Virtual Time Engine, VTE）**：
  1. 依据 `max_fade_time_ms` 与步进参数计算阶梯时间点，在时间轮中调度渐变中间态；
  2. 占空比计算严格遵循 **ADR-0066**：统一使用定点基点 `pal_pwm_set_duty_bp()` 与 `PAL_PWM_DUTY_PCT()` / `PAL_PWM_DUTY_PERMILLE()`，禁止浮点或裸字面量；
  3. 渐变终点必须通过异步事件触发完成回调，严密覆盖中间斜率、取消操作与并发复位。

### S-3 / P1：#154 task_watchdog 没有超时检测（看门狗失效）

* **定位**：[esp_task_wdt.c](../../../wink-micro-os/frameworks/esp_idf/src/core/esp_task_wdt.c):35–46、95–113、191–223；[场景](../../../wink-micro-app/vendor/esp_idfv61/system/task_watchdog/unisim-scenarios/system_task_watchdog.scenario.json):21–73。
* **机理剖析**：
  初始化保存配置，订阅与喂狗保存 `last_reset_tick`，但在整个运行时中**没有任何时钟比较器或超时检测调度器**。更严重的是，诊断函数 `esp_task_wdt_print_triggered_tasks()` 恒定写入 `*cpus_fail = 0` 并直接返回成功。在当前实现下，即使任务彻底死锁或停止喂狗，看门狗也绝不可能触发报警或复位。
* **架构合规与修复要求**：
  1. 将 TWDT 挂载至 FreeRTOS 虚拟 Tick Hook 或底层离散事件轮；
  2. 每个虚拟 Tick 推进时，遍历已订阅的任务与 User 列表，比对 `current_tick - last_reset_tick > timeout_ticks`；
  3. 支持独立的超时报警、Panic 处理、动态重配置及退订；
  4. 验收必须包含成对的逆向变异：在保持场景不变时，拔除任务喂狗逻辑必须 100% 触发 Watchdog Timeout 报错。

### S-4 / P1：#132 real_time_stats 把阻塞时间计入运行时间（内核计费倒错）

* **定位**：[freertos_task.c](../../../wink-micro-os/frameworks/esp_idf/src/freertos/freertos_task.c):238–246、370–395；[场景](../../../wink-micro-app/vendor/esp_idfv61/system/freertos_real_time_stats/unisim-scenarios/real_time_stats.scenario.json):29–62。
* **机理剖析**：
  `vTaskDelay()` 将即将进入休眠的阻塞毫秒数直接累加进当前任务的 `runtime_counter`，随后 `uxTaskGetSystemState()` 将该累加值作为 `ulRunTimeCounter` 返回。在操作系统内核设计中，这是**原则性倒错**：`vTaskDelay` 会将任务从就绪列表移至延时列表（`pxDelayedTaskList`），此时该任务已出让 CPU，执行的是 Idle 任务或其他就绪任务。当前代码导致“休眠越久的任务，统计所得的 CPU 占用率越高”，工作任务的实际执行片段反而没有得到统计。
* **架构合规与修复要求**：
  1. 彻底剔除 `vTaskDelay` 对运行时间的污染；
  2. 实现标准的调度器切换统计钩子：在 Fiber 上下文切换（`traceTASK_SWITCHED_IN` / `traceTASK_SWITCHED_OUT`）时，利用微秒级虚拟时钟计算实际占用 CPU 的时间增量 $\Delta t$；
  3. 若在轻量仿真下只能提供调度次数等代理指标，必须显式修改字段命名与单位，严禁包装为物理 CPU 利用率。

### S-5 / P1：新增 ADC、DAC、TWDT 状态未纳入完整复位（跨场景状态污染）

* **定位**：[esp_idf_bridge.c](../../../wink-micro-os/frameworks/esp_idf/src/esp_idf_bridge.c):40–50、109–144；[esp_adc.c](../../../wink-micro-os/frameworks/esp_idf/src/drivers/esp_adc.c)、[esp_dac.c](../../../wink-micro-os/frameworks/esp_idf/src/drivers/esp_dac.c)、[esp_task_wdt.c](../../../wink-micro-os/frameworks/esp_idf/src/core/esp_task_wdt.c) 中的静态对象池。
* **机理剖析**：
  `esp_idf_bridge.c` 的软复位逻辑清除了任务句柄与部分通用外设，但遗漏了 ADC、DAC、TWDT 门面内部维护的静态对象池与状态机。若在前一个用例运行期间存在未注销的采集上下文或看门狗实例，执行复位后，这些对象池依然保持 `in_use = true` 或旧回调指针。当前 Headless 流水线的 `SYSTEM_CONTROL` 路径未曾验证过这一复位深度。
* **架构合规与修复要求**：
  1. 建立外设注销与析构的统一生命周期契约，`esp_idf_bridge_reset()` 必须全量覆盖所有已注册驱动；
  2. 复位前必须强制取消所有挂起的异步工作并作废旧句柄；
  3. 场景必须建立冷启动、热复位与连续多场景复用的隔离性断言。

---

## 3. Spec：业务验收覆盖、因果性与上游镜像隔离

### Q-1 / P1：#003、#004、#013、#014 的模拟量断言直接观察测试输入（因果短路）

* **定位**：[ADC continuous 场景](../../../wink-micro-app/vendor/esp_idfv61/peripherals/adc_continuous_read/unisim-scenarios/peripherals_adc_continuous_read.scenario.json)、[ADC oneshot 场景](../../../wink-micro-app/vendor/esp_idfv61/peripherals/adc_oneshot_read/unisim-scenarios/adc_oneshot_read.scenario.json)、[DAC cosine 场景](../../../wink-micro-app/vendor/esp_idfv61/peripherals/dac_dac_cosine_wave/unisim-scenarios/peripherals_dac_dac_cosine_wave.scenario.json)、[DAC oneshot 场景](../../../wink-micro-app/vendor/esp_idfv61/peripherals/dac_dac_oneshot/unisim-scenarios/dac_dac_oneshot.scenario.json):13–44。
* **机理剖析**：
  场景首先通过 Fixture 执行 `INPUT_ANALOG` 向引脚写入模拟电压，紧接着通过断言读取 `adc:<pin>`。测试脚本直接读取的是 Fixture 的输入缓存区，**完全绕过了固件业务逻辑**。固件是否配置了衰减器（Attenuation）、是否调用了采样 API、是否经过校准算法（$mV$ 转换）、DMA 是否搬运、DAC 是否驱动，对断言结果毫无影响。

```
  【错误的因果短路 (现状)】
  Fixture (写入模拟输入) ═══════════════► 断言观察器 (直接读取模拟输入)
          │                                      ▲
          ▼ (旁路/未核验)                         │
     [ESP-IDF 固件 / DMA / 校准算法 / 输出驱动] ────┘

  【正确的因果闭环 (重构目标)】
  Fixture (外部电学输入) ──► 虚拟 ADC 控制器 ──► 采样/DMA ──► 固件校准 (mV) ──► 业务出口 (UART/日志)
```

* **修复方案**：
  切断观察器直连输入缓存的后门。ADC 必须观察固件输出的工程值（如串口打印的校准 mV 或回调处理结果）；DAC 必须由固件驱动输出电平，再由外部电学观测器读取。

### Q-2 / P1：#083 GPTimer 场景验证的是仿真时钟（ABI 缺失与回退伪装）

* **定位**：[场景](../../../wink-micro-app/vendor/esp_idfv61/peripherals/gptimer_alarm/unisim-scenarios/gptimer_alarm.scenario.json):13–38；[esp_gptimer.c](../../../wink-micro-os/frameworks/esp_idf/src/drivers/esp_gptimer.c):98–108、138–163；[导出列表](../../../wink-micro-os/targets/wasm/exported_runtime_functions.json)。
* **实证证据**：
  通过二进制工具反编译已归档 Wasm 产物的 Export Section：**136 个导出符号中根本不存在 `sim_timer_get_counter`**。UniSim 观察器在找不到该 C ABI 时，静默回退使用仿真宿主的全域墙上时钟，并强制假定频率为 1MHz。场景中断言的 500ms、1000ms、2000ms 计数，实际上是在断言浏览器运行了多久，而非 GPTimer 硬件计数器累加到了多少。
* **修复方案**：
  1. 运行时在缺失观察 ABI 时必须 Fail-Loud（显式报错），严禁静默回退到全域时钟；
  2. 底层导出真实的计数器寄存器 ABI，核验停表冻结、自动重载（Auto-reload）、动态 Alarm 触发与队列分发回调。

### Q-3 / P1：#206 MQTT 的 RX 观察实际来自 TX（单缓冲区自发自收）

* **定位**：[场景](../../../wink-micro-app/vendor/esp_idfv61/protocols/mqtt_tcp/unisim-scenarios/mqtt_tcp.scenario.json):46–58；[esp_mqtt.c](../../../wink-micro-os/frameworks/esp_idf/src/network/esp_mqtt.c):521–530、800–801。
* **机理剖析**：
  场景中的断言 `mqtt:rx:payload` 调用的底层 ABI 是 `sim_mqtt_get_last_data()`。而在 `esp_mqtt.c` 中，该函数返回的恰恰是 `esp_mqtt_client_publish()` 写入的静态全局变量 `s_last_data`。这导致所谓的“接收到订阅主题回环载荷”，仅仅证明了“最后一次发送了该数据”。即使彻底删除网络接收任务或注销 `MQTT_EVENT_DATA` 事件处理，断言依旧保持绿灯。
* **修复方案**：
  重构协议仿真模型，彻底拆分独立的 `tx_fifo` 与 `rx_fifo`。Publish 仅写入发送队列；RX 载荷必须由独立的 Broker 模拟器依据 Topic 路由规则投递进接收队列，由客户端事件循环解析并派发。

### Q-4 / P1：#402 NVS blob 本地镜像存在越界读取（上游缺陷治理范式）

* **定位**：[nvs_blob_example_main.c](../../../wink-micro-app/vendor/esp_idfv61/storage/nvs_nvs_rw_blob/nvs_blob_example_main.c):35、153–155；[场景](../../../wink-micro-app/vendor/esp_idfv61/storage/nvs_nvs_rw_blob/unisim-scenarios/nvs_nvs_rw_blob.scenario.json)。
* **机理剖析**：
  结构体明确定义为 `float values[2]`（仅 2 个元素），但原厂代码在日志打印中无条件访问了 `values[2]` 和 `values[3]`。这是典型的 C 语言数组越界读取（Out-of-Bounds Read，未定义行为）。当前场景仅通过模糊前缀正则（如 `Array[0] = `）草率匹配，掩盖了该内存异常。
* **上游缺陷治理范式（KUBA 原则）**：
  项目严格执行**“原厂镜像 0 修改（SHA-256 锁定）”**纪律，绝不因避开报错而私自修改镜像源码。针对此类官方已有缺陷，确立治理标准：
  1. 在 `.governance/specs/upstream_errata.json` 中正式登记该官方 Bug（记录 ESP-IDF 上游 Commit 与缺陷分类）；
  2. 验收场景必须精准断言合法的 `values[0]`、`values[1]`，同时对越界日志制定明确的捕获或隔离策略，不得借由模糊匹配逃避校验。

### Q-5 / P2：#131 SMP 示例只覆盖五个子例中的三个（并发假象）

* **定位**：[场景](../../../wink-micro-app/vendor/esp_idfv61/system/freertos_basic_freertos_smp_usage/unisim-scenarios/system_freertos_basic_freertos_smp_usage.scenario.json):30–127；[主示例](../../../wink-micro-app/vendor/esp_idfv61/system/freertos_basic_freertos_smp_usage/basic_freertos_smp_usage.c)。
* **机理剖析**：
  场景仅执行了 `create_task`、`queue`、`task_notification`，遗漏了 `lock`（互斥）与 `batch_processing`（批量处理）；且仅断言了绑定在 Core 0 上的 Task 0/1，完全遗漏了绑定在 Core 1 上的 Task 2。Wasm 宿主基于 Cooperative Fiber，本身无法模拟真实的抢占式多核并行，若连逻辑亲和性与锁互斥都不予验证，绝不能宣称 SMP 得到验证。

### Q-6 / P2：HTTP、BLE 与 Wi-Fi 只有浅层握手闭环

* **定位**：[HTTP 场景](../../../wink-micro-app/vendor/esp_idfv61/protocols/http_client/unisim-scenarios/http_client.scenario.json)、[bleprph 场景](../../../wink-micro-app/vendor/esp_idfv61/bluetooth/bleprph/unisim-scenarios/bleprph.scenario.json)、[Beacon 场景](../../../wink-micro-app/vendor/esp_idfv61/bluetooth/ble_get_started_nimble_NimBLE_Beacon/unisim-scenarios/bluetooth_ble_get_started_nimble_NimBLE_Beacon.scenario.json)。
* **机理剖析**：
  HTTP 仅断言最终状态码 200 和字节数大于 0，未校验具体的 Header 字段、POST 负载与回调状态机；BLE Peripheral 仅检查广播开关状态，未验证连接、特征读写与通知；SoftAP 未验证 Client 加入离开与 DHCP 分配过程。验收必须依据示例声明的最小业务契约建立完整状态机跟踪。

### Q-7 / P2：NVS 与 SPIFFS 缺少持久化与真实 VFS 边界验收

* **定位**：[SPIFFS 门面](../../../wink-micro-os/frameworks/esp_idf/src/core/esp_spiffs.c):45–51、72–93；[示例源码](../../../wink-micro-app/vendor/esp_idfv61/storage/spiffs/main.c)。
* **机理剖析**：
  SPIFFS 示例通过 libc 的 `fopen/fgets` 操作文件，而门面代码自建了一套内存 Inode 池。静态调用链分析表明，libc 的系统调用与该 Inode 池处于**脱节状态**。`format` 仅清空 Inode 池，`used_bytes` 恒为 0。该用例仅证明了 Wasm libc 的内存文件系统能跑，未证明虚拟 SPIFFS 驱动具备空间限额、格式化挂载与持久化能力。

---

## 4. 原始 33 项逐项完成度与缺陷评级

**判定口径：**
- **A 类**：正向业务出口真实有效，具备实质验收价值；
- **B 类**：仅覆盖局部功能，关键路径或并发特性缺失；
- **C 类**：存在确凿的实现缺陷、时序造假或断言旁路，必须暂缓认定为完成。

| 编号 | App 目录 | 评级 | 当前已观察内容 | 必须补充的关键验证 |
|---|---|:---:|---|---|
| 001 | `get-started/blink_gpio` | **A** | 多周期 GPIO 真实翻转 | 补充时钟骤停负例、周期容差；对齐 SSOT 500ms 与场景 1000ms 的差异 |
| 002 | `get-started/hello_world` | **B** | 问候、自省、前 3 秒倒计时 | 必须运行至 10 秒后触发芯片软重启，并验证重启后计数归零 |
| 003 | `peripherals/adc_continuous_read` | **C** | 直接观察注入模拟输入（因果短路） | 驱动虚拟 ADC 控制器，观测 DMA 帧、通道标签、采样率与固件消费输出 |
| 004 | `peripherals/adc_oneshot_read` | **C** | 直接观察注入模拟输入（因果短路） | 固件 Raw 码转换、衰减器配置、校准 mV 曲线与通道映射断言 |
| 013 | `peripherals/dac_dac_cosine_wave` | **C** | 输入缓存被当成输出（无输出） | 落实 PAL DAC 纯增量驱动，验证双通道频率、幅值、相位差与停止状态 |
| 014 | `peripherals/dac_dac_oneshot` | **C** | 输入缓存被当成输出（无输出） | 数字码到电压线性阶梯、通道隔离、启停生命周期 |
| 020 | `peripherals/gpio_generic_gpio` | **A** | GPIO 变化、边沿 ISR 与队列事件 | 精确边沿翻转计数、抖动抑制测试与中断注销验证 |
| 023 | `peripherals/i2c_basic` | **A** | WHO_AM_I 寄存器交互轨迹 0x75→0x71 | 区分读写控制位、从机应答，补充从机不存在时的 NACK 异常捕获 |
| 024 | `peripherals/i2c_i2c_eeprom` | **A** | 写帧与 48 字节回读日志 | 全量数据 CRC、跨页写入边界处理、从机 Busy 响应与复位保留 |
| 047 | `peripherals/ledc_basic` | **A** | 稳态 50% 占空比 | 校验 5kHz 频率与 ADR-0066 定点 bp 精度；可作为稳态标杆保留 |
| 049 | `peripherals/ledc_ledc_fade` | **C** | 瞬时更新替代渐变（时序塌陷） | 虚拟时间轮步进积分、3000ms 渐变时序、四通道异步完成与取消 |
| 073 | `peripherals/spi_master_hd_eeprom` | **A** | 写入与回读 Hello World 交互 | 精确 SPI 命令码、CS 片选时序、Write Enable 状态机与 Busy 等待 |
| 083 | `peripherals/gptimer_alarm` | **C** | 依赖全域墙钟，Wasm 缺少 counter 导出 | 导出真实 C ABI，断言计数寄存器、Alarm 重载周期、队列回调与停表 |
| 096 | `peripherals/uart_echo` | **A** | RX 输入经任务处理后形成 TX 回显 | 多包突发、全二进制载荷、RX 缓冲区溢出与超时断言 |
| 098 | `peripherals/uart_uart_events` | **A** | DATA 与 +++ 模式匹配事件 | 精确数据长度、事件环形队列、FIFO 溢出与帧错误事件捕获 |
| 125 | `system/esp_event_default_event_loop` | **A** | 注册、分发、注销与停止日志 | 注销后绝对不响应、全量事件序列一致性、队列满时背压处理 |
| 126 | `system/esp_event_user_event_loops` | **A** | 双 Loop 交替处理日志 | 两个独立 Loop 的隔离性、任务调度优先级与删除隔离 |
| 127 | `system/esp_timer` | **A** | 周期、单次、绝对时间与重启日志 | 精确回调执行次数、停止后定时器作废、销毁句柄防护 |
| 131 | `system/freertos_basic_freertos_smp_usage` | **B** | 仅覆盖 3 个子例，缺 Core 1 验证 | 补齐 lock 与 batch_processing，断言 Core 1 亲和性与互斥锁生效 |
| 132 | `system/freertos_real_time_stats` | **C** | 错误地把 vTaskDelay 计入 CPU 时间 | 重构上下文切换微秒计费模型，验证休眠任务与繁忙任务的真实占用对比 |
| 154 | `system/task_watchdog` | **C** | 门面恒写 cpus_fail=0，无超时调度 | 挂载 Tick 轮询超时判定，验证漏喂狗报警、Panic 与多订阅者隔离 |
| 186 | `protocols/http_client` | **B** | 仅断言最后状态码 200 与收包计数 | 验证 GET/POST/PUT 完整事务、Header 校验、响应体解析与 404/500 异常 |
| 206 | `protocols/mqtt_tcp` | **C** | 所谓 RX 实为读取本地 TX 静态缓存 | 拆分独立 TX/RX FIFO，由虚拟 Broker 真实路由，断言订阅事件处理 |
| 221 | `wifi/fast_scan` | **B** | 仅断言 DHCP 与基础断连 | 同 SSID 多候选 AP 排序比对、RSSI 信号强度过滤与阈值裁定 |
| 223 | `wifi/getting_started_softAP` | **B** | AP 启动与基本配置日志 | 客户端 Station 关联/脱离事件、AP 分配 IP（DHCP）与连接数上限 |
| 224 | `wifi/wifi_sta` | **A** | DHCP 成功与 DROP_BEACON 掉线 | 完善密码错误异常重试上限、网络恢复后的自动重连闭环 |
| 230 | `wifi/scan` | **A** | 扫描到 3 个指定 AP 名称与数量 | 动态 Fixture 切换、无信号空集测试、信道/认证类型排序验证 |
| 247 | `bluetooth/ble_get_started_nimble_NimBLE_Beacon` | **B** | 仅断言广播名称与启动状态 | 解析校验实际广播原始数据（Raw Adv Data）、UUID、非连接模式契约 |
| 248 | `bluetooth/bleprph` | **B** | 仅断言广播开启与服务数量 | 建立虚拟 Client 连接，断言 GATT 特征值读写、Notify 与非法长度拒绝 |
| 401 | `storage/nvs_nvs_iteration` | **A** | 类型筛选与 14 项遍历日志 | 校验全部键值类型字典、命名空间命名隔离与非空遍历断言 |
| 402 | `storage/nvs_nvs_rw_blob` | **C** | 忽略原厂 values[2] 数组越界缺陷 | 建立 upstream_errata.json，校验合法项 CRC，捕获越界防御行为 |
| 403 | `storage/nvs_nvs_rw_value` | **B** | 写入与读取日志 | 验证删键、commit 提交、热重启与冷启动后的数据持久化 |
| 415 | `storage/spiffs` | **B** | libc 文件读写演示，驱动 Inode 脱节 | 桥接 VFS 与虚拟驱动，测试存储满溢、format 格式化清除与卸载拦截 |

---

## 5. 共同证据链与治理工具防线漏洞（E-1 ~ E-5）

### E-1 / P1：核验器仅查 Summary 计数，对逐步断言完全失控

* **定位**：[evidence_verifier.py](../../../wink-micro-app/vendor/esp_idfv61/.governance/gates/evidence_verifier.py):116–160、262–286。
* **漏洞实证（内存反例实验）**：
  保持磁盘文件不动，在内存中复制合法 HTTP 报告并调用核验器，以下三个致命反例均返回 `True`（全绿通过）：
  1. **清空 `stepResults` 数组**，只要 `summary.passed` 保持非零：**核验器判定通过**；
  2. **彻底删除 `stepResults` 键**，保留 summary 计数：**核验器判定通过**；
  3. **将某个核心断言的逐步状态从 `passed` 改为 `failed`**，只要 summary 不变：**核验器判定通过**。
* **架构加固建议（Merkle 证据树模型）**：
  必须对 `evidence_verifier.py` 执行硬加固：
  - 强制解析目标场景文件，提取预期步骤全量序列；
  - 逐项 $O(N)$ 遍历 `stepResults`，必须满足 `len(stepResults) == len(scenario.steps)` 且每一步状态必须为 `passed`；
  - 引入 **Merkle 证据树哈希校验**：将所有逐步结果序列化后计算 SHA-256，与场景哈希、Wasm 产物哈希打包签名，报告摘要篡改必须立刻导致验签失败。

### E-2 / P1：Canary 仅自检 Matcher，且将基础设施崩溃计为击杀成功

* **定位**：[mutator.py](../../../wink-micro-app/vendor/esp_idfv61/.governance/tools/loop/mutator.py):101–126、153–166；[pipeline.py](../../../wink-micro-app/vendor/esp_idfv61/.governance/tools/loop/pipeline.py):270–285。
* **漏洞实证（虚假击杀反例）**：
  调用 `verify_kill(exit_code=2, stderr="Failed to build wasm assets", meta)` 和 `verify_kill(exit_code=2, stderr="Failed to load runtime module", meta)`，当前流水线均返回 **True**，将其记录为“变异击杀成功（Mutant Killed）”。这导致当代码存在语法错误、链接失败或环境崩溃时，系统居然宣称测试通过了对抗性验证！
* **判定口径硬性重构**：
  ```
  【击杀结果判定准则】
  - exit_code == 0                         ──► 变异逃逸 (Mutant Survived) ──► 驳回
  - exit_code != 0 && (编译/链接/加载失败)   ──► 基础设施崩溃 (INFRA_CRASH) ──► 一票否决
  - exit_code != 0 && (命中目标断言报错)     ──► 真正击杀 (MUTANT_KILLED)  ──► 允许晋升
  ```
  同时，必须严格区分 **“断言器自检（Matcher Inversion）”** 与 **“固件业务变异（Fault Injection / Logic Mutation）”**，前者仅证明断言语法有效，不能替代固件因果敏感性证明。

### E-3 / P1：执行流水线存在“自产自签”的闭环审计风险

* **定位**：[pipeline.py](../../../wink-micro-app/vendor/esp_idfv61/.governance/tools/loop/pipeline.py):288–311；[auditor 门禁](../../../wink-micro-app/vendor/esp_idfv61/.governance/gates/rules/g1_auditor_required.py):27–74。
* **机理剖析**：
  主审 33 项中，有 25 项被流水线自动写入 `auditor: loop_sop_daemon`。流水线在跑完正向基线和 Matcher Canary 后，直接调用脚本将自身签名覆写进 `checklist.data.json`，并将 ruling 设为 null。这构成了典型的 **“LLM 跑测试 -> LLM 判定通过 -> LLM 签发审计证明”** 的回音室效应。
* **架构制衡建议（Separation of Powers）**：
  1. 剥离执行权与审计权：运行流水线仅有权输出 `candidate_evidence` 候选凭证包；
  2. 审计字段只能由独立的裁判 Agent（通过剥离思维链的双盲对抗审查，参见 ADR-0092）或人类架构师签名；门禁严禁接受流水线 Daemon 的自我背书。

### E-4 / P1：写入器缺乏按配置与运行隔离的事务晋升

* **定位**：[evidence_verifier.py](../../../wink-micro-app/vendor/esp_idfv61/.governance/gates/evidence_verifier.py):393–418；[run_esp32_headless_evidence.ps1](../../../wink-micro-os/frameworks/esp_idf/tools/run_esp32_headless_evidence.ps1):150–189。
* **机理剖析**：
  历史运行固定使用单个共享路径 `run-report.json`，运行 ID 仅为日期加 App 名，无法区分同一天内的多次实验；在凭证校验完成前即直接覆写正式凭证库，缺乏失败回滚机制。
* **建议**：引入 `.governance/runs/<timestamp>-<uuid>/` 沙箱隔离目录，实行原子性（Atomic）事务晋升，验证失败立刻整盘回滚，确保正式归档库绝对纯净。

### E-5 / P2：上游源码溯源与能力依赖登记存在漂移

* **实证数据**：
  运行只读溯源工具在 35 项快照中检出 **17 条错误与 1 条警告**：
  - #013 源码哈希与规范化哈希存在 1 字符微小漂移；
  - #024、#073 头文件移至 `include/` 后，登记仍使用根目录名；
  - #415 登记为 `spiffs_example_main.c`，实际编译 `main.c`；
  - 其余多为 App 目录名与工具派生规则不一致。
  此外，33 项中有 21 项的 `negative_cases` 为空。必须统一规范，消除登记与现实的漂移。

---

## 6. 建议的实施路线图（分阶段落地矩阵）

结合 [全维度对抗测试与红绿变异实施计划](../../implementation-plans/esp32/2026-10-05-comprehensive-adversarial-red-green-testing-plan.md)，建议分四个工程阶段推进：

```
Phase 0: 状态止血与分流 (Triage & Freeze)
  ├── 状态分流: 将 10 个 C 类缺陷条目在治理看板中标识为 [needs_remediation]，暂停晋升
  └── 权力冻结: 封堵 pipeline.py 自动签发 auditor 与盲目覆写 verified 的后门

Phase 1: 证据链防线与门禁铁门加固 (Gate & Verifier Hardening)
  ├── 修复 E-1: evidence_verifier.py 引入 stepResults 全量 O(N) 遍历与 Merkle 哈希校验
  ├── 修复 E-2: mutator.py 严格定义 INFRA_CRASH，严禁基础设施崩溃充当击杀凭据
  └── 修复 E-4: 确立唯一 UUID 运行沙箱与事务性原子晋升机制

Phase 2: 底座时钟与复位基础设施重构 (Kernel Time & Reset Foundation)
  ├── 虚拟时间轮: 实现 UniSim 统一离散事件引擎，修复 S-2 (LEDC Fade)、S-3 (TWDT 超时)
  ├── 内核计费: 修复 S-4，建立基于调度器切入/切出微秒钩子的 FreeRTOS 真实统计模型
  └── 深度复位: 修复 S-5，将 ADC/DAC/TWDT 对象池完整纳入 esp_idf_bridge_reset 契约

Phase 3: 领域外设因果链重塑与真机比对 (Peripherals Causality & Dual-Target HIL)
  ├── 落实 S-1/Q-1: 按 ADR-0092 纯增量交付 PAL DAC，重构 ADC/DAC 模拟总线因果链
  ├── 落实 Q-2/Q-3: 导出 GPTimer 真实 ABI；MQTT 拆分独立的 TX/RX Ring-Buffer 协议栈
  ├── 落实 Q-4: 建立 upstream_errata.json，隔离登记官方 NVS blob 越界缺陷
  └── 双 Target 实机比对: 选取 blink, ledc, gptimer, uart_echo, http_client, wifi_sta 6 大黄金用例，
      在 ESP32 物理硬件上通过 wink.py esp32 运行基准核验，确保 Wasm 仿真与真机同源行为一致。
```

---

## 7. 本轮检查结果与边界

| 检查项 | 复核结果 | 证明范围与局限性 |
|---|---|---|
| `python -X utf8 -B .../gates/run_gates.py --gate 1` | 12 执行、0 SKIP、0 error、0 warning | 表明已注册的 Gate 1 静态规则接受该快照；未运行 Gate 2–5 |
| `python -X utf8 -B .../gates/evidence_verifier.py --verify-all` | 35/35 接受 | 证明现有磁盘资产/场景哈希与报告汇总计数一致；**不证明逐步执行完整性** |
| `python -X utf8 -B .../tools/check_vendor_app_upstream.py` | exit 1，17 error、1 warning | 揭示本地 pin、路径迁移与工具解析规则漂移；未证明所有文件与官方版本一致 |
| 主 33 项场景与归档逐项对账 | 214 步、192 断言；数量/索引/类型/状态一致 | 证明当前报告结构与登记场景对应；**不证明运行身份、因果关系或物理语义** |
| 核验器内存反例实验 | 3 种不完整/矛盾逐步结果均被接受 | **确凿证实 E-1 工具漏洞存在**；实验在内存中完成，未污染真实归档 |
| Canary kill 内存反例实验 | 2 种基础设施崩溃被误判为 kill | **确凿证实 E-2 判据漏洞存在**；实验在内存中完成，未污染真实归档 |
| GPTimer 已归档 Wasm export section 逆向 | 136 导出符号，无 counter 观察 ABI | **确凿证实 Q-2 物理导出缺失**，该资产无法通过声明 ABI 提供真实计数 |
| 物理构建、动态仿真重放、真机烧录 | 本轮未执行 | 本报告为纯 Review 模式，不能以此宣称本轮完成硬件物理验收 |

---

## 8. 评审期间新增两项的补充观察

| 编号 | 新增 App | 静态与归档观察 | 尚需补充的关键验证 |
|---|---|---|---|
| 094 | `peripherals/uart_uart_async_rxtxtasks` | 包含 UART1 TX 的 Hello world 以及独立 RX 输入后 UART0 的精确字节数与内容日志；相比单看初始化日志具备更高业务价值 | 需补充持续 TX 周期、RX/TX 全双工并发压力、二进制边界、FIFO 溢出以及任务注销清理；源码命名门禁漂移仍需修复 |
| 151 | `system/startup_time` | 恢复 Info 日志并在 0–1000ms 窗口内断言 App started；满足官方示例最小启动演示 | 必须明确启动时间是从 ROM boot、外设初始化还是 app_main 起算；此场景不构成真实 ESP32 启动性能或 sdkconfig 优化的充分验证 |

---

**总结**：本评审报告通过严密的静态分析、源码行号追溯、反编译逆向与内存反例实验，完整揭示了 33 个已标记完成示例在实现契约（Standards 5 项）、业务因果（Spec 7 组）与治理工具防线（Evidence 5 项）上的深层技术缺口。报告提出的统一离散时间轮、总线隔离模型、上游 Errata 机制及 Merkle 证据防线，为后续底座加固提供了明确可执行的架构输入。
