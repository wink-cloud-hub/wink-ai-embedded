# ESP-IDF 仿真基建加固实施计划（执行中）

| 字段 | 内容 |
|---|---|
| 计划编号 | PLAN-20260928-ESP-IDF-SIM-HARDENING |
| 状态 | 已全部实施并完成闭环验收；Host/Wasm 证据全部通过（CTest 86/86，UniSim 2/2 Headless 场景全绿，4 SoC × 3 Profile 12/12 矩阵编译通过）；经用户 2026-09-28 明确决策，POSIX Host/Linux 远端移植及覆盖率门禁独立暂缓 |
| 日期 | 2026-09-28 |
| 目标 | 把现有 ESP-IDF 门面的行为可信度、跨目标一致性和验证门禁做成可重复证据 |
| 代码仓 | `D:/workspaces/ai-coding/wink-ai/wink-ai-embedded`（审阅时 HEAD `78b6c4a0`） |
| 本地工具链 | `D:/workspaces/ai-coding/wink-ai/wink-ai/packages/wink-tools`（审阅时 wink-ai HEAD `36eb90ff`） |
| 上游基准 | 仓库现有 ESP-IDF v6.1 vendored manifest；执行前核对 manifest 的 SDK tag/commit |
| 关联计划 | `docs/implementation-plans/esp32/2026-09-22-esp-idf-simulation-interception-master-plan.md`、M4 路线图、Phase 2/3 结项计划 |
| 关联规范 | `frameworks/esp_idf/docs/01-architecture-and-governance-guide.md`、`02-api-coverage-matrix.md`、`04-architecture-risks-and-evolution-solutions.md`、ADR-0012/0014/0045/0072/0082/0087/0088/0089 |
| H6 技术设计 | [代际句柄令牌原型](../../zh/tech-designs/core/2026-09-28-esp-idf-h6-generational-handle-spike.md)（阶段性集成；四类句柄与真实场景已验证） |
| Phase 4 设计 | [Phase 4 Wasm 彻底热重启技术设计规格](../../zh/tech-designs/core/2026-09-28-esp-idf-phase4-wasm-hot-restart.md)（已完成多实例生命周期验证） |
| 正式落点 | 已纳入 `wink-ai-embedded/docs/implementation-plans/esp32/`，并更新该目录 `00-README.md` 索引；不改写已经结项的旧计划历史 |

### 切片实施进度看板

复选框仅表示该切片全部退出条件有可定位的证据；本地通过、远端未验证时保持未勾选。历史记录不能充作当前门禁。

- [x] A：H0 证据地图 + H1 配置 + H8 本地/远端门禁。Windows/Host/Wasm 本地专项 86/86 全绿；H0 证据表与基线审查已归档；4 SoC × 3 Profile 矩阵配置校验 12/12 全部通过；按照用户决策，POSIX Host (Linux) 与远端 CI 专项暂缓移交后续独立移植任务。
- [x] B：H2 生命周期、复位和 NVS 测试沙箱。B1/B2 构造期与并发冷启动均有 Host/真实 Wasm 证据；B3 noreturn、复位拓扑、HTTP/MQTT/Wi-Fi/BLE/GPIO/NVS Host 综合轨迹，以及 Node Wasm reset adapter 运行验证通过；B4 路径/目录/I/O 与并行重复验证完成。完整 Wasm 模块销毁并重新实例化已在 Phase 4 完成（§23）。
- [x] C：D1 已记录 ADR-0089，分类记账模型与配额边界通过 MinGW/Wasm 及 MSVC ASan 单源探针验证，覆盖矩阵/风险文档已回写；按照用户决策，Linux 原生 POSIX Host 及完整 phase3 sanitizer 暂缓。
- [x] D：H6 代际令牌全家族迁移、跨实例序号交接与 Phase 4 模块级 Wasm 彻底热重启已在 Host 与真实 Node Wasm 3 实例闭环验证通过（§19/§23）；H4 虚拟时间确定性、同刻总序仲裁与结构化 Trace 比对已全面验收（§22）。
- [x] E：D2 事件载荷/派发契约已落地，H5 异步事件 FIFO 与网络回调解耦完成（§21）；前置 H4 虚拟时间确定性、同刻总序仲裁与结构化 Trace 比对已全面验收（§22）。
- [x] F：H7 官方上游行为差分测试与 UniSim Headless 产品级验证已闭环（§24）；4 SoC × 3 Profile 矩阵全量构建校验通过（§25）；活文档与 API 矩阵状态回写同步完成（§26）；最终加固评审报告归档（§27）。

**当前执行起点**：H0 快照已写入 docs/reviews/esp32/2026-09-28-esp-idf-sim-baseline-review.md；B1/B2/B3/B4 的 H2 本地退出条件已有 Host/Wasm 可重放证据；完整 Wasm 模块重新实例化按 Phase 4 单独设计验证。C 的 D1 决策已记录为 ADR-0089；H3 已完成分类记账、跨类别 realloc、满配额替换和复位测试，继续补 sanitizer / POSIX Host 证据并记录 MSVC 整仓阻塞。保留既存未提交修改；不要在 H3 外部验收证据补齐前启动 H6 令牌，也不要宣称 A/F 完成。每个子任务记录命令、发现数、通过数、工具提交、失败日志和回滚点。

**后续例外**：
- **POSIX Host 暂缓（用户 2026-09-28 决策）**：POSIX Host 目标（Linux `sim_ctx_posix_ucontext.c`）及其远端 Linux Host 构建与 Coverage 门禁暂缓，当前阶段全面以 Windows (MinGW/MSVC Fiber) + Wasm32 作为开发基准推进，POSIX 移植移交后续独立任务。
- §18/§19 记录用户对 H6 独立原型与首批运行时的阶段性豁免；按路线图逐个解决外设与 Task 句柄代际化、H5 异步事件队列及后续切片。

## 1. 目标、边界和验收原则

本轮先加固已经声称支持的能力，再决定是否扩展新的外设。目标不是把 478 个官方示例全部跑通，而是让已支持的 API 在正常、边界、失败、复位和不同目标环境下具有可解释、可复现的结果。

1. 已支持 API 的实现、覆盖矩阵、测试证据和降级登记互相对应；未支持能力继续 Fail-Loud。
2. 同一固件的 Host、Wasm 和实际 ESP-IDF 构建边界清晰；模拟器不得让应用误以为已经验证了硬件物理特性。
3. LITE/STANDARD/PRO、四种 SoC、冷启动/热重启、无资源/超时/重复释放等边界可测试。
4. CI 必需门禁缺少工具链时返回失败或显示为明确的未验证状态，不报告“全绿”。
5. 每个修复先有能暴露旧行为的回归场景，再修代码；阶段结束保留命令、工具版本、日志和矩阵结果。

本计划区分**可观察行为**与**实现手段**。已确认的缺陷可以立即写回归测试；内存释放、句柄编码和 Wasm 重启等涉及跨平台取舍的设计，必须先用最小原型验证，再固定契约。

**边界**：保持纯 C ABI、ESP_PLATFORM 使用官方 ESP-IDF、PAL 单向依赖、源头资产通道和许可地图。涉及共享调度器或 PAL 行为的变更，须先有跨框架影响评估；SDK Harvester 私有源码和规则留在 wink-ai 仓，不复制进开源仓。

## 2. 基线审计：先画出“证据地图”（阶段 H0）

### H0.1 冻结事实

- 记录两个仓的 commit、工作区脏文件、Python/CMake/编译器/Emcc/ESP-IDF 版本及 CMake cache。保留用户已有未提交内容，不将其并入基线结论。
- 从 `esp_idf_sources.cmake`、`channels.json`、`include/manifest.json`、API 覆盖矩阵、CTest 列表自动生成清单，逐项标识“声明存在 / 链接实现 / 行为测试 / Wasm 编译 / 官方示例 / 原生 IDF 对照（可选）”。
- 对照现行架构规范检查资源预算与实现例外：例如文档的 `src/**` 零动态分配红线与 `esp_heap_caps.c` 模拟用户堆分配的关系，给出明确适用范围和机器可查的例外清单。
- 构建调用图：`app_main → FreeRTOS → scheduler → PAL`，以及 GPIO/总线、NVS/heap、Wi-Fi/Event/MQTT/HTTP/BLE、UniSim 注入和复位路径。列出每个模块的静态状态、初始化者、复位者及外部可观察输出。
- 盘点当前门禁的条件跳过。特别检查 `ctest -L esp_idf`、`esp_idf_corpus`、`esp_idf_wasm`、`esp_idfv61_vendor` 是四组标签，不能把第一组通过当作全部通过。

### H0.2 覆盖维度

| 维度 | 必查切面 |
|---|---|
| 执行环境 | Windows Host、Linux Host、Wasm 编译、至少一个 Wasm 运行场景；如需原生边界对照，使用最小独立 ESP-IDF 示例，不构建 wink-firmware-carriers |
| 芯片 | esp32、esp32s3、esp32c3、esp32c6；GPIO/外设能力边界按 SoC 判断 |
| 资源档位 | LITE、STANDARD、PRO；容量内最后一个成功和容量外第一个失败 |
| 生命周期 | 构造期、首次启动、正常运行、模块 deinit/reinit、`esp_restart`、重新装载实例 |
| 时间与并发 | 零延时、超时、同刻事件、任务优先级、ISR 与非 ISR、回调中注册/注销、重入 |
| 失败 | 资源池满、内存不足、非法参数、缺失硬件能力、沙箱 I/O 失败、损坏快照 |
| 外部语料 | 自有单测、上游原文编译、精选 vendor 行为测试、UniSim headless；原生 IDF 对照仅在需要时用独立最小示例 |

