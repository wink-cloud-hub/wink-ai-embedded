<!-- SPDX-License-Identifier: GPL-3.0-only -->
# ESP-IDF Wave 1 网络示例功能完整性与长期可维护性评审

| 项 | 内容 |
|---|---|
| 评审编号 | REVIEW-20261008-ESP-IDF-WAVE1-NETWORK |
| 日期 | 2026-10-08，Asia/Shanghai |
| 文档性质 | 独立评审快照；问题、改进建议与验收条件归档 |
| 评审者 | Codex AI 辅助评审；不代表人工架构签署 |
| 评审模式 | Review：源码、场景、配置、历史报告与只读门禁 |
| 首轮评审 HEAD | `6fdc5eb6fef8a3953338e77fa513f07bb6f0f04b` |
| Wave 1 变更比较基线 | `f891b40090edd6ec2f2243d2c366dc3fd079f6a3`；Wave 1 最后一次晋升提交为 `65c1fb39` |
| 整理时观察到的 HEAD | `c6b425b12a883aef1ae8850a4459cf109bc6852e`；期间存在 deep_sleep 等并行提交 |
| 主审范围 | #195、#196、#197、#209、#212、#214、#232，精确配置均为 `wasm_sim_standard` |
| 本轮写入范围 | 仅本评审文档；不修改代码、场景、资产、清单、审计或交付凭据 |
| 状态解释 | 评审意见已记录；改进方案尚未实施，也不等同于 Accepted ADR 或批准的实施计划 |
| 后续维护 | 交付后作为历史快照保存；整改执行另建实施计划，复验另建记录，不回改本轮结论 |

关联依据：

- [高 ROI 执行计划](../../implementation-plans/esp32/2026-10-07-esp-idf-high-roi-checklist-execution-plan.md)。
- [CLASSIFICATION-SPEC](../../../wink-micro-app/vendor/esp_idfv61/.governance/specs/CLASSIFICATION-SPEC.md)、[PLAYBOOK](../../../wink-micro-app/vendor/esp_idfv61/.governance/specs/PLAYBOOK.md)。
- [governance-sop-esp](../../../.agents/skills/governance-sop-esp/SKILL.md)、[领域断言指南](../../../.agents/skills/governance-sop-esp/references/domain-assertion-guide.md)。
- [内存安全](../../../.agents/skills/_embedded-shared/memory-safety.md)、[并发约束](../../../.agents/skills/_embedded-shared/concurrency.md)。
- [ADR-0012：契约诚实](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)、[ADR-0003：仿真保真边界](../../decisions/unisim/0003-simulation-fidelity-boundary.md)。
- [ADR-0091：多配置实例](../../decisions/unisim/0091-esp-idf-multi-config-orthogonal-schema.md)、[ADR-0092：示例治理与能力宪章](../../decisions/unisim/0092-esp-idf-simulation-governance-and-capability-charter.md)。
- [既有防假绿计划](../../implementation-plans/esp32/2026-10-01-anti-false-green-verification-plan.md)、[门面加固计划集](../../implementation-plans/esp32/facade-hardening/README.md)。已有计划的完成声明不代替本轮源码核查。

## 1. 总体判断与证据边界

**Wave 1 已完成有价值的示例接入和部分正向业务回归，但尚未完整实现计划定义的网络闭环，也不足以证明网络底座已经具备长期批量扩展所需的契约完整性。**

现有工作值得保留的部分包括：原厂业务源码与适配层分离；确定性场景和独立资产已经落盘；HTTP 有真实处理函数出口；Wi-Fi 存在事件与 Netif 基础；静态池和有界缓冲便于控制仿真资源。这些基础适合继续加固，无需重做全部运行时。

当前限制集中在三个方面：

1. 通用门面包含固定对端输入、回显和授时结果，环境行为与被测实现的职责没有完全分开。
2. 存在内存边界、停止重启、异步执行、错误返回和复位清理方面的明确实现缺陷。
3. 场景和归档主要证明正向路径；协议分支、故障、有效业务变异、恢复与运行配置身份尚未形成完整证据。

本报告严格区分以下口径：

| 口径 | 本轮能够确认的事实 | 不能外推的结论 |
|---|---|---|
| 登记状态 | 七项的 `wasm_sim_standard` 均登记为 `verified` | 全部配置、全部 ESP32 衍生芯片或真机已交付 |
| 归档报告 | 七份正向报告合计 53 个步骤通过，失败、跳过、错误计数均为 0 | 53 个完整业务断言、全部场景执行、故障和变异闭环 |
| 资产核验 | 初轮现有核验器输出 `45/45` 通过，包含七个 Wave 1 配置 | 全部领域语义与配置运行身份得到证明 |
| 源码镜像 | 九个原厂 `.c` 与各自 Manifest 锁定 SHA-256 一致 | 已重新从官方仓库独立证明上游来源与版本身份 |
| 静态分析 | 本文可定位的实现缺陷与测试缺口 | 已通过运行实验复现崩溃、泄漏或协议失效 |

### 1.1 快照与并行工作

正文源码行号以首轮 HEAD `6fdc5eb6` 中的文件为准。相对链接打开的是工作区文件，后续行号可能移动；复核时应结合提交快照和附录哈希。

整理时重新比较确认，`esp_http_server.c`、`esp_sockets.c`、`esp_sntp.c`、`esp_netif.c` 的工作区内容与首轮提交一致，忽略 CRLF/LF 差异。复位桥、运行脚本和能力目录已有并行改动，因此这些文件的结论明确限定在首轮快照，不把后续状态混入历史判断。

Wave 2 的 OTA、deep_sleep，其他示例资产更新，以及跨仓组件内部实现均不属于本轮主审范围。对 UniSim 和工具链仅使用本仓公开契约、脚本和报告，不推断外仓内部源码。

### 1.2 问题分类

- **S：Standards / 实现契约**。初轮五组主要问题在落档时细分，并补核 HTTP 生命周期与输入边界，共九项。
- **Q：Spec / 计划验收**。六项，分别评价要求与证据是否匹配，不与 S 轴合并计数。
- **G：治理与规划补充**。三项，不视为已经复现的运行故障。
- **P1**：应优先修复的正确性问题，或阻碍完整验收结论的关键缺口。
- **P2**：后续扩量前应明确或补齐的维护、范围与规划问题。

“静态确认”表示可由代码或配置直接证明；“历史观察”表示归档已有实际输出；“待实验”表示后果或杀伤效果需要后续隔离运行验证。优先级不表示经过量化的生产事故概率。

## 2. 七个示例的登记与覆盖情况

下表七项的配置身份均为 `config_id=wasm_sim_standard`、`backend=wasm_browser`、`target_soc=esp32`、`profile=standard`。这些是登记信息；报告尚未充分绑定实际运行身份，见 Q-06。

