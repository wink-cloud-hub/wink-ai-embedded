<!-- SPDX-License-Identifier: GPL-3.0-only -->
# 阶段二：驱动语义修复、当前源码复验与仿真真实性

| 项 | 内容 |
|---|---|
| 计划编号 | `PLAN-20261009-ESP-IDF-AFG-PHASE2-DRIVER-HARDENING` |
| 日期 / 修订 / 状态 | 2026-10-09，Asia/Shanghai；v1.1 Draft；全部验收待执行 |
| 总控 / 共同规则 | [总控计划](00-MASTER-OVERVIEW.md)、[执行质量规则](05-EXECUTION-QUALITY-GATES.md) |
| 技术设计 / 活规范 | [AFG 契约](../../../zh/tech-designs/esp32/esp-idf-anti-false-green-verification-engine-contract.md)、[Loop 契约](../../../zh/tech-designs/esp32/esp-idf-loop-reliability-contract.md)、[一致性规范](../../../zh/design/04-wasm-simulation/04-assurance/01-consistency-spec.md) |
| 历史发现 | [S-01～S-05、Q-01～Q-06 与 G 问题](../../../reviews/esp32/2026-10-08-esp-idf-verified-checklist-deep-review.md)、[I-01/I-02 与 RT 账本](../2026-10-09-esp-idf-loop-issues-and-remediation-plan.md) |
| 实现 / 复核角色 | C 驱动维护者 / PAL 与领域复核者；具体人员由 T0.4 指定 |

## 1. 进入条件与当前事实

驱动源码定位与反例准备在 QG-0 后进行，真实端到端复验依赖 QG-1。按能力分批关闭，不因 unrelated 驱动未就绪阻止已满足条件的领域准备；未完成项保持显式缺口。

当前 S-01 的计数观察、S-02 的单次回调收尾、S-03 的采样失败传播已有源码调整，不能继续一概写成“底软未修”。这些调整不等于已完成构建与业务验证，仍须重建新资产、检查 ABI 和反例敏感性。其他历史发现先在当前基线复核，避免仅凭旧行号重复整改。

旧 Pilot C 的拒绝来自合成输入，不作为驱动故障动态复现。先证明当前 ADC 的生产/消费、采样配置与背压真实行为，再判断修复和配置资格。删除旧合成记录不能直接让 Pilot C“转绿”。

## 2. 任务拆分与真实源码映射

G = `wink-micro-app/vendor/esp_idfv61/.governance/`。路径为当前内仓 Code-Mapping；需要新增文件时先明确模块所有权与设计依据，不创建原计划中不存在的 vendor/facade 目录来绕过分层。

| 任务 | 当前源码 / 契约位置 | 必需行为与交付 |
|---|---|---|
| T2.1 ADC 生产/消费与失败语义 | [esp_adc.c](../../../../wink-micro-os/frameworks/esp_idf/src/drivers/esp_adc.c)、[pal_wasm_ch3_adc.c](../../../../wink-micro-os/targets/wasm/pal_wasm_ch3_adc.c)、[adc_continuous.h](../../../../wink-micro-os/frameworks/esp_idf/include/esp_adc/adc_continuous.h) | 参数化自发生产、有界 pool、帧与溢出策略、真实错误、生命周期和恢复；S-03 |
| T2.2 GPTimer 设备观察 | [esp_gptimer.c](../../../../wink-micro-os/frameworks/esp_idf/src/drivers/esp_gptimer.c)；公开观测 ABI | 未创建/运行/停止/重载/删除/代际状态正确，实际计数与全局时间可区分；S-01/Q-02 |
| T2.3 告警回调与重入 | [pal_wasm_hwtimer.c](../../../../wink-micro-os/targets/wasm/pal_wasm_hwtimer.c)、esp_gptimer.c | 消费旧事件与回调重装的先后、调度代际和 catch-up 边界；S-02/Q-02 |
| T2.4 SPI 构建、模型归属与实例隔离 | [esp_spi.c](../../../../wink-micro-os/frameworks/esp_idf/src/drivers/esp_spi.c)；实际模型/总线绑定由 T0.1 定位 | 两个 EEPROM 与非 EEPROM 设备独立；reset/旧句柄和错误传播；I-01/S-04 |
| T2.5 NVS 持久化与启动恢复 | [esp_nvs.c](../../../../wink-micro-os/frameworks/esp_idf/src/core/esp_nvs.c)；冻结存储后端契约 | 写入/替换中断、启动恢复、损坏与占用；保留完整旧/新值，不静默清空；I-02 |
| T2.6 SPIFFS 后端一致性 | [esp_spiffs.c](../../../../wink-micro-os/frameworks/esp_idf/src/core/esp_spiffs.c)；实际 libc/VFS 路由 | 文件读写、rename、format、容量与挂载同一状态来源；S-05 |
| T2.7 DAC/LEDC 观测与保真对齐 | [esp_dac.c](../../../../wink-micro-os/frameworks/esp_idf/src/drivers/esp_dac.c)、[esp_ledc.c](../../../../wink-micro-os/frameworks/esp_idf/src/drivers/esp_ledc.c)；公开输出/波形契约 | 固件输出因果与正确时序，禁止输入回读；阶跃/渐变声明与已实现子集一致；Q-01/Q-04 |
| T2.8 构建、ABI、回归与 Pilot 复验 | 受影响目标构建配置、G 下真实新 run 目录 | 适用目标完整构建链接、实际出口、领域反例/正例与恢复；三项 Pilot 本轮身份对账 |

