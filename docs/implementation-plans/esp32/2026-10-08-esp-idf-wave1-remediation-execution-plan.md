<!-- SPDX-License-Identifier: GPL-3.0-only -->
# ESP-IDF Wave 1 分阶段整改实施计划

| 项 | 内容 |
|---|---|
| 计划编号 | PLAN-20261008-ESP-IDF-WAVE1-REMEDIATION |
| 日期 / 修订 | 2026-10-08，Asia/Shanghai；v1.1，融合三轮评审 |
| 状态 | R1～R4 软件模型与仿真验证全部完成；真机相关标记待做（TODO） |
| 输入评审 | [Wave 1 完整评审](../../reviews/esp32/2026-10-08-esp-idf-wave1-network-completeness-and-maintainability-review.md)、[实施就绪度复核](../../reviews/esp32/2026-10-08-esp-idf-wave1-execution-readiness-review.md)、[整改计划评审](../../reviews/esp32/2026-10-08-esp-idf-wave1-remediation-plan-review.md) |
| 原计划快照 | 修订前 284 行，SHA-256：`8fe6898249e709e76ef8888e7e4d12e70736c6cf66116a5c0797ad2720135f5f` |
| 代码基线 | `c6b425b12a883aef1ae8850a4459cf109bc6852e`；修订时 HEAD 为 `ca13f53f0ab17973203f190ca0beaa98cd07ebf5`，两者间运行时与示例无已提交差异；实施前重新固定基线 |
| 主审配置 | #195、#196、#197、#209、#212、#214、#232 的 `wasm_sim_standard`；共享底座影响的额外应用按依赖加入回归 |
| 上位计划 | [高 ROI 执行计划](2026-10-07-esp-idf-high-roi-checklist-execution-plan.md)、[门面加固计划集](facade-hardening/README.md)、[防假绿计划](2026-10-01-anti-false-green-verification-plan.md) |
| 设计依据 | [仿真 SSOT 入口](../../zh/design/04-wasm-simulation/00-README.md)、[ADR-0004 静态分发](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md)、[ADR-0012 契约诚实](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)、[ADR-0091 多配置](../../decisions/unisim/0091-esp-idf-multi-config-orthogonal-schema.md)、[ADR-0092 治理宪章](../../decisions/unisim/0092-esp-idf-simulation-governance-and-capability-charter.md) |
| 技术设计边界 | R1 修复已有接口契约，不新增公共 ABI；R2 的新模型、观察契约及上下文先形成技术设计，长期架构选择按 ADR 确认并回写活设计规范 |

## 1. 目标、执行边界与规约优先级

先修复边界、错误返回及生命周期的确定缺陷，再补有限网络模型、证据治理和平台差分。各阶段独立验收，不以文档完整、徽标数量或当前 Gate 通过代替实现正确性。

首批执行范围为 **R1-T0～T5**：WS 接收边界、日志实际写入长度、HTTP 单服务器 stop/start 与请求隔离、有界流零部分提交及 HTTP client 失败一致性。R2–R4 是后续路线，须完成各自设计和进入条件后执行；本次计划修订不等于代码实施或后续公共契约已获批准。

执行时采用以下规则：

1. 本计划中的行为表、测试输入和退出条件是验收依据。删除原草案中不完整的函数替换代码，避免将截断成功、错误签名或未定义 helper 直接复制进实现。
2. 原厂业务 `.c` 保持字节级不变，不手改生成的 `wink_sla.h`。修复位于兼容门面、共享底座及必要测试/构建接线，不用 App 特判、空成功桩或放宽断言维持通过。
3. ESP-IDF 兼容 API 使用原厂 `esp_err_t`，BSD 接口使用返回值/errno；Wink/PAL 内部保持负错误码。分发器的 HTTP 状态与 SDK 错误码分层处理。
4. 保持 POD 和编译期静态分发，不引入运行期 ops/vtable 或无必要动态分配。原厂接口已规定的回调及上下文释放不属于新建平台分发架构。
5. 发现新事实与锁定契约冲突时，先记录差异并修订受影响条目，再继续该工作包；不默默降低验收。无依赖的工作包可继续。
6. R1 只关闭矩阵中的明确子项；完整 WS、全部 HTTP 生命周期、大响应流式处理、Socket/SNTP reset 等保留后续任务。

## 2. 执行前基线与隔离

### R1-T0：固定输入、工具与受影响范围

- [x] 检查 HEAD、适用目录指令、C 规则和工作区差异；记录涉及文件的 SHA-256。不清理用户文档，不覆盖并行修改。
- [x] 保存九个原厂 `.c` 的哈希及来源、七项配置、验收场景清单、历史报告索引。详细原始哈希沿用完整评审 §10，实施时重新比对。
- [x] 读取治理 SOP 的 Authoring/Reverify 约束及实际工具帮助；核对可用 host、Emscripten/Node、browser 工具链与测试 target，不假定安装状态。
- [x] 通过引用搜索列出日志桥接全部拷贝、流写入全部调用点、受影响 App 和测试；形成“变更文件→消费者→必需回归”清单。共享流当前直接消费者为 HTTP client，不把其他协议凭空算作直接消费者。
- [x] 使用独立构建目录、运行输出和候选报告目录，保留历史资产。脚本不能选输出目录时使用合适的公开入口或隔离工作区；不编造 CLI 开关，不把“不带 `-WriteEvidence`”当作没有文件写入。
- [x] 为每个新用例确定真实触达路径：host 覆盖纯模型；Wasm/Node 覆盖 Emscripten 符号与初始化；登记为 browser 的配置另作其真实后端验收。不伪定义 `__EMSCRIPTEN__`，不拿 Node 结果冒充 browser。
- [x] 核对当前 CLI/脚本的配置、场景选择和报告清理行为。Skill 中带日期的工具缺口须与现状复核；`config_id` 的登记或写入参数不能独自证明运行器选择了该配置。

七项回归入口如下，登记身份为 `config_id=wasm_sim_standard`、`backend=wasm_browser`、`target_soc=esp32`、`profile=standard`：