| 编号 | App 相对 vendor 根目录 | 原厂 `.c` 数量 | 归档结果数 / 步骤数 | 本轮完成度判断 |
|---|---|---:|---:|---|
| #196 | `protocols/http_server_simple` | 1 | 1 / 7 | HTTP 处理函数与最终 echo 有正向证据；头部、每请求响应及边界不足 |
| #195 | `protocols/http_server_restful_server` | 2 | 1 / 7 | REST API 子集；静态页面关闭，mDNS 和文件系统能力未证明 |
| #197 | `protocols/http_server_ws_echo_server` | 1 | 1 / 7 | 文本处理与推送示例路径；会话、二进制、握手和故障仍不完整 |
| #232 | `wifi/softap_sta` | 1 | 1 / 11 | 双模式启动与 STA 状态；AP 客户端及跨接口转发未闭环 |
| #209 | `protocols/sntp` | 1 | 1 / 8 | 固定授时及事件调用路径；真实环境驱动同步、重试未闭环 |
| #212 | `protocols/sockets_tcp_client` | 2 | 1 / 6 | 内置 echo 对端下的收发、断连路径；通用传输语义不足 |
| #214 | `protocols/sockets_tcp_server` | 1 | 1 / 7 | 接收和关闭日志；对端是否收到正确回显未被断言 |
| 合计 | 七项 | 9 | 7 / 53 | 均不能由单一正向报告推出完整验收 |

## 3. Standards：明确实现问题与改进方案

### S-01 / P1：WebSocket 接收在容量边界多写一个字节

**证据与状态：静态确认，未运行内存检测。** [HTTP 门面](../../../wink-micro-os/frameworks/esp_idf/src/network/esp_http_server.c) 432–446 行复制 `min(body_len, max_len)` 字节后执行 `pkt->payload[to_copy] = '\0'`。原厂 [WS 示例](../../../wink-micro-app/vendor/esp_idfv61/protocols/http_server_ws_echo_server/ws_echo_server.c) 159–168 行分配 `len + 1`，使现有示例不能暴露该问题。

**触发与影响：** 调用者只分配 `max_len` 字节，帧长度达到容量时越界写一字节。二进制接收也被强制添加文本终止符，破坏调用者的容量契约。不能要求所有应用额外分配一字节来迁就门面。

**改进方案：**

1. 公共帧 API 只复制载荷字节，不添加字符串终止符；字符串处理留给应用。
2. 对空 payload、容量不足和查询长度模式分别按原厂契约处理；不以截断成功掩盖容量不足。
3. 保留真实 opcode 与长度，配合 Q-03 补二进制路径。

**验收条件：** 用恰好 N 字节的缓冲接收 N 字节，边界哨兵不改变；覆盖 0、1、容量减一、容量、超容量；二进制包含 `0x00` 时逐字节一致。内存检测或等价边界检查通过，原厂文本 echo 仍通过。

### S-02 / P1：日志截断后的长度用于 UART，造成越界读

**证据与状态：静态确认。** [Sockets 门面](../../../wink-micro-os/frameworks/esp_idf/src/network/esp_sockets.c) 24–39 行用 `char buf[512]` 格式化，把 `vsnprintf()` 返回值 n 直接传给 `pal_uart_write()`。相同逻辑存在于 [SNTP 配置](../../../wink-micro-app/vendor/esp_idfv61/protocols/sntp/include/sdkconfig.h)、[AP+STA 配置](../../../wink-micro-app/vendor/esp_idfv61/wifi/softap_sta/include/sdkconfig.h) 和 TCP client/server 配置头。

**触发与影响：** 格式化期望长度达到或超过 512 时，n 超过缓冲内有效文本长度，UART 读取栈外数据。普通短日志测试不能证明该路径安全。

**改进方案：** 区分格式化返回值与实际存储长度；仅发送缓冲中有效字节，处理负返回值。把 stdout/UART 桥接集中到公共 console 层，统一初始化、截断策略和返回语义；`sdkconfig.h` 保持宏配置职责，消除多个 constructor 竞争全局日志钩子的隐式耦合。

**验收条件：** 空日志、边界长度、长日志、格式化失败和可控 UART 写失败都有明确结果；不得读取哨兵区。各 App 不再复制桥接实现，安装钩子的顺序不改变观察行为。公共实现单独验证一次，应用仅保留必要集成验证。

### S-03 / P1：SNTP 与 TCP 状态未纳入软复位清理

**证据与状态：静态确认遗漏；具体残留后果待实验。** 首轮 [复位桥](../../../wink-micro-os/frameworks/esp_idf/src/esp_idf_bridge.c) 134–163 行处理 HTTP、Wi-Fi、Netif、事件与定时器，但未调用 Sockets 的 reset；`esp_sockets.c` 81–87 行虽提供 `sim_sockets_reset()`，本仓未找到调用者。SNTP 没有清理所有状态、服务器、回调和时钟锚点的完整 reset 入口。

**触发与影响：** 在连接占用、故障已注入或 SNTP 已初始化时软复位，可能留下连接槽、fault、同步标记或旧回调。每场景新建 Wasm 实例可遮蔽同实例重启缺陷，不能代替软复位验证。

**改进方案：**

1. 接入既有复位 DAG，在调度器边界取消异步工作，清理连接与协议会话、旧回调、故障注入和计数。
2. 明确 cold boot、软件重启、deep sleep wake 的保留表。RTC 时间和已提交持久化数据不能一概清零；保留时间时须处理单调时钟重置后的锚点衔接。
3. 旧连接的异步工作通过内部代际标识作废，避免 fd 重用后投递到新会话。保持公共 BSD fd 语义，不随意改公共接口。
4. 对象初始化、失败回滚、stop、reset 的资源所有权分别定义，不能只清计数而遗漏回调或资源。

**验收条件：** 在连接中、授时等待中、异步待发送时重启；重启后无幽灵回调，池占用回到声明基线，fault 清理。连续重复重启不累积资源；RTC 按各 reset 类型的保留契约验证，而非只检查数值归零。

### S-04 / P1：固定对端业务写入通用网络门面

**证据与状态：静态确认。** `esp_sockets.c` 177–190 行在 `accept()` 自动生成 `Data to ESP`；218–225 行在客户端 `send()` 自动写回接收缓冲；249–253 行固定第四次 echo 断线。[SNTP 门面](../../../wink-micro-os/frameworks/esp_idf/src/network/esp_sntp.c) 225–246 行固定生成 2026-10-07 的时间。

**触发与影响：** 没有真实虚拟对端也能收包或授时；目的端点、载荷和断连时间无法作为独立环境变量。迁移到其他协议需要改底座，难以判断测试是否依赖实际应用处理。

**改进方案：** 把 echo、主动客户端和 NTP 服务器作为可复用 fixture/peer 行为，显式绑定端点、Netif、输入字节和虚拟时序。门面只处理通用协议状态与 API。连接就绪依赖既有 network broker；默认环境无对端时按契约等待或失败。确定性由固定输入和种子保证，无需硬编码业务输出。

**验收条件：** 不提供对端时不会凭空收包、连接或授时；改变对端地址、端口、输入、响应时间及关闭策略能改变结果；同一底座无需源码修改即可运行 echo 与另一个不同载荷协议。原厂业务源码保持不变。

### S-05 / P1：未实现的 Socket / DHCP 操作静默成功

