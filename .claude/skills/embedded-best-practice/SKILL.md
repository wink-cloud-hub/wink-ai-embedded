---
name: embedded-best-practice
description: 嵌入式 C 最佳实践（本项目静态分发专用）。Use when writing, modifying, reviewing, or debugging wink-micro-os / chigo-micro C firmware, drivers, DAL, PAL, HAL, RTOS tasks, or embedded platform code. Enforces POD + named APIs, compile-time static dispatch, wink_status_t negative error codes, memory/concurrency/hardware safety, dual-target wasm/ESP32 constraints, and risk-based post-edit safety review. Do NOT use for Linux/Zephyr runtime-polymorphism reading; use c-runtime-polymorphism-reading instead.
---

# 嵌入式 C 最佳实践（本项目静态分发专用）

本 skill 服务 wink-micro-os / chigo-micro 的嵌入式 C 编码、修改、审查与排错。默认范式是**编译期静态分发**：POD 结构 + 命名 API + CMake/codegen 静态绑定。

> ⚠ **chigo-micro 为外部对照仓库**（`D:\workspaces\ai-coding\chigo\chigo-micro`，见 MEMORY），**非本仓库子目录**；其代码仅作对照/溯源，AI 勿在本仓库内按相对路径 Read/Grep 它。

> **文档集根（唯一权威源）**：`.claude/skills/embedded-best-practice/references/`（下文简称 `REF/`）。
> **共享工程纪律（两 skill 共用同一份）**：`.claude/skills/_embedded-shared/`（下文简称 `SHARED/`）。
> 运行期多态阅读已拆到 `c-runtime-polymorphism-reading`。

---

## 第 0 步：确认边界

```text
你在做什么？
│
├── 写 / 改 / 审 wink-micro-os 或 chigo-micro 的 C 固件 / 驱动 / HAL
│     → 使用本 skill
│     → 读 REF/static-dispatch/ + SHARED/
│     → 禁止生成 vtable / container_of / struct xxx_ops 作为器件抽象
│
├── 读 Linux / Zephyr / RT-Thread / STM32 HAL 源码，理解 C-OOP
│     → 改用 c-runtime-polymorphism-reading
│
└── 只做通用 C 工程纪律判断
      → 读 SHARED/
```

### 本项目 = 静态分发（ADR-0004）

本项目器件是 **POD 结构 + 命名式 API**：`dal_rc_servo_set_angle(&dev, angle)`、`motor_driver_set_outputs(&drv, out)`。**没有** `struct device_ops`、**没有**运行期虚表、**没有** `container_of`。换芯片 / 换器件靠**编译期选择**（CMake 链接 / codegen 静态绑定）。

如果用户明确要求解释 `ops` / `container_of` / Linux 设备模型，停止使用本 skill，改用 `c-runtime-polymorphism-reading`。

（已知偏差：wink-micro-os 现有代码尚有 `bool`/`float` 返回 + `dal_*_get_distance` 旧命名，属待迁移形态，见 `REF/static-dispatch/README.md` 偏差框。）

---

## 读哪个文档（按任务）

| 任务 | 读 |
|------|----|
| 设计本项目模块结构 / 写新器件驱动 | `REF/static-dispatch/architecture.md` → `templates.md` |
| 接口契约模板 (Blocking/ISR) / YAML 元数据 | `REF/static-dispatch/contracts.md` |
| 资源生命周期 / No-malloc / 配置状态分离 | `REF/static-dispatch/lifecycle.md` |
| 仿真保真分级 / Wasm-JS 错误注入 | `REF/static-dispatch/simulation.md` |
| 函数设计、命名、错误处理、const/static | `SHARED/clean-code.md` |
| 错误码（0=ok/负数=错误，禁 `if(status)`） | `SHARED/error-codes.md` |
| 堆 / 栈 / 缓冲区 / VLA·strcpy·sprintf 禁令 | `SHARED/memory-safety.md` |
| 线程 / 临界区 / ISR→信号量→工作线程 / ISR 优先级上限 / volatile≠原子 | `SHARED/concurrency.md` |
| RTC / 非阻塞驱动 / DMA / 看门狗 / NVS / 双 target 同源 | `SHARED/realtime-hardware.md` |
| 工具链 / CI 正则门禁 / lint / 栈用量门禁 | `SHARED/tooling.md` |
| 测试策略 / host 单测 / 帧解析 fuzzing / HIL | `SHARED/testing.md` |
| 代码审查 / 排错（本项目） | `REF/static-dispatch/pitfalls.md` |
| 架构评审 Grilling 问题清单 | `REF/static-dispatch/grilling.md` |
| 编辑后安全审查 | `SHARED/safety-checklist.md` |

