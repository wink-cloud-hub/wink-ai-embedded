<!-- SPDX-License-Identifier: GPL-3.0-only -->
# ESP-IDF Wave 1 修订评审复核与实施就绪度评审

| 项 | 内容 |
|---|---|
| 评审编号 | REVIEW-20261008-ESP-IDF-WAVE1-READINESS |
| 日期 | 2026-10-08，Asia/Shanghai |
| 对象 | [修订后的 Wave 1 评审](2026-10-08-esp-idf-wave1-network-completeness-and-maintainability-review.md) |
| 对象指纹 | 500 行，SHA-256 `624f770ed40d2536e32add1546cbb06609d32abd2978d98a121a68f6a945203b`；按本轮读取的文件字节计算 |
| 代码快照 | `c6b425b12a883aef1ae8850a4459cf109bc6852e`；四个主审网络实现与首轮 `6fdc5eb6` 一致，忽略 CRLF/LF |
| 模式 | Review；源码、契约、文档、只读门禁；不构建、不仿真、不晋升 |
| 结论 | 评审覆盖足够支持分批整改；需校准若干表述和补齐实施约束，不适合把全部建议作为一个未经拆分的代码任务 |
| 后续计划 | [Wave 1 分阶段整改实施计划](../../implementation-plans/esp32/2026-10-08-esp-idf-wave1-remediation-execution-plan.md)，待用户确认首批范围 |

## 1. 总体评价

修订版补充的描述符冲突、同步重入、单调时钟以及有限协议模型约束，均有助于实施人员理解共享底座风险。原有的实现/验收/治理三轴、七个配置、18 项发现、证据边界、变异矩阵与关闭条件已经较完整。

下一步应进入有明确退出条件的整改。继续追求“评审涵盖所有未来需求”不会证明实现正确，也容易扩大范围。尚需处理的重点是：风险陈述的证据等级、兼容层的实现边界、有界流失败语义，以及将建议映射为可独立验证的工作包。

本轮不覆盖或改写用户修订的原评审。以下补充作为新快照，与原评审共同构成实施输入。原评审中的历史检查结果继续保留，本轮结果单独记录。

## 2. 需要校准和补充的事项

### C-01 / P1：区分 `close()` 的确定错误与链接路径风险

**定位：** 原评审 S-05，137–143 行；[Sockets 实现](../../../wink-micro-os/frameworks/esp_idf/src/network/esp_sockets.c) 305–322 行。

**确定事实：** 仿真 `close()` / `closesocket()` 对没有找到的 Socket 返回 0，且前者是 Emscripten 分支内的全局函数。这不符合无效描述符应明确失败的边界。

**仍需验证：** “所有文件关闭都会经过此函数”“已经发生 flush 丢失或文件泄漏”不能仅由这段源码证明。本仓 [RAM VFS](../../../wink-micro-os/frameworks/esp_idf/src/core/esp_vfs_ram.c) 228–236 行另有 `esp_vfs_ram_close()`；[已有测试](../../../wink-micro-os/frameworks/esp_idf/test/run/test_esp_idf_safeguards.c) 95–139 行显式调用该命名 API，并未证明 libc `fclose()`、WASI `fd_close` 与 Socket `close()` 是同一调用链。文件系统、libc 与仿真 Socket 还可能拥有不同的 fd 空间。

**建议表述：** “未知 fd 被错误接受是静态确认缺陷；文件路径是否被截获，以及泄漏/数据完整性后果，需要真实链接与混合 I/O 实验确认。”

**补充验收：** 在同一 Wasm 镜像中检查符号解析和调用链，分别验证普通文件 `open/read/write/close`、stdio `fopen/fwrite/fclose`、RAM VFS、Socket 与无效 fd。检查关闭后句柄释放、错误码和必要的持久化结果，不把 `close()` 一律等同于 stdio 缓冲刷新。

### C-02 / P1：VFS 整改不能直接引入新运行期虚表架构

**定位：** 原评审 S-05 第 141 行与 §6.2 第 345 行。