**证据与状态：静态确认。** `esp_sockets.c` 325–340 行的 `setsockopt()` 没有保存选项，`getsockopt()` 不写输出却返回 0；[Netif 门面](../../../wink-micro-os/frameworks/esp_idf/src/wifi/esp_netif.c) 165–175 行忽略 DHCP option 操作与选项。`close()` 315–322 行对非仿真 socket fd 也直接返回成功。

**触发与影响：** 超时、keepalive 等配置看似生效，实际没有行为；查询结果可能是旧值。更关键的是 POSIX 符号拦截（Shadowing）与 VFS 冲突隐患：在 Emscripten/Wasm 环境下直接导出全局强符号 `close(int fd)` 会遮蔽 libc 标准符号；若上层业务或系统组件通过 VFS/LittleFS 打开了文件（如 fd=3），调用 `close()` 时因非仿真 socket 被门面直接返回 0，底层文件系统句柄和缓冲无法 flush 与真正释放，造成静默文件描述符泄漏与数据截断。这超出了简单 errno 缺陷，影响整个运行时的 I/O 契约。

**改进方案：** 列出最小受支持选项并实现相应状态和效果；不支持的组合通过原厂允许的 errno/错误码明确失败。查询 API 校验指针和长度并真实写回值。严禁在全局无条件强行覆写标准 POSIX `close()` 强符号；Socket 描述符分发应遵循 ESP-IDF 原厂架构接入统一的 VFS 文件描述符分发机制（如通过 `esp_vfs_socket_register()` 或统一 VFS 虚表分发）或采用内部命名空间映射，严格区分 Socket fd 与文件/标准流 fd，避免未知描述符被吞掉。

**验收条件：** 支持选项可设置、查询且产生对应行为；未知选项明确失败；无效 fd 不被错误接受；文件与 socket 混合读写关闭集成回归通过。不得仅把所有调用统一返回成功以维持示例全绿。

### S-06 / P1：HTTP 停止和异步接口缺少会话生命周期语义

**证据与状态：静态确认。** `esp_http_server.c` 134–141 行的 stop 仅设置停止标志，未清空路由和上下文；149–154 行重复注册会报 handler exists。421–429 行所有有效请求返回 fd=1，队列接口直接同步执行工作；493–504 行异步发送忽略服务器和 fd。

**触发与影响：** stop/start 后重新注册可能失败，旧 user_ctx 可能仍被引用；多个连接无法独立定位；非法或已关闭连接的发送也可能被接受。更严重的系统级风险是**同步重入自死锁（Reentrancy Deadlock）**：原厂契约明确要求 `queue_work` 投递给 HTTPD 后台任务异步执行（见第 10 节）；当前门面直接在调用者栈中同步执行 `work(arg)`。若调用方在持有互斥锁的临界区内排队工作，而 `work` 回调内部也需获取该不可重入锁，真机跨任务异步原本安全，在单线程 Wasm 仿真中将直接导致同线程自死锁；反之，同步直调也彻底掩盖了真机多任务环境下的并发时序与数据竞争。

**改进方案：** 建立有界会话池和单一分发上下文；工作项入队应挂载至任务队列，在调度边界或主事件循环 Tick 中异步派发，严格与入队调用栈解耦，避免同线程重入死锁。实现 stop 的路由、session、队列、上下文清理；核对 free 回调所有权，避免双重释放。若短期只支持单服务器，应明确拒绝二次启动，不能让多个 handle 静默指向同一实例。

**验收条件：** start/register/stop/start 后能重新注册并使用新上下文；工作不在入队调用栈同步执行；停服或断连后的排队工作按契约取消或失败；两个连接不会串响应。HTTP API 本身并非全部线程安全，不要求凭空增加全局并发保证，重点是保持已声明的上下文与顺序契约。

### S-07 / P1：HTTP 响应超过容量被截断却返回成功

**证据与状态：静态确认。** `esp_http_server.c` 337–363 行把响应体截到 2047 字节；366–382 行累计 chunk 到容量后继续返回成功。最后响应观察又缩短到 512 字节数组。

**触发与影响：** 大页面、JSON、分块下载或长 WS 数据可能只保留前缀。不能区分“观测摘要被截断”和“传输本身被截断”。未来文件托管、OTA 流式数据和大消息更容易受影响。

**改进方案：** 业务数据用有界字节流传输，观察摘要与传输存储分开；容量不足时背压、分块推进或按契约明确失败。定义总字节数、消息长度和摘要各自含义，避免统计声称发送完整数据但对端只能得到前缀。

**验收条件：** 响应跨过 512、2048 字节边界和多个 chunk 后，对端字节、顺序、长度或摘要校验一致；超容量路径无静默丢弃；空 chunk 结束语义正确，取消和 reset 后资源回到基线。

### S-08 / P1：HTTP 注入边界丢失请求头与部分 JSON 语义

**证据与状态：静态确认。** `esp_http_server.c` 697–706 行先将 Host 写进 `s_curr_aux`，随后调用 dispatch；dispatch 在 522–523 行清空同一结构，因此处理函数读不到该 Host。620–636 行用字符串搜索提取 JSON 值，675–684 行统计花括号时不区分字符串内容；544–550 行未知方法默认当 GET。

**触发与影响：** 依赖 Host 的分支不会按输入执行。合法 body 含 `}` 或 Unicode 转义时，手写解析可能截断或错误解码；不支持的方法可能被错误派到 GET。当前只检查最终 echo，不能覆盖这些问题。

**改进方案：** 构造完整请求对象后一次交给分发器，初始化必须发生在填充数据之前。采用已有、固定版本的可靠解析能力，或由公开契约传递类型化、长度明确的数据；避免再次手工维护通用 JSON 解析器。未知方法明确拒绝，不能回退 GET。新增请求/响应身份字段时先做契约变更，不把草案字段当已支持场景语法。

**验收条件：** 固件处理函数读取到注入 Host 和其他受支持头部；覆盖 body 含引号、反斜杠、花括号、Unicode 与长值；未知方法不会触发 GET handler；各请求的字段在连续分发时不串用。

### S-09 / P2：App 私有辅助桩与自维护库增加行为分叉

**证据与状态：静态确认；覆盖程度需逐 API 确认。** [SNTP 网络辅助头](../../../wink-micro-app/vendor/esp_idfv61/protocols/sntp/include/protocol_examples_common.h) 中 `example_connect/disconnect()` 直接成功。[REST mDNS](../../../wink-micro-app/vendor/esp_idfv61/protocols/http_server_restful_server/include/mdns.h) 返回成功而不创建可观察服务；[LittleFS 头](../../../wink-micro-app/vendor/esp_idfv61/protocols/http_server_restful_server/include/esp_littlefs.h) 存在固定成功实现，但当前静态网页分支关闭，不能说这些文件系统调用已被业务执行。

[REST 自维护 cJSON](../../../wink-micro-app/vendor/esp_idfv61/protocols/http_server_restful_server/support/cJSON.c) 258–271 行仅实现部分值类型；111–115 行直接接收 `realloc` 结果，扩容失败未处理。简化实现与原库的解析、转义和分配失败语义不等价。