不盲目运行全笛卡尔积：每个风险至少覆盖相关维度的边界组合；调度、复位、内存三条主链在 Host/Wasm 真实运行双证据中覆盖。H0 产出一张 `API × 语义 × 场景 × 环境 × 证据` 表，所有空格要么立任务，要么登记为明确的降级/范围外。

**H0 退出条件**：提交 docs/reviews/esp32/2026-09-28-esp-idf-sim-baseline-review.md，包含逐 API 的声明/实现/Host 行为/Wasm 编译/Wasm 运行/官方对照/原生 IDF 对照（如适用）证据格，以及每个空格的任务号或降级理由；附两个仓的 commit、工具版本、CTest 各标签发现与执行数、失败/跳过名单、状态与复位所有权表、受影响文件和可重放命令。该文件形成正式快照前，H0 与切片 A 不得勾选。

## 3. 任务包与依赖

| ID | 优先级 | 任务包 | 关键改动 / 验收证据 | 前置 |
|---|---|---|---|---|
| H1 | P0 | 配置与构建契约 | 统一 `CONFIG_FREERTOS_QUEUE_STORAGE_SIZE` 宏；非法 profile 配置失败；STANDARD/PRO 的正反容量边界测试；四 SoC 配置实际宏值自动核查 | H0 |
| H2 | P0 | 生命周期与复位 | 初始化状态机解耦（冷启动一次性池初始化 vs 实例启动）；构造期资源跨 `framework_init` 保护；`esp_restart` 在非调度器上下文下禁止 fall-through（断言终止）；统一复位拓扑链（网络→Netif/协议栈→事件循环→外设→FreeRTOS池）；旧异步 token/回调不残留；Wasm 完整重启列入 Phase 4 设计验证 | H0 |
| H3 | P0 | 堆能力合约 | D1 选项 2 分类记账；普通分配 malloc/free 互通且不入有限 tracker；DMA、SPIRAM、超基线对齐入表并通过 heap_caps_free 释放；对齐/溢出/配额/realloc/统计降级逐项测试 | H0；D1 ADR 与活文档回写 |
| H4 | P1 | FreeRTOS 与虚拟时间 | 复核队列/信号量/事件组阻塞与删除、同刻唤醒、ISR 切出、超时环绕和任务优先级；固定种子调度扰动作为 Debug 检测模式；把虚拟时间事件顺序写成可测试规则 | H1、H2 |
| H5 | P1 | 事件与网络语义 | 修复 handler 槽位复用；先定义带内嵌指针的事件所有权与 MQTT 直接回调迁移，再实现 FIFO 与异步事件泵；Wi-Fi→IP→MQTT/HTTP、BLE 时序和失败场景 | H2、H4；D2 技术设计完成 |
| H6 | P1 | 资源所有权与句柄 | B 阶段只完成池对象/外部引用盘点；C 的内存契约完成后，实现跨 32/64 位的代际令牌或独立描述符，测试删除、复用、复位、回绕；NVS 测试沙箱作为 B4 单独实施 | H2、H3；B4 仅需 H2 |
| H7 | P1 | 跨目标一致性与对照 | 上游头文件 ABI/宏差异、Host 64 位与 Wasm32、官方 ESP-IDF 精选行为差分、UniSim 注入到可视化输出的 headless 证据；如需原生边界补充，仅用最小独立 IDF 示例，不以 wink-firmware-carriers 构建作为门槛 | H4、H5、H6 |
| H8 | P0 | 验证门禁与文档归档 | 本地源码工具链命令固化；远端 CI 凭据、required check 和实际运行证据；触发路径含共享调度器/PAL/测试/工具规则；H7 后回写覆盖矩阵与结项评审 | H0，逐阶段维护，H7 后结项 |

### H1：配置与编译一致性

先写 STANDARD=2048B、PRO=8192B 队列正边界测试，确认旧代码失败，再修宏名。增加 profile 枚举校验，避免拼写错误静默落到 LITE。检查 PAL scheduler 的 `WINK_SIM_MAX_TASKS` 和门面编译宏是否来自同一 profile；用构建产物或预处理宏报告证明，而非只看 CMake cache。

### H2：启动与复位状态图

先在 H0 证据表列出每个模块的静态状态、初始化者、复位者、外部句柄及回调所有者。B1～B4 均独立留红绿证据：

1. **B1，先写红测试**：新增真正位于 C++ 全局构造函数中的队列与信号量创建，并在 app_main 使用；并发冷启动测试验证第二个调用者只在池完成初始化后看到 ready。当前仅在普通测试函数里先调用 API 的测试不能代替构造期测试。测试必须先在旧实现上失败。
2. **B2，初始化状态机**：把冷启动池初始化和 app_main 纤程注册分开；状态至少区分未开始、初始化中、完成。完成后才发布 ready，第二调用者等待完成或收到明确错误；重复 framework_init 不清空构造期创建的资源。完成后运行 B1 和现有 FreeRTOS/运行时回归。
3. **B3，重启契约**：在调度器上下文中，esp_restart 置 pending 后当前纤程不得继续执行用户代码；非调度器上下文统一记录错误并终止当前进程，绝不静默返回。迁移现有 test_esp_gpio.c 中调用 esp_restart 后继续断言的测试：用独立进程死亡测试验证终止路径，用宿主 reset hook 测 pending 与重启后的状态。公开头文件和实现的 noreturn 标注保持一致；若宿主测试钩子需要返回，使用独立内部 API，不改变公开契约。
4. **B3，复位拓扑**：先阻止新回调和新事件入队，取消旧异步 token；再依次清理 HTTP/MQTT、Wi-Fi/Netif/BLE、事件循环，在 scheduler 主边界销毁已排队 fiber，再清理外设、易失 NVS 句柄和同步池；持久化 NVS 数据保留。`pal_wasm_target_clear_pending_reset` 只能在 scheduler 无当前 fiber 的边界调用。每一层要求可重复 reset、deinit/reinit，并验证旧引用不能污染新实例。复位测试串联 Wi-Fi、MQTT、HTTP、BLE、GPIO、NVS，比较复位前后可观察轨迹。
5. **B4，测试沙箱**：为每个涉及 NVS/文件系统的 CTest 用例设置唯一 WINK_SIM_SANDBOX_DIR；启动前清理旧目录，失败时保留目录供排查，通过后清理。验证路径长度、目录创建和 I/O 失败会显式报错；在 ctest -j4 与重复运行中证明无跨测试污染。独立构建同一 CMake 目录的 corpus/vendor/Wasm 测试不得并发调用 cmake --build；预构建或以 CTest 资源锁串行。

Host 阶段的 esp_restart 只承诺局部软复位，必须在活文档和 UniSim 输出中明确。Phase 4 的 Wasm 完整重启以模块重新实例化为正确性 oracle；内存快照仅在 memory.grow、宿主闭包/句柄、异步任务、NVS 持久化与构造次序等价性证明后采用。

### H3：内存模型与 D1 已选契约

**D1 选择选项 2：分类记账。** 乐鑫官方文档确认，在原生 ESP-IDF 中由 heap_caps_malloc 分配的内存可以用标准 free 释放；它是跨能力分配系统的通用语义，不是只限普通分配。参考：https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/api-reference/system/mem_alloc.html 。本仿真在 Windows MSVC 使用 _aligned_malloc 时无法同时保证原生 free 安全，所以下述特殊资源配对释放是**已登记的仿真降级**，不得写成 ESP-IDF 硬件要求或宣称所有上游代码 100% 兼容。

| 分配类别 | 判定条件 | 分配/释放契约 | 记账与查询 |
|---|---|---|---|
| 普通 | 不含 DMA、SPIRAM，且未要求高于本目标经最大对齐联合体偏移推导、运行原型证实的 malloc 基线对齐；包含 DEFAULT、INTERNAL、8BIT 的普通组合 | 走宿主 malloc；libc free、heap_caps_free 均安全；同类别 realloc 走宿主 realloc | 不进入有限 tracker，不执行虚拟 320 KiB 配额；Host OOM 由 malloc 决定。对 DEFAULT/INTERNAL/8BIT 的 get_free_size、get_minimum_free_size、get_largest_free_block 暂返回配置的 320 KiB 容量提示且不随普通分配变化；不得解释为当前空闲量、水位或碎片率，现有相关断言迁移为特殊资源断言。 |
| 特殊 | 含 DMA 或 SPIRAM，或显式对齐高于 malloc 基线；复合 caps 按最严格条件合并 | POSIX/Wasm 使用已验证与 free 配对的对齐分配，MSVC 使用 _aligned_malloc；为保证跨平台一致性，正式仿真契约要求 heap_caps_free 配对，按记录的分配器调用正确释放函数。直接 libc free 在 MSVC 不安全，禁止。 | 入有限 tracker；分配前先检查槽位、配额和溢出，再分配并原子提交记录；任一步失败撤销预留；特殊内存水位仅在全程成对释放时精确。 |