每项任务有独立缺陷/正确对照、受影响配置集合、文件白名单和复核责任。T2.8 可逐领域执行；不足以关闭其他领域的验收。需要观测契约扩展时走技术设计/ADR，不猜测外仓实现。

## 3. ADC 的时间、数据和背压模型

### 3.1 单一虚拟时钟，读取仅消费

虚拟时间仅由既有 Runner/Gate/DES 推进。ADC 生产不依赖 read/getter 的调用次数；消费者挂起时硬件采样继续服从同一个虚拟时间源。Read 只消费既有数据或按既有调度合同等待；probe/getter 不改变时钟和设备状态，不新增第二个宿主墙钟推进器。

采样数量由有效 `sample_freq_hz`、通道 pattern 和格式决定，处理非整周期余数，禁止每次 read 固定生成一帧。大步进按事件时刻与同 Tick 因果顺序分解，不能因宿主跑得快或 catch-up 上限而静默丢事件。若有能力边界，按冻结保真合同显式报告。

### 3.2 参数化容量与故障时刻

按配置记录实际 pool 容量、初始水位、每转换结果字节数、总生产速率和帧大小，不能将所有配置固定为 4096B 或 100ms。忽略消费时可先估算：

```text
R_bytes = 有效总转换速率 × 每转换结果字节数
T_fill = (pool_capacity_bytes - initial_queued_bytes) / R_bytes
```

实际事件时刻还受帧量化、保留/丢弃策略与调度顺序影响，依据锁定 SDK 契约核对满池/溢出边界。设计阈值前、阈值后及多组容量/采样率实验；100ms 只可作为特定配置的一组输入。

使用原生 `on_conv_done`、`on_pool_ovf` 及 `flush_pool` 策略：明确丢新数据或丢旧数据、通知频次、有效帧内存生命周期、丢样计数和下一次 read 的行为。不能用额外打印日志代替真实事件及业务观察。参考 [ESP-IDF v6.1 ADC 官方契约](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/adc/adc_continuous.html)，执行时冻结实际 SDK 版本，不依赖 stable 地址永远不变。

### 3.3 错误域、状态与恢复

PAL 内部保持合法负状态；ESP-IDF 门面保持原生 `esp_err_t`。现有 `WINK_ERR_OVERFLOW` 表示算术溢出，现行映射不会自动得到 ESP_ERR_INVALID_STATE，禁止照搬 v1.0 的未经设计映射。按已冻结状态/错误转换核验具体符号与值，不改通用映射来影响其他外设。

覆盖无效通道、未初始化、非法参数、重复 start/stop、停止后无新数据、flush 使用状态、回调内受允许操作和 deinit/重建。真实采样失败不得成为正常数据；当前错误传播修正必须以无效输入反例复验。按声明清空易失队列并恢复正常吞吐，不靠读 API 偷推时间恢复。

## 4. 其他驱动的边界要求

