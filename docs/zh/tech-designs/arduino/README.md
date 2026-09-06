# Arduino 兼容层技术方案与 RFC (Arduino Tech-Designs)

本目录归档 Arduino 兼容框架（`wink-micro-os/frameworks/arduino/`）的详细设计与技术选型，包括板级引脚常量、Arduino API 沙箱桥接与真机/仿真引脚映射等主题。

---

## 📂 技术设计索引

| 文档名称 | 核心主题与设计目标 | 管辖 ADR / 状态 |
| :--- | :--- | :--- |
| **[2026-09-06-arduino-board-pin-constants-codegen-design.md](./2026-09-06-arduino-board-pin-constants-codegen-design.md)** | 板级引脚常量（`D2`/`A0`/`LED_BUILTIN`）供给方案选型：codegen 生成 `wink_board_pins.h` vs 手写静态变体头 | Draft（消费端已落地，emitter 待办）；拟提炼 ADR-0078 |

---

## 🔗 相关领域

- **[ADR-0035 Arduino 兼容多态沙箱](../../../decisions/core/0035-arduino-compat-polymorphism-sandbox.md)**：Arduino 框架作为 C++ 叶子沙箱与纯 C 内核解耦的总纲。
- **[mcs51 领域](../mcs51/README.md)**：8051 侧以语言原生 `sbit`/SFR 机制实现零修改，与 Arduino 侧整数引脚常量模型形成对照。