**影响：** 原厂 `.c` 哈希不变，仍可能因 App 内辅助头和库改变而获得不同业务语义。纯离线 HTTP handler 测试可以有明确的离线前置条件，但不能同时宣称 Wi-Fi 连接、发现服务和文件系统也通过集成验收。

**改进方案：** 收敛重复辅助实现到受治理的公共适配层；对需要的连接与发现能力建立真实状态出口，对暂未支持的能力明确失败或采用已获准的编译配置裁剪。cJSON 优先采用可溯源的固定版本依赖，保留许可和 NOTICE；若继续保留子集，须明确 API/语法范围并处理 OOM，不能只保留相同函数名。

**验收条件：** 已启用路径不依赖空成功桩；离线子集与网络集成配置分别登记和验收；辅助头纳入来源/闭包管理。JSON 合法输入、失败输入、转义和分配失败具有可检查结果，不能靠修改原厂业务掩盖依赖差异。

## 4. Spec：计划要求与现有证据的差距

### Q-01 / P1：SNTP 尚未形成环境驱动的授时与重试闭环

**对应要求：** 执行计划 §4 / 1.4 要求虚拟 NTP 服务器注入固定时间，验证 `time_t` 对齐和无网络时优雅重试。

**证据：** `esp_sntp.c` 249–265 行在等待函数中直接调用固定授时；没有检查指定 NTP 对端、网络就绪或响应到达。`tout=0` 仍能触发同步。`adjtime()` 忽略 delta 返回成功，smooth 模式也未形成实际校正过程。更隐蔽的系统风险在于**墙钟跳变与网络重试超时计算溢出**：若时间从 1970 初始纪元突然跳变为目标固定时间（如 2026 年），依赖 `time(NULL)` 计算相对超时的第三方网络库（如某些 HTTP/MQTT client）在执行重试间隔差值计算时会出现严重下溢或溢出，引发状态机假死或重试风暴。

**历史观察：** [SNTP 归档](../../../wink-micro-app/vendor/esp_idfv61/.governance/reports/protocols/sntp/run-report.json) 中纽约、上海都打印 `Wed Oct 7 18:15:30 2026`；[正向场景](../../../wink-micro-app/vendor/esp_idfv61/protocols/sntp/unisim-scenarios/protocols_sntp.scenario.json) 只匹配两个时区提示前缀。这里能够确认的是错误输出未被场景拦截，具体 libc/TZ 根因未在本轮定位。

**改进方案：**

1. 网络请求或服务行为由虚拟 NTP 对端驱动；等待函数只等待同步事件，不生成结果。
2. 分开验证初次同步、无网络/无响应超时、恢复后的同步及回调次数。优先实现 immediate 模式；smooth 模式若保留声明就实现语义，否则明确其支持边界。
3. 独立建模单调时钟和 UTC 墙钟。系统调度、`vTaskDelay`、Socket 等待及网络重试超时必须纯由单调时钟（`esp_timer_get_time()`）驱动，严禁与 UTC 墙钟混淆；授时调整不得使单调等待截止时刻发生跳变。
4. 时区验证断言完整输出或结构化值；避免受宿主本地时区影响，并按 UTC 锚点验证两个时区的各自结果。

**验收条件：** 无对端或超时不能同步成功；零等待不制造授时；响应在虚拟期限后到达时先超时，再按恢复规则同步。`time_t`、微秒部分、事件数据和回调一致；阻断同步核心行为后正向断言失败，恢复后通过。具体对端输入和观察通道须先验证公开契约可执行。

### Q-02 / P1：AP+STA 的独立接口与转发尚未被证明

**对应要求：** 执行计划 §4 / 1.3 要求双 Netif 独立 IP、状态机转换和路由转发因果。

**证据：** `esp_netif.c` 190–211 行的 NAPT 只保存布尔标记，未找到消费该标记的转发数据路径。[AP+STA 场景](../../../wink-micro-app/vendor/esp_idfv61/wifi/softap_sta/unisim-scenarios/wifi_softap_sta.scenario.json) 主要验证启动日志、STA IP 和状态，没有 AP 客户端经过设备访问上游的出口证据。

**改进方案：** 先建立独立 AP/STA 链路、地址与事件，再把 AP 对端输入经设备路由/NAPT 处理后送到 STA 对端；响应沿对应路径返回。NAPT 不一定要复制真实协议栈全部细节，但至少要有受接口状态、地址映射和开关控制的数据面。

**验收条件：** AP 客户端获得来自设备 DHCP 的地址；AP、STA 地址及断连事件独立；AP→STA 请求和返回的端点、载荷可观察。关闭 NAPT、移除上游或选错出口会使该转发验收失败，STA 恢复后转发恢复。AP 本地服务是否继续可用按明确产品契约判断。

**范围说明：** RF 射频细节、吞吐量或芯片级信道性能不是本轮已验证能力；双模式初始化通过不能代表中继网关能力完整。

### Q-03 / P1：WebSocket 的帧、握手、异步与异常路径不完整

**对应要求：** 执行计划 §4 / 1.2 要求握手升级、文本与二进制帧双向收发，以及畸形帧或超时断开。

**证据：** 门面把所有输入标记为 TEXT；dispatch 589–603 行直接调用握手回调和业务 handler。[WS 场景](../../../wink-micro-app/vendor/esp_idfv61/protocols/http_server_ws_echo_server/unisim-scenarios/protocols_http_server_ws_echo_server.scenario.json) 用 HTTP GET/body 表示帧，只检查最后一次 `Async data`。非法 Token 的 403 场景属于鉴权拒绝，不能代替帧畸形或连接超时。

**改进方案：**

1. 先解决 S-01 与 S-06，建立连接级 `HTTP → WS_OPEN → CLOSING → CLOSED` 状态和唯一会话身份。
2. 区分握手与数据帧，不对每个数据消息重复调用握手回调；保留 opcode、FIN/fragment、字节长度和二进制内容。
3. 补 text、binary、Ping/Pong、Close，以及承诺支持的分片路径；异步发送投递到正确连接。
4. 明确保真层：若场景在解码后的帧层注入，可证明应用处理行为；若要声明原始握手/帧解析正确，则必须经过对应解析实现并验证 Upgrade、mask、长度等规则。不能用 handler 直接调用替代线协议验证。

**验收条件：** 文本与含 NUL 二进制分别逐字节回显；握手每会话只发生一次；两个连接不串帧；排队后关闭的发送失败或取消。选定畸形输入与超时导致声明的关闭行为，恢复新连接仍可工作。原始帧层不支持的范围明确记录，不能列为已验收。

### Q-04 / P1：HTTP / REST 的验收小于计划承诺

**对应要求：** 执行计划 §4 / 1.1 包含静态网页与 REST 控制，并明确要求异常 URI 404、异常 Payload 400。

