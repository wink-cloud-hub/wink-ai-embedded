# 嵌入式 C 架构最佳实践 · 本项目静态分发导览

> 在 wink-micro-os / chigo-micro 写代码前，先读本页。本 docset 的默认范式是**编译期静态分发**。
>
> ⚠ **chigo-micro 为外部对照仓库**（`D:\workspaces\ai-coding\chigo\chigo-micro`，见 MEMORY），**非本仓库子目录**；本文 chigo-micro 示例仅作对照/溯源，勿在本仓库内按相对路径 Read/Grep 它。

---

## 治理摘要

| 子目录 | 性质 | 是否本项目标准 |
|--------|------|----------------|
| [`static-dispatch/`](./static-dispatch/README.md) | 本项目实际采用的架构 | 是——写本项目代码以此为准 |
| [`_embedded-shared/`](../../_embedded-shared/clean-code.md) | 范式无关的工程纪律（两 skill 共用） | 是 |

运行期多态（`ops` / `container_of` / Linux 风格）已拆到独立 skill：`.claude/skills/c-runtime-polymorphism-reading/`。写本项目代码时不要套用该范式，见 ADR-0004。

---

## 总体目标

```text
换主控芯片，业务代码零修改。
换电机 / 换器件，业务零修改。
换协议，业务零修改。

加新功能几乎都是「加一个文件」，老代码很少动。
```

本项目选择静态分发，因为 AI 生成友好与 Wasm 仿真性能是 P0 约束，而设备拓扑在编译期确定。

---

## 静态分发 vs 运行期多态（一句话对照）

| 维度 | 静态分发（本项目标准） | 运行期多态（外部阅读 skill） |
|------|------------------------|------------------------------|
| 调用形式 | `dal_servo_set_angle(&dev, ...)` 直调 | `me->ops->on(me)` 指针跳转 |
| 对象结构 | 扁平 POD，无函数指针 | base 嵌入 + `ops` 指针 |
| 子类恢复 | 不需要，编译期类型已知 | `container_of` 减偏移 |
| 注册 / 绑定 | `device_tree.c` codegen / CMake 文件链接 | `MODULE_INIT` / 链接段 / 运行期注册 |
| AI 生成友好度 | 命名确定、可静态校验 | 指针强转 / container_of 易幻觉 |
| Wasm 仿真 | 可直调优化、零间接跳转 | `call_indirect` 成本高 |
| 适用场景 | WinkMicroOS / chigo-micro | Linux / Zephyr / RT-Thread 源码阅读 |

---

## 决策树

```text
你在做什么？
│
├── 在 wink-micro-os / chigo-micro 里写 / 改 / 审 C 代码
│     → static-dispatch/ + ../../_embedded-shared/
│     → 禁 vtable / container_of / struct device_ops 器件抽象
│
├── 读 Linux / Zephyr / RT-Thread / HAL 源码，理解 C-OOP
│     → 使用 c-runtime-polymorphism-reading skill
│
└── 任何 C 代码都要遵守的工程纪律
      → ../../_embedded-shared/
```

---

## 快速导航

### 工程纪律（`_embedded-shared/`，两 skill 共用）

| 文档 | 内容 |
|------|------|
| [clean-code.md](../../_embedded-shared/clean-code.md) | 硬限表、命名约定、函数设计、防御式编程、const/static、BARR-C、MISRA-C/CERT-C 对齐 |
| [error-codes.md](../../_embedded-shared/error-codes.md) | 0=成功/负数=错误、禁 `if(status)`、两项目错误码布局 |
| [memory-safety.md](../../_embedded-shared/memory-safety.md) | 实时路径禁 malloc、堆规、VLA/strcpy/sprintf/strncpy 禁令、栈/缓冲 |
| [concurrency.md](../../_embedded-shared/concurrency.md) | 线程安全选择表、临界区四规、ISR→工作线程、ISR 优先级上限、volatile≠原子 |
| [realtime-hardware.md](../../_embedded-shared/realtime-hardware.md) | RTC 合规、非阻塞驱动、DMA+环形+D-Cache、看门狗、NVS、双 target 同源 |
| [tooling.md](../../_embedded-shared/tooling.md) | 编译器警告门禁、clang-tidy/cppcheck、CI 正则门禁、栈用量门禁、双 target 一致性 |
| [testing.md](../../_embedded-shared/testing.md) | host 单测共享层、帧解析 fuzzing、HIL、静态分发可测性红利 |
| [safety-checklist.md](../../_embedded-shared/safety-checklist.md) | 12 阶段安全清单、风险分级、严重性分级 |

### `static-dispatch/`（本项目标准）

| 文档 | 内容 |
|------|------|
| [README.md](./static-dispatch/README.md) | 治理声明 + SSOT + 代码现状 vs 目标偏差框 |
| [architecture.md](./static-dispatch/architecture.md) | App/BAL→DAL→PAL→Targets 分层 + 4 种静态分发形态 |
| [templates.md](./static-dispatch/templates.md) | POD 器件 / device_tree codegen / 平台文件切换 / control_algo 局部 vtable 模板 |
| [pitfalls.md](./static-dispatch/pitfalls.md) | 命名漂移 / 签名冲突 / SIMULATION 过宽 / wasm 假锁 |
| [evolution.md](./static-dispatch/evolution.md) | bool/float→wink_status_t 迁移 + 局部多态化退出路径 |

---

## 一句话

> 本项目采用 **静态分发**：POD + 命名 API + 编译期绑定。运行期多态是外部源码阅读主题，不是本项目编码范式。
