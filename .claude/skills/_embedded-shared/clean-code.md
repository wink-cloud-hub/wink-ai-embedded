# Clean Code 与代码风格（范式无关）

> 适用范围：**两种架构风格共用**（运行期多态参考基线 + 本项目静态分发）。
> 本文件只讲范式无关的工程纪律。风格选择见所在 skill 的 index / 导航文档。

---

## 硬性限制（不可商量）

| 规则 | 限制 |
|------|------|
| 最大行宽 | 80 列（超长必须换行） |
| 最大函数长度 | 80 行（接近上限就拆子函数） |
| 最大嵌套深度 | 4 层（用 early return / guard clause 降嵌套） |
| 最大函数参数数 | 5 个（更多就组合成配置结构体） |
| 死代码 / 注释掉的代码 | 必须删除 |
| 未使用的符号 | 必须删除 |

---

## 魔法数字（硬规）

所有数字字面量（除 `0`、`1` 和明显的布尔类值外）**必须定义为宏**：

```c
/* ❌ 反面示例 */
if (retry_count > 3) { ... }
uint8_t buffer[256];
timeout = 5000;

/* ✅ 正面示例 */
#define MAX_RETRY_COUNT      (3U)
#define RX_BUFFER_SIZE       (256U)
#define DEFAULT_TIMEOUT_MS   (5000U)

if (retry_count > MAX_RETRY_COUNT) { ... }
uint8_t buffer[RX_BUFFER_SIZE];
timeout = DEFAULT_TIMEOUT_MS;
```

### 宏定义风格

- 值用括号包裹：`#define FOO (42U)`
- 无符号常量使用 `U` 后缀
- 大型无符号常量使用 `UL` 或 `ULL` 后缀
- 相关宏使用统一前缀分组

### 枚举固定宽度（强制）

枚举存储类型必须显式指定，禁止依赖编译器默认（int）：

```c
/* ❌ 错误：依赖编译器默认 int */
typedef enum {
    DEV_STATE_IDLE,
    DEV_STATE_ACTIVE,
    DEV_STATE_COUNT
} dev_state_t;

/* ✅ 正确：显式固定宽度 */
typedef uint8_t dev_state_t;
enum {
    DEV_STATE_IDLE = 0,
    DEV_STATE_ACTIVE,
    DEV_STATE_COUNT
};
```

> 理由：嵌入式 RAM 宝贵，8 位足够的枚举不应占 32 位。且跨平台/序列化时宽度一致。

---

## 命名约定（以本项目为准）

> ⚠ **本项目用纯 snake_case**。zhaoming 参考基线用 `PascalCase_t` 类型 + `Module_Action`
> 函数名——那是外部风格，**本项目不采用**，阅读其文档时注意区分。

| 元素 | 约定 | 示例 |
|------|------|------|
| 宏 / 常量 | 全大写蛇形 | `NUM_JOINTS`、`PID_PERIOD_US`、`RX_BUFFER_SIZE` |
| 类型 | snake_case + `_t` | `motor_driver_t`、`dal_servo_t`、`cmd_frame_t` |
| 函数 | `模块_动作()` 小写蛇形 | `motor_driver_init`、`dal_servo_set_angle` |
| 局部变量 / 结构体成员 | 小写蛇形 | `retry_count`、`current_angle` |
| 枚举值 | `UPPER_CASE` 或 `前缀_名称` | `MOTOR_MODE_IDLE`、`CMD_TYPE_TRAJECTORY` |
| 文件级 static 变量 | `s_` 前缀 | `static int s_pending_count;` |
| 全局变量 | `g_` 前缀 | `g_motor_driver`、`g_state_mutex` |
| 函数指针 typedef | `*_fn` 后缀 | `cmd_handler_fn`、`event_callback_fn` |

**命名即文档**：作用域越大，名字越长越完整。循环计数器可短（`i`、`n`）；公共函数必须
带完整上下文（`motor_driver_check_overcurrent`，不是 `check`）。布尔用 `is_`/`has_`/
`can_`/`should_` 前缀。

**App Role / BAL 操作三类（A 活动 · B 能力 · C 动作）**：  
口诀——主交付进事件队列 → `enable_*`；否则有后台会话 → `start/stop`；一次做完 → `set/get/request/…`。Role↔BAL 同操作同动词。  
SSOT：`docs/design/07-platform-governance/coding-conventions.md` §3；决策：`docs/design/decisions/0032-bal-role-operation-naming-classes.md`。写新 API 前先归类；**勿在本文件复制整表**。

