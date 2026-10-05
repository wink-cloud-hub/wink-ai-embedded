<!-- SPDX-License-Identifier: GPL-3.0-only -->
# ESP-IDF v6.1 已完成示例的实现与验收评审

| 项 | 内容 |
|---|---|
| 日期 | 2026-10-05（Asia/Shanghai） |
| 类型 | Review；实现/规范与业务验收双轴审查 |
| 固定范围 | HEAD `e1c4ce2fb53449a262b04eaac62420b96d6717ba` 中的 34 个已打勾示例 |
| 清单 | [CHECKLIST.md](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/CHECKLIST.md) |
| 规范 | [CLASSIFICATION-SPEC](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/.governance/specs/CLASSIFICATION-SPEC.md:64)、[PLAYBOOK](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/.governance/specs/PLAYBOOK.md:332)、[governance-sop-esp](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/.agents/skills/governance-sop-esp/SKILL.md) |
| 关联计划 | [防假绿验证计划](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/docs/implementation-plans/esp32/2026-10-01-anti-false-green-verification-plan.md)、[对抗测试计划](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/docs/implementation-plans/esp32/2026-10-05-comprehensive-adversarial-red-green-testing-plan.md) |
| 操作边界 | 阅读源码、场景和历史凭据，运行只读门禁与临时目录中的 Python 单测；未构建、未重放仿真、未上真机、未修改凭据/看板/业务代码 |

**结论：不能把当前 34 个勾选解释为 34 项核心功能已完整交付。** 当前核验器接受全部凭据；进一步审查发现 9 项必须重新判断完成度，其中存在实际实现缺失、错误时序和与固件业务无关的观测。另 15 项仅有部分业务覆盖，10 项已有较明确的正向业务轨迹。后两组也不等于完成完整交付验收。

这里的“正向轨迹 / 部分覆盖 / 需重判”是本次审查意见，不是新的 `delivery_state` 枚举，未写回任何治理状态。所有被审项登记的完成配置均为 `wasm_sim_standard / wasm_browser / esp32 / standard`，没有据此证明硬件配置交付或物理电气性能。

审查开始时看板为 33 项，期间新增并提交了 #151 startup_time，故将范围固定为上述 34 项。之后观察到 UART async 等新条目正在推进，未纳入本记录。审查不是整个仓库的一次原子运行；这里列出的缺陷已通过固定范围内源码交叉核对，实时计数差异单独注明。

**本轮验证结果与边界**

| 检查 | 实际结果 | 能证明什么 |
|---|---|---|
| `python -X utf8 -B .../gates/run_gates.py --gate 1` | 12 条实际执行；0 error，0 warning，0 skip | 当时登记数据被现有 Gate 1 接受 |
| `python -X utf8 -B .../gates/evidence_verifier.py --verify-all` | 34/34 通过 | 当时磁盘资产/指定场景哈希和报告汇总被现有核验器接受 |
| evidence_verifier 与 scenario_semantic_integrity 两组既有单测 | 18 + 15 = 33 passed | 既有单测通过；不覆盖下述四种报告异常 |
| 仅内存中的报告对抗检查 | 四种异常报告均被错误接受 | 核验器缺乏逐步结果与完整执行集合/身份检查 |
| 仅内存中的 Canary 故障分类检查 | 编译失败 `Failed to compile: expected expression` 被判定为有效 kill | 基础设施错误可误算业务杀伤 |
| `check_ssot_invariants.py` | 固定快照中 audited 汇总 33，实际 34，失败 | 发布快照存在统计不一致；之后又有并发增量变化，不能把中途差异当成新增功能失败 |
| 上游文件元数据核查 | 处理 include 移位、SPIFFS 重命名和 CRLF 后，51 条记录中 50 条与镜像匹配；DAC cosine 哈希有一处不一致 | 支持大部分镜像与登记文件的对应；不证明上游源码无缺陷 |
| 固件依赖、有效业务变异、完整故障/复位重放、CTest 与硬件差分 | 本轮未执行 | 本记录不能代替这些正式验收 |

Python 单测首次因系统临时目录访问限制而在 fixture 阶段报错；改用当前会话允许写入的专用临时目录后，33 项全部通过。该环境错误不计为代码或场景失败。

**Standards：实现与治理规范**