**证据：** [REST 配置](../../../wink-micro-app/vendor/esp_idfv61/protocols/http_server_restful_server/include/sdkconfig.h) 第 7 行关闭静态页面部署。Simple 注入 `/hello` 和 `/echo`，最终仅检查 `/echo`；REST 注入 system/temp/brightness，最终仅检查 brightness 的固定成功字符串。URI 404 不证明 body 校验与 400。

**原厂边界：** [REST 原厂处理函数](../../../wink-micro-app/vendor/esp_idfv61/protocols/http_server_restful_server/rest_server.c) 124–127 行直接解引用 JSON 对象字段，缺字段或解析失败不天然保证 400。计划的防御性要求与原厂示例本身能力需要协调，不能修改镜像或让门面凭空补出业务 400。

**改进方案：**

1. 每个请求分别绑定响应状态、必要头部和 body；system/info 解析字段，温度按原厂语义验证范围，brightness 验证输入确实进入处理逻辑。
2. 将“REST API 子集”和“启用静态页面”作为明确配置/验收范围。后者接真实文件后端、正确 MIME 和完整 chunk；缺失该能力时如实记录。
3. 对原厂不保证的非法 JSON 路径记录实际行为与限制；需要 400 的产品化应用另行编写防御层并独立验收，或按治理流程修订计划范围。
4. 修复 S-07、S-08，补长载荷、头部和停服恢复。

**验收条件：** 改坏任意一个被承诺的 handler 后对应断言失败，不能被最后一次成功覆盖；有效 GET/POST 的每次响应可复核。静态配置能完整取文件，异常 body 行为与已批准契约一致，原厂文件哈希保持不变。

### Q-05 / P1：TCP 回显出口与流语义未闭环

**对应要求：** 执行计划 §4 / 1.5 将 BSD Sockets 定义为第三方 C 库和私有协议的底座，§6 A-3 要求请求/响应双向闭环。

**证据：** [Server 场景](../../../wink-micro-app/vendor/esp_idfv61/protocols/sockets_tcp_server/unisim-scenarios/protocols_sockets_tcp_server.scenario.json) 只断言接受连接、收到 `Data to ESP` 和关闭日志。`recv()` 的 EOF 不依赖服务端回显是否发出；[原厂发送循环](../../../wink-micro-app/vendor/esp_idfv61/protocols/sockets_tcp_server/tcp_server.c) 49–59 行的部分发送处理从未被门面覆盖。移除该发送循环预计仍满足当前观察项，实际杀伤实验尚未执行。

**其他静态限制：** `send()` 用单一接收缓冲覆盖旧数据并始终返回完整长度；`recv()` 在没有数据时返回 0，把暂时无数据与有序关闭混为一谈；连接不匹配真实监听端点。Client 场景有应用收包日志，但对端是底座自动 echo，尚不能证明目的端点正确。

**改进方案：** 实现最小可用的有界 TCP 字节流：端点匹配、监听/accept、独立 TX/RX 队列、部分收发、等待/超时、FIN/RESET 和连接清理。对端记录从固件出口实际接收的字节；重连由独立故障输入驱动。IPv6、select/poll、UDP 等未被本轮承诺的能力按实际依赖后置，不为扩展性一次实现全部 BSD。

**验收条件：** Server 对端收到正确地址/连接上的完整 echo；删除或改坏 send 后该断言失败。覆盖连续两次发送未读、分段读取、部分发送、暂时无数据、FIN、RST、连接失败恢复和池耗尽；相同输入下结果可重复，恢复后资源基线一致。

### Q-06 / P1：正向报告不足以证明反假绿与配置身份闭环

**对应要求：** 执行计划 §6 A-4 及治理 SOP 要求缺陷敏感性；交付还需核对完整验收集合、实际配置和恢复证据。

**证据：** 七个归档均只有一个正向 result。SNTP/TCP/AP+STA 的若干 `.fail` 场景使用故意不存在的 Matcher 文本，属于断言器自检。未在这七份归档及其 evidence 引用中找到正常/故障/固件禁用/有效业务变异/恢复的完整成组记录；这不等于断言历史上绝未在别处运行过。

**工具现状校准：**

- 首轮 [运行脚本](../../../wink-micro-os/frameworks/esp_idf/tools/run_esp32_headless_evidence.ps1) 175–199 行已把 `-Scenario` 传入实际执行，并在 188–192 行删除旧输出；不能重复 Skill 的 10 月 1 日旧快照，声称这些能力仍未修复。
- `-WriteEvidence` 默认选择首个非 `.fail` 场景，不能证明完整验收集合。`-ConfigId` 在 217 行仅传给写入器，并未在该调用链证明它控制了实际构建与执行配置。
- [核验器](../../../wink-micro-app/vendor/esp_idfv61/.governance/gates/evidence_verifier.py) 132–192 行核对步骤数量和 passed，但不核对完整场景集合、步骤身份、target、matcher 或运行后端身份。
- 写入器 489–505 行在未知 config 时回退首个配置；486 行 run_id 由日期和 App 构成，同日重复不唯一。证据包含资产与场景哈希，但没有完整绑定本轮报告、依赖源码与实际运行身份。

**改进方案：**

1. 区分正常业务、故障处理、断言器自检、固件依赖、有效业务变异、恢复六类证据，不用一个 Canary 同时代替它们。
2. 运行前固定 entry/config/backend/SoC/profile、实际构建宏、场景集合、运行器标识、源码输入和唯一 run_id；未知配置立即失败。
3. 每轮使用独立产物/报告目录。候选证据包同时绑定各场景、资产、报告、因果检查和恢复记录，核验完整集合与逐项身份后再事务晋升。
4. 预期失败实验保留真实失败结果与归因，并由验收汇总识别其“预期失败符合要求”；不能把错误日志改成 passed，也不能把正常交付误标为回归。
5. 对照既有计划补实际未闭环的任务。新增字段和多场景凭据须先对齐 Schema/运行契约，下述验收矩阵是设计要求，不是可直接执行的新场景模板。

**验收条件：** 错 config、漏场景、同数量但不同步骤的旧报告、重复 stepIndex、错 backend/宏以及篡改报告均被拒绝；有效变异使指定核心断言在期限内失败，恢复基准通过。失败候选不覆盖已有有效历史凭据，不自动补审计身份。

## 5. 治理与规划补充

### G-01 / P2：能力目录没有反映 BSD 实现及真实依赖

**证据：** 首轮 [能力目录](../../../wink-micro-app/vendor/esp_idfv61/.governance/catalog/capability-catalog.yaml) 466–472 行的 `cap.net.bsd_socket` 仍为 `planned`，owned path 指向不存在的 `esp_socket_shim.c`，实际文件是 `esp_sockets.c`。#212/#214 的 `required_capabilities` 未声明该 BSD 能力。HTTP 能力描述包含 session，而实际会话语义存在 S-06 的差距。

**建议：** 将实现路径、支持子集、状态和原子依赖对应起来；SNTP、WS、转发的依赖粒度经审查后确定，不为每个函数随意制造能力 ID。门禁需要校验真实依赖闭包，不能只检查已列出的 ID 是否存在。