---

## static inline 与 restrict 优化（硬规）

### static inline 使用规范

访问器函数和小函数必须使用 `static inline` 以获得最佳性能：

```c
/* ✅ 正确：访问器用 static inline */
static inline __attribute__((always_inline))
float dal_servo_get_angle(const dal_servo_t *self)
{
    return self->state.current_angle;
}

/* ✅ 正确：小于 5 行的热路径函数 */
static inline __attribute__((always_inline))
bool pid_is_within_deadband(float error, float deadband)
{
    return (error >= -deadband) && (error <= deadband);
}
```

> 规则：
> 1. 所有 getter/setter 访问器必须 `static inline`
> 2. 小于 5 行的热路径函数必须 `static inline`
> 3. 头文件中定义的 `static inline` 必须加 `__attribute__((always_inline))` 防止降级
> 4. 超过 20 行的函数不要 inline，避免代码膨胀

### restrict 关键字

指针参数无别名时必须使用 `restrict` 优化：

```c
/* ✅ 正确：源和目标无别名，用 restrict */
void buffer_copy(uint8_t *restrict dst,
                 const uint8_t *restrict src,
                 size_t len)
{
    /* ... */
}
```

> restrict 告诉编译器指针不会别名，允许更激进的优化（如向量化、重排加载）。
> memcpy/memmove 的目标指针必须 restrict。

---

## 头文件规范（硬规）

### 头文件保护

每个头文件必须有头文件保护宏，格式为 `MODULE_NAME_H`：

```c
#ifndef DAL_ULTRASONIC_H
#define DAL_ULTRASONIC_H

/* ... 内容 ... */

#endif /* DAL_ULTRASONIC_H */
```

### 自包含头文件

每个头文件必须能**独立编译**。包含它所引用的所有类型——不要依赖引用者来提供。

### 路径约定

在添加 `#include` 之前，搜索项目中其他文件是如何包含同一头文件的。使用完全相同的路径格式：

```c
/* 如果项目中其他文件这样写： */
#include "drivers/uart_driver.h"

/* 那么你也必须这样写： */
#include "drivers/uart_driver.h"

/* ❌ 不要写成： */
#include "uart_driver.h"
#include "../drivers/uart_driver.h"
```

### 包含顺序

遵循项目现有的包含顺序。本项目约定：

```c
/* 1. 对应头文件（用于 .c 文件） */
#include "my_module.h"

/* 2. 平台/RTOS 头文件 */
#include "FreeRTOS.h"
#include "task.h"

/* 3. PAL / DAL 头文件 */
#include "pal/hal/pal_hal_gpio.h"
#include "dal/dal_servo.h"

/* 4. 项目模块头文件 */
#include "app/protocol.h"

/* 5. 标准库（如使用） */
#include <string.h>
#include <stdint.h>
```

---

## 函数文档注释

公共函数必须有文档注释，使用 Doxygen 兼容格式：

```c
/**
 * @brief   读取超声波传感器距离。
 * @param   self      传感器实例指针（不能为 NULL）
 * @param   distance_cm  输出距离（厘米），失败时不修改
 * @return  WINK_OK = 成功；WINK_ERR_TIMEOUT = 回声超时；
 *          WINK_ERR_INVALID_ARG = 参数无效
 * @note    非阻塞，调用后需等待测量完成。
 *          线程安全：可从任何任务上下文调用，不可在 ISR 中调用。
 */
wink_status_t dal_ultrasonic_read(dal_ultrasonic_t *self, float *distance_cm);
```

### 注释质量规则

1. 注释解释**为什么**，而非「是什么」（代码自己能说明做什么）
2. 注释必须与代码一致——修改代码时同步更新注释
3. 删除被注释掉的代码；使用版本控制来保存历史
4. 不写重复代码内容的冗余注释（如 `i++; /* i 加 1 */`）

---

## 函数设计