1. **[P1] #154 的 TWDT 只有记账，没有到期检测。** [esp_task_wdt.c](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/core/esp_task_wdt.c:35) 保存 `timeout_ms` 和 `last_reset_tick`，未实现期限比较、定时触发或超时事件；`print_triggered_tasks()` 固定报告零失败。原厂 [task_watchdog_example_main.c](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/system/task_watchdog/task_watchdog_example_main.c:51) 明确说明删任意喂狗应触发 TWDT。当前生命周期绿灯不能证明监控能力。补任务及两个 user 分别停止喂狗、3s 到期、触发对象、panic 配置、退订取消监控及恢复后无误报。官方契约要求订阅后的任务/user 定期喂狗避免超时。[ESP-IDF TWDT 文档](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/wdts.html)

2. **[P1] #013/#014 的 DAC 未产生模拟输出。** [esp_dac.c](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/drivers/esp_dac.c:82) 的 oneshot 只存 `last_value` 并打印日志，cosine start 只置 `is_running`，未接电压出口或波形模型；能力字典却声明输出 implemented。应实现从数字值到电压的业务出口，oneshot 验每 500ms 阶梯变化，cosine 验双通道 8kHz、180° 相位、6dB 衰减与 stop。官方 DAC 契约区分电压输出及带频率、幅度、相位的余弦输出。[ESP-IDF DAC 文档](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/dac.html)

3. **[P1] #049 的 fade 是瞬时写终值，既有单测还固化了这一行为。** [esp_ledc.c](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/drivers/esp_ledc.c:143) 忽略时间、步长和 cycle_num，`ledc_fade_start()` 即刻更新终值并回调。[test_esp_ledc.c](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/test/core/test_esp_ledc.c:161) 在 500ms NO_WAIT 启动后立即断言完成。原示例每次 fade 为 3000ms，当前场景反而要求 800ms 内已升降完成。应共同修实现和测试，验中间值、持续时间、回调只在完成时发生，以及 WAIT/NO_WAIT 差异。[ESP-IDF LEDC 文档](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/ledc.html)

4. **[P1] #132 的运行时间统计把休眠记作执行时间。** [freertos_task.c](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/freertos/freertos_task.c:239) 将 `vTaskDelay()` 等待毫秒累加到 runtime_counter，再作为 `ulRunTimeCounter` 返回。这会误导任务负载诊断。应在明确的仿真执行计量模型下记账，不能把无法精确模拟 CPU 执行成本掩饰为真实 CPU 占比；验睡眠时计数不增长、负载变化能改变分布、总量与可用核数一致。

5. **[P1] 凭据核验器未核对逐步执行记录及配置身份。** [verify_execution_report](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/.governance/gates/evidence_verifier.py:111) 检查 `ok/status/summary`，没有核对 `stepResults`。保持顶层成功汇总时，以下四种内存样本均返回 `True`：
   - 删除全部 `stepResults`；
   - 把某条实际 step.status 改为 failed；
   - 把 templateId 换成其他应用；
   - 只保留第一条 step，同时把 summary 改为 1/1。
   
   这违反“必要步骤全部执行且成功”的验收要求。应从本轮输入重建所需场景及步骤集合，核对唯一索引、类型、状态、实际值和预期值，并绑定 run_id、配置、后端、芯片、profile、runner 标识、源码/配置/报告哈希。报告校验函数还应重算汇总，不接受缺失/重复/跳过步骤。

6. **[P1] 改预期值的 Canary 被误称为业务缺陷敏感性。** [mutator.py](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/.governance/tools/loop/mutator.py:112) 只修改 `matcher`；此法证明断言器会拒绝错误预期，不能证明 ADC 采样、DAC 输出或 MQTT 接收被删后会失败。[verify_kill](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/.governance/tools/loop/mutator.py:140) 又凭 `Failed/expected/FAIL` 等文字判定 kill，本轮编译失败样本被误收。多份 Completed 计划将“篡改 matcher”写为非等价业务变异，与 [CLASSIFICATION-SPEC](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/.governance/specs/CLASSIFICATION-SPEC.md:67) 的区分不符。应分别保存断言器自检、固件禁用、有效业务变异和故障处理证据；只计指定业务断言失败，编译/解析/Runner 失败须为无效实验。

补充缺陷与工程建议：