“统一 VFS 虚表分发”的候选方案与本仓默认的静态分发约束存在需要裁定的张力。ESP-IDF 原厂内部机制可以作为兼容性参考，不能直接据此要求本仓新增一套运行期 ops/vtable。

本仓公开头/实现未找到 `esp_vfs_socket_register()`；本轮查阅的 [v6.1 VFS 文档](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32/api-reference/storage/vfs.html) 也未列出该名字。不能把未核实的示例接口作为可直接实现的依赖，更不能把原生 ESP-IDF 的注册函数视为 Emscripten 已有能力。

**建议：** 将要求收敛为“描述符归属明确、命名空间不冲突、非 Socket 调用能到达正确后端”。先评估局部适配/静态分发和实际工具链允许的符号方案；确需偏离默认架构时，另写 ADR 并回写活规范。公共 ESP-IDF 回调接口仍按原厂契约支持，不能因静态分发原则删除业务必需回调。

**实施保护：** 接入 Socket reset 时也须检查静态库链接边界。强引用 `sim_sockets_reset()` 可能把含 POSIX 覆盖和全局日志 constructor 的整个对象拉入其他 App。这是未来修复的待验证风险，不是本轮已发生故障；选择既有可选模块接入方式，并检查不使用 Socket 的固件是否意外增加该对象和行为。

### C-03 / P2：单调时钟约束应规定语义，保留 OSAL 与 Tick 分层

**定位：** 原评审 Q-01 第 201 行。

“超时不受 UTC 校正影响”正确，但“所有等待必须由 `esp_timer_get_time()` 驱动”过于限定实现。本仓 [vTaskDelay](../../../wink-micro-os/frameworks/esp_idf/src/freertos/freertos_task.c) 216–240 行已将 Tick 换算后基于 `pal_os_get_us()` 调度；[互斥等待](../../../wink-micro-os/frameworks/esp_idf/src/freertos/freertos_semphr.c) 170–208 行同样使用 PAL 单调时间。