- **单一职责（SRP）**：能用「和」描述的函数就该拆。`validate_and_send` → `validate_packet` + `send_packet`。
- **抽象层级一致**：一个函数内所有操作处于同一抽象层级。不要把高层编排和裸寄存器位操作混在一起。
- **命令-查询分离（CQS）**：函数要么执行动作（命令），要么返回信息（查询），不要兼而有之。例外：原子「取并改」（如 `queue_pop`）。
- **无副作用**：一个 `validate_xxx` 偷偷改全局状态或写硬件，是缺陷。
- **参数 ≤ 5**：超过就打包成配置结构体（如 7 参 `uart_init` → 传 `uart_config_t`）。

---

## 防御性编程：断言 vs 运行时检查

| 情况 | 机制 |
|------|------|
| 程序员错误（不应该发生） | `ASSERT()` |
| 外部 / 不可信输入（用户、网络、传感器、跨模块调用） | 运行时检查 + 错误码 |
| 硬件故障检测 | 运行时检查 + 恢复机制 |

- 断言抓「内部契约被破坏」（如 `self != NULL`），**不**抓运行时错误。失败 = 程序员 bug。
- 外部输入必须运行时校验 + 优雅失败（返回错误码），**不**用断言。
- 防御编码 6 条：解引用前校验所有指针；校验数组下标；校验枚举范围；检查每个返回值；所有变量初始化；switch 即使枚举穷尽也写 `default`。

---

## const / static（硬规，非建议）

- **`const` 尽量多用**：防止意外修改 + 表达意图。最常见的是「指向 const 数据的指针」。
  - **不要** const 值参数（`const uint16_t len`）——C 里值是副本，毫无意义。
- **`static` 强制**：所有非公共 API 的函数和变量必须 `static`（信息隐藏，缩窄链接域）。
- 头文件只暴露公共契约；内部细节留在 `.c`。

---

## DRY 与表驱动

- **DRY**：发现复制粘贴带微小差异、多处相似 switch/if-else、重复校验模式 → 提取共享函数；变化部分用函数指针（策略）。优先 `static inline` 而非宏。
- **表驱动**：用查找表替代冗长 switch/if-else。适用：错误码→字符串、命令分发表（ID→函数指针）、状态机转移表、配置参数范围校验、协议字段解析。

---

## BARR-C 安全编码四条

1. **大括号**：`if`/`for`/`while`/`do-while` 即使单行也必须加大括号。禁止 `if (err) return;`。
2. **固定宽度整数**：禁用裸 `int`/`short`/`long`/`unsigned`；必须用 `stdint.h`（`int32_t`、`uint8_t`）。
3. **位操作安全**：只对无符号操作；移位计数 < 操作数位宽；有符号左移是 UB。
4. **返回值检查**：非 `void` 返回值必须检查，或显式标注忽略。

---

## MISRA-C / CERT-C 对齐

> 本项目「安全关键」纪律以 **BARR-C**（见上）为骨架，并向 **MISRA-C:2012** 与 **CERT C** 看齐。
> 不追求逐条合规，而是遵循其**高价值子集**；偏离可接受，**未记录的偏离不可接受**。

| 来源 | 高价值规则（本项目强制） |
|------|------------------------|
| MISRA-C:2012 | Dir 4.1（运行时故障最小化）、Rule 8.x（声明/定义一致）、Rule 17.x（指针）、Rule 18.x（整数与位操作用无符号）、Rule 21.x（标准库 `strcpy/sprintf/atoi` 等禁用——见 [memory-safety.md](./memory-safety.md)） |
| CERT C | MEM（内存）、INT（整数溢出/移位）、STR（字符串）、CON（并发，见 [concurrency.md](./concurrency.md)）、EXP（求值序） |

要点：这三套在「禁裸 int / 禁 `strcpy` / 检查返回值 / 位操作无符号 / 指针校验」上**高度一致**——
本项目硬规则已覆盖大多数。工具侧用 clang-tidy 的 `cert-*` / `misra-*` 检查子集自动卡
（见 [tooling.md](./tooling.md)）。任何偏离必须注释写明：规则号、理由、风险、缓解。

---

## 注释

- 解释「为什么」，不是「做了什么」（代码自己能说明做什么）。
- 与代码同步，过时即删。
- 删掉注释掉的代码。
- 不写冗余注释（`i++; /* i 加 1 */`）。

---
> **源出（溯源）**：zhaoming `ai-coding-skill/references/code-style.md` + `clean-code.md`。
> 本文为范式无关提炼，命名约定已改写为本项目（snake_case）标准。