**验收：** 删除关键实现、将所需能力标回未实现或移除所需配置时，受影响配置无法晋升；不能通过漏写依赖维持 verified。目录状态只能随真实能力和证据变化更新。

### G-02 / P2：Wasm 登记不能代表真机或其他宿主完成

**证据：** 七项仅有同一 Wasm 配置；相应载体 CMake 中存在仅 Emscripten 导出源码的分支。没有本轮 ESP32 编译、硬件串口/网络实证或 host CTest 运行结果。

**建议：** 将浏览器、Node、host、ESP32 配置分开记录，只对产品实际需要的组合投入；先补关键应用的 ESP32 构建和少量硬件差分验证。比较 API 结果、完整载荷、状态迁移和允许的时间窗口，不要求不同平台绝对微秒恒等。

**验收：** 真机配置有独立 ELF/BIN、构建配置和硬件运行证据；Wasm 资产不能充当物理固件证据。多芯片支持表与实测/编译覆盖明确区分。

### G-03 / P2：ROI 成本量表方向与公式相反

**证据：** 执行计划 §2 的 C 定义为 1=复杂、5=轻量，却位于分母。在 F/U/O 固定时，复杂项的得分反而是轻量项的五倍。

**建议：** 统一为“越高越昂贵”的成本量表后再除，或显式转换当前易实现得分。保留原始评分和估算依据，记录依赖收益、置信度与实际耗时；不要只调整文字而不重算排序。

**验收：** 在其他维度相同时，更昂贵的候选不会获得更高 ROI。计划中的 252/254、37/39 等时间点数字与 20 项清单按既定快照说明，不手工制造第二套实时统计。

## 6. 长期架构建议与约束

以下是待评审的改进方向，不是对现有实现的描述，也不批准新增 PAL 或外仓接口。

### 6.1 分清环境输入、通用门面与固件输出

```mermaid
flowchart LR
    S[确定性场景与故障安排] --> P[虚拟对端：HTTP / WS / TCP / NTP]
    P --> N[接口状态、端点路由与有界传输]
    N --> F[ESP-IDF 公共门面]
    F --> A[未修改的原厂业务]
    A --> F
    F --> N
    N --> O[对端实际收到的响应与状态事件]
    O --> V[逐请求、逐连接、逐字节验收]
```

场景可以定义对端输入、时延、响应和关闭；预期值不能直接写入被断言的固件结果。虚拟对端 echo 本身可以是合法测试环境，关键是显式配置、真实接收固件发送数据，并能通过阻断业务处理验证依赖关系。

优先复用 [sim_network_broker](../../../wink-micro-os/frameworks/esp_idf/src/network/sim_network_broker.h) 的 Netif 就绪和事件，以及 [sim_bounded_stream](../../../wink-micro-os/frameworks/esp_idf/src/network/sim_bounded_stream.h) 的有界缓冲。复用前要核对其背压、取消与复位语义，不能仅因已有头文件就认定可直接满足全部需求。

### 6.2 用有限模型满足真实契约

| 领域 | 建议优先实现的最小契约 | 暂不强加的范围 |
|---|---|---|
| TCP | 端点匹配、独立连接、字节流、等待/超时、部分收发、FIN/RST | 完整拥塞算法、真实吞吐预测、全部 BSD 扩展 |
| HTTP | 每请求输入/响应、容量诚实、头部与路由、stop/start | 无需求的一次性多服务器泛化 |
| WS | 连接状态、text/binary、控制帧、异步工作和关闭 | 未声明的扩展压缩、全部网络故障模型 |
| SNTP | 对端驱动同步、UTC/单调时钟分离、超时恢复 | 未承诺的高精度网络时钟校准 |
| AP+STA | 独立接口、AP 对端地址、可观察转发 | RF 和硬件吞吐的精确仿真 |

**警惕虚拟对端的过度工程陷阱**：整改目标是用有限模型实现真实契约，**严禁走向“在 Wasm 中编译/移植完整 LwIP 或全量 BSD 协议栈”的重资产泥潭**。仿真测试核心是因果闭环与边界诚实，通过确定性有界字节流（TX/RX FIFO）配合极简状态机（LISTEN/ESTABLISHED/FIN/RST）即可满足绝大多数业务验证需求，避免过度复杂化膨胀运行时体积与维护负担。

保持编译期静态分发和 POD 状态，不引入不必要的运行期 ops/vtable。公共 ESP-IDF 兼容 API 保留原厂类型、正数 `esp_err_t` 和 BSD errno 语义；内部 PAL/Wink API 继续遵循本项目负错误码，不能机械把兼容 API 改为 `wink_status_t`。

若改动只涉及门面与协议模型，不应为了满足“三 target”字样强行扩展 PAL。确需新增通用 PAL 时遵守 ADR-0092 的纯增量、跨平台头和 Wasm/ESP32 同步实现要求。

### 6.3 保真声明与观察模型

每个能力明确写出：支持配置、最大连接/缓冲/队列容量、时间模型、错误集合、复位保留规则、观察出口和已知限制。全局 `full_buffer`、`cycle_accurate` 等标签不能自动代表 TCP、WS、NTP 的全部语义或物理性能精确。

`last_resp` 和累计字节适合作为诊断摘要，不能独自承担多请求、多连接验收。建议输出可关联请求/连接、方向、端点、序号、虚拟时间、载荷长度、字节或摘要的记录；字段命名与 ABI 必须先经公开契约审查。二进制与长载荷不能依靠 C 字符串作为唯一事实源。

### 6.4 资源、时钟与安全边界

- 对连接池、队列、stream blocks、待发工作、回调订阅建立容量和峰值指标；以基线差额验证清理，不靠单一“已 reset”日志。
- 明确单调时钟负责超时，UTC 负责业务日期；调整 UTC 不应改变任务等待截止时刻。警惕墙钟跳变造成以 `time(NULL)` 为基准的第三方网络重试状态机溢出或死锁。
- 外部帧和 JSON 使用有界长度校验，超限明确失败；不在临界区阻塞或动态分配。
- 共享 console 不因单个协议接入而改变全局日志生命周期；第三方库有固定版本、来源、许可和异常路径。
- 鉴权 Token 示例不代表产品安全方案。TLS、证书校验、权限、重放防护等按对应产品/波次单独设计，不把它们冒充本轮已完成能力。

## 7. 建议的验收与变异矩阵

以下均为待实施的测试设计，未在本轮执行。涉及新增信号、step 或 fixture 的条目须先验证公开契约，不能把表内文字直接当现有场景 JSON。