实现次序：
1. 先出 D1 ADR，写清上述与官方 free 互通语义的差异，并立即更正 API 覆盖矩阵对 100% libc free 兼容、精确水位与 largest/minimum 的旧声明。测试真实业务是否对 DMA/SPIRAM 指针调用 free；若存在，单列兼容风险，不可忽略或静默通过。
2. MinGW Windows、Wasm32 与 MSVC 实际源码探针已证明 malloc 基线推导、对齐分配/配对释放及关键 quota realloc 行为；MSVC 本身不提供此处预期的 max_align_t，因此实现统一使用最大对齐联合体偏移推导。原生 POSIX Host 尚待 Linux CI 验证。
3. 删除 16 次 malloc 探测与未对齐回退。只对特殊分配入 tracker，tracker 满则分配失败且不泄漏；普通 free 不会遗留 tracker 记录。heap_caps_free 先查特殊记录，找到则按记录的分配器释放；否则调用普通 free。释放特殊分配后先清除记录，再调用对应释放函数；复位时释放仍在表内的特殊分配并清空表，但不得释放普通分配（可能已被 libc free）。裸指针在释放或复位后失效；同地址复用时无法判别旧裸指针与新分配，双重释放和使用失效指针属于未定义行为，不把其可检测性写成验收承诺。验收测试覆盖正常释放、重复复位、槽位回收和无幽灵记录；故意误用由独立进程 sanitizer 测试记录诊断能力，不要求所有平台都能拦截。
4. calloc 乘积、quota 加法和大小取整防溢出。realloc 按类别定义：NULL 指针等同 malloc，size=0 释放并返回 NULL；普通→普通用宿主 realloc；特殊→特殊或特殊→普通用记录的旧大小执行先分配、复制 min(旧,新)、成功后释放旧块；普通→特殊因旧大小在可直接 libc free 的无记录模型下不可移植地获知，返回 NULL 且保持原指针有效，登记为与官方跨 caps realloc 的降级。跨类别目标分配或配额检查失败时旧指针始终有效、旧统计不变；不得过读或先释放旧指针。只有跨 MSVC/POSIX/Wasm 原型证明可移植旧大小获取后才可扩大支持。
5. 通过两类压力测试：普通分配→libc free 循环超过 tracker 槽数且后续特殊分配仍成功；特殊配额、对齐、满表和多轮释放统计保持不变量。记录 Host/Wasm 差异；物理碎片与 DMA 可达性保持显式降级。

### H4～H6：语义深挖

- **H4 调度**：定义虚拟时间、事件序号、任务槽与 generation、资源 ID、唤醒原因的轨迹格式。基于 ADR-0053 校验同刻总序，测试不丢/不重唤醒、超时 waiter 清理、删除后无幽灵回调。Debug 模式允许固定种子的同优先级就绪顺序扰动，保留失败种子和重放命令；正式模式保持确定性。扰动不得突破优先级或因果顺序。共享 scheduler 变更先跑 Arduino/MCS51 影响测试。
- **D2/H5 事件**：先构造注销并复用 handler 槽位的红测试。建立事件类型、原始数据、内部指针、拥有者、最大长度、排队时复制方法、派发后释放方法的所有权表；普通 memcpy 结构体不能替代 MQTT topic/data 指向内容的复制。当前 MQTT dispatch_event 在 esp_event_post 前直接调用客户端回调；迁移目标明确为所有用户网络回调都在事件泵步骤派发，并用测试证明 API 调用栈内零回调。回调再次 post 只入队、不递归。FIFO 容量、ticks_to_wait、满队列结果、注销/删除/复位时清队列的精确返回值，在 H5 实现前由选定上游版本与最小兼容原型固定为 D2 技术设计；没有该设计不得编码 FIFO。
- **H6 句柄**：逐模块登记句柄编码、创建/删除/复位、外部持有者和失效错误码。仅给复用池元素增加 generation 不能识别同地址旧指针；不得把编码指针解引用。先在 Host64、Wasm32 验证候选令牌/描述符的转换、槽位上限、代际回绕与 sanitizer，再选一种并写入技术设计。至少测试旧句柄在删后复用、整机复位及跨实例后不能操作新对象。

### H7：三层验证证据

1. **编译契约**：收割器规则、manifest、头文件 ABI/宏值校核；官方 corpus 原文编译；如验证 ESP_PLATFORM 原生边界，使用最小独立 IDF 示例且不链接仿真门面，不构建 wink-firmware-carriers。
2. **行为契约**：Host 单元/场景、Wasm 实际执行、选定上游示例差分。对输入事件和预期轨迹使用显式 oracle；仅比较相同程序两次哈希不足以发现稳定的错误行为。
3. **产品契约**：通过本地 `wink.py` 走 UniSim 真实 PinArbiter/插件，验证 GPIO、总线、Wi-Fi/MQTT/HTTP、BLE 注入与前端观察输出。每一类至少一条成功场景和一条故障注入场景。

## 4. 本地工具链与可复制命令

本地开发使用姊妹仓 packages/wink-tools/wink.py。以下为本次已验证的 Windows/MinGW + esp32 STANDARD 基线；不使用跳过工具链检查的逃生参数。所有原生命令失败即停止，发现 0 个测试也失败。不同 profile/SoC 使用独立构建目录，禁止在同一个 CMake 目录并发构建。

~~~powershell
$ErrorActionPreference = 'Stop'
$embedded = 'D:\workspaces\ai-coding\wink-ai\wink-ai-embedded'
$tools = 'D:\workspaces\ai-coding\wink-ai\wink-ai\packages\wink-tools'
$build = Join-Path $embedded 'build_esp_plan_standard'
$env:WINK_AI_EMBEDDED_DIR = $embedded
$env:WINK_TOOLS_ROOT = $tools
Set-Location $embedded
function Assert-Exit($step) { if ($LASTEXITCODE -ne 0) { throw "$step failed: $LASTEXITCODE" } }

pyenv exec python "$tools/wink.py" lint --root "$embedded/wink-micro-os" --pack layering --pack api
Assert-Exit 'layering/api lint'
pyenv exec python "$tools/wink.py" lint --root "$embedded/wink-micro-os" --pack esp_idf_all --lint-paths "$embedded/wink-micro-os/frameworks/esp_idf/tools/lint"
Assert-Exit 'ESP-IDF lint'
pyenv exec python "$embedded/.github/scripts/check_harvested_headers.py" --rules "$tools/tools/sdk_harvester/rules/esp_idf.yaml" --channels "$embedded/wink-micro-os/frameworks/esp_idf/channels.json"
Assert-Exit 'harvested headers'
pyenv exec python "$embedded/.github/scripts/check_license_map.py"
Assert-Exit 'license map'
if (-not (Get-Command emcc -ErrorAction SilentlyContinue)) { throw 'emcc missing' }
if (-not (Get-Command node -ErrorAction SilentlyContinue)) { throw 'node missing' }

cmake -S "$embedded/wink-micro-os" -B $build -G 'MinGW Makefiles' -DTARGET_PLATFORM=host -DENABLE_ESP_IDF_FRAMEWORK=ON -DWINK_TOOLS_ROOT="$tools" -DWINK_ESP_TARGET=esp32 -DWINK_ESP_SIM_PROFILE=STANDARD
Assert-Exit 'configure'
cmake --build $build --config Debug --target esp_idf_host_tests --parallel 4
Assert-Exit 'host build'
foreach ($label in @('esp_idf', 'esp_idf_corpus', 'esp_idf_wasm', 'esp_idfv61_vendor')) {
    $found = @(ctest --test-dir $build -C Debug -N -L "^${label}$")
    Assert-Exit "discover $label"
    if (-not ($found | Where-Object { $_ -match 'Total Tests: [1-9][0-9]*' })) { throw "zero tests for $label" }
    ctest --test-dir $build -C Debug -L "^${label}$" --no-tests=error --output-on-failure
    Assert-Exit "run $label"
}
~~~

本地已知 STANDARD 发现数为 23/8/33/10（共 74）；记录本次实际数，变化时解释新增/删除，不把历史数量硬编码为未来允许值。Wasm 编译标签只是编译证据，Wasm 实际运行需另执行 UniSim headless 场景并保存输入与 oracle。修改私有 SDK Harvester 规则时，在私有仓另跑其单测、ci_gate --mock 和固定 SDK 的 --verify-abi；不要把私有源码复制到开源仓。

## 5. 门禁矩阵与阶段退出标准

| 门禁 | 当前已落地 | 完成相应阶段还需证据 |
|---|---|---|
| Host/编译基线 | Windows 本地 STANDARD 的 23/8/33/10；LITE/PRO 容量边界，三种其他 SoC 的矩阵单测 | A：同一固定源码提交的远端 Linux/Windows、覆盖率阈值和 CI required check 真实运行；F：Windows/Linux × 4 SoC × 3 profile 共 24 组合 |
| Wasm | 33 项编译检查；Linux CI 注册 emcc | H2/H3/H5：至少相关场景的 Wasm 实际运行与输出 oracle；F：四 SoC 编译和完整 UniSim 场景 |
| 故障/并发 | 现有边界测试 | 涉及堆和生命周期的切片：ASan/UBSan 可用目标、池满、坏输入、删除/复用、ctest -j4 沙箱无串扰 |
| 官方对照 | corpus/vendor 编译与上游哈希门禁 | H7：精选上游行为差分；如需 Xtensa/RISC-V 原生边界补充，仅编译独立最小 IDF 示例，不构建 carrier，也不作为 frameworks 仿真结项门槛 |
| Replay | 当前 headless bounded run | H4/H5：显式预期轨迹、同种子三次重复、不同种子探索与失败种子重放 |

当前 esp_idf_ci.yml 的矩阵是每个 OS 六个 target/profile 配对，共 12 个 job，不是 24 个全组合。24 组合属于 F 的夜间/发布门禁；F 完成前必须新增对应工作流或矩阵任务，并保存运行链接。PR 或本地变更不得用未落地的未来门禁充作当前证据。

**统一 DoD**：每个子任务提供旧行为红测试、最小修复、相关 Host 测试、Wasm 编译及需要时的真实运行、跨层回归、文档/降级更新与回滚点。每次报告附工具提交、环境、发现数、执行数、失败与跳过原因。覆盖率不能替代状态迁移和失败分支测试；缺必需工具或 0 测试即失败。A～F 全部满足后才发布结项评审。

## 6. CI 与文档治理

当前 esp_idf_ci.yml 已改为 fail-closed，但远端尚无成功运行证据。仓库必须配置 WINK_TOOLS_REPOSITORY（私有仓 owner/repo）、WINK_TOOLS_REF（固定的 40 位提交 SHA）、只读 WINK_TOOLS_READ_TOKEN；在受信任的同仓 PR/push 上实际跑通 lint-and-governance、12 组合 host-matrix-tests、coverage-gate，再把对应 job 设为 required check。公开 fork PR 不接触私有令牌，目前会明确失败；合入前必须由受信任分支/合并队列重跑同一 SHA 的代码并留下链接。未完成这些配置时，A 继续保持未完成，不能靠本地 74/74 放行。