如有疑问，先读 `REF/index.md`。安全关键代码，过度检查总好过检查不足。

---

## 编辑后安全审查协议

按改动风险选择清单范围。修复任何致命 / 高级问题后，从阶段 1 重新审查受影响范围。

| 风险级别 | 触发条件 | 必跑阶段 |
|----------|----------|----------|
| 低 | 纯注释、格式化、无语义重命名 | 1、10、12 |
| 中 | 普通 DAL/PAL 业务逻辑、错误处理、API 调整 | 1、2、3、4、10、12 |
| 高 | ISR、DMA、内存分配、共享状态、驱动时序、并发、硬件访问 | 完整 1–12 |

输出审查结论时使用：

```text
Safety review:
- Risk level:
- Checklist phases run:
- Findings:
- Fixed:
- Assumptions:
- Commands run:
```

---

## 核心原则

1. **安全第一** —— 安全关键系统代码，每次修改都当生命攸关对待。
2. **面向对象设计思维** —— 数据 + 行为归位、封装、信息隐藏；本项目以 POD + 命名 API 落地。
3. **Clean Code** —— 函数只做一件事、命名揭示意图、无副作用、同抽象层级、DRY、表驱动。
4. **零容忍阻塞** —— RTC/事件驱动调用链非阻塞；阻塞操作交给内部工作线程。
5. **验证到底层** —— 硬件交互查到寄存器级；永远不假设某 API 非阻塞。
6. **防御性编程** —— 断言内部契约、运行时校验外部输入、错误码传播、绝不静默吞失败。

## 改完代码 30 秒自检（致命 5 项）

低 / 中风险改动用这个秒级自检；**任一项不过 → 转完整 12 阶段清单**（见上方「编辑后安全审查协议」）。

```text
□ 返回 wink_status_t 且用 if(status<0) 检查？（非 bool / 非 float 哨兵）
□ 业务层没直接碰 PAL / 寄存器？（走 DAL 命名 API）
□ 没扩大 #ifdef SIMULATION 范围？（只旁路最低物理信号层）
□ 没发明未在契约 / Registry 注册的 API？
□ 没在实时路径 malloc / 没吞错误码？
```

## 硬性规则

行宽 80 列、函数 ≤80 行、嵌套 ≤4 层、参数 ≤5、禁魔法数字、禁裸 int、禁 `if(status)`、禁实时路径 `malloc`、禁 `strcpy`/`sprintf`/`strncpy`、纯 snake_case（函数 `模块_动作()` / 类型 `xxx_t` / 宏 `UPPER`）——**完整硬限表见 `SHARED/clean-code.md` §硬性限制**（不在本文件重复，避免两份漂移）。

## 上下文感知编码

写任何代码前，先看周围代码库：

1. 同目录兄弟文件如何用信号 / 回调 / 事件 / 命名 API。
2. 头文件包含路径如何写。
3. 堆内存 API 是否使用项目 allocator，绝不混用。
4. 设备访问是否使用命名 API：`dal_xxx_read(&dev, ...)`。
5. 编码风格、错误传播、测试方式如何组织。
6. **业务事实寻路**：绝对不要猜测或硬编码具体的设备 API 与硬件配置。必须先查阅 `docs/design/07-platform-governance/01-device-model-registry.md`、具体外设的 YAML 声明或项目现有的 `.h` 头文件，获取最新、最真实的 API 签名和物理契约。本 Skill 文件及 references 目录中的任何 API/YAML 段落仅作为编写范式与格式示例，如果在分析过程中你觉得 **`docs/design/`里定义的业务事实不够完善或有问题，你需要停下来提出你的建议让我确认**。

## 驱动开发检查清单（本项目）