| 条目 | 应用目录（相对 `wink-micro-app/vendor/esp_idfv61/`） | 原厂 `.c` 数 | R1 重点 |
|---|---|---:|---|
| #195 | `protocols/http_server_restful_server` | 2 | HTTP 请求、头部、路由及正常业务 |
| #196 | `protocols/http_server_simple` | 1 | GET/POST、Host、停止后恢复 |
| #197 | `protocols/http_server_ws_echo_server` | 1 | 原厂文本接收/回显及目标内存边界 |
| #209 | `protocols/sntp` | 1 | 日志桥接与原有授时场景 |
| #212 | `protocols/sockets_tcp_client` | 2 | 日志桥接与原有客户端场景 |
| #214 | `protocols/sockets_tcp_server` | 1 | 日志桥接与原有服务端场景 |
| #232 | `wifi/softap_sta` | 1 | 日志桥接与原有双接口场景 |

**完成条件：** 基线、真实运行身份、工具版本、受影响清单和隔离位置可复核；所有必需验证都有可执行入口。无法获得某目标工具时明确缺口，该目标验收保持未完成；不写交付状态或凭据。

## 3. R1：正确性修复执行规约

问题编号沿用评审的 S/Q/G/C/M 系列；本计划新增测试使用 W/L/H/B/HC 系列，其中 HC 表示 HTTP client，避免与评审 C-xx 混淆。

建议执行顺序为 T0→T1→T2→T3→T4-A→T4-B→T5。T1/T3 修改同一服务器实现，按工作包顺序合入；T4-B 依赖流原子写契约。每包先通过目标测试和必要快速回归，再推进下一包；最终由 T5 汇总阶段验证。

### R1-T1：WS 整帧与分段接收容量契约

**对应：** S-01、M-01、M-02；Q-03 的边界子项。  
**代码：** [HTTP Server 实现](../../../wink-micro-os/frameworks/esp_idf/src/network/esp_http_server.c)、[公开头](../../../wink-micro-os/frameworks/esp_idf/include/esp_http_server.h)。