- **[P2] esp_timer 微秒接口降为 10ms tick。** [esp_timer.c](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/core/esp_timer.c:121) 向下整除至 tick，[FreeRTOSConfig.h](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/include/freertos/FreeRTOSConfig.h:5) 固定 100Hz；例如 15000µs 变为 10000µs，小于 10000µs 至少延至 10000µs。现有整毫秒大周期示例可跑，但不证明微秒调度保真。建议接微秒事件调度，或如实收窄契约；补 1µs/9999µs/15000µs 与跨周期漂移验收。
- **[P2] NVS 未遵守只读/类型契约，提交存在丢失旧值窗口。** [esp_nvs.c](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/core/esp_nvs.c:230) 忽略 open_mode，typed getter 调 blob getter 未核验存储类型；[持久化替换](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/core/esp_nvs.c:532) 在 rename 前删除旧文件，失败时旧已提交数据可能丢失。补 READONLY 写拒绝、TYPE_MISMATCH、长度探测、提交失败、重启/掉电恢复，以及有恢复语义的替换机制。这不否定已观察到的正常读写路径。
- **[P2] 源码身份与交付快照尚不可充分重建。** #132/#151/#154/#402 的 `verified_commit` 对应 Git tree 中尚没有本应用目录，说明仅存当时 HEAD 没绑定候选工作树。DAC cosine 登记的上游哈希含 `...bbcf68...`，当前文件实际 `...bbfcf68...`，需核对后修正元数据；SPIFFS 是合法内容匹配的重命名/换行，不判源码破坏。建议锁定候选源码/配置清单和工具链，再以同一完整候选包事务性晋升。发布前原子核验 summary 与看板，避免并发增量产生计数漂移。

Standards 轴确认 6 类 P1 问题，最高影响为无超时检测的 TWDT 及会接受缺失/失败逐步记录的凭据核验器；另列 3 类 P2 增强，不以此泛称所有已有正常路径无效。

**Spec：场景是否验到原厂业务**

1. **[P1] ADC/DAC 四项断言只检查 Fixture 自己填写的值。** 当前真实链路为 [INPUT_ANALOG](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/docs/.internals/packages/unisim/src/simulation-runner/headless/signal-dispatcher.ts:99) → [writeStimulusNorm](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/docs/.internals/packages/unisim/src/core/domains/AdcDomainHandler.ts:68) 写入 Map → [adc target](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/docs/.internals/packages/unisim/src/simulation-runner/headless/headless-sim-runner.ts:564) → readNorm 返回 Map。ADC 两项未观察固件 raw/mV；DAC 两项甚至以输入激励代替输出。由源码可推知禁止业务采样/输出不会改变这些断言；本轮未执行这一固件变异。应使激励和业务结果来自不同通道，并验证固件依赖。
2. **[P1] GPTimer 的 counter 是运行器全局时钟回退值。** [timer target 解析](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/docs/.internals/packages/unisim/src/simulation-runner/headless/headless-sim-runner.ts:601) 缺 getter 时回退 `clock.getUs()`，frequencyhz 恒为 1MHz；本轮读取当前 Wasm 导出节确认不存在 `sim_timer_get_counter`。因此场景 500ms/1s/2s 的计数断言仅验证仿真时钟。原业务包含 alarm 后 stop、set count=100、自动重载和动态重设 alarm，应接真实 handle 出口并验证各阶段；缺出口应报不支持，不能返回成功外观的默认值。
3. **[P1] MQTT 把 TX 缓存暴露为 RX 载荷。** [运行器分派](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/docs/.internals/packages/unisim/src/simulation-runner/headless/headless-sim-runner.ts:690) 将 `mqtt:rx:payload` 与 `tx:data` 都映射 [sim_mqtt_get_last_data](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/network/esp_mqtt.c:800)，其数据由 publish 写入。因此当前订阅回环断言只证明发送。应独立 RX 事件与应用处理出口，验 topic/payload、接收次数、取消订阅后的无消息，以及屏蔽接收回调时正向业务断言失败。

LEDC fade、TWDT、统计也在 Spec 轴失败，具体见前述实现及表中条目，不重复计作新的根因。

需要补齐的五组业务验收：