1. 器件结构是纯 POD（无函数指针 / 无 ops / 无父类嵌入）。
2. 公共 API 是命名自由函数，返回 `wink_status_t`（0=ok，负数=错误）。
3. 内部需要非阻塞时，用工作线程 + 消息队列 + 回调封装；公共 API 入队即返回。
4. 周期性轮询留在驱动内部，不对外暴露 poll 接口。
5. 错误状态变化通过回调通知上层。
6. 文档化初始化顺序、线程安全保证、回调上下文、清理要求。

> vtable 仅在「同抽象需切换多算法」（策略模式，如 `control_algo_t`）时合法，且封装在模块内部、ops 是 const、绝不用于器件抽象。见 `REF/static-dispatch/architecture.md` 形态 4。

## AI 生成代码专用开发禁令（安全护栏）

为保证 AI 自动生成的 C 代码具备绝对的安全性与确定性，AI 编码必须遵守以下硬性防呆禁令。CI 中的 AST 静态检查器（Linter）将强行验证这些规则：

1. **禁止直接调用 PAL / 寄存器**：App/BAL 层必须且仅能通过 DAL 命名式 API（如 `dal_led_on`）操控器件，严禁绕过 DAL 直接调用 `pal_` 接口或物理引脚读写函数，更严禁直接读写芯片外设寄存器（如 `*(volatile uint32_t *)`）。
2. **零动态内存分配 (Zero Dynamic Allocation)**：在整个驱动与业务层代码中，严禁调用 `malloc`、`free`、`realloc`、`calloc` 等任何堆内存分配 API。所有设备实例与状态结构体必须静态全局实例化。
3. **严禁吞错误码**：所有返回 `wink_status_t` 类型的 API 必须使用 `WINK_WARN_UNUSED_RESULT` 宏（见 error-codes.md）修饰或被显式检查，严禁静默吞掉任何错误。错误码必须逐级向上安全传播，直至 App 层进行安全降级。
4. **禁止扩大 `#ifdef SIMULATION` 范围**：禁止将整个业务函数包裹在 `#ifdef SIMULATION` 中。只有最底层的物理信号电平读取或 Web 仿真直通（DAL Value Bypass）允许条件编译隔离，上层的物理转换与防抖逻辑必须仿真与真机同源。
5. **禁止引入非项目第三方库**：禁止在 C 固件中包含未在 CMake 依赖树中声明的第三方头文件或数学计算库。
6. **禁止运行期多态**：禁止发明 `ops` 函数指针虚表做器件抽象；如果有演进多态需求，必须封装在 DAL 文件内部并使用 `switch-case` 进行静态路由分发。

## 例外机制

硬性规则需要例外时，必须满足：

1. 例外局部化，范围尽可能小。
2. 注明原因与替代方案评估。
3. 不降低内存安全、并发安全、实时性或双 target 同源要求。
4. 可被 review 和 CI 明确识别。

## 术语表

| 术语 | 含义 |
|------|------|
| App/BAL | 应用层（用户/AI生成）+ 算法层（可复用），只调 DAL 语义 API |
| DAL | 器件语义抽象，负责把器件行为翻译到 PAL |
| PAL | 平台抽象层，包含 HAL 与 OSAL 契约 |
| Target | wasm / esp32 / stm32 等平台实现 |
| POD | 纯数据结构，无函数指针、无继承式嵌入 |
| 静态绑定 | 通过 CMake / codegen 在编译期决定实现 |
| 同源编译 | 同一 C 逻辑同时过 wasm 与真机 target |
| Simulation bypass | 仿真旁路，范围应压到底层物理信号层 |

## 好坏例子

`if(status)` vs `if(status<0)`、命名 API vs `dev->ops->`、`snprintf` vs `sprintf` 等好坏对照——**见 `SHARED/error-codes.md`（头号雷）与 `REF/static-dispatch/pitfalls.md`**，本文件不重复以避免两份漂移。

## SOLID / Clean Code 速查

SRP / OCP / LSP / ISP / DIP 速查与函数设计要点——**见 `SHARED/clean-code.md`**。

---

## 附：文档集结构

```text
references/
├── index.md                 静态分发边界 + 导航
└── static-dispatch/         本项目标准（wink-micro-os + chigo-micro）

工程纪律（两 skill 共用，SHARED/）：.claude/skills/_embedded-shared/
运行期多态参考已拆到：.claude/skills/c-runtime-polymorphism-reading/
```
