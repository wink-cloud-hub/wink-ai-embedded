# 工具链与 CI 强制（范式无关）

> 适用范围：两种架构风格共用。**规则不配执行器就会腐烂**——本文件把 `clean-code.md` /
> `memory-safety.md` / `error-codes.md` 的铁律落成可自动卡住的闸门。
> 与 [safety-checklist.md](./safety-checklist.md) 的人工 12 阶段互补：人查语义，机查模式。

---

## 为什么需要强制层

- AI 生成的头号雷（`if(status)`、`container_of`、`strcpy`、VLA）都是**可正则匹配**的——
  一条 CI 规则就能挡住，比事后 code review 划算几个数量级。
- 双 target（wasm + xtensa）必须**两边都过同一套闸**，否则「虚实同源」会在编译期就裂开。
- lint 配置本身是**可评审的 SSOT**：规则进仓库，全员对齐，不再依赖个人记忆。

---

## 编译器警告门禁（硬底线）

两个 target 共用：

```
-Wall -Wextra -Werror -Wconversion -Wshadow -Wdouble-promotion
-Wformat=2 -Wundef -Wunused -Wpedantic -std=c11
```

- `-Werror`：**警告即错误，零容忍**（见 [safety-checklist.md](./safety-checklist.md) 阶段 1）。
- `-Wconversion`：抓隐式截断/符号转换——本项目禁裸 `int`、用 `stdint.h` 的配套闸。
- `-Wdouble-promotion`：`float`→`double` 静默提升，热路径性能与正确性双杀。
- `-Wshadow`：变量遮蔽，并发/重构时的隐蔽 bug 源。
- 禁用 clang-only / GCC-only 扩展（ADR-0002，见 [realtime-hardware.md](./realtime-hardware.md)）。

> 任何「为了过编译而加的强制转换 `(void)x;` 或关警告 `#pragma diagnostic`」必须在注释里写明
> 为什么，且尽量收窄到单行。

---

## 静态分析

| 工具 | 抓什么 | 集成 |
|------|--------|------|
| **clang-tidy** | `bugprone-*`、`cert-*`、`misra-*`、`readability-*`、空指针解引用、未初始化、误用 `volatile` 当同步 | `.clang-tidy` 进仓库；CI 跑 `run-clang-tidy`，新违规 fail |
| **cppcheck** | 缓冲区越界、资源泄漏、空指针、死代码、`memcpy` size 异常 | `--enable=warning,style,performance --inline-suppr`，CI fail on error |
| **编译器自身** | 见上节 `-W` 闸 | 已是零警告底线 |

> 优先让 **clang-tidy / cppcheck** 抓 `if(status)`、`strcpy` 这类（语义级），CI 正则（下节）
> 做兜底——两者重叠不浪费，因为正则快、便宜、零误报可定制。

---

## ⭐ CI 正则门禁（防 AI 翻车的第一道闸）

在 CI 对**全量改动 diff** 跑以下正则，命中即 fail（合法策略 vtable 必须在调用行加 `/* lint-allow: strategy-vtable */` 显式行级豁免，否则 fail）：

| 禁止模式（示例 grep） | 含义 | 合法例外 |
|----------------------|------|----------|
| `if\s*\(\s*\w*[sS]tatus\s*\)` | `if(status)`：负数 truthy，把失败当成功（[error-codes.md](./error-codes.md) 头号雷） | 无——一律 `if(status < 0)` |
| `\bcontainer_of\b` | 运行期多态向下转型，违反 ADR-0004 | 仅运行期多态参考基线（对照阅读） |
| `struct\s+\w+_ops\b` / `\w+_ops_t` 类型名、`->ops->` / `.ops->` 调用点 | 器件抽象 ops 虚表（**三重匹配**：类型名 + ops 字段 + 调用点）| 仅策略层 + **行级 `/* lint-allow: strategy-vtable */`** 标注，否则 fail。**白名单无默认路径**——任何 `ops` 相关模式必须显式标注豁免 |
| `\b(strcpy\|sprintf\|strncpy\|gets\|alloca\)\s*\(` | 无界写入 / 栈分配 | 无（见 [memory-safety.md](./memory-safety.md)） |
| `\b(malloc\|free\|calloc\|realloc\)\s*\(` | 实时路径动态分配 | 非实时路径 + 配对释放 + 文档化所有权 |
| `#pragma\s+pack` | 跨 target 强制对齐，破坏 wasm/xtensa 一致性 | 无（ADR-0002） |
| `volatile\s+\w+\s*[;=]`（裸 volatile 当同步原语） | volatile 不保证原子性/可见性（[concurrency.md](./concurrency.md)） | MMIO 寄存器指针、且单标志位单核约定 |