- **HTTP**：能力已经接通，旧 Skill 中“不支持 INJECT_NET_FIXTURE”的快照已过时。当前只检 status=200、rx>0、request_count≥1；门面记录指标早于应用 ON_DATA/FINISH，所以不能证明固件响应处理。应逐请求校验 method/path/query/body、响应体/应用完成，并明确原示例 HEAD、redirect、chunk、stream、native 流程的覆盖配置；补 timeout/404 和恢复。
- **BLE**：Beacon 应校验 AD/scan response 字节、URL、广播间隔及不可连接属性，无需强加连接流程。bleprph 应校验 UUID、属性、GATT 读写、descriptor、订阅后 notify/indicate、非法长度错误和断连后再广播；服务数量 ≥1 只证明注册入口。
- **UART/Wi-Fi**：UART events 的字符串 matcher 使用 includes，`[UART DATA]: 1` 也命中 10/15/100，需精确长度/字节和普通 echo、pattern 分包、溢出恢复。Wi-Fi 补同 SSID 多 AP 的 fast/all/threshold、扫描排序、SoftAP 对端 join/leave，以及 STA 应用 retry/失败/恢复。
- **NVS blob 已知上游缺陷**：[结构体](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/storage/nvs_nvs_rw_blob/nvs_blob_example_main.c:35) 定义 values[2]，[日志](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/storage/nvs_nvs_rw_blob/nvs_blob_example_main.c:153) 却读 values[2]/[3]。本机 SDK 原文件也存在同样代码，归一化哈希与镜像及 pin 一致，未把它误判为迁移新增。当前场景避开 Values/Counts，不能证明无 UB。保持原厂镜像约束，记录上游缺陷，补 sanitizer 复现及独立 blob 字节完整性、长度、旧值保留和重启持久性验收。
- **启动/系统生命周期**：startup_time 只验 App started!，不能验启动性能；hello_world 未验真正 restart；事件/定时器缺完整有序次数与删除后静默。已有协作纤程模型是声明边界，本轮不把单核确定性仿真一概判为 SMP 实现错误，仍须说明未验证物理双核竞争。

Spec 轴确认 3 类独立 P1 因果/观测问题，并与 Standards 的实现缺陷共同影响 9 项完成认定；另列 5 组业务覆盖建议。

**34 项逐项结论**

“正向轨迹”10 项、“部分覆盖”15 项、“需重判”9 项。这是静态实现与历史证据审查分类；10 项不等于已重新执行或正式验收完成。