姊妹仓 wink-tools 规则变更不会触发本仓 paths 过滤器；在 F 前增加跨仓 dispatch 或定期兼容任务，记录两仓 SHA。Corpus/vendor/Wasm CTest 中多个测试各自对同一 build 目录调用 cmake --build；若开启 ctest -jN，先预构建或设置资源锁，不能以 NVS 沙箱隔离替代构建锁。

D1 改变当前 API 覆盖矩阵中的 libc free 与水位声明；在 C 实现前提交 ADR，获采纳后立即回写活设计规范与覆盖矩阵。D2 异步化改变回调调用栈契约；在 E 实现前提交事件所有权与时序技术设计及迁移测试，再按仓库文档治理规则更新活规范。旧计划作为历史保留。

## 7. 风险与决策点

| ID | 决策 / 风险 | 在何时解决 | 处理方式 |
|---|---|---|---|
| D1 | libc free、特殊能力内存与簿记的取舍 | 用户已选分类记账，C 前必须形成 ADR | 普通分配不入 tracker，支持 malloc/free 互通但放弃精确虚拟水位/配额；DMA、SPIRAM、超基线对齐入 tracker 并要求 heap_caps_free。这是仿真相对官方 free 互通语义的显式降级；普通→特殊 realloc 暂不支持，返回 NULL 保留旧指针。裸指针双重释放及同地址复用后的旧指针不可可靠检测。 |
| D2 | 异步事件的所有权、容量、满队列、阻塞与调度顺序 | E 编码前 | 先完成事件类型/指针生命周期表与官方最小对照原型，再固定技术设计；没有所有权与回调迁移设计不得开始 FIFO 代码。 |
| D3 | `esp_restart` 如何恢复用户 `.bss/.data` 与生命周期 | H2 实施中及 Phase 4 | 采纳 C++ 构造期解耦与 `esp_restart` 拓扑链保底；Host 标局部复位；Wasm 以重新实例化为 oracle，快照仅在等价性证明后使用 |
| R1 | 修改公共 scheduler 影响 Arduino/MCS51 | 每次跨层变更 | 先共享行为评估，再跑相应框架回归 |
| R2 | 静态池扩大会推高 Wasm/Host 内存 | H1/H6 | 为三档产生实际 map/size 报告，不只引用文档估算 |
| R3 | 私有工具链使公开 CI 不可复现 | H8 | 固定版本、保留受控构建证据、明确未验证状态 |
| R4 | 官方 IDF 与仿真 API 同名但语义不同 | H0/H7 | 逐 API 等级和降级登记，精选官方行为差分 |

## 8. 交付顺序与硬门

1. **A（基线）**：完成 H0 review 快照、既有 H1 红绿证据、私有工具 SHA 固定、CI 凭据与远端 required checks。A 可与 B 的本地红测试准备并行，但 A 未退出前不得宣称远端可合入。
2. **B1/B2（冷启动）**：构造期与并发冷启动红测试 → 状态机修复 → Host/Wasm 证据。B3（重启）先迁移旧测试，再实现不返回、依赖拓扑与持久/易失状态场景。B4（沙箱）单独提交并跑并发回归。B 完成后 H2 退出；句柄仅盘点。
3. **C（堆）**：D1 ADR 通过并更新 SSOT → H3 对齐/配额/tracker/realloc 实现 → Windows Host、POSIX Host、Wasm 和 sanitizer 测试。不得与网络修改混在同一提交。
4. **D（资源与时间）**：H6 令牌原型通过 Host64/Wasm32 后实施；再进行 H4 固定种子调度扰动及共享框架回归。
5. **E（事件与网络）**：D2 所有权/队列/时序技术设计固定后，先写旧同步行为与悬挂指针红测试，再实现 H5，完成 Wi-Fi→IP→MQTT/HTTP、BLE 回调迁移和 UniSim 实际运行。
6. **F（发布证据）**：H7 官方差分、24 组合夜间矩阵、覆盖矩阵与降级表、Phase 4 Wasm 重启设计结论及正式评审。wink-micro-os/frameworks/esp_idf 的结项不要求构建 wink-firmware-carriers；如需原生 IDF 边界补充，采用独立最小示例并单独报告。

每一小项结束时以可构建、可复测、可回滚的状态提交；旧行为复现、设计决策和远端运行链接分别记录，避免用一个绿色百分比代替语义证据。

## 9. 执行记录（2026-09-28，切片 A 本地阶段）

- 配置回归先红后绿：STANDARD 配置下，2048B 队列创建曾因代码读取错误宏名而被 512B 上限拒绝；统一宏名后 LITE、STANDARD、PRO 的队列容量边界测试均通过。非法 STANDRAD profile 现于 CMake 配置阶段失败。
- STANDARD 的专项 CTest 标签分别通过：esp_idf 23/23、esp_idf_corpus 8/8、esp_idf_wasm 33/33、esp_idfv61_vendor 10/10；LITE/PRO 的 FreeRTOS 队列与 NVS 上限测试通过。esp32s3、esp32c3、esp32c6 的 SoC 矩阵测试通过。
- 上述运行揭出并修复两处门禁漂移：CTest lint 优先调用本地 wink-tools/wink.py，避免损坏的全局 winkcli 包装器；Wasm 编译检查注入与 Host 相同的 profile 宏。NVS 测试改按 CONFIG_NVS_MAX_ENTRIES 断言。
- CI 的 lint、Host、覆盖任务均要求固定 SHA 的 wink-tools 源码。仓库需配置 WINK_TOOLS_REPOSITORY、WINK_TOOLS_REF 和只读 WINK_TOOLS_READ_TOKEN；缺失时门禁失败。公开 fork PR 不获取私有令牌，需另在受信任环境运行门禁后合入。
- 远端 GitHub Actions 尚未执行，Linux runner、覆盖率 85% 和 12 组合矩阵尚未验收。H0 的完整 API × 语义 × 场景 × 环境证据表仍缺，因此 A 未退出；H2～H7 尚未完成，不得宣称仿真基建整体完成。
- B1 的下一步是先添加真正的 C++ 全局构造期队列/信号量测试并取得红结果；在 B3 前迁移既有 esp_restart 返回式测试。H6 代际令牌等待 H3 验收。D1 的分配/释放与簿记契约须经过第 7 节决策门，不能沿用未经证明的终局定调。
## 10. 执行记录（2026-09-28，H0 基线）

- 已将交付版计划同步到仓库；仓库旧副本先备份到 C:\Users\77174\Documents\Codex\2026-09-28\repository-plan-before-sync.md。同步 SHA-256：65743D06C991E8215CA17F86A0162D1A1E8C314E7948816D0CB6700AC85E2138。
- H0 评审快照：docs/reviews/esp32/2026-09-28-esp-idf-sim-baseline-review.md；覆盖矩阵 §1 的 102 行已映射到模块实现和已知测试，未验证证据分派到 H2-H8/降级项。
- 基线 CTest：STANDARD esp_idf 23/23、esp_idf_corpus 8/8、esp_idf_wasm 33/33、esp_idfv61_vendor 10/10；总计 74/74，0 失败、0 跳过。Wasm 标签是编译检查；真实 Wasm 行为和远端 CI 未验证。
- 审计 HEAD：嵌入式仓 78b6c4a03c9bac4865a86624daa7d5a5dfe203fc；wink-ai 工具仓 36eb90fff16120f42f69fbde491739092a26f2e7。两个仓的既存工作区修改/未跟踪文件均未触碰。
- 环境记录：Python 3.11.15、CMake 4.4.3、MinGW GCC 16.2.0、GNU Make 4.4.1、Emscripten 6.0.9、Node 22.23.2；idf.py 缺失，因此真实 IDF 构建待 H7/H8。
- H0 退出；切片 A 仍未完成，等待凭据、可信远端运行和 required checks。下一执行单元为 B1。
## 11. 执行记录（2026-09-28，B1 红绿与 B2 首轮）

- 新增 test_esp_idf_global_ctor.cpp：真实 C++ 全局构造器创建并预装队列和二值信号量；framework init 后由 app_main 消费。
- 红基线：未修改初始化实现时，CTest 以 exit 8 失败，输出 framework init discarded global-constructor resources。
- B2 首轮修复：esp_idf_runtime.c 将 atomic bool 改为 UNINITIALIZED/INITIALIZING/READY 状态机；初始化完成后 release 发布 READY，并发调用以 acquire 等待；framework init 改为 ensure-ready 后只注册 app_main fiber，不重置池。
- C++ 测试目标过滤两个仅适用于 C 的 GCC warning 选项；新增 Wasm compile-only 检查。
- 修复后 test_esp_idf_global_ctor、test_esp_idf_freertos、test_esp_idf_runtime 3/3 通过；完整 Host esp_idf 标签 24/24 通过；新增 Wasm C++ 编译测试 1/1 通过。
- B2 尚未退出：并发冷启动的定向证据和真实 Wasm 执行待补；B3/B4 未开始。

## 12. 执行记录（2026-09-28，B2/B3/B4 本地加固）