> VLA（`int a[n];`）正则难精确抓，靠 `-Wvla`（含在 `-Wpedantic` 下）+ 评审兜底。

落地示例（CI 脚本片段，伪代码）：

```sh
# 对本 PR 改动的 .c/.h 跑禁令扫描；带 `lint-allow:` 行级标注的命中行豁免
for pattern in \
  'if[[:space:]]*\([[:space:]]*[A-Za-z_]*[Ss]tatus[[:space:]]*\)' \
  '\bcontainer_of\b' \
  '\b(struct[[:space:]]+\w+_ops|\w+_ops_t)\b' \
  '->\s*ops\s*->|\.\s*ops\s*->' \
  '\b(strcpy|sprintf|strncpy|gets|alloca)[[:space:]]*\(' \
  '#pragma[[:space:]]+pack' ; do
    if git diff --name-only | grep -E '\.[ch]$' \
       | xargs grep -nE "$pattern" | grep -v 'lint-allow:' ; then
        echo "FORBIDDEN PATTERN: $pattern" ; exit 1
    fi
done
```

---

## 规则追踪表

| 规则 | 自动化手段 | 人工检查 |
|------|------------|----------|
| 禁 `if(status)` | CI 正则 / clang-tidy 自定义检查 | 确认错误路径传播语义 |
| 禁器件 `ops` / `container_of` | CI 正则（类型名 + ops 字段 + `->ops->` 调用点）+ 行级 `lint-allow` | 确认没有绕开 ADR-0004 |
| 禁 VLA / alloca | `-Wvla` / CI 正则 | 检查栈帧与调用链深度 |
| 禁 `strcpy` / `sprintf` / `strncpy` | CI 正则 / cppcheck | 检查截断处理与显式终止 |
| 实时路径禁动态分配 | CI 正则 + 路径白名单 | 确认所有权、失败路径、时序影响 |
| 栈帧过大 | `-fstack-usage` 阈值 | 检查任务栈高水位 |
| 回调上下文 | 难自动化 | 必须文档化线程 / ISR / 锁上下文 |

---

## 栈与资源门禁

- **栈用量**：链接期 `-fstack-usage`（GCC/clang）生成 `.su` 文件，CI 校验**最大栈帧 < 阈值**
  （如 256B），超阈值 fail——把「栈紧张时大局部改堆」（[memory-safety.md](./memory-safety.md)）前置成自动检查。
- **运行时高水位**：debug 构建里 FreeRTOS 任务用 `uxTaskGetStackHighWaterMark` 断言余量 > 阈值，
  启动自检阶段打印各任务栈余量。
- **init/deinit 对称**：lint 规则或 CI 脚本核对每个 `*_init` 是否有配对 `*_deinit` 路径。
- **Wasm Asyncify 栈预算**（ADR-0002 已知风险，[realtime-hardware.md](./realtime-hardware.md)）：`-fstack-usage` / FreeRTOS 高水位只覆盖真机；Wasm 的 Asyncify 栈是**独立预算**，溢出表现为深调用链时仿真**静默挂起**（无明确 reset reason，极难定位）。三道闸：
  - **栈预算声明**：wasm target 构建脚本把 `ASYNCIFY_STACK_SIZE` 显式声明为项目常量，文档化其与最深同步阻塞调用链（如 `App → BAL → DAL → pal_delay_ms → Asyncify 栈帧`）的关系。
  - **编译期断言**：对已知深嵌套路径用 `static_assert` 或构建期脚本校验调用链深度 < `ASYNCIFY_STACK_SIZE` 余量。
  - **CI 回归**：CI 跑 wasm target 时对最深的同步阻塞路径做 Asyncify 栈用量回归（与真机 `.su` 栈门禁并列，任一超阈 fail）。

---

## 双 target 一致性门禁（ADR-0002）

CI 必须对**两个 target 各跑一遍**且都过零警告 + 全部门禁：

| Target | 工具链 | 闸 |
|--------|--------|----|
| `wasm32` | Emscripten/Asyncify | `-Wall -Wextra -Werror` + 正则门禁 + host 单测（见 [testing.md](./testing.md)） |
| xtensa | ESP-IDF | 同上 + 真机构建 +（可选）HIL 冒烟 |

> 任一 target 不过 = CI 红。这是「仿真→烧录行为一致」在编译期的护栏。

---
> **源出（溯源）**：本项目本地沉淀，对齐 BARR-C / MISRA-C / CERT-C 的「规则可机器化」原则 +
> chigo-micro `c-embedded.md` 的零警告纪律 + wink-micro-os ADR-0002 双 target 约束。