| 编号 | 对象 | 正常或故障输入 | 必须观察的出口 | 非等价业务变异 | 恢复要求 |
|---|---|---|---|---|---|
| T-01 | 内存边界 | N 字节缓冲接 N 字节帧，长日志 | 边界哨兵、实际 UART 长度 | 恢复越界补零/错误长度 | 修复版本同输入安全且原业务通过 |
| T-02 | HTTP Simple | 分别 GET hello、POST echo | 每次响应状态、头部、完整 body | 改坏 hello 响应或 echo 内容 | 各自断言先失败、恢复后通过 |
| T-03 | REST API | system/temp/brightness 分开请求 | JSON 字段、合法范围、实际输入处理 | 错字段/错误亮度解析 | 对应业务断言失败；镜像资产恢复 |
| T-04 | 静态文件 | 多 chunk、缺文件、长文件 | 完整文件字节/摘要、MIME、错误 | 丢一个 chunk 或改 MIME | 完整性断言失败，资源清理 |
| T-05 | WS | text、含 NUL binary、Ping/Close | 连接绑定、opcode、完整响应 | 丢 binary 分支或错连接发送 | 正确断言失败，新连接恢复 |
| T-06 | WS 异步 | 入队后关闭或停服 | 排队顺序、取消结果、无幽灵发送 | 同步执行工作或忽略关闭 | 顺序/关闭断言失败，恢复基准通过 |
| T-07 | AP+STA | AP 客户端经设备访问上游 | DHCP 分配、双接口、往返载荷 | 关闭转发或选错出口 | 转发断言失败，上游恢复可继续 |
| T-08 | SNTP | 延迟响应、无响应、掉线恢复 | UTC、事件、回调次数、超时结果 | 阻断时间应用或写错 epoch | 指定对齐断言失败，恢复正确时间 |
| T-09 | 时区/时钟 | 同 UTC 的纽约/上海转换、UTC 跳变 | 完整当地时间、单调等待期限 | 忽略 TZ 或用 UTC 控制超时 | 对应数值/期限失败，恢复后通过 |
| T-10 | TCP Client | 正确/错误端点，分段响应，断连 | 对端实际收到请求、应用处理结果 | 改请求端口或跳过接收处理 | 端点/业务断言失败，再连接通过 |
| T-11 | TCP Server | 两次连续写、部分 send、FIN/RST | 对端收到的完整 echo、状态迁移 | 去掉发送循环或错一个字节 | 核心回显断言失败，恢复基准通过 |
| T-12 | 生命周期 | 池耗尽、反复 stop/start/reset | 池占用差额、旧工作作废 | 漏清一个连接或留旧回调 | 资源断言失败，恢复后无累积 |
| T-13 | 证据工具 | 错配置、少场景、同计数旧报告 | 明确拒绝与历史包保持 | 篡改报告身份/步骤/摘要 | 候选不晋升，合法包仍通过 |

有效变异必须事先指定目标断言、失败期限和预期归因。编译失败、缺资产、无关崩溃不能代替目标业务断言杀伤。变异使用隔离副本或受控补丁，不污染原厂镜像与历史资产。

断言器自检可以继续保留，但单独归类。错误密码、断网验证故障处理；它们不能单独证明固件依赖。TCP/HTTP 是请求响应业务时需要观察双向出口；其他单向发布业务不必为了统一模板强加无关回包。

### 7.1 组合验证与真机抽样

在单能力契约稳定后，增加一个 AP+STA + HTTP/WS + SNTP 组合应用，覆盖网络掉线恢复、授时期间业务通信、多个连接以及请求处理中重启。该应用应是独立集成样例，不能改写官方零修改镜像来拼接。

先做所需 ESP32 配置构建，再对 HTTP 响应、WS 帧、TCP 连接与 SNTP 对齐进行少量真实硬件差分验证。仿真中的固定虚拟期限与硬件允许窗口分别登记；真实 RF 性能、TLS 安全与深睡电流不由该组合测试自动证明。

## 8. 整改组织与关闭条件

本节是整改建议的依赖顺序，不是已确认的实施排期。复杂代码改动仍需按仓库规则形成实施计划并获得确认；重大契约选择按 ADR 流程记录，Accepted 后回写活设计规范。

| 建议批次 | 主要工作 | 依赖 | 退出条件 |
|---|---|---|---|
| R1 正确性 | S-01、S-02；S-03 清理链；S-05 明确失败 | 先固定复现输入与兼容契约 | 边界、返回值、软复位测试通过；正常场景恢复 |
| R2 协议模型 | S-04、S-06～S-09；Q-01～Q-05 | R1；请求/连接观察契约确认 | 对端独立、逐请求/帧/字节闭环、关键异常与恢复通过 |
| R3 证据治理 | Q-06、G-01，运行身份与候选包 | 与 R2 并行设计；以真实契约收口 | 完整集合、身份与因果核验；失败不覆盖历史证据 |
| R4 集成与硬件 | G-02、组合验证、资源预算 | 单能力与证据链稳定 | 所需配置分别验收；共享底座回归完成 |
| R5 规划校准 | G-03 与路线图数字口径 | 可独立进行 | ROI 方向正确，后续优先级可复核 |

建议按逻辑模块保持原子提交：公共 console、网络生命周期、TCP 流、HTTP/WS、SNTP、AP 转发、证据工具、元数据。协议修改与对应验证应能关联，不能只提交“通过”的看板变化。

关闭任一问题时记录修复提交、实际配置、测试输入、逐项结果、变异/故障归因及恢复结果；用新的复验记录引用本报告 ID，不回改本报告为“当时没有问题”。

后续改 C 代码适用的检查至少包括分层/API lint 与许可门禁；按真实改动范围执行治理 Gate 和受影响回归，不能把 SKIP 当通过：

```powershell
winkcli lint --pack layering --pack api
python .github/scripts/check_license_map.py
```

HTTP、Sockets、时钟或共享日志改动可能影响已有 http_client、MQTT、存储及其他日志依赖示例。结合实际依赖选择回归范围；不得因 Wave 1 场景绿灯就忽略共享底座影响，也不要求在每个小改动后重复无关全量测试。

## 9. 本轮实际检查记录

### 9.1 已执行

1. 阅读计划、源码、相关规范、七个精确执行配置、全部对应正向场景与归档报告，并检查负例场景与公共底座。
2. 比较 Wave 1 引入前后的提交，排除 OTA 与后续 sleep 等范围外变更。
3. 重算九个原厂 `.c` 的 SHA-256，与 Manifest 锁值逐一比对，九个均一致。
4. 执行以下两条只读命令；没有 `-WriteEvidence`：

```powershell
python -X utf8 -B wink-micro-app/vendor/esp_idfv61/.governance/gates/run_gates.py --gate 1
python -X utf8 -B wink-micro-app/vendor/esp_idfv61/.governance/gates/evidence_verifier.py --verify-all
```

| 初轮检查 | 原始结果 | 解释 |
|---|---|---|
| Gate 1 | 12 条执行，11 PASS、1 FAIL；退出码 1 | `g1.carrier_landing_integrity` 报 #122 deep_sleep 缺声明场景，属于当时并行落地的 Wave 2 项 |
| Evidence verifier | 输出 `45/45 verified entries passed`；退出码 0 | 包含七个 Wave 1 配置；代表现有核验器接受已有资产、场景和报告 |
| Wave 1 报告解析 | 7 个 result，53 步通过，失败/跳过/错误均为 0 | 每项一份正向；不能当作故障与因果验收 |
| 本地镜像哈希 | 9/9 与 Manifest 一致 | 未重新联网比对官方上游原件 |

