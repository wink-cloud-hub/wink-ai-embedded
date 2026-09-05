# ✅ 静态分发 —— 本项目实际采用的标准

> **本项目标准**。在 `wink-micro-os/` 与 `chigo-micro/` 里写 C 代码时，**以本文件夹为准**。
> 不要套用运行期多态（`ops` / `container_of`）；该主题已拆到 `c-runtime-polymorphism-reading`，本项目有意偏离。

---

## 一句话

**编译期静态分发 + 命名式 API + POD 结构体**。没有 `struct device_ops`、没有运行期虚表、
没有 `container_of`。换芯片 / 换器件靠**编译期选择**（CMake 链接 / codegen 静态绑定），
不靠运行期查表。

决策依据：[ADR-0004](../../../../../docs/design/decisions/0004-static-dispatch-vs-runtime-ops.md)。

---

## 为什么偏离业界通用的运行期多态

本项目（Wink-AI / WinkMicroOS）不是通用 RTOS，而是「**以 LLM 代码生成为核心体验、以
浏览器端高保真 Wasm 仿真为命门**」的垂直平台。两条 P0 约束压倒了「运行期扩展性」：

| 约束 | 静态分发优势 | 运行期多态劣势 |
|------|--------------|----------------|
| **AI 生成友好** | 命名直观（`dal_ultrasonic_read`），LLM 可 100% 确定性生成、静态校验指针安全 | 指针强转 / `container_of` 嵌套易幻觉，难静态分析 |
| **Wasm 仿真性能** | 无 `call_indirect`，控制流可优化；可旁路直通渲染器 | 大量 `call_indirect` 破坏优化、增大体积 |
| RAM | 每实例零指针开销 | 每实例 +4~8B ops 指针 |
| 可调试 | gdb 直接看 POD 全部数据 | 虚表迷雾 |

---

## 二、静态分发的反例边界（何时不适用）

静态分发（方案 B）并非包治百病的万灵药。作为架构师，必须清晰界定其局限性，避免在以下场景盲目套用静态分发：

1. **运行时热插拔设备 (Hot-plug Devices)**：若设备在系统运行期间需要动态插入或拔出（如 USB 鼠标、SD 卡挂载、外部传感器热拔插），必须依赖运行时动态枚举和注册机制。
2. **统一设备模型驱动系统 (Unified Device Model)**：类似 Linux、Zephyr、RT-Thread 的标准驱动框架，需要通过同一套通用 API（如 `device_find` / `device_read`）集中调度和轮询任意挂载的设备，此时运行时虚表（`ops`）是唯一解。
3. **闭源二进制驱动分发 (Binary-only Drivers)**：当第三方供应商仅提供编译好的二进制驱动文件（如 `.a` 或 `.o` 包）且没有开放源码时，系统无法通过 Codegen 静态交织它们的内部字段，必须使用标准运行期接口（Vtable）进行动态调用绑定。
4. **运行时动态加载的算法或协议 (Dynamic Loading)**：如支持运行时从 Flash 加载不同的滤波或运动控制算法，必须使用函数指针或运行时多态来动态切入执行逻辑。
5. **大规模同类设备运行时动态枚举与池化 (Heterogeneous Pooling)**：例如有一个动态传感器池，任务会根据运行时可用性，任意抓取一个空闲的距离传感器用于计算，此时将设备抽象为统一父类句柄更利于容器化管理。

---

## ⚠ 代码现状 vs 目标（重要：阅读本文件夹代码前必看）

wink-micro-os 的**实际代码尚处于 ADR-0001 / ADR-0004 落地前**的形态，与文档目标有偏差。
**以 ADR / 设计文档为 SSOT**，下表是已知 drift（迁移 delta 见 [evolution.md](./evolution.md)）：

| 项 | 代码现状（旧） | 目标（ADR） | 说明 |
|----|----------------|-------------|------|
| 返回类型 | `bool` / `float` + 哨兵 `-1.0f` | `wink_status_t`（0=ok，负数=错误） | ADR-0001 |
| 结构体形态 | 扁平字段（顶层 `last_distance` / `current_angle`，字段非 `const`） | `const` 配置区 + `struct state{}` 可变区分离 | 见 lifecycle.md §2、evolution.md §1.4 |
| 超声波读 API | `dal_ultrasonic_get_distance` | `dal_ultrasonic_read` | Device Registry SSOT 命名 |
| `js_sim_*` 签名 | 三处冲突（代码 / DAL doc / Registry） | 以 Registry 为准 | SSOT 未强制，反例 |
| `device_tree.c` | **尚未生成**（codegen 设计态） | 由 codegen 静态生成 | 见 templates.md |
| `#ifdef SIMULATION` | 整函数重复（过宽） | 只旁路最低物理信号层 | ADR-0003 |

> 读代码看到 `bool`/`float` 返回是「旧形态待迁移」，不是「本项目就这么写」。写新代码
> 请用 `wink_status_t` 目标形态。

---

## SSOT（单一事实来源）

写本文件夹文档时遵循的权威来源：

- **ADR-0004**：静态分发 vs 运行期 ops 选型（`docs/design/decisions/0004-...`）
- **ADR-0001**：错误码符号约定（`docs/design/decisions/0001-...`）
- **ADR-0002**：双 target 同源编译（`docs/design/decisions/0002-...`）
- `docs/design/02-wink-micro-os/01-dal-device-abstraction.md`、`02-pal-platform-abstraction.md`
- `docs/design/07-platform-governance/01-device-model-registry.md`（Device Registry / SSOT）
- 实际代码：`wink-micro-os/dal/`、`wink-micro-os/pal/`、`wink-micro-os/targets/`；chigo-micro（外部对照仓库 `D:\workspaces\ai-coding\chigo\chigo-micro`，**非本仓库路径**）

> 本文件夹是上述 SSOT 的**蒸馏 + 模板 + 陷阱**，不替代它们。ADR 变更时以 ADR 为准。

---

## 导航

| 文档 | 内容 |
|------|------|
| [architecture.md](./architecture.md) | App/BAL→DAL→PAL→Targets 分层 + 4 种静态分发形态及 Codegen 拓扑排序规则 |
| [templates.md](./templates.md) | DAL POD 器件 / device_tree codegen / X-Macros 批量生成 / 静态 Observer |
| [patterns.md](./patterns.md) | **[NEW]** 静态分发范式下的设计模式（状态机、表驱动、策略、观察者、单例、适配器）+ 反模式黑名单 |
| [contracts.md](./contracts.md) | [NEW] DAL / PAL 接口契约规范模板 (Blocking/ISR-safe/线程安全) |
| [lifecycle.md](./lifecycle.md) | [NEW] 资源所有权与生命周期模型 (No-malloc / 配置状态显式分离) |
| [simulation.md](./simulation.md) | [NEW] 仿真保真分级 (L0-L4) 与 Wasm-JS 错误注入策略 |
| [pitfalls.md](./pitfalls.md) | 命名漂移 / 签名冲突 / SIMULATION 过宽 / wasm 假锁 / 何时回退运行期多态 |
| [evolution.md](./evolution.md) | 局部多态化退出路径 + bool/float→wink_status_t 迁移 delta (含 callback 改造示例) |
| [grilling.md](./grilling.md) | [NEW] 架构评审 Grilling 挑战与深度解答 Checklist |

> 范式无关的工程纪律（错误码、内存、并发、清单）在 [../../../_embedded-shared/](../../../_embedded-shared/)，两 skill 共用，同样适用。