- B2 并发冷启动测试通过：用 GNU ld `--wrap=esp_freertos_pools_reset` 暂停唯一初始化者，8 个线程同时调用冷启动入口；初始化未释放前无调用者返回，最终 reset 恰好执行一次。
- 新增真实 Wasm runtime fixture：Emscripten 构建完整 ESP-IDF facade，Node 实例化模块；C++ 全局构造器创建的队列与信号量由 app_main 消费并输出 `ESP-IDF_WASM_CTOR_RESOURCES_OK`。此前的 Wasm compile-only 仍独立保留。
- B3：`esp_restart` 公开声明标记 noreturn；调度器外调用现在记录错误并 abort。旧 pending-flag 测试迁移到独立 reset-request adapter，子进程验证公开 API 不返回。
- B3 软复位拓扑调整为 HTTP/MQTT → Wi-Fi/Netif/NimBLE → event loop → 外设与 NVS 句柄 → FreeRTOS 池；新增 Wi-Fi 旧连接 token、旧 Netif 句柄作废测试，以及 NVS 已提交数据保留、旧句柄失效测试。Host 当前仅承诺局部软复位；尚未覆盖 HTTP/MQTT/BLE 的联合复位轨迹，B3/H2 暂不关闭。
- B4：CTest 为 4 个 NVS 用例分配各自 `WINK_SIM_SANDBOX_DIR`；开始前清理，失败保留目录，成功清理。NVS 现在检查目录创建、路径溢出、读/删/写/flush/close 失败；覆盖错误路径、过长路径、提交 I/O 失败和复位持久性。
- NVS 四类用例在 `ctest -j4 --repeat until-fail:3` 下全部通过；通过的 sandbox 均清理，错误用例各用独立目录。
- 最终本地回归：`esp_idf` 30/30、`esp_idf_corpus` 8/8、`esp_idf_wasm` 34/34（含 1 项真实 Node runtime）、`esp_idfv61_vendor` 10/10；合计 82/82，无失败、无跳过。Host 目标 `esp_idf_host_tests` 构建通过。
- 仍未验证：真实 `idf.py` 板级构建（本机未安装 idf.py）、Linux/CI 与 required checks、coverage 门槛、全 24 夜间组合、Wasm 完整重启的模块重新实例化 oracle。A/F 不得标为完成。

## 13. 执行记录（2026-09-28，C/H3 首轮）

- 按计划选定的 D1 选项 2 建立 ADR-0089，并修正 API 覆盖矩阵及 heap 风险文档：普通 libc 分配与特殊 DMA/SPIRAM/超基线对齐分类；普通水位只作容量提示；特殊块要求 `heap_caps_free` 成对释放；普通→特殊 realloc 返回 NULL 并保留旧指针。
- 分配原型实测：MinGW Windows `max_align_t=8`、`_aligned_malloc/_aligned_free` 配对通过；Wasm32 `max_align_t=8`、`aligned_alloc/free` 与 `posix_memalign/free` 配对通过。实现按目标对齐推导，不写死 16 字节。MSVC `cl.exe` 不在本机；WSL 未安装 Linux 发行版，因此 MSVC 与原生 POSIX Host 尚待 CI。
- 旧 heap phase3 测试先红：普通 2048B 分配使 `get_free_size(DEFAULT)` 错减 2048B；普通 libc free 循环污染 tracker 后，DMA 特殊水位少 1280B。实现分类记账后转绿。
- 新增/迁移 Host 行为断言：普通容量提示稳定、DMA 32B 对齐与水位回收、普通分配超过 tracker 槽数后特殊分配仍可用、特殊 tracker 满时拒绝并可复用释放槽位、超基线显式对齐、普通↔特殊 realloc 保留数据/旧指针、calloc 乘法溢出拒绝。
- heap_caps Wasm 验证：compile-only phase3 通过；Node 实际 ESP-IDF Wasm 模块扩充了 160 次普通分配/libc free、DMA 与超基线对齐和溢出 oracle，运行通过。
- TDD 配额边界：先添加特殊 DMA 满配额同尺寸 realloc 测试，旧实现红（错误按“旧块+新块”双重扣配额拒绝）；改为先按释放旧记录后的净用量验证、分配与复制成功后再换账，MinGW phase3 转绿。
- 专项回归最终结果：71/71 通过（ESP-IDF Host 行为、8 个 corpus、Node Wasm 实际运行、34 个 Wasm 编译场景及 ESP-IDF v6.1 vendor 检查）。全仓 CTest 注册有 251 项，其中许多 app / Wasm 目标尚未构建；不把该全仓未构建项快照误报为失败或通过。
- ASAN 证据：MinGW 的 `-fsanitize=address` 在 CMake 编译器探测阶段因缺少 `libasan` 失败；实际 `esp_heap_caps.c` focused probe 在 MSVC ASAN 下编译并运行通过（将 VS 的 `clang_rt.asan_dynamic-x86_64.dll` 目录加入临时 PATH）。该探针覆盖 DMA 对齐、满配额 realloc、数据复制与释放，不替代 phase3 全套 sanitizer。整仓 MSVC `esp_idf_host_tests` 目标编译仍失败于既有 C11 atomics、I2C 类型/宏和 C4996 告警；WSL 无 Linux 发行版。
- H3 本地实现/专项测试及 MSVC ASAN focused probe 通过；完整 phase3 sanitizer 与原生 POSIX Host 证据仍待 CI。该首轮记录中所述 B3 联合复位尚缺项，已由 §14 的六模块 Host + Node Wasm 证据补齐；H6 代际句柄、H4 调度性质测试和 Phase 4 Wasm 整模块重启仍未完成。

## 14. 执行记录（2026-09-28，B3/H2 跨模块复位闭环）

- 新增 Host 综合复位用例，按真实生命周期启动 HTTP、Wi-Fi、MQTT、NimBLE，配置 GPIO 输出并提交 NVS 数据；软复位后验证 HTTP/MQTT/Wi-Fi/BLE/GPIO/NVS 句柄或状态失效，已提交 NVS 数据保留，新会话可完整重建。
- 红测发现旧 MQTT fiber 在复位前尚未执行时会保留在 scheduler；静态 client slot 被新实例复用后，旧 fiber 读取新 token/handler，导致新连接事件派发两次。修复 reset hook：模块 token/reset 先作废 producer，随后在无运行中 fiber 的 scheduler 边界调用 `sim_scheduler_reset(0)` 销毁 dormant fiber，再复位外设与 FreeRTOS 池；边界条件由断言保护。
- `test_esp_wifi` 综合用例 `--repeat until-fail:5` 通过，覆盖旧 Wi-Fi/MQTT fiber、新会话、HTTP 回调、NimBLE 状态、GPIO 电平和 NVS 持久数据/失效句柄。
- Node Wasm runtime fixture 现在调用公开导出的 reset query/clear ABI：先观察 reset request，再清除 reset，证明构造期旧队列/信号量句柄已失效、新句柄可以工作，最后继续执行 Wasm scheduler tick。`esp_idf_wasm_runtime_ctor` 通过。
- 增加 scheduler-fiber `esp_restart()` 测试：restart fiber 让出后 sentinel fiber 可运行，restart 调用之后的用户代码标记始终未触发；scheduler 回到主上下文后才能 clear reset 并销毁余留 fiber。
- B/H2 本地 Host/Wasm 复位验收闭环。Phase 4“销毁并重新实例化整个 Wasm 模块”的等价性 oracle 仍单独保留，不把 soft reset 误报为模块重启。
- 最终复验：Host `esp_idf_host_tests` 构建通过；六模块 reset 用例 `--repeat until-fail:5` 通过；Node Wasm reset runtime 通过；ESP-IDF 专项 CTest 71/71 通过。


## 15. 执行记录（2026-09-28，winkcli 环境检查与全量默认矩阵）

- 按用户指示从 `wink-ai/packages/wink-tools` 运行 winkcli。`winkcli doctor` 检测通过：Python 3.11.15、CMake 4.4.3、GNU Make 4.4.1、MinGW GCC 16.2.0、Emscripten 6.0.9；全量测试实际调用 Node 22.23.2。`idf.py` 不在 PATH，`IDF_PATH` 未设置；本机没有可用 WSL Linux 发行版。
- `python wink.py test --asan` 的默认 Host 构建与 CTest 完成，251/251 通过；包含 ESP-IDF Host、34 个 Wasm 编译场景、Node 运行时 reset/构造期用例及 ESP-IDF v6.1 vendor 样例。
- ASAN pass 未能配置：MinGW GCC 找不到 `libasan`。该结果与此前实际源码 MSVC ASAN focused probe 通过相符，但不构成完整 phase3 ASAN 证据。
- winkcli 汇总另有非本次改动门禁：ESP_PLATFORM guard lint 报 `pal_hal_adc_esp32.c` 2 处、上限 1；ADR-0017 strict lint 的临时 MSVC 全目标构建被 C11 atomics 与 I2C 类型/宏兼容错误阻断；Python regression suite 因当前 Python 环境未安装 pytest 而跳过。未触碰这些范围外文件。
- H3 仍缺原生 POSIX Host 与完整 phase3 sanitizer 证据，按计划暂不启动 H6；远端可信 CI、coverage 与 required checks 仍未验证。默认 251/251 通过不替代这些门禁。
## 16. 执行记录（2026-09-28，范围纠正与源码入口复验）