[官方 FreeRTOS 契约](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32/api-reference/system/freertos_idf.html#_CPPv410vTaskDelayK10TickType_t) 规定 `vTaskDelay()` 的 Tick 语义。维持统一单调时间源以及正确的 Tick 换算即可，不需要为网络整改重写调度器或把内部依赖反向绑定到 ESP Timer 门面。

**补充验收：** UTC 前跳/后跳时等待期限稳定；边界采用既有 Tick 回绕测试，并补网络超时与授时交互。明确 cold boot、软件重启、deep sleep wake 的单调计数与墙钟保留规则，不能假定跨重启单调计数永不归零。

### C-04 / P1：异步重入风险需要条件化，并补工作项所有权

**定位：** 原评审 S-06 第 149–153 行。

`httpd_queue_work()` 同步调用 `work(arg)` 是静态确认的契约偏差。[官方契约](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32/api-reference/protocols/esp_http_server.html#_CPPv416httpd_queue_work14httpd_handle_t15httpd_work_fn_tPv) 要求在 HTTPD 上下文异步执行。

自死锁则依赖非递归锁、调用方持锁、回调再次阻塞取同锁及等待策略。不能仅凭“单线程 Wasm”判定所有调用必死锁，也不能承诺原生异步在任意持锁条件下安全：队列拥塞、调用方等待工作完成等也可能形成环形等待。

**新增实施约束：**

1. 工作由 HTTPD 的逻辑执行上下文消费，不能仅把回调推迟到任意主循环 Tick 或底层软定时器中执行；可阻塞回调不得占住不可阻塞的调度上下文。
2. 明确队列容量、满队列结果、顺序、入队失败回滚、重复 stop 和 reset 的行为。
3. 公共 `queue_work` 的 `arg` 是调用者传入的任意指针，不能通用地 `free(arg)`。原厂 WS 工作回调会自行释放特定参数；取消策略必须结合实际所有权，避免漏释放或重复释放。
4. 会话关闭与 fd 重用后，旧工作不得向新连接发送。内部代际检查与公共 fd 契约分别处理。

**补充验收：** 入队返回与回调执行有可检查的先后关系；持锁入队测试使用有限期限并能定位等待链；满队列、关闭、stop、reset 和 fd 重用不泄漏、不串连接。

### C-05 / P1：新增确定缺口——有界流错误返回前已写入前缀

**定位：** 原评审 §6.1 建议复用的 [sim_bounded_stream.c](../../../wink-micro-os/frameworks/esp_idf/src/network/sim_bounded_stream.c) 48–82 行；[容量声明](../../../wink-micro-os/frameworks/esp_idf/src/network/sim_bounded_stream.h) 为 4 × 1024 字节。

**静态路径：** 空流一次写入 4097 字节时，循环可先写满 4096 字节，再因申请第 5 块失败返回 `ESP_ERR_NO_MEM`。接口只返回 `esp_err_t`，没有告诉调用方已接受的前缀长度。如果把该错误理解为零提交再重试全部数据，可能重复或污染流。

另有 `data == NULL && len > 0` 返回成功的问题。HTTP client 的 [调用点](../../../wink-micro-os/frameworks/esp_idf/src/network/esp_http.c) 中，低级 `open()` 在 385–387 行忽略流写入结果，仍以完整 `response_len` 返回打开成功，后续读取可能只取得前缀。`perform()` 在 337–348 行则于流写失败后记日志，继续分发全长 ON_DATA 与 ON_FINISH。后者的事件数据直接来自完整响应对象，不能仅凭缓存失败就宣称事件消费者也丢了数据；需要分别校准事件消费与低级读取两种模式。新 TCP/HTTP 模型未经审查直接复用此实现，会继续带入部分提交与结果不一致的问题。

这些是静态确认的路径；本轮未运行 4097 字节实验，未声称已复现某个应用的重复数据。

**首批推荐方案：** 保持现有内部 `esp_err_t` 接口，预检可接收容量；失败时不提交任何新载荷，合法零长度写入明确处理。HTTP client 低级读取模式对失败明确返回/上报；事件模式区分完整业务交付与诊断缓存，不因有限诊断缓存单独失败而破坏合法流式消费。后续真正需要部分写入时，再设计明确返回已接受字节数的内部接口，并同步审查全部调用者。

**验收：** 空流和非空流分别覆盖容量减一、容量、超容量；错误后原有字节与顺序不变；读出并重试不会重复；空指针非零长度明确失败；HTTP 回调、读取长度与最终结果一致。

### C-06 / P2：避免永久技术禁令，补整机资源预算

**定位：** 原评审 §6.2 第 343 行与 §6.4。

本轮采用有限状态机与有界字节流是合理选择。“严禁以后移植完整 lwIP”则属于长期架构决策，当前评审没有比较所有未来产品需求。建议改为“本轮不引入完整协议栈；后续确有底层协议保真需求时按 ADR 比较收益、体积和维护代价”。

每流 4096 字节不等于整机只占 4096 字节。假设 8 个连接各有 TX/RX 两个流，仅载荷池就达到 8 × 2 × 4096 = 65536 字节，还未计队列、会话和元数据。该算式是预算示例，不是当前实现占用测量。

**补充要求：** 按实际配置/profile 记录连接数、每向缓冲、队列、静态 BSS、峰值栈和必要 heap；禁止在请求处理中动态扩容掩盖背压。容量不足必须有可验收结果。固件双 target 的目标不意味着必须把仅仿真模型的内存池搬到原生 ESP32。

### C-07 / P2：收紧日志边界表述，落实问题到关闭记录

原评审 S-02 的核心缺陷成立，但 512 字节数组中 `vsnprintf()` 返回 512 时，UART 读取 512 字节仍在数组内，只是包含结尾 NUL、长度语义不对；返回值大于 512 时才会跨越数组边界。建议区分“超过有效文本长度”和“越过数组边界”，测试分别覆盖 511、512、513。

实施计划至少记录“问题 ID → 工作包 → 测试 → 修复提交 → 复验记录”的关系。部分修复必须写明剩余内容：只清 HTTP 路由不能关闭完整 S-06，只接 Socket reset 不能关闭 SNTP reset 缺口，只补错误返回不能宣布全部 BSD 语义完成。

## 3. 是否直接执行

**建议：开始首批正确性整改；协议扩展和正式证据交付分阶段进行。**

| 范围 | 建议 | 原因 |
|---|---|---|
| 首批：WS 边界、日志长度、HTTP stop/start 与 Host、流写入失败一致性 | 按独立实施计划确认后执行 | 缺陷明确，能够用有区分力的局部回归证明，暂不需要新增跨仓 ABI |
| Socket / SNTP reset 与 VFS 分发 | 先完成链接、保留表和描述符调查，再落修复 | 牵涉模块装载、RTC、持久化与 libc，不能机械清零或强拉全局覆盖对象 |
| TCP 对端、WS 会话/异步、SNTP 对端、AP 转发 | 先确定有限模型、上下文与公开观察契约，随后逐能力实施 | 当前评审描述的是目标，不是已存在的可执行接口 |
| 凭据身份、场景集合、事务晋升 | 在首批代码修复期间并行细化，交付前闭环 | 防止继续依赖同计数报告和共享历史文件；不必阻塞所有局部源码修复 |
| 真机与组合验证 | 相关单能力稳定后执行 | 独立配置实证，避免将 Wasm 成功外推为硬件交付 |

另一个 [Twin-Proof 试点计划](../../implementation-plans/esp32/2026-10-08-esp-idf-zero-modification-twin-proof-breakthrough-plan.md) 仍标为待确认，且只与本轮七项中的 #196 重叠。它不能替代 Wave 1 底座整改。环境故障、有效业务变异、固件依赖、人工审计仍须分别证明；同产物故障试验不等于已完成业务变异敏感性验证。本轮未对该计划作全面评审，也未授权填写审计身份或晋升徽标。

## 4. 本轮验证记录

| 检查 | 结果 | 限制 |
|---|---|---|
| 四个主审网络实现与初轮提交比较 | 均一致，忽略 CRLF/LF | 文档修订尚未对应这些代码修复 |
| 修订评审本地引用 | 未发现断链 | 链接存在不表示相关契约已实现 |
| Gate 1 | 12/12 PASS，0 SKIP，退出码 0 | 只证明当前规则接受当前登记与文件 |
| Evidence verifier | 46/46 verified entries passed，退出码 0 | 不代表本轮重放、有效变异或硬件测试完成 |

用于记录的 Gate 1 运行时间为 `2026-10-08T00:36:48.341748+00:00`，上海时间 08:36:48。初轮 #122 的历史失败不再作为当前阻断；初轮 45/45 与本轮 46/46 各自保持时间点含义。

实际只读命令：

```powershell
python -X utf8 -B wink-micro-app/vendor/esp_idfv61/.governance/gates/run_gates.py --gate 1
python -X utf8 -B wink-micro-app/vendor/esp_idfv61/.governance/gates/evidence_verifier.py --verify-all
```

未执行构建、Headless 重放、内存检测、混合 I/O 链接实验、业务变异、ESP32 烧录或硬件差分。没有修改 `delivery_state`、资产、场景或历史凭据，没有代替人工审计签署。

## 5. 后续文档与决策边界

依据 [AGENTS.md](../../../AGENTS.md) 第 54 行：“复杂变更必须先产出实施计划（Implementation Plan）并获得用户确认。”本轮已准备具体计划；当前用户请求是复核与实施建议，尚未视为对全部复杂代码整改的确认。

首批缺陷修复维持既有 API 和分层，不需要为每个数组边界补丁新增 ADR。VFS 架构例外、公开 ABI、异步上下文与复位保留策略若形成新的长期选择，则走 ADR/技术设计流程。每阶段完成后用新复验记录关闭对应范围，不反复扩写已归档评审。