- **GPTimer**：设备计数包含分辨率、方向和偏置；停止后冻结，设置非零值与重载可观察。未创建、删除或旧代际通过有效性状态明确不可用，不能将合法计数 0 当成通用无效值，也不能回退为全局时钟。观测 ABI 缺失显式失败，新产物确认实际导出/执行。
- **告警重入**：旧事件先消费或用调度代际判断回调是否重建槽；回调内重装/停止/删除/重建不被旧收尾覆盖。验证绝对 alarm_count 到相对等待转换，至少连续三次告警与允许的 catch-up 行为。
- **SPI**：门面只适配通用事务，器件模型按总线/设备实例绑定；维护静态分发。两个模型状态独立，非 EEPROM 命令不进入其协议解释，reset 与旧句柄的处理可检出。
- **NVS**：选择既有后端支持的原子替换或已冻结恢复协议，覆盖真实 OS 的提交各边界、CRC 损坏、单/双副本失败和重启；保留错误与旧值，不通过格式化把恢复失败转换为成功。
- **SPIFFS**：fopen/rename/unlink、挂载、format 和 info 指向同一后端；验证 used 变化、格式化后旧文件不可用、卸载失败和复位/持久化策略。块设备模型只有在冻结设计要求时实现，不能把“统一接口”当作已经统一状态。
- **DAC/LEDC**：观察固件输出，fixture 只提供输入/环境；停用写入/启动使对应原业务断言失败。DAC 余弦与阶梯、LEDC 渐变或声明的阶跃子集分别核验时间、波形/中间值和完成语义，不沿用约束错误的旧场景迫使正确实现退化。

## 5. 验收矩阵

| AC | 真实实验与正确对照 | 必需证据 |
|---|---|---|
| AC-2.1 | 多组 ADC 速率/容量下消费者读取、暂停、恢复；不调用 read 也生产 | 虚拟时间、样本/帧计数、水位、溢出/丢样、原生回调与读结果 |
| AC-2.2 | ADC 阈值前后、不同步进划分、只读 probe、start/stop/flush/无效输入 | 注入确实生效；错误域、状态与数据来源；等价步进在合同内一致 |
| AC-2.3 | GPTimer 未创建/运行/停止/非零偏置/重载/删除/旧代际 | 设备状态与计数可区分全局时钟；本轮实际 ABI/加载摘要 |
| AC-2.4 | 告警回调内重装/停止/删除/重建、连续三次与边界步进 | 回调/队列结果、间隔及槽代际；旧事件不关闭新调度 |
| AC-2.5 | 两 SPI EEPROM 与非 EEPROM、reset/旧句柄、真实错误 | 实例状态与实际事务；原缺陷变异可检出，正确控制通过 |
| AC-2.6 | NVS 各提交边界中断后重启、CRC/副本/占用反例 | 完整旧/新值恢复或明确拒绝；实际文件与恢复日志 |
| AC-2.7 | SPIFFS 写入/容量/format/卸载与复位 | 同后端文件状态；业务结果与控制面一致 |
| AC-2.8 | DAC 删除业务、正确输出；LEDC 正确声明子集与错误时序变异 | 固件依赖、实际输出、动态/稳态与保真合同绑定 |
| AC-2.9 | 适用目标构建、链接与 ABI、相关领域和三项 Pilot 复验 | 当前输入/工具/产物、逐项检查、清理与恢复；不使用合成 Pilot |

公共头文件/API 与 App 同源分别在 Wasm/真实 SDK 下验证，目标专有翻译单元由对应 target 构建，不要求 Wasm 专用文件强行在 Xtensa 编译。目标芯片决定 Xtensa 或 RISC-V 工具链；不把所有 ESP32 家族写成 Xtensa。适用 C 变更运行分层/API、PWM 和许可检查。语法检查只能早期预检，不能替代完整链接与实际观测。

## 6. QG-2 出口

- [ ] T2.1～T2.8 当前状态、源码/提交、问题映射、正确与缺陷样本回执完整；历史已修项有新的定向复验。
- [ ] mandatory AC 子例真实执行，时钟、观察和产物身份无缺口；所需 ABI/场景/静态门禁均支持。
- [ ] Pilot A hello_world、Pilot B UART Echo 与 Pilot C ADC 的正常业务、适用变异/故障/恢复完成。是否 ELIGIBLE 由真实结果推导，不能强制预设。
- [ ] 未修复/未支持项明确保留能力与配置缺口，阻止依赖它的后续配置验收；不以全局“3/3”覆盖缺失任务。
- [ ] C 变更、适用目标、领域回归及门禁有真实回执；新增复验报告，不改写历史 reviews。

若必需驱动验收缺失，本阶段保持未完成。某个正确配置得到拒绝时先诊断模型、场景、契约和工具，不能预先归因为驱动或通过调整回执转绿。