Gate 1 日志时间为 `2026-10-07T23:47:49.933127+00:00`，即上海时间 2026-10-08 07:47:49。其 #122 失败是历史快照：整理期间已经出现 deep_sleep 新提交，本报告未重新运行并判定该项当前状态，不能将历史失败长期表述为当前故障。

### 9.2 未执行与不作的推断

未重新编译 Wasm、未重放 Headless、未运行本轮 CTest/内存检测、未进行有效业务变异、未执行 ESP32 构建或烧录、未验证浏览器与 Node 的宿主一致性。门禁原始输出保留在本聊天工具记录中，本次没有额外创建治理凭据或运行结果文件。

没有变更任何 `delivery_state`、审计签署、产品范围或能力目录；没有因为缺口自动回退 verified，也没有自动创建新交付状态。后续晋升/回退应按授权的治理流程及真实复验执行。

## 10. 复核资料与快照指纹

### 10.1 原厂镜像 SHA-256

路径均相对 `wink-micro-app/vendor/esp_idfv61/`，本轮与各 App 的 `upstream.files` 一致：

| 文件 | SHA-256 |
|---|---|
| `protocols/http_server_simple/main.c` | `6691c523df4a2b2e688c2a7e1d581000026dac36ffbd93937ea9413442547f23` |
| `protocols/http_server_restful_server/esp_rest_main.c` | `345b8a07dbb5401f7e18cd040cedb38b222dad31c3b14f49461de39f49625379` |
| `protocols/http_server_restful_server/rest_server.c` | `3e3fe8c0940be86552cdabfcd4003e7651cac3a60b7f8dfffa0354c325bd0b23` |
| `protocols/http_server_ws_echo_server/ws_echo_server.c` | `903897b7eebd725d4b744c9d86bdbf974c4b15579b95ba534a8e8c759ca09d6f` |
| `protocols/sntp/sntp_example_main.c` | `189600773c4afa58eed0c36c5944ce2f23addd58897a7284df34635948e071ba` |
| `protocols/sockets_tcp_client/tcp_client_main.c` | `2046ccb852f86bcc0e18b1a6405b2374f36f6bd5d73c696aa6607f43e0f66fba` |
| `protocols/sockets_tcp_client/tcp_client_v4.c` | `75a9909a4783ddb50b1f1a0962dd5d726e43379f9d8c44ea4bc6af3e797a9ee3` |
| `protocols/sockets_tcp_server/tcp_server.c` | `07e35655e9e91e2e24c1fbc879f3d7bfd2a1763a882e668b552dba33ec82fb64` |
| `wifi/softap_sta/softap_sta.c` | `27c4ef6bc0bcf6a854e257522d0f0507643671054267a9d762bde9f731686fa8` |

### 10.2 公共实现指纹

以下为 `git show 6fdc5eb6:<path>` 返回字节的 SHA-256，不含工作区 CRLF 转换。目录前缀为 `wink-micro-os/frameworks/esp_idf/`：

| 文件 | SHA-256 |
|---|---|
| `src/network/esp_http_server.c` | `6c53c92759c00249ce5325064b0357e2893269f6e26d96e98930d41cc95247f1` |
| `src/network/esp_sockets.c` | `dbf72cf622db8c6d414fd796398a5f75b2a9b5455558387927d309fa6ae17fa5` |
| `src/network/esp_sntp.c` | `018fd6033249ab0f4291c505103fda589c76c80e948d966fe47aa5618e3710cc` |
| `src/wifi/esp_netif.c` | `cae94e41d8f1695836092f948024b47d841df7cb080a924f4624b18418dbf4be` |
| `src/esp_idf_bridge.c` | `865f16b8877c3fde8b10ca8cf0091031cabe626d90e87b0ca0871ef4b8d0bc24` |
| `tools/run_esp32_headless_evidence.ps1` | `e6aa0e5e41678a94f7d1055c1c0a3209942a1cfe7d731de94c85342a710e6e3f` |

同快照下，`wink-micro-app/vendor/esp_idfv61/.governance/gates/evidence_verifier.py` 的哈希为 `48327e4f747dc0729700250f435b6b648ded6c8fd4f72b951370d655c8ae2240`；`catalog/capability-catalog.yaml` 为 `81e55c5a1f3fabe9bcb8be7064d371d5e36a03744a3a0ffde9baed1640c10d86`。

### 10.3 报告索引

以下路径相对 vendor 根目录；资产和场景哈希保留在首轮提交的 SSOT 对应 execution 中。本报告不重写它们：

| 编号 | 报告引用 | 历史报告时间（UTC） |
|---|---|---|
| #196 | [.governance/reports/protocols/http_server_simple/run-report.json](../../../wink-micro-app/vendor/esp_idfv61/.governance/reports/protocols/http_server_simple/run-report.json) | 2026-10-07T02:54:45.912Z |
| #195 | [.governance/reports/protocols/http_server_restful_server/run-report.json](../../../wink-micro-app/vendor/esp_idfv61/.governance/reports/protocols/http_server_restful_server/run-report.json) | 2026-10-07T04:33:23.261Z |
| #197 | [.governance/reports/protocols/http_server_ws_echo_server/run-report.json](../../../wink-micro-app/vendor/esp_idfv61/.governance/reports/protocols/http_server_ws_echo_server/run-report.json) | 2026-10-07T05:53:28.902Z |
| #232 | [.governance/reports/wifi/softap_sta/run-report.json](../../../wink-micro-app/vendor/esp_idfv61/.governance/reports/wifi/softap_sta/run-report.json) | 2026-10-07T09:49:39.469Z |
| #209 | [.governance/reports/protocols/sntp/run-report.json](../../../wink-micro-app/vendor/esp_idfv61/.governance/reports/protocols/sntp/run-report.json) | 2026-10-07T10:29:52.927Z |
| #214 | [.governance/reports/protocols/sockets_tcp_server/run-report.json](../../../wink-micro-app/vendor/esp_idfv61/.governance/reports/protocols/sockets_tcp_server/run-report.json) | 2026-10-07T12:36:26.862Z |
| #212 | [.governance/reports/protocols/sockets_tcp_client/run-report.json](../../../wink-micro-app/vendor/esp_idfv61/.governance/reports/protocols/sockets_tcp_client/run-report.json) | 2026-10-07T12:37:41.623Z |

### 10.4 官方契约参考

- [ESP-IDF v6.1 HTTP Server](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32/api-reference/protocols/esp_http_server.html)：实例、stop 清理、会话、帧类型与异步工作上下文。
- [ESP-IDF v6.1 ESP-NETIF / SNTP](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32/api-reference/network/esp_netif_programming.html)：初始化、同步事件、等待与超时返回。
- [RFC 6455](https://www.rfc-editor.org/rfc/rfc6455.html)：握手、text/binary、mask、Ping/Pong 与关闭语义；仅用于明确协议保真范围，不表示本轮已测试全部 RFC。

以上官方资料在本次会话中查阅，访问日期 2026-10-08。源码与工具行为以提交快照优先，规范与现状不一致时保留具体差异，不选择较宽松的描述宣称完成。