| 编号 | 应用及场景 | 本轮意见 | 已有依据与补充 |
|---|---|---|---|
| #001 | [get-started/blink_gpio](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/get-started/blink_gpio/unisim-scenarios/blink_gpio.scenario.json) | 正向轨迹 | 7 个电平点支持周期翻转；补整段周期/边沿容差、固件依赖和复位。 |
| #002 | [get-started/hello_world](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/get-started/hello_world/unisim-scenarios/hello_world.scenario.json) | 部分覆盖 | 输出与倒计时有观察；未覆盖实际 restart 和重启后基线。 |
| #003 | [peripherals/adc_continuous_read](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/peripherals/adc_continuous_read/unisim-scenarios/peripherals_adc_continuous_read.scenario.json) | 需重判 | 断言直接读取注入 Map；未验固件采样、DMA、回调与采样格式。 |
| #004 | [peripherals/adc_oneshot_read](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/peripherals/adc_oneshot_read/unisim-scenarios/adc_oneshot_read.scenario.json) | 需重判 | 断言直接读取注入 Map；未验固件 raw、校准 mV 和采样变化。 |
| #013 | [peripherals/dac_dac_cosine_wave](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/peripherals/dac_dac_cosine_wave/unisim-scenarios/peripherals_dac_dac_cosine_wave.scenario.json) | 需重判 | 门面无波形输出，场景自行填写结果；补 8kHz、相位和幅度。 |
| #014 | [peripherals/dac_dac_oneshot](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/peripherals/dac_dac_oneshot/unisim-scenarios/dac_dac_oneshot.scenario.json) | 需重判 | 门面仅存数字值，场景自行填写结果；补真实阶梯电压输出。 |
| #020 | [peripherals/gpio_generic_gpio](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/peripherals/gpio_generic_gpio/unisim-scenarios/generic_gpio.scenario.json) | 正向轨迹 | 输出与中断日志有观察；补第二组回环、双边沿与中断次数。 |
| #023 | [peripherals/i2c_basic](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/peripherals/i2c_basic/unisim-scenarios/i2c_basic.scenario.json) | 部分覆盖 | WHO_AM_I 有观察；缺复位寄存器写入、NACK/超时和资源释放验证。 |
| #024 | [peripherals/i2c_i2c_eeprom](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/peripherals/i2c_i2c_eeprom/unisim-scenarios/peripherals_i2c_i2c_eeprom.scenario.json) | 正向轨迹 | 48 字节回读有观察；补页边界、busy、错误地址与失败后恢复。 |
| #047 | [peripherals/ledc_basic](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/peripherals/ledc_basic/unisim-scenarios/ledc_basic.scenario.json) | 部分覆盖 | 50% duty 稳态有观察；频率、引脚映射、更新时刻未验。稳态测试本身合法。 |
| #049 | [peripherals/ledc_ledc_fade](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/peripherals/ledc_ledc_fade/unisim-scenarios/peripherals_ledc_ledc_fade.scenario.json) | 需重判 | 3s 渐变被实现与场景验成瞬时跳变；需改实现及现有单测。 |
| #073 | [peripherals/spi_master_hd_eeprom](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/peripherals/spi_master_hd_eeprom/unisim-scenarios/spi_master_hd_eeprom.scenario.json) | 正向轨迹 | 字符串写入/读回有观察；补命令、地址、busy、页边界与写保护。 |
| #083 | [peripherals/gptimer_alarm](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/peripherals/gptimer_alarm/unisim-scenarios/gptimer_alarm.scenario.json) | 需重判 | 计数读全局时钟 fallback、频率恒为 1MHz；未验真实 alarm、停止与重载。 |
| #096 | [peripherals/uart_echo](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/peripherals/uart_echo/unisim-scenarios/uart_echo.scenario.json) | 正向轨迹 | RX 激励到 TX 业务回显有闭环；补精确长度、二进制、分包与缓冲边界。 |
| #098 | [peripherals/uart_uart_events](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/peripherals/uart_uart_events/unisim-scenarios/peripherals_uart_uart_events.scenario.json) | 部分覆盖 | 模式检测日志有观察；长度字符串过宽，缺普通 echo 与错误事件恢复。 |
| #125 | [system/esp_event_default_event_loop](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/system/esp_event_default_event_loop/unisim-scenarios/default_event_loop.scenario.json) | 正向轨迹 | 多 handler 与定时/任务事件有观察；补注销后不回调、重复事件和复位。 |
| #126 | [system/esp_event_user_event_loops](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/system/esp_event_user_event_loops/unisim-scenarios/system_esp_event_user_event_loops.scenario.json) | 正向轨迹 | 不同 loop 处理路径有观察；补完整 1–10 序列、不丢不重与 loop 隔离。 |
| #127 | [system/esp_timer](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/system/esp_timer/unisim-scenarios/system_esp_timer.scenario.json) | 部分覆盖 | 周期/单次回调阶段有观察；缺微秒精度、改周期后的间隔/次数与删除后的静默。 |
| #131 | [system/freertos_basic_freertos_smp_usage](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/system/freertos_basic_freertos_smp_usage/unisim-scenarios/system_freertos_basic_freertos_smp_usage.scenario.json) | 部分覆盖 | 已有 3 类交互命令；缺其余 lock/batch/core 路径，明确协作纤程模型的验证边界。 |
| #132 | [system/freertos_real_time_stats](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/system/freertos_real_time_stats/unisim-scenarios/real_time_stats.scenario.json) | 需重判 | runtime_counter 把 delay 当运行时间；场景只验表头/任务名。 |
| #151 | [system/startup_time](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/system/startup_time/unisim-scenarios/system_startup_time.scenario.json) | 部分覆盖 | 仅验证 App started!；不证明启动耗时或启动优化配置。 |
| #154 | [system/task_watchdog](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/system/task_watchdog/unisim-scenarios/system_task_watchdog.scenario.json) | 需重判 | 只有初始化/订退与喂狗记账；没有超时触发实现及饿狗验收。 |
| #186 | [protocols/http_client](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/protocols/http_client/unisim-scenarios/http_client.scenario.json) | 部分覆盖 | HTTP 路由已接通；只验聚合指标，缺请求身份、应用响应处理与完整业务流程。 |
| #206 | [protocols/mqtt_tcp](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/protocols/mqtt_tcp/unisim-scenarios/mqtt_tcp.scenario.json) | 需重判 | mqtt:rx:payload 是发送缓存别名，无法证明订阅接收。 |
| #221 | [wifi/fast_scan](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/wifi/fast_scan/unisim-scenarios/wifi_fast_scan.scenario.json) | 部分覆盖 | 单 AP 连接路径有观察；缺 fast/all 差异、同 SSID 多 AP、排序和阈值。 |
| #223 | [wifi/getting_started_softAP](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/wifi/getting_started_softAP/unisim-scenarios/wifi_getting_started_softAP.scenario.json) | 部分覆盖 | 初始化有观察；缺对端 join/leave、AID 和 AP 配置参数。 |
| #224 | [wifi/wifi_sta](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/wifi/wifi_sta/unisim-scenarios/wifi_sta.scenario.json) | 部分覆盖 | IP 分配与断网状态已有观察；缺应用 retry、失败 bit、重连与恢复。 |
| #230 | [wifi/scan](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/wifi/scan/unisim-scenarios/wifi_scan.scenario.json) | 正向轨迹 | AP 名和数量有观察；补 RSSI/auth/channel、排序、空列表与重复扫描。 |
| #247 | [bluetooth/ble_get_started_nimble_NimBLE_Beacon](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/bluetooth/ble_get_started_nimble_NimBLE_Beacon/unisim-scenarios/bluetooth_ble_get_started_nimble_NimBLE_Beacon.scenario.json) | 部分覆盖 | 只验广播态/名字；缺 AD/scan response 载荷、URL、间隔及 non-connectable 参数。 |
| #384 | [bluetooth/bleprph](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/bluetooth/bleprph/unisim-scenarios/bleprph.scenario.json) | 部分覆盖 | 只验广播/服务数量；缺 UUID/属性、GATT 读写、descriptor、notify/indicate。 |
| #401 | [storage/nvs_nvs_iteration](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/storage/nvs_nvs_iteration/unisim-scenarios/storage_nvs_nvs_iteration.scenario.json) | 正向轨迹 | 计数与类型样例有观察；补所有 key/value、筛选互斥与 iterator 生命周期。 |
| #402 | [storage/nvs_nvs_rw_blob](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/storage/nvs_nvs_rw_blob/unisim-scenarios/nvs_nvs_rw_blob.scenario.json) | 部分覆盖 | 部分字段有观察；缺完整 blob/数组、持久性；镜像含已确认的上游数组越界。 |
| #403 | [storage/nvs_nvs_rw_value](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/storage/nvs_nvs_rw_value/unisim-scenarios/nvs_nvs_rw_value.scenario.json) | 部分覆盖 | 当轮读写有观察；缺 commit/erase 失败及重启恢复验证。 |
| #415 | [storage/spiffs](D:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/storage/spiffs/unisim-scenarios/storage_spiffs.scenario.json) | 正向轨迹 | 写→rename→read→unmount 有观察；补重挂载、资源与文件容量、失败路径。 |

**建议的处理顺序**

1. 先完成证据核验器及观测出口的修复，使必要步骤缺失、固件禁用、业务变异能可靠报红；对于以上 9 项，在正式交付中按现有规则重新评估并保留历史有效证据，本轮不代改状态。
2. 补 DAC 输出、LEDC 时间渐变、TWDT 到期与运行时间计量，修正固化错误语义的单测；GPTimer 接真实导出，MQTT 分离 RX/TX。
3. 针对每个已交付配置绑定正向、已声明故障、固件依赖、有效业务变异、恢复基准与复位隔离记录。固定输入和正确预期，只修改指定业务，确认指定断言在契约窗口失败。
4. 完成 HTTP/BLE/Wi-Fi/NVS/UART 的领域闭环与边界验收，再按实际影响运行完整底座 CTest、分层/API lint 和适用治理门禁。
5. 在现有 executions 模型内分别记录仿真及硬件配置。对波形、计时、看门狗、NVS 掉电等关键能力补硬件差分；对协作纤程与物理 SMP 的能力边界如实说明。无需把所有示例同时扩大为全 SDK 支持。

本轮仅新增此评审记录，未修改任何实现、实施计划、审计身份、交付状态、凭据或看板。

