---
paths:
  - "**/*.c"
  - "**/*.h"
---

# C Coding Standards & API Conventions

> ⚠️ **本文件已迁移至 Skill 文档体系**。以下是索引，所有详细规则请查看对应 Skill 文档。

---

## 规则索引

| 主题 | Skill 文档位置 |
|------|---------------|
| **错误码约定** (`wink_status_t`) | `_embedded-shared/error-codes.md` |
| **Clean Code 与代码风格** | `_embedded-shared/clean-code.md` |
| **内存安全与 Struct 布局** | `_embedded-shared/memory-safety.md` |
| **并发与线程安全** | `_embedded-shared/concurrency.md` |
| **实时与硬件相关规则** | `_embedded-shared/realtime-hardware.md` |
| **静态分发架构与设计模式** | `embedded-best-practice/references/static-dispatch/patterns.md` |
| **DAL/PAL 分层架构** | `embedded-best-practice/references/static-dispatch/architecture.md` |
| **分层门禁（生成前自查）** | 修改/生成 C 代码后运行：`python wink-micro-os/tools/wink.py lint --pack layering --pack api`（ADR-0043） |

---

## 为什么迁移

原 `c-code.md` 的内容已全部整合进更系统化的 Skill 文档体系，优势：

1. **单一事实来源 (SSOT)**：避免文档漂移
2. **范式分离**：清晰区分「范式无关的工程纪律」与「本项目静态分发专用规则」
3. **更完整覆盖**：补充了状态机、表驱动等设计模式指南
4. **跨项目复用**：`_embedded-shared/` 下的规则可同时用于 wink-micro-os 和 chigo-micro

---

## 本项目核心范式

wink-micro-os 采用 **编译期静态分发**：

- ✅ POD 结构体 + 命名式 API（如 `dal_servo_set_angle(&dev, angle)`）
- ❌ 不使用 `struct device_ops` 运行期虚表
- ❌ 不使用 `container_of` 向下转型

详细说明见：
- [ADR-0004](../docs/design/decisions/0004-static-dispatch-vs-runtime-ops.md)
- `embedded-best-practice/references/static-dispatch/README.md`

---

## 快速链接

- ✅ Skill 入口：`.claude/skills/embedded-best-practice/`
- ✅ 范式无关工程纪律：`.claude/skills/_embedded-shared/`
- ✅ 架构决策记录：`docs/design/decisions/`