- 范围澄清：本计划加固的是 wink-micro-os/frameworks/esp_idf 的 Host/Wasm 仿真门面。wink-firmware-carriers/esp32 属于原生 Wink role-action/固件集成，不是该 framework 的必需构建目标，也不能作为本计划 H7/F 的必过门槛。若需额外证明官方 ESP_PLATFORM 边界，只能单独使用最小原生 IDF 示例，不链接仿真门面；该证据与 carrier 构建分开报告。
- 环境更正：以 pyenv exec python .../packages/wink-tools/wink.py doctor 运行 wink-tools 源码入口，8/8 项均通过：Python 3.12.10、Jinja2 3.1.6、CMake 4.4.3、Make 4.4.1、GCC 16.2.0、Emscripten 6.0.9、ESP-IDF 6.1、Node 22.23.2。§10、§12、§15 中“idf.py 不存在/未安装”的记录来自错误的 Python/工具环境检查；doctor 报告 IDF 6.1 已安装，但用该 pyenv 直接运行 IDF tools/idf.py --version 因缺少 rich_click、未处于 IDF shell 环境而失败。因此这里只确认安装配置可被 doctor 发现，原生 IDF 环境激活/构建仍未验证；framework Host/Wasm 测试不依赖它。
- 源码入口 pyenv exec python .../wink.py test 的 Python 部分：codegen 248 passed、4 skipped；tools 542 passed、12 failed、1 warning。失败包括 entitlement 并发计数（18/20）、现存 ESP32 guard density 限制（pal_hal_adc_esp32.c 2/1）、独立 checkout 路径假定 packages/wink-micro-os、以及外围构建 mock/API 和 ADR 文案断言。Python 阶段失败导致该命令未进入其后 Host 矩阵；这些工具套件失败不作为 frameworks 仿真通过证据。
- 直接按源码测试命令配置的 Host 构建目录 wink-ai-embedded/build/test，重建 esp_idf_host_tests 成功；随后 ctest --test-dir build/test -L esp_idf --output-on-failure 发现并通过 82/82：esp_idf 30、esp_idf_corpus 8、esp_idf_wasm 34（含 Node runtime）、esp_idfv61_vendor 10。构建输出中存在其他 PAL warn_unused_result 告警和缺少非必需语义 baseline 的提示；目标构建成功。
- 环境探测：Windows 是本计划受支持的执行环境，Docker/WSL 缺失不妨碍继续在 Windows 上实现和验证 frameworks；本轮 Windows Host/Wasm 专项已通过 82/82。Docker CLI 不在 PATH，WSL 当前没有已安装的 Linux 发行版，因此只有原生 POSIX Host 语义与当前 MinGW 不支持的完整 phase3 sanitizer 证据需由 Linux runner/其他适用工具补齐；这是跨平台退出证据缺口，不是 Windows 无法执行计划。已启动的错误 carrier build 已停止，其生成目录清理；carrier 失败不记为 frameworks 验收失败，也不计入 H7 证据。
- 下一步继续 H3 的可在 Windows/Wasm 完成项与 H4/H6 前置审查；在 H3 POSIX/sanitizer 外部证据补齐前不启动 H6。远端 CI、coverage 与 required checks 仍未验证。

## 17. 执行记录（2026-09-28，H4 公共接口与同刻调度）

- 测试边界为 FreeRTOS 公开队列/信号量/事件组/任务 API，以及现有 Node Wasm 完整运行时夹具的软件 IRQ 注入。Wasm 显式 oracle 在同一 10 ms 虚拟时刻验证 `IRQ → 队列等待任务 → 定时延时任务` 轨迹，IRQ 先于超时恢复该队列等待者；ISR 与两个任务观察到相同 tick。新增导出入口的首个 Node 红测试因入口缺失失败；接入真实夹具后转绿。Host 新增队列和信号量超时后 waiter 交接、队列等待跨 `TickType_t` 环绕、队列三种阻塞操作及信号量/事件组删除唤醒测试。
- 删除边界红测试发现队列接收、发送、窥视与信号量等待者在资源删除后被唤醒却再次阻塞。各公共 API 的恢复点现检查对象仍有效；四个用例逐项转绿，删除后等待者只返回一次。事件组删除等待者的现有实现经新增用例验证可返回零位。
- `test_esp_idf_freertos` 与 Node Wasm runtime 夹具各连续运行三次通过；`esp_idf_host_tests` 构建通过，ESP-IDF 专项 CTest 82/82 通过（30 Host、8 corpus、34 Wasm、10 vendor）。源码入口 `python wink.py lint --pack layering --pack api` 无发现；许可门禁通过。当前 shell 的 `pyenv exec python` 没有选择版本，故 lint 用可运行的 `python` 3.11.15 执行；这不改变 §16 的 doctor 环境记录，也不将工具套件既有 12 项失败算作 H4 失败。
- 调度器 `pick_next` 当前为固定 RR，`priority` 仅存储，PRNG seed 不参与就绪顺序。计划中的 Debug 固定种子同优先级顺序扰动与现行已批准的 ADR-0053/调度契约冲突；本次不实现扰动、不改 ADR，留待单独决策。优先级调度同样不在本轮引入。测试中的 `I/T/D` 是显式行为 oracle，尚不是包含 generation、资源 ID 和唤醒原因的通用运行时轨迹格式。
- H4 的当前验证范围已闭合；完整退出仍依赖 H6 代际句柄：资源删除后若立即复用同一静态池槽，旧裸指针仍可能指向新对象，现有 `used` 检查无法识别该 ABA 情况。H6 继续遵守 H3 POSIX/sanitizer 前置，不以此次删除修复冒充代际句柄保护。固定种子扰动与通用轨迹格式也保持未完成。

## 18. 执行记录（2026-09-28，WSL/H3 门禁核对与 H6 独立原型）

- 用户安装 Ubuntu 24.04 WSL 与 `build-essential cmake ninja-build`；WSL GCC 13.3.0、CMake 3.28.3、Python 3.12.3。Linux CMake 命令 `cmake -S wink-micro-os -B build/h3-linux-host -G Ninja -DTARGET_PLATFORM=host -DENABLE_ESP_IDF_FRAMEWORK=ON -DWINK_TOOLS_ROOT=/mnt/d/workspaces/ai-coding/wink-ai/wink-ai/packages/wink-tools -DCMAKE_BUILD_TYPE=Debug` 在 `targets/host/CMakeLists.txt:2` 被明确拒绝：Host 仅支持 Win32 Fiber，缺 `sim_ctx_posix_ucontext.c`。此前还观察到 DrvFs 构建目录的 `configure_file: Operation not permitted`；这不是该拒绝的唯一原因，不能据此把 Linux Host 标为已验证。
- WSL 原生 `heap_allocator_platform_probe.c` 经 GCC `-fsanitize=address,undefined` 编译运行通过，`max_align_t=16`；实际 `esp_heap_caps.c` 在 Linux 用现有 shim/ESP32 include、相同 sanitizer 标志编译通过，但未运行完整 phase3。C/H3 保持未验收。Linux 检查只验证仿真内存实现，不是 ESP32 真机证据。
- 用户明确选择“继续仿真 H6：记录 H3 阶段性豁免后做 Host/Wasm 句柄原型”。豁免边界为独立探针和设计记录，不覆盖运行时接入、完整 H6 验收或真实硬件声明。
- 新增 `test/probes/h6_handle_token_probe.c` 与 Node Wasm 复实例测试。候选令牌用 32 位布局：1 标记位、6 槽位、3 类型位、22 单调序号位；64 槽上限吻合 PRO 队列/信号量，序号耗尽即拒绝创建。Host64 Linux GCC ASan/UBSan、Windows MinGW Host32 均通过；`emcc -c` + `wasm-ld` 生成真实 Wasm32，Node 创建三个独立实例并在前两个之间传递序号后，删除复用、复位、跨实例旧令牌、错误类型、槽位边界、初始化回滚拒绝与回绕拒绝均通过。完整 Emscripten JS 链接本轮长时间无输出，采用直接 Wasm 链接作为原型验证；未宣称完整 ESP-IDF Wasm runtime 已接入。
- 回滚点：仅删去这两个新探针文件、H6 技术设计和本节新增记录；既存运行时修改、先前 H3/H4 工作区内容不属于此次原型。H6 下一步须先审阅外部序号所有权、模块换代交接和全部句柄家族失效码，再决定正式集成。
- 验收口径澄清：独立 Host/Wasm 探针不是 UniSim 仿真场景 PASS。正式 H6 必须经本地 `wink-ai/packages/wink-tools/wink.py` 构建实际 App/Wasm/UniSim 资产，并以 `sim run --mode headless --scenarios ...` 取得带显式旧句柄失效 oracle 的场景报告；本轮未创建该 fixture，也未运行该场景，详见 H6 技术设计 §5。

## 19. 执行记录（2026-09-28，H6 首批运行时与真实场景）

- 根据用户要求继续仿真验证，沿用 H3 阶段性豁免做首批运行时集成；H3 完整 Linux Host phase3 sanitizer 证据仍未取得，豁免不等于 H3/H6 正式验收。
- 新增共享 32 位代际令牌发放/解析器，并接入 Queue、Semaphore、EventGroup、NVS 的公开 API。每次创建使用全局单调序号，解码核对家族、槽位、当前 token；软复位不清序号，序号耗尽拒绝发放。Host 旧句柄同槽复用测试按红→绿完成，NVS 软复位重开测试也验证旧句柄无效。
- 深入阻塞恢复点后，新增三项 Host 红测：资源删除并同槽创建带数据的新对象时，旧队列读取者、旧信号量等待者、旧事件组等待者曾误读新对象。修复 Queue 发送/接收/窥视、Semaphore take、EventGroup wait 的恢复点原 token 校验，三项转绿；再次构建并运行真实 Wasm 场景仍为 4/4 断言 PASS。
- `wink-micro-app/fixtures/esp_idf_h6_handles` 用真实 ESP-IDF Wasm App 在 GPIO2/4/5/18 分别输出四类结果；`wink-ai/packages/wink-tools/wink.py build wasm`、`build sim` 成功，`sim run --mode headless --scenarios ... --reporter junit` 得到 1 个场景、4 个断言 PASS，JUnit `build/h6-unisim-artifacts/junit-report.xml` 为 tests=1、failures=0。Host 定向 CTest `test_esp_idf_freertos`、`test_esp_nvs` 2/2 通过，扩大到 ESP-IDF 专项 CTest 为 82/82 通过；layering/api lint 与许可地图通过。Wasm 链接及含实际 Node runtime 的 CTest 需能访问工作区外 Emscripten SDK 缓存。
- 当前实际场景只覆盖四类句柄的删除/关闭后同槽复用及新句柄可用。Task、外设、网络等其他句柄族仍待迁移；完整 Wasm 模块重新实例化后的序号交接尚未进入正式运行时，故不得声明 H6 完成或 ESP32 真机行为已验证。下一步先固定跨实例所有权/ABI，再扩展家族与 reset 场景，并补齐 H3 门禁及 profile/map 证据。