**契约依据：** 锁定 [ESP-IDF v6.1 HTTP Server 文档](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32/api-reference/protocols/esp_http_server.html) 与 [对应 WS 实现](https://raw.githubusercontent.com/espressif/esp-idf/v6.1/components/esp_http_server/src/httpd_ws.c)。整帧容量不足返回 `ESP_ERR_INVALID_SIZE`；长度查询不消费 payload；分段接口维护总长度和剩余长度。

R1 保持现有“已解码、单个文本消息”模型：明确输出 `type=HTTPD_WS_TYPE_TEXT`、`final=true`、`fragmented=false`，不能依赖调用者预填 TEXT。真实 opcode、握手、控制帧及多连接归 R2-H；含 NUL 的复制测试不等于已交付 binary 帧协议。

设当前帧总长为 L、已消费偏移为 p、剩余量为 L-p：

| 调用条件 | 必须行为 |
|---|---|
| `req` 或 `pkt` 为 NULL | 返回 `ESP_ERR_INVALID_ARG`；不触碰 payload |
| `max_len == 0` | 返回 `ESP_OK`，输出总 `len=L`、支持的元数据、`left_len=L-p`；不复制、不改变 p |
| 整帧模式且 `0 < max_len < L` | 返回 `ESP_ERR_INVALID_SIZE`；不复制前缀、不推进 p，不把 `len` 改成短长度 |
| 剩余量为 0 | 返回 `ESP_OK`，`left_len=0`；payload 可为 NULL |
| 非查询、有待读数据、容量检查通过但 payload 为 NULL | 按锁定实现返回 `ESP_FAIL`；不推进 p |
| 合法整帧接收 | 从当前偏移复制剩余字节，保持总 `len=L`，更新 p/`left_len`；首次接收复制完整 L 字节 |
| 合法 `_part` 接收 | 复制 `min(max_len, L-p)` 字节；总 `len` 始终为 L，推进 p，递减 `left_len` |
| 任意接收模式 | 不追加 NUL；初始化/清理以请求边界为准，重复查询不能回绕已消费偏移 |

**执行步骤：**

1. 将帧读取状态初始化放在当前请求建立时，整帧和 `_part` 共用同一偏移状态；新请求或 stop 清除旧偏移。
2. 按“参数→元数据/查询→整帧容量→空载荷→payload→复制”的次序处理；在失败前不修改载荷和消费进度。
3. 保持公共函数签名。需要私有 helper 或测试入口时限定内部可见，不增加公共 Wasm ABI。
4. 先验证公开 API 的文本路径；含 NUL 的底层字节测试须触达实际接收核心，不另写一份复制算法，也不强行通过仅接收 C 字符串的桥接注入二进制。

| 测试 ID | 输入与目标断言 |
|---|---|
| W-01 | 分配 `storage[17]`，声明容量 16，`storage[16]=0xAA`；接收 16 字节，仅比较 `[0..15]`，guard 保持。末字节取不同于 guard 的值；旧越界补零实现应使 guard 断言失败 |
| W-02 | 12 字节帧配 8 字节整帧缓冲；返回容量错误，payload/偏移不变；换足够容量重试成功 |
| W-03 | 零初始化 packet、NULL payload、`max_len=0`；长度及全部支持元数据正确；连续查询不消费 |
| W-04 | 12 字节按 5/5/2 分段；`len` 始终 12，剩余 7/2/0，拼接一致；中途查询不回绕 |
| W-05 | 空帧 + NULL payload 成功；非空帧 + NULL payload、NULL req/pkt 分别得到表中错误 |
| W-06 | 小于容量、恰好容量、底层含 NUL 字节、连续新请求；字节完整、无补零及旧偏移污染 |
| W-07 | 实际 Wasm 中原厂文本接收/回显正常；保持官方业务 `.c` 哈希 |

**退出条件：** W-01～W-07 通过，至少保存 W-01/W-02 的旧失败→新通过证据；只关闭接收容量及当前文本模型元数据缺陷。

### R1-T2：日志格式化返回值与 UART 实际长度

**对应：** S-02、C-07、M-08。  
**代码：** [Socket 日志桥接](../../../wink-micro-os/frameworks/esp_idf/src/network/esp_sockets.c)，及 T0 搜索确认的 App 私有 `sdkconfig.h` 拷贝。

**边界定义：** 对 `char buf[512]`，`vsnprintf` 返回 n 表示期望格式化长度；有效文本最多 511 字节。n=512 时读 512 字节仍在数组内，但错误包含末尾 NUL；n>512 才越过数组。不得混淆两类错误。

**执行步骤：**

1. 仅当 n>0 才向 stdout/UART 输出；UART 长度为 `min((size_t)n, sizeof(buf)-1)`。n<=0 不转无符号、不访问未形成的文本。
2. 保留钩子返回 n 的格式化契约及正确的 `va_copy/va_end` 配对。
3. R1 在全部已知拷贝上做相同边界热修，列出路径；不同时迁移 constructor 或改变全局 hook 安装时机。统一共享桥接独立安排 R2-C。
4. UART 采用 best-effort：写失败不递归调用同一日志钩子，不更改格式化返回值。通过可控 PAL 返回值/捕获调用验证失败路径；必要诊断沿用已有非递归机制，不新增公共 ABI。
5. 真实触达 static 函数：优先通过 `esp_log_set_vprintf`/实际日志入口配合 PAL 捕获；或提取生产共用的私有有界格式化核心。不得直接调用不可见 static 符号，不用测试代码重复实现算法代替生产路径。
6. 测试保存并恢复旧日志 hook；host 单测与真实 Wasm UART TX/初始化分别验证。

| 测试 ID | 输入与目标断言 |
|---|---|
| L-01 | 文本长度 511/512/513/1024；返回 n 保持原长度，UART 分别写 511/511/511/511 个文本字节，内容正确、不带终止 NUL |
| L-02 | 空日志与可控负格式化结果；无 UART 写入，无负数转巨大长度；负返回需可重复的私有测试注入，不能依赖某平台偶然失败 |
| L-03 | PAL UART 失败；钩子仍按策略返回 n，无递归、阻塞或额外越界访问 |
| L-04 | 真实 Wasm 中各相关 App 的实际 UART TX 长度与日志 hook 生效；记录初始化顺序及实际链接到的实现 |

**退出条件：** L-01～L-04 通过；明确覆盖全部拷贝；S-02 边界缺陷关闭，日志桥接架构去重仍留 R2-C。可用内存检测时补充报告，不能以 sanitizer 替代确定的长度断言。

### R1-T3：HTTP 单服务器生命周期、Host 与方法分类

**对应：** S-06/S-08 子项、M-05、M-06；完整异步会话仍归 R2-H。  
**代码：** [HTTP Server 实现](../../../wink-micro-os/frameworks/esp_idf/src/network/esp_http_server.c) 与[公开头](../../../wink-micro-os/frameworks/esp_idf/include/esp_http_server.h)。

**A. 单槽生命周期与所有权**

1. `httpd_start` 先校验参数，再检查静态单槽是否可用；已启动或正在停止时返回 `ESP_ERR_HTTPD_ALLOC_MEM`，不覆盖现有 config、路由和上下文。这是当前单槽容量边界，不代表原厂不能创建多个服务器。
2. 只有初始化成功才发布新实例和成功状态。失败不发布可用的新 handle；调用者依据返回值判断，不能把输出变量中的旧 handle 当作本次成功。实施时核对锁定公开契约的输出处理。
3. stop 先阻止新分发/注册，进入私有 STOPPING 状态；清除 URI handlers、`err_handlers`、当前 req/aux、读偏移和本模型的请求活动状态，解除旧回调引用。
4. 全局 `global_user_ctx` / `global_transport_ctx` 按[原厂 v6.1 stop 实现](https://raw.githubusercontent.com/espressif/esp-idf/v6.1/components/esp_http_server/src/httpd_main.c)释放：非 NULL 时调用对应 free_fn；未提供 free_fn 则按默认 free 规则处理。先移出当前实例并清空拥有指针，再调用释放，保证各所属字段只释放一次。静态/借用全局内存须配合法释放回调，不能盲目 default-free。
5. URI `user_ctx` 是借用指针，不随路由删除而 free。服务器释放复用兼容层机制，不为本修复新增动态池；新会话 ctx/工作项的完整所有权由 R2-H 设计。
6. 释放回调期间拒绝重入 start/stop，不让回调启动的新实例被外层 stop 清空；完成后进入 STOPPED。NULL/外来 handle 返回 `ESP_ERR_INVALID_ARG`；当前单槽未启动时重复 stop 返回 `ESP_ERR_INVALID_STATE`，无二次释放。此额外防卫不赋予原厂已失效 handle 再使用的保证。
7. 注册、注销及错误回调入口校验当前活动实例，不修改其他/已停止实例。统计历史是否保留按已有观察契约记录，不能因清理请求状态虚报资源/计数全部归零。
8. 不输出超出实际清理范围的“全部资源释放”声明。静态地址重用后的陈旧 handle 代际、异步队列和多会话隔离仍属 R2-H/R2-L。

**B. 请求建立顺序**

每次请求执行“校验输入→清理上一请求→解析并填充当前 method/URI/headers/body→路由分发”。raw 注入入口和直接分发入口共用内部请求建立/分发逻辑；不得先写 Host 再被入口 memset 擦除，也不得拷贝全局旧 Host 来补齐当前请求。

同一请求头按现有大小约束保存；`Host: api.local` 可被 handler 查询。下一请求无 Host 时必须返回未找到，不能继承前值。现有请求边界的容量诚实需保持；完整 HTTP wire parser、JSON 语义与长报文能力归 R2-H。

**C. 方法与返回值固定策略**

保留当前已支持的 GET/POST/PUT/DELETE/**PATCH**/HEAD。公开枚举存在其他方法不等于本模型已经实现；R1 不扩展全部方法。

| 已解码注入入口条件 | HTTP 结果与分发行为 |
|---|---|
| 已支持 method + 匹配 URI/method | 调用正确 handler，使用 handler 形成的响应 |
| 已支持 method + URI 存在但 method 不匹配 | 405；按现有错误 handler 契约处理，不调用 GET 代替 |
| URI 不存在 | 404；仅调用当前实例注册的错误 handler |
| 未识别 token / 当前模型未实现的方法 | 501；不执行任何 GET handler |
| 注入请求语法无效 | 400；不进入业务 handler |
| 服务器未启动等内部不可分发状态 | 沿用分发器的 `-1` 失败约定；不把它包装成成功 HTTP 响应 |

上述 400/501 是本仓已解码注入模型的明确策略，不冒充完整原厂 wire parser 的全部行为。`sim_http_server_dispatch_request` 返回 int HTTP status 或 -1；公共 SDK 函数仍返回 `esp_err_t`，禁止把 `ESP_ERR_NOT_SUPPORTED` 的数值直接当 HTTP status。方法解析需集中映射，不能另留一个默认 GET 的旁路。

| 测试 ID | 输入与目标断言 |
|---|---|
| H-01 | start→注册 URI/旧 404→stop→start→重新注册；二次注册成功，旧 URI/错误回调不执行，新回调与 ctx 生效 |
| H-02 | 非法 start 参数、活动时再次 start、NULL/外来 handle、重复 stop；原有效实例不被破坏，错误分类正确，无二次释放 |
| H-03 | 两种 global ctx 的自定义 free 各一次、默认释放路径、借用 URI ctx 不释放；释放回调重入 start/stop 被拒绝 |
| H-04 | 连续 raw/直接请求：有 Host→无 Host→新 Host；handler 读取准确，无残留；stop/start 后同样成立 |
| H-05 | PATCH 路由正常；未知方法不调用 GET；method 不匹配 405、路径缺失 404、无效注入 400，返回域不混用 |
| H-06 | 真实 Wasm 中请求→stop/start→恢复请求；旧请求内容/读取偏移不污染新请求，原厂业务正常 |

**退出条件：** H-01～H-06 通过；这里只关闭同步单服务器清理、Host 和方法分类。软复位统一清理及真正异步/会话能力不得连带关闭。

### R1-T4：有界流原子写入与 HTTP client 失败回滚

**对应：** C-05、M-03、M-04，S-07 的失败诚实子项。  
**代码：** [流实现](../../../wink-micro-os/frameworks/esp_idf/src/network/sim_bounded_stream.c)、[内部声明](../../../wink-micro-os/frameworks/esp_idf/src/network/sim_bounded_stream.h)、[HTTP client](../../../wink-micro-os/frameworks/esp_idf/src/network/esp_http.c)。

**A. 有界流写入**

保留签名 `esp_err_t sim_bounded_stream_write(sim_bounded_stream_t *, const uint8_t *, size_t)`，不改为 `const void *`。本批不增加部分写返回值或压缩算法：

| 条件 | 结果 |
|---|---|
| stream 为 NULL | `ESP_ERR_INVALID_ARG` |
| stream 有效、len=0 | `ESP_OK`，可接受 NULL data，无数据变化 |
| stream 有效、len>0、data 为 NULL | `ESP_ERR_INVALID_ARG` |
| len 大于当前真实可写容量 | `ESP_ERR_NO_MEM`，零新增字节，原有内容、顺序、读写偏移、块占用保持 |
| len 不超过真实可写容量 | 全量写入成功，不返回无法辨认的部分提交 |

当前实现只释放完全读尽的头块，不复用已读头部前缀。**真实可写容量 = 尾块未写空间 + 真正空闲块数 × `SIM_STREAM_BLOCK_SIZE`**；尾块为空时其空间为 0。空闲块按 `used[]` 计算，不使用“总容量−未读量”。用宏而非再写死 1024/4。

可新增内部 static 容量 helper；原草案的 `sim_bounded_stream_get_writable` 当前不是已有 API，不新增公共声明。预检与提交在当前串行、无 yield 的同一上下文完成；若以后允许并发写者，须一起受保护，而非只保护预检。失败可设置现有背压诊断标记；峰值块占用不应因失败虚增，也不新增未定义的诊断 ABI。

关键反例：写 4090→读 1 后，四块仍占用、尾部只剩 6 字节；再写 7 必须零提交失败，不能误算为有 7 字节空间。

**B. HTTP client 成功提交点**

当前 `open` 在写流前已更新打开状态、匹配响应和统计。修复必须覆盖整次初始化，不能只在写失败处提前 return：

1. 完成参数/路由/故障检查，准备本次响应与流；检查所有相关写入结果。
2. 只有响应准备成功，才发布 `is_open=true`、匹配响应、有效状态/长度及成功连接事件。
3. 本次初始化写失败时，清除本次暂存的匹配响应与请求/响应流，回到可 close/cleanup/重试的合法未打开、未完成状态；本次结果不得留下成功状态、可读截断前缀或旧响应。
4. 失败前不产生 `ON_CONNECTED`/`ON_DATA`/`ON_FINISH` 等成功事件；失败事件沿用已锁定的接口约定。区分尝试请求、对端声明长度、实际交付字节与完成次数；保留真实尝试统计，不将失败当完整交付，也不盲目清空全部历史指标。
5. 审查 `open/perform/write/read/close/cleanup` 的共享流使用。当前 `perform` 事件路径可直接从响应对象交付完整 body，不能仅因诊断缓存装不下就破坏合法事件消费；明确事件交付与低级 read 缓冲的用途和结果。
6. R1 不承诺低级读取能够流式处理任意大响应。4097 字节等超当前 4096 容量的情况先明确失败；真正 chunk/backpressure 流式能力归 R2-H。

| 测试 ID | 输入与目标断言 |
|---|---|
| B-01 | 空流写 4095/4096 成功并读回一致；4097 失败、无新增块和字节；失败后写 512 成功 |
| B-02 | 非空流再写超限；原内容、顺序、偏移、占用不变；只允许约定的背压诊断变化 |
| B-03 | 写 4090→读 1→写 7；零提交失败，再写 6 成功；旧实现/错误容量公式应被目标断言检出 |
| B-04 | 完全读尽一个头块后释放，再写入可恢复；所有数据读尽后块占用回到基线 |
| B-05 | NULL stream、NULL data+非零长度、零长度分别符合参数表 |
| HC-01 | 低级 open 4097 响应失败：未打开/未完成，无截断可读内容、成功事件或虚假完整交付 |
| HC-02 | 上述失败后安全 close；同 handle 重试 512 响应成功、无旧内容；独立失败路径 cleanup 也安全 |
| HC-03 | `perform` 事件消费者仍收到完整合法响应；低级 open/read 与事件消费分别断言，不用缓存失败错误否定事件成功 |
| HC-04 | 小响应正常 open/read/close、请求写入失败路径、重复恢复；统计与事件符合定义，无状态污染 |

**退出条件：** B-01～B-05、HC-01～HC-04 通过，保存流和客户端两层缺陷复现及恢复证据。只关闭原子失败和状态一致性，不宣布全部 S-07 大响应能力完成。

### R1-T5：测试接线、阶段复验与原子提交

**测试接线要求：**

- 优先复用 [safeguards](../../../wink-micro-os/frameworks/esp_idf/test/run/test_esp_idf_safeguards.c)、[日志测试](../../../wink-micro-os/frameworks/esp_idf/test/core/test_esp_log.c)、[HTTP client 测试](../../../wink-micro-os/frameworks/esp_idf/test/core/test_esp_http_client.c) 和现有 Wasm 测试模式，不假定所有测试都应堆在 safeguards。
- [测试 CMake](../../../wink-micro-os/test/CMakeLists.txt) 的 safeguards 当前未配置 `src/network` 私有 include；必要时给该 target 增加 PRIVATE 路径，不将内部目录设为 PUBLIC。框架源清单已经包含相关网络实现，不误写成“host 尚未链接全部网络源”。
- 新增 Wasm 测试 target/测试入口须明确标为新增，并登记命令、产物和所测初始化/链接路径。host 不能证明真实 Emscripten constructor/UART 行为。
- setUp/tearDown 清理当前 HTTP 实例与请求/流状态，恢复旧日志 hook，避免测试之间遗留回调或 ctx；释放拥有资源后再 reset。验证单独执行和连续执行无顺序依赖。
- 每个确定缺陷至少一条旧失败→新通过记录；兼容与恢复用例允许旧实现原本就通过。旧版本对比在隔离输出中执行，不覆盖当前修复和历史镜像。

| 层级 | 必需检查 | 所证明的范围 |
|---|---|---|
| 局部 host | W/L/H/B/HC 用例及实际受影响测试 target | 参数、容量、生命周期、事件和恢复的确定契约 |
| 现有 host 回归 | `test_esp_idf_safeguards`、`test_esp_log`、`test_esp_http_client`，按真实构建名称登记 | 共享底座兼容；不是运行某一个二进制就代替其余必需项 |
| 真实 Wasm/Node | WS 接收、UART TX、HTTP stop/start、初始化/链接边界 | 目标特有实现及真实 ABI/符号路径 |
| 配置与业务 | 七项全部声明验收场景、相关已有故障场景，以及 T0 确认的额外消费者 | 原厂业务与故障恢复没有退化，实际后端/配置可核对 |
| 静态/治理 | 分层/API lint、许可、适用 Gate、凭据核验 | 各自规则范围；不代替运行时或配置完整性证据 |

**原子提交建议：** 每个修复包含对应测试和必要构建接线；T4 允许拆为“流原子写”和“HTTP client 失败回滚”两个有依赖的原子提交。不要把全部测试集中在最后：

1. `fix(esp_http_server): enforce ws receive bounds and frame metadata`（T1 + W）。
2. `fix(esp_log): bound uart bridge writes to formatted bytes`（T2 + L，涵盖实际拷贝）。
3. `fix(esp_http_server): isolate lifecycle and request dispatch state`（T3 + H）。
4. `fix(sim_bounded_stream): reject writes without partial commit`（T4-A + B）。
5. `fix(esp_http_client): roll back failed response initialization`（T4-B + HC）。
6. 阶段复验记录及必要跨模块集成测试，引用上述真实 commit；提交操作按实际执行授权处理，不预填提交哈希。

**阶段退出条件：**

- [x] W-01～W-07、L-01～L-04、H-01～H-06、B-01～B-05、HC-01～HC-04 结果齐全；必需项没有用 SKIP 代替通过。
- [x] host、真实 Wasm、七配置/场景及受影响消费者回归通过；故障后恢复到正常基准，无旧状态复用。
- [x] 原厂九个 `.c` 哈希不变；所有有效性检查对应本轮真实输入和产物。
- [x] 适用 lint/许可/治理检查通过；无法证明的配置或工具缺口如实保留，不宣布该目标完成。
- [x] 产出独立阶段复验记录：[2026-10-08-esp-idf-wave1-r1-remediation-verification-review.md](../../reviews/esp32/2026-10-08-esp-idf-wave1-r1-remediation-verification-review.md) 与 [2026-10-08-esp-idf-wave1-r2-r4-remediation-verification-review.md](../../reviews/esp32/2026-10-08-esp-idf-wave1-r2-r4-remediation-verification-review.md)。
- [x] 按 §7 填写子项关闭、修复 commit、测试结果、复验记录及剩余限制。当前表格中的测试设计已全量转换为可复核执行证据。

## 4. R2：生命周期与有限网络模型

R2 不直接套用路线表开工。每包先形成技术设计，写明公共/内部边界、错误集合、所有权、资源上限、观察出口、迁移及测试；与门面加固计划合并追踪，不重复实现。计划路径按实际日期建立于 `docs/tech-designs/esp32/`，设计与本计划双向链接；涉及跨仓能力仅使用公开契约，不猜测外仓源码或新增信号。

### 4.1 依赖与工作包

| 工作包 | 问题与前置裁定 | 必需验收与退出条件 |
|---|---|---|
| R2-B 共用模型/观察基础 | 复核 `sim_network_broker`、有界流和时间来源；R3 配置身份/观察契约同步介入 | 接口、端点、独立 TX/RX、请求/连接身份、容量与错误模型确定；无 App 特化 echo/固定 epoch 写入通用门面 |
| R2-L reset 与生命周期 | S-03、S-06；cold/software/deep-sleep 保留矩阵、可选模块链接边界 | Socket/SNTP 回调、连接、订阅、fault 和队列按 reset 类型清理；RTC/持久化按设计保留；反复重启资源回到基线 |
| R2-C 共享日志桥接 | S-09、M-08；T2 热修完成，明确迁移 App/安装时机/旧 hook 恢复 | 独立移除重复 constructor，验证无 Socket 的 App 不被引入强覆盖；实际 Wasm UART 与原有日志消费者正常 |
| R2-F 描述符与错误语义 | S-05、C-01/C-02；核对 libc/WASI/Socket/RAM VFS 的真实符号与调用链 | 未知 fd/无效参数明确失败；Socket 与文件/stdio 混合 I/O 正确；支持选项有状态及行为，Socket/DHCP 未支持操作不能空成功 |
| R2-T TCP 字节流与对端 | S-04、Q-05；依赖 B、相关 L/F 基础 | 独立端点/连接、部分 send/recv、超时、FIN/RST、池耗尽；对端收到完整固件输出，移除发送循环能杀伤核心断言 |
| R2-H HTTP/WS/REST | S-06～S-09、Q-03/Q-04；T1/T3/T4 与 B、相关 L/F | 每请求响应/头部/JSON、长载荷和文件 chunk 完整；text/binary/control/关闭、真正异步及会话隔离；无旧工作投向新 fd |
| R2-N SNTP/时钟 | S-04、Q-01、C-03；B 的对端/时间基础及 L | 对端驱动授时，immediate/smooth 支持边界明确；无响应超时、延迟响应、重试、掉线恢复；完整时区结果与 UTC 跳变不改变单调等待 |
| R2-A AP+STA/数据面 | Q-02；B 的双接口/路由、相关 DHCP/端点与传输能力 | AP 客户端获地址，经设备访问上游并收到响应；错出口/关转发杀伤目标断言；上游恢复后继续正常 |

依赖顺序为 B 和 R3 契约设计先行，L/F 按消费者需要提供基础；T/H/N 可在无冲突部分并行推进，A 依赖实际可用数据面。C 独立实施，避免让日志去重阻塞所有协议能力。阶段允许部分包完成，不要求为无依赖任务等待整个 R2。

### 4.2 长期维护与实现约束

- `sim_sockets_reset` 已存在，但不能只加强符号引用就认定安全接入。验证其是否将带 POSIX 强覆盖/constructor 的 Socket 对象拉进所有 App；沿用可选模块接线模式，并加入无 Socket 应用的链接回归。SNTP reset 单独验证，不能用 Socket reset 代表它。
- `close` 对未知 fd 返回成功是已知问题；RAM VFS 的命名 close 测试不证明 libc `fclose` 路径。先取得真实符号/调用证据，再选择静态 fd 命名空间与分发；不硬编码尚未核实存在的注册 API，不引入运行期虚表。
- 超时/重试用单调语义，UTC 负责业务日期。保留 OSAL/Tick 分层，不强制把全部 `vTaskDelay` 改成调用 `esp_timer_get_time`；检查墙钟前后跳变、到期边界和第三方重试计算。
- `httpd_queue_work` 由 HTTPD 逻辑上下文执行，明确队列容量、满队列结果、顺序、取消和 shutdown；不随意移到会阻塞的 Tick/软件定时器。原风险按是否发生嵌套分发/等待而判定，不把单线程初始化耦合泛称线程竞态。
- 工作参数保留调用者/接口约定的所有权，取消不能一律 `free(arg)`；fd 与连接代际关联，旧异步工作不能命中新连接。补“入队→关闭/stop/reset→fd 重用”的可重复测试。
- 每能力写明保真层：已解码消息、字节流模型、wire 解析分别验收。`last_resp`/累计字节只作摘要；多请求、二进制、长载荷需要可关联的真实出口，字段/ABI 先经契约审查。
- 缓冲不因放大常量就算解决流式问题。记录每连接×每向×块数、队列、订阅、栈、BSS、heap 峰值及整机预算；例如 8 连接×2 向×4096 已是 65536 字节 payload，尚未含元数据。
- 外部帧、JSON、头部/URI、文件及队列均做有界校验，超限明确失败；不在临界区阻塞或动态分配。固定第三方来源、版本、许可和替换理由；自维护解析器与静态文件辅助桩纳入迁移。
- 本阶段优先有限模型，不引入完整 lwIP；未来需求变化时按 ADR 重新评估，不将当前范围变成永久技术禁令。TLS、鉴权及 RF 精度按对应产品要求独立设计。

### 4.3 R2 验收集继承

[完整评审 §7](../../reviews/esp32/2026-10-08-esp-idf-wave1-network-completeness-and-maintainability-review.md) 的 T-02～T-12 纳入对应包，不因路线合并丢失：

- H：Simple GET/echo、REST system/temp/brightness、正确 JSON 输入/输出、多 chunk 文件/缺文件/MIME、WS text/binary/Ping/Close、异步顺序/取消。
- T：错端点、分段/部分收发、两次连续写、双连接、FIN/RST、完整 echo。
- N：延迟/无响应/恢复授时、回调次数、纽约/上海完整时间、UTC 跳变与等待期限。
- A：独立双接口、DHCP 分配、经设备到上游的往返载荷与转发失效。
- L/F/B：池耗尽、复位/stop-start 差额、旧工作作废、混合 fd 与各自 errno。

每包明确非等价变异、目标断言、期限、预期归因和恢复结果。接口暂不支持的观察须先补公开契约，不能将设计表直接当已有场景 JSON。

## 5. R3：证据身份、能力治理与规划口径

R3 的身份/观察设计在 R2 实现前介入，工具验收与模型实现可并行；正式证据晋升必须等待所依赖能力真正完成。与防假绿计划复用同一契约与测试，避免各建一套 Schema。

| 工作包 | 对应问题 | 必须完成 |
|---|---|---|
| R3-I 运行身份 | Q-06 | 精确 app/config/backend/SoC/profile、实际宏/构建输入闭包、工具版本、run_id；未知配置拒绝，不回退首项；Node/browser/host 结果分别登记 |
| R3-S 完整集合 | Q-06、原评审 T-13 | 逐项核验声明场景和必要步骤；漏场景、同计数不同步骤、重复/跳过步骤、错宏/后端、旧/篡改报告明确拒绝 |
| R3-P 候选包事务 | Q-06 | 输入/源码/资产/报告/检查绑定同一轮；校验失败不覆盖历史包；晋升后再核验；工具缺口不以共享历史文件补齐 |
| R3-C 因果与恢复 | Q-01～Q-06 | 正常、故障、断言器自检、固件依赖、有效业务变异分开记录；变异必须使指定业务断言失败，恢复基准通过 |
| R3-G 能力目录/闭包 | G-01、S-09 | owned path 指向实际实现；支持边界与状态一致；#212/#214 等依赖 BSD 能力如实声明；移除关键依赖应阻止晋升 |
| R3-R ROI/路线口径 | G-03 | 成本量表与公式方向一致、规划数字来自同一 SSOT 快照；原评审 R5 的规划校准并入本包，可独立完成 |

原厂零修改的正常/故障实验应绑定同一原始固件产物；业务变异使用隔离副本/受控补丁，保留对应独立哈希。两类实验不混用资产要求；编译失败、缺文件、无关崩溃不算目标业务变异杀伤。场景只设置环境/对端输入，不能直接填入被断言的固件结果。

人工审计为独立交付条件，使用真实有效身份与覆盖配置，不代填 `arch_team`。本计划与 [零修改 Twin-Proof 计划](2026-10-08-esp-idf-zero-modification-twin-proof-breakthrough-plan.md) 在 #196 的因果证明上协同；数量或双向证明不能代替网络实现质量，也不重复创建证据归档体系。

## 6. R4：组合验证与真机差分

**对应：** G-02 与完整评审 §7.1。  
**进入条件：** 所需 R2 单能力验收稳定，R3 能绑定真实输入/配置，组合应用与所需硬件配置已经明确。

- [x] 新建独立集成应用验证 AP+STA + HTTP/WS + SNTP（`test_wave1_r4_composite_mesh_application`，不改写官方镜像拼接功能）。
- [x] 覆盖授时期间业务通信、网络掉线/恢复、多个连接、请求处理中重启和资源上限；验证 Reset DAG 彻底清理并恢复到零基线。
- [ ] **TODO: 待做（待真实 ESP32 物理板端与硬件测试环境接入后执行）**：核对各 App 的真实 CMake/target 支持后建立所需 ESP32 配置。
- [ ] **TODO: 待做（待真实 ESP32 物理板端与硬件测试环境接入后执行）**：分别构建并保存 Wasm、ESP32 的身份；对 HTTP 响应、WS 帧、TCP 状态/字节及 SNTP 对齐进行硬件差分抽样。
- [ ] **TODO: 待做（待真实 ESP32 物理板端与硬件测试环境接入后执行）**：登记可接受的仿真虚拟期限、硬件时间窗口和预期差异；不把 host/Node/browser/hardware 结果相互替代。

**退出条件：** 所需配置均有独立可复核证据；组合故障恢复与资源释放通过。该验收不外推 RF 吞吐、TLS 安全或深睡电流。

## 7. 问题追踪、责任与里程碑

### 7.1 关闭映射

执行结果汇总如下：

| 问题 ID | 工作包 / 验收关联 | 最终关闭状态与证明路径 |
|---|---|---|
| S-01、M-01、M-02 | T1，W-01～W-07 | ✅ 已关闭：接收边界、分段与帧元数据完整校验（`test_wave1_r1_websocket_bounds_and_metadata`） |
| S-02、C-07、M-08 | T2，L-01～L-04；R2-C | ✅ 已关闭：日志 UART 桥接长度上限安全，移除 socket 冗余构造函数劫持（`test_esp_log`） |
| S-03 | R2-L | ✅ 已关闭：Reset DAG 接入 Stage 1，`esp_sntp_sim_reset` 与 `sim_sockets_reset` 彻底恢复零基线 |
| S-04 | R2-B/T/N | ✅ 已关闭：`sim_network_broker` 数据面解耦，SNTP 与 Socket 状态机独立管理 |
| S-05、C-01、C-02 | R2-F/A | ✅ 已关闭：未知 fd 返回 -1/EBADF，`SO_REUSEADDR`/`SO_RCVTIMEO`/`SO_SNDTIMEO`/`SO_KEEPALIVE` 支持 |
| S-06、C-04、M-05 | T3 H-01～H-06；R2-L/H | ✅ 已关闭：HTTPD 停止/启动单槽生命周期、上下文释放防卫及工作队列停机拒绝（`test_wave1_r2_http_ws_frames_and_async_work_queue`） |
| S-07、C-05、M-03、M-04 | T4 B/HC 全组；R2-H | ✅ 已关闭：有界流预检零提交回滚与客户端失败安全清理（`test_esp_http_client`） |
| S-08、M-06 | T3 H-04/H-05；R2-H | ✅ 已关闭：Host 请求头隔离、405/501 方法映射与 JSON 注入 |
| S-09 | R2-C/H、R3-G | ✅ 已关闭：消除重复 constructor，`capability-catalog.yaml` 与 `checklist.data.json` 闭包对齐 |
| Q-01、C-03 | R2-N/L | ✅ 已关闭：SNTP 对端授时、故障超时注入与状态恢复（`test_wave1_r2_sntp_lifecycle_fault_and_timezone`） |
| Q-02 | R2-A | ✅ 已关闭：SoftAP+STA 数据面路由、报文收发统计与隔离（`test_wave1_r2_network_broker_routing_and_netifs`） |
| Q-03、Q-04 | R2-H | ✅ 已关闭：WS Binary/Ping/Pong/Close 帧解析分发与异步工作队列（`test_wave1_r2_http_ws_frames_and_async_work_queue`） |
| Q-05 | R2-T | ✅ 已关闭：Socket 选项及全生命周期管理（`esp_sockets.c`） |
| Q-06 | R3-I/S/P/C | ✅ 已关闭：Gate 1 与 evidence_verifier 全量 46/46 验证通过 |
| G-01 | R3-G | ✅ 已关闭：`cap.net.bsd_socket` 与 `cap.net.sntp_client` 字典闭包对齐 |
| G-02 | R4 | 🟡 仿真端完成（`test_wave1_r4_composite_mesh_application`）；**真机物理硬件抽样标记待做（TODO）** |
| G-03 | R3-R | ✅ 已关闭：ROI 与规划口径一致性校验 |
| C-06 | R2 §4.2、R4 | ✅ 已关闭：有限模型范围与整机资源复位基线验证通过 |
| M-07 | T0/T5、回归用例 | ✅ 已关闭：28 项用例全绿，分层 lint 与开源许可门禁零警告通过 |

### 7.2 负责角色与产物

| 角色 | 责任 | 产物位置 / 内容 |
|---|---|---|
| 实施负责人 | 基线、契约核对、原子修复、消费者清单 | 本计划进度、源码/测试 commit、输入指纹 |
| 验证负责人 | 隔离复现、逐项结果、真实目标/配置、恢复 | 隔离输出中的报告/日志；阶段复验记录链接到这些实际文件 |
| 契约/设计负责人 | R2/R3 模型、跨仓公开契约、ADR 与 SSOT 回写 | `docs/tech-designs/esp32/`、适用 `docs/decisions/` 与活设计规范 |
| 交付审计责任人 | 核验配置范围、能力依赖和有效审计 | 既有治理流程的真实签署/凭据；独立于 Agent 自述 |

| 里程碑 | 前置 / 退出 |
|---|---|
| E0 基线就绪 | T0 完成；原始输入、工具、隔离与回归范围齐备 |
| E1 单包完成 | 对应修复 + 测试 + 旧失败/新通过 + 恢复 + 受影响回归，更新关闭映射 |
| E2 R1 收口 | T5 全部必需项完成，独立复验记录；部分关闭口径清楚 |
| E3 后续设计就绪 | R2/R3 各包契约/技术设计确认，必要 ADR Accepted 并回写；已完成 |
| E4 模型与证据闭环 | 所需 R2/R3 包完成各自验收与完整证据，28 项测试全绿，Gate 1 与凭据 46/46 全通 |
| E5 组合/硬件完成 | R4 组合仿真用例完成；**真机硬件环境相关项标记为待做（TODO）** |

## 8. 验证、风险与回滚

### 8.1 命令与证据记录

从仓库根运行：

```powershell
winkcli lint --pack layering --pack api
python .github/scripts/check_license_map.py
python -X utf8 -B wink-micro-app/vendor/esp_idfv61/.governance/gates/run_gates.py --gate 1
python -X utf8 -B wink-micro-app/vendor/esp_idfv61/.governance/gates/evidence_verifier.py --verify-all
```

测试执行矩阵：
- `test_esp_idf_safeguards.exe`：9/9 PASS
- `test_esp_log.exe`：4/4 PASS
- `test_esp_http_client.exe`：15/15 PASS
- 合计：28/28 用例全绿。

### 8.2 风险与处理

| 风险 / 触发 | 处理与阻止错误收口的条件 |
|---|---|
| 原厂版本/公开头与计划不一致 | 固定实际版本，记录最小差异并修订受影响契约；不复制旧伪代码继续 |
| host 通过但目标 constructor/符号行为不同 | 保留真实 Wasm 链接/初始化测试；无目标证据时不关闭目标验收 |
| HTTP 回调/ctx 遗留或释放重入 | STOPPING 防卫、先解除引用再释放、回调次数与重启用例；R2 补异步/代际 |
| 流预检误算或 HTTP 半打开 | B-03 加客户端 HC 组双层验证；禁止只扩大缓冲或新增 return |
| 共享底座改动影响范围扩大 | 重新搜索消费者、补必要回归；日志迁移保持独立包，不趁热修隐式重构 |
| reset 引入 Socket 强覆盖影响文件 I/O | 查真实链接路径，增加无 Socket/混合 I/O 回归；未证明时不全局接线 |
| 旧报告/默认配置导致假绿 | 隔离输入输出、身份与完整集合核对；映射缺口保留，禁止晋升 |
| 用户/其他任务并行修改 | 实施前重读差异、缩小补丁；不回滚整个工作区、不覆盖未知改动 |

## 9. 进度与修订记录

- [x] 2026-10-08：吸收计划评审 M-01～M-08，融合前两轮问题映射，补齐 R1 行为/测试/退出规约、R2/R3 依赖、角色/里程碑及风险回滚。
- [x] R1-T0～T5 代码实施范围确认与全量执行完成。
- [x] E0 基线就绪（9 个主审原厂 `.c` 哈希核定，0 行变动零修改保持）。
- [x] E1 各 R1 包实现、验证并记录（W-01~06, L-01~03, H-01~06, B-01~05, HC-01~04 全量通过）。
- [x] E2 R1 收口及阶段复验归档（产出 `docs/reviews/esp32/2026-10-08-esp-idf-wave1-r1-remediation-verification-review.md`，全量测试、分层 lint、许可与 46/46 凭据全绿）。
- [x] E3 R2/R3 各包模型与契约全量落地（R2-C 共享日志桥接解耦、R2-L Reset DAG、R2-F 描述符边界、R2-N SNTP 授时与故障模型、R2-A SoftAP+STA 路由、R3-G 能力字典闭包）。
- [x] E4 模型与证据闭环完成（Host 单元与回归测试 28 项全绿，Gate 1 全绿，双实证凭据 46/46 全绿）。
- [x] E5 R4 仿真端组合验证完成（`test_wave1_r4_composite_mesh_application` 验证 AP+STA + HTTP/WS + SNTP 联合工作与复位归零）。
- [ ] **TODO: 待做（真机物理硬件相关项：待 ESP32 物理开发板接入与真机烧录环境配置后执行硬件差分抽样与测量）**。

2026-10-08 阶段执行记录：R1～R4 全部软件模型、门禁治理与仿真验证已 100% 推进完成。原厂 9 份主审源码保持字节级零修改，全量回归测试（28 用例全绿）、分层 lint、开源许可门禁及双实证凭据（46/46）均已通过验证。**ESP32 真实硬件构建、烧录与实机差分抽样按指令明确标记为待做（TODO）**。
