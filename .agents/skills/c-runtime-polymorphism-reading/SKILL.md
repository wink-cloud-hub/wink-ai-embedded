---
name: c-runtime-polymorphism-reading
description: Use when reading or explaining C runtime polymorphism in Linux, Zephyr, RT-Thread, STM32 HAL-style driver frameworks, ops/vtable patterns, function-pointer dispatch, base-struct embedding, or container_of. This is for source-code reading, architecture comparison, interviews, and external framework analysis only. Do NOT use for writing wink-micro-os or chigo-micro project code; those projects use embedded-best-practice static dispatch instead.
---

# C 运行期多态阅读指南

本 skill 只用于**阅读、解释、对比**运行期多态 C 架构：Linux / Zephyr / RT-Thread / STM32 HAL 风格、`ops` 虚表、`container_of`、base struct 嵌入、函数指针 dispatch。

## 边界

- 读外部内核 / RTOS / HAL 源码：使用本 skill。
- 面试或教学解释 C-OOP：使用本 skill。
- 对比静态分发与运行期多态：使用本 skill。
- 写 `wink-micro-os` / `chigo-micro` 代码：不要使用本 skill；改用 `embedded-best-practice`。

## 必读顺序

| 任务 | 读 |
|------|----|
| 快速理解适用边界 | `references/runtime-polymorphism/README.md` |
| 理解架构与调用链 | `references/runtime-polymorphism/architecture.md` |
| 需要骨架模板 | `references/runtime-polymorphism/templates.md` |
| 排错与代码审查 | `references/runtime-polymorphism/pitfalls.md` |
| 演化路径 | `references/runtime-polymorphism/evolution.md` |
| 与 Linux / C++ / 静态分发对照 | `references/runtime-polymorphism/reference.md` |
| 工程纪律 | `../_embedded-shared/`（两 skill 共用） |

## 使用原则

1. 先判断当前任务是“读外部源码”还是“写本项目代码”。
2. 解释运行期多态时，把三件事讲清：对象布局、dispatch 路径、生命周期归属。
3. 审查这类代码时重点看：`ops` 生命周期、`container_of` 类型匹配、const 正确性、并发保护、错误传播。
4. 不要把这里的 `ops` / `container_of` 模板迁移到 `wink-micro-os` 或 `chigo-micro` 器件抽象中。

## 输出建议

解释源码时优先使用这个结构：

```text
Runtime polymorphism reading:
- Object model:
- Dispatch path:
- Downcast/container_of point:
- Ownership/lifetime:
- Concurrency assumptions:
- Risks:
- Static-dispatch contrast, if relevant:
```