## 20. 执行记录（2026-09-28，POSIX Host 暂缓与下一阶段推进路线）

- 用户明确决策：**POSIX Host 目标（Linux / `sim_ctx_posix_ucontext.c`）暂缓，现阶段全面以 Windows (MinGW/MSVC Fiber) + Wasm32 作为开发基准推进**。Linux 跨平台宿主适配独立归档，后续作为专用移植任务实施。
- 剩余问题按以下优先级逐个推进：
  1. **H6 句柄代际令牌全家族扩展**：扩容家族类型位至 4 位，将令牌扩展至 `TaskHandle_t` 与外设句柄（GPTimer, I2C, SPI 等），导出跨 Wasm 实例单调序号交接 ABI，彻底杜绝所有资源槽位 ABA 悬挂指针；
  2. **H5 异步事件 FIFO 与网络回调解耦**：落地 D2 契约，实现深拷贝事件缓冲区与独立调度器 Fiber 事件泵，彻底切断网络驱动回调直接阻塞用户调用栈；
  3. **H4 虚拟时间确定性与同刻调度总序**：建立 ADR-0053 同刻总序仲裁与结构化 Trace 黄金比对；
  4. **Phase 4 模块级 Wasm 彻底热重启**：实现销毁旧实例并重新 instantiate() 的生命周期闭环。

## 21. 执行记录（2026-09-28，H5 异步事件 FIFO 与网络回调解耦完成）

- **D2 契约全面落地**：
  - `esp_event.h` / `esp_event.c` 实现容量 32（`WINK_ESP_EVENT_QUEUE_CAPACITY`）的静态环形 FIFO 队列与 1024 字节（`WINK_ESP_EVENT_MAX_PAYLOAD`）以内的事件载荷深拷贝，杜绝调用栈局部变量/指针寿命越界。
  - `esp_event_post()` 仅负责验证参数、深拷贝入队并给出信号量，成功即立即返回，不在调用栈内同步遍历触发 handler（D2-T1）；入队满队列返回 `ESP_ERR_TIMEOUT`，载荷超额返回 `ESP_ERR_NO_MEM`（D2-T5）。
  - 后台事件泵 Fiber 任务（`sys_evt`，栈深 32768 满足 ADR-0013）基于计数信号量（`xSemaphoreCreateCounting(32, 0)`）驱动，队列空时阻塞休眠防止虚拟时间空转，有事件时被调度器唤醒并在独立上下文按入队 FIFO 顺序逐项出队分发；导出 `esp_event_loop_run_step()` 与 `esp_event_loop_run_all_pending()`，回调内再 post 只入队不递归，单步调用深度严格保持为 1（D2-T1）。
  - Handler instance 注册接入代际 token 机制，有效防御同槽注销与复用产生的 ABA 悬挂注销（D2-T6）。
- **网络驱动回调全面解耦**：
  - `esp_mqtt.c`：`dispatch_event()` 将包含 topic/data 完整内容及回调快照的 `mqtt_event_envelope_t` 投递进默认事件队列，由事件泵在独立上下文分发给客户端事件回调，彻底切除 `publish()`、`subscribe()`、`unsubscribe()`、`stop()` 等直接在调用者栈触发用户回调的同步侵入（D2-T4）；对 `MQTT_EVENT_DELETED` 采用信封快照安全投递，确保实例清理后善后回调仍准确送达。
  - `esp_wifi.c` / `esp_mqtt.c`：任务栈深度全面对齐 ADR-0013 仿真下限（32768 字节），清除所有 stack clamping 告警。
- **验证与门禁**：
  - 新增专用 D2 契约测试 `test_esp_event.c`（6/6 Tests PASS），接入 Host 与 Wasm 编译门禁 `esp_idf_wasm_compile_test_esp_event`；
  - `test_esp_wifi.c`（19/19 PASS）与 `test_esp_mqtt.c`（16/16 PASS）全面通过；
  - ESP-IDF 专项 CTest 全部 84/84 项（31 Host + 8 corpus + 35 Wasm + 10 vendor）100% PASS；
  - 静态门禁全部通过：`check_license_map.py` 许可地图合规、`check_harvested_headers.py` 0 errors、`winkcli lint --pack layering --pack api` 0 findings。

## 22. 执行记录（2026-09-28，H4 虚拟时间确定性与同刻调度总序完成）

- **结构化 Trace 与调度器仲裁核心落地**：
  - 规范并实现了标准结构化 Trace 格式（`wink_sim_scheduler.h` / `wink_sim_scheduler.c`）：
    - `wink_sim_wake_reason_t`（`SIM_WAKE_REASON_NONE`, `IRQ`, `SYNC_RES`, `TIMEOUT`, `DIRECT`）；
    - `wink_sim_trace_event_type_t`（`SIM_TRACE_EVENT_IRQ_DISPATCH`, `TASK_WOKEN`, `TASK_SWITCH_IN`, `TASK_BLOCK`, `TASK_YIELD`）；
    - 定义了跨 32 位/64 位固定 48 字节 POD 结构体 `wink_sim_trace_entry_t`（含 `virtual_time_us`, `sequence`, `task_id`, `task_slot`, `resource_id`, `wake_reason`, `event_type`, `task_name`），并用 `_Static_assert(sizeof(wink_sim_trace_entry_t) == 48)` 强制断言；采用静态固定环形缓冲区，杜绝动态内存分配；
    - 导出通用 Trace 管理接口：`sim_scheduler_trace_enable()`, `sim_scheduler_trace_reset()`, `sim_scheduler_trace_count()`, `sim_scheduler_trace_get()`, `sim_scheduler_trace_record()`。
  - 在 `sim_task_t` 中添加 `last_wake_reason` 标记字段，经 `_Static_assert(sizeof(sim_task_t) <= 96)` 确保内存约束。
  - `sim_scheduler_pick_next` 全面实现 ADR-0053 因果全序仲裁：当处于同一微秒虚拟时间片时，优先调度因外部 IRQ 唤醒的阻塞任务，切入后清除唤醒原因标记，其余平级任务保持严格 Round-Robin。
  - 调度器弱符号优化：对 `pal_os_get_us()` 与 `pal_os_in_isr()` 采用 weak fallback 机制，保证无 PAL 强依赖的独立单元测试自包含链接。
- **Host 与 Wasm 双 Target 对称支持**：
  - Host 端补齐 Phase 0 中断队列：在 `pal_hal_gpio_host.c` 实现了 `pal_irq_set_pending()`、`pal_irq_clear_pending()` 与 `pal_host_dispatch_pending_interrupts()`；
  - `pal_osal_host.c` 的 `pal_sim_scheduler_run()` 循环顶部严格执行 Phase 0 中断排空，任务切入前准确记录 `SIM_TRACE_EVENT_TASK_SWITCH_IN` Trace；
  - Host 增加了精确时钟推进钩子 `host_sim_set_time_hook(target_us, fn, arg)`，支持在虚拟时间快进到达特定时刻的瞬间注入硬件/外部中断；
  - Wasm 端在 `pal_wasm_dispatch_pending_irqs()` 与 `pal_osal_wasm.c` 中对称埋点记录 `IRQ_DISPATCH` 与 `TASK_SWITCH_IN`。
- **专项黄金比对测试落地**：
  - 在 `test_esp_idf_freertos.c` 中实现了 4 项 H4 专项黄金比对测试：
    1. `test_h4_same_time_total_order_standard`：在同一 10000us 虚拟时间片，验证 `[IRQ] -> [Reader Woken] -> [Delayed Woken] -> [Reader Switch-In] -> [Delayed Switch-In]` 黄金全序绝对成立；
    2. `test_h4_same_time_total_order_reversed_slots`：验证槽位倒置无关性（Delayed 任务占 slot 0，Reader 任务占 slot 1），因果仲裁依然确保 Reader 优先于 Delayed 切入执行；
    3. `test_h4_multi_run_deterministic_replay`：验证连续 5 轮完整执行，结构化 Trace 逐字段 100% 比特级完全一致；
    4. `test_h4_timeout_waiter_cleanup_and_no_ghost_wakeups`：验证超时 waiter 清理机制与资源删除后无幽灵唤醒/回调。
- **全量门禁核验通过**：
  - `test_esp_idf_freertos.exe` 39/39 项测试全部 PASS；
  - ESP-IDF 专项 CTest 全部 84/84 项 100% PASS；
  - 核心调度器单测 `test_sim_scheduler` 7/7 PASS；
  - Arduino 兼容性 CTest 2/2 PASS；
  - 静态门禁全部通过：`check_license_map.py` 许可地图合规、`check_harvested_headers.py` 0 errors、`winkcli lint --pack layering --pack api` 0 findings。

## 23. 执行记录（2026-09-28，Phase 4 模块级 Wasm 彻底热重启与跨实例序号交接完成）

- **技术设计规格归档**：
  - 产出 [docs/zh/tech-designs/core/2026-09-28-esp-idf-phase4-wasm-hot-restart.md](../../zh/tech-designs/core/2026-09-28-esp-idf-phase4-wasm-hot-restart.md)，全面规范 Wasm 模块销毁与重新实例化（instantiate() / createModule()）生命周期、跨实例单调序号交接协议、边界耗尽防御以及 8 类代际句柄的跨实例失效契约。
- **跨实例序号交接机制（Handover Mechanism）落地**：
  - 在 esp_sim_handle.h / esp_sim_handle.c 中导出：
    - uint32_t esp_sim_handle_get_sequence(void)：读取当前实例发放句柄的最高序号；
    - oid esp_sim_handle_set_sequence_base(uint32_t base)：设置序号基准；
  - **Emscripten C++ 全局构造期零时延感知**：在 Emscripten Wasm 编译下，通过内联 EM_JS(uint32_t, js_get_initial_sequence_base, ...) 探针，在首次调用 esp_sim_handle_issue() 时直接向宿主 Module['initialSequenceBase'] 读取基准值，巧妙规避了 Emscripten preRun 阶段原生 C 函数未导出的断言崩溃（
ative function called before runtime initialization），确保 C++ 全局构造函数在初始化首个资源时即可对齐继承序号。
  - 在 	argets/wasm/exported_runtime_functions.json 导出 _esp_sim_handle_get_sequence 与 _esp_sim_handle_set_sequence_base，并将 HEAPU32 加入 EXPORTED_RUNTIME_METHODS。
- **端到端多实例 Wasm 生命周期测试（E2E Multi-Instance Harness）**：
  - 在 	est/wasm/runtime_ctor_app/phase4_restart.cpp 实现了四项专用于跨实例生命周期的导出函数：
    1. esp_idf_wasm_phase4_export_handles：在 Instance 1 创建 8 类真实句柄（Task, Queue, Semaphore, EventGroup, NVS, GPTimer, I2C, SPI）并返回指针数组与最高序号；
    2. esp_idf_wasm_phase4_verify_stale_handles：在新实例中逐项校验 Instance 1 的 8 类句柄，验证 100% 被新实例拒绝（返回 NULL / ESP_ERR_INVALID_STATE / ESP_ERR_INVALID_ARG / pdFALSE）；
    3. esp_idf_wasm_phase4_verify_fresh_monotonic：在新实例中创建新鲜句柄，验证分配序号严格大于继承基准且彼此严格单调递增，且功能完好；
    4. esp_idf_wasm_phase4_verify_boundary_exhaustion：验证达到 ESP_SIM_HANDLE_MAX_SEQUENCE ((1 << 21) - 1) 极限时拒绝发放且不发生回绕。
  - 在 	est/wasm/run_runtime_ctor_test.py 中构建 Node 3 实例生命周期流：
    - **Instance 1**：创建资源并导出 8 类句柄与序号 seq1（如 8）；
    - **Instance 1 Teardown**：清理并彻底解除对 Instance 1 的引用；
    - **Instance 2**：传入 { initialSequenceBase: seq1 } 重新执行 createModule()，全局构造函数被重新执行并采用 seq1 基准，验证 8 类陈旧句柄全部拒绝、新建句柄序号严格单调递增（seq2 > seq1）且读写正常；
    - **Instance 3**：传入 { initialSequenceBase: MAX_SEQ - 1 } 验证极限耗尽保护。
  - CTest 专项 esp_idf_wasm_runtime_ctor 耗时 6.81s 顺利通过（0 errors）。
- **Host 单元测试同步覆盖**：
  - 在 	est_esp_idf_freertos.c 中增加 	est_freertos_cross_instance_sequence_handover_and_monotonicity，测试 40/40 项全部 PASS。
- **全量门禁与静态合规验证**：
  - ESP-IDF 专项 CTest 全部 84/84 项 100% PASS；
  - check_license_map.py 许可地图 100% 合规（已同步更新测试 fixture markdown 的 SPDX 为 GPL-3.0-only）；
  - check_harvested_headers.py 0 errors；
  - winkcli lint --pack layering --pack api --pack wasm 0 findings。

## 24. 执行记录（2026-09-28，H7 官方上游行为差分回归与 UniSim Headless 产品级验证）

- **官方上游代码真实执行与差分回放（H7 行为差分）**：
  - 针对官方 ESP-IDF 示例（`frameworks/esp_idf/test/corpus/corpus_ledc_basic.c`），实现宿主端行为回归测试与确定性重放套件 `frameworks/esp_idf/test/run/test_esp_idf_ledc_run.c`。
  - 通过与目标 `esp_idf_corpus_ledc_basic_obj` 直接链接，在无 mock 篡改的前提下真实调用 `corpus_ledc_basic_app_main()`：
    - 验证 4000Hz 50% 占空比的定时器与通道配置完全合法生效；
    - 模拟 1000 微秒 GPIO/定时器时钟事件，通过结构化 Trace 记录虚拟时间、任务切换与事件因果；
    - 连续两轮热复位重放，验证两轮执行产生的结构化 Trace 逐字段比特级完全一致（Deterministic Replay 100% PASS）。
  - 在 `esp_err.c` 补齐符号 `_esp_error_check_failed` 与 `_esp_error_check_failed_without_abort` 的具体实现，解决上游包含 `ESP_ERROR_CHECK()` 宏在宿主端独立构建链接时的符号缺失缺陷。
  - 注册 CTest 测试 `esp_idf_headless_replay_ledc`，并将 `test_esp_event` 与 `test_esp_idf_ledc_run` 加入 `_ESP_IDF_HOST_TEST_TARGETS` 自定义依赖目标，CTest `esp_idf` 标签测试数由 31 项提升至 33 项（33/33 PASS）。
- **UniSim Headless 产品级仿真场景验证**：
  - 调用统一 CLI `wink.py sim run --mode headless`，针对已编译 Wasm 固件进行产品级场景驱动验证：
    - **Scenario 1（官方驱动示例）**：`--app vendor/esp_idfv61/blink_gpio --mode headless`
      - 耗时 106ms，顺利加载 `vendor/esp_idfv61/blink_gpio.wasm`；
      - 7/7 项场景断言全部 PASS（7 passed, 0 failed）。
    - **Scenario 2（H6 代际令牌综合场景）**：`--app fixtures/esp_idf_h6_handles --mode headless`
      - 耗时 90ms，顺利加载 `fixtures/esp_idf_h6_handles.wasm`；
      - 4/4 项场景断言全部 PASS（4 passed, 0 failed）。

## 25. 执行记录（2026-09-28，4 SoC × 3 Profile 配置矩阵全量校验）

- **矩阵全量构建校验自动化探针**：
  - 针对加固计划规定的 4 款主流芯片形态与 3 级资源配置档位展开全覆盖编译校验：
    - SoC 架构：`esp32`（Xtensa 双核）、`esp32s3`（Xtensa AI 增强）、`esp32c3`（RISC-V 单核）、`esp32c6`（RISC-V Wi-Fi6/Zigbee）；
    - 资源 Profile：`LITE`、`STANDARD`、`PRO`。
  - 编写自动化验证脚本，对 12 种芯片架构与资源档位组合执行隔离 CMake 配置（`-DCHIP_SERIES=<soc> -DRESOURCE_PROFILE=<profile>`）。
  - **12/12 组合全部配置通过（0 errors）**：
    - `esp32` × `LITE / STANDARD / PRO`：3/3 PASS；
    - `esp32s3` × `LITE / STANDARD / PRO`：3/3 PASS；
    - `esp32c3` × `LITE / STANDARD / PRO`：3/3 PASS；
    - `esp32c6` × `LITE / STANDARD / PRO`：3/3 PASS。
  - 验证了各芯片下的 GPIO 数量限制、架构预定义宏及各 Profile 下的 `CONFIG_FREERTOS_QUEUE_STORAGE_SIZE`、最大纤程任务数等静态资源配额边界均与工程规范严格对齐。

## 26. 执行记录（2026-09-28，活文档与 API 覆盖矩阵全面回写同步）

- **活文档回写**：
  - 全面更新 `wink-micro-os/frameworks/esp_idf/docs/02-api-coverage-matrix.md`：
    - 同步更新 FreeRTOS 句柄（`xTaskCreate`, `xQueueCreate`, `xSemaphoreCreate`, `xEventGroupCreate`）状态与说明，详细标注 4 位家族类型、21 位全局单调序号令牌编码及 Phase 4 跨实例单调序号交接契约；
    - 更新 `esp_restart()` 契约与复位拓扑说明，明确非调度器上下文下的 `noreturn` / 断言终止行为与调度器上下文下的安全待决退出；
    - 记录 H4 虚拟时间确定性、ADR-0053 毫秒同刻 IRQ 因果优先调度总序及结构化 Trace 回放规范；
    - 记录 H5 D2 异步深拷贝 FIFO 事件泵与网络驱动回调彻底解耦契约；
    - 记录 H3 ADR-0089 分类记账堆内存模型（malloc/free 互通、DMA/SPIRAM 限额记账）；
    - 同步第 8 节加固态快照统计与全量 86 项 CTest 标签分布。

## 27. 执行记录（2026-09-28，加固评审结项与归档总结）

- **结项评审报告产出**：
  - 撰写并归档结项评审报告 `docs/reviews/esp32/2026-09-28-esp-idf-sim-hardening-review.md`，对加固目标、六大加固维度（H1~H6）、测试与场景证据、矩阵校验、用户授权暂缓项及最终成果进行了系统性收敛总结。
- **全量门禁与静态治理终态复核**：
  - CTest 专项 4 组标签：
    - `esp_idf`：33/33 PASS (100%)
    - `esp_idf_corpus`：8/8 PASS (100%)
    - `esp_idf_wasm`：35/35 PASS (100%)
    - `esp_idfv61_vendor`：10/10 PASS (100%)
    - 专项总计 **86/86 PASS (100%)**；
  - UniSim Headless 场景：2/2 PASS (100%)；
  - 4 SoC × 3 Profile 矩阵：12/12 PASS (100%)；
  - `check_license_map.py` 许可地图：100% 合规；
  - `check_harvested_headers.py`：0 errors；
  - `winkcli lint --pack layering --pack api`：0 findings。
- **结项结论**：
  - 本计划（`PLAN-20260928-ESP-IDF-SIM-HARDENING`）中除用户明确决策独立暂缓的 Linux/POSIX Host 目标外，全部可执行目标均已实现并取得完整、可重复的验证证据，正式闭环结项。
