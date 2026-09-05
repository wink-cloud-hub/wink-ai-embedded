# 错误码约定（范式无关）

> 适用范围：两种架构风格共用。核心决策见 **ADR-0001**。

---

## 核心规则

**所有可能失败的函数返回状态码：`0 = 成功，负数 = 错误`**（对齐 Linux/POSIX）。

```c
/* wink-micro-os */
wink_status_t dal_rc_servo_set_angle(dal_rc_servo_t *dev, float angle);

/* chigo-micro（同为 0/负数约定，类型为 int） */
int motor_driver_init(motor_driver_t *drv);
```

---

## ⚠ 头号脚雷：禁用 `if (status)`

负数在 C 里是 **truthy**。`if (status)` 会把一个负数错误码当成「真」——即当成「有情况」，
而在「0 = 成功」约定下，成功反而是假。这是 AI 代码生成最易踩的雷。

```c
wink_status_t status = dal_ultrasonic_read(&radar, &dist);

/* ❌ 错误：负数错误码是 truthy，会把失败当成功 */
if (status) { /* ... 误以为成功了 ... */ }

/* ✓ 正确 */
if (status < 0)         { /* 失败处理 */ }
if (status != WINK_OK)  { /* 失败处理 */ }   /* WINK_OK == 0 */
```

---

## 两项目的错误码布局对照

| 项目 | 返回类型 | 布局 |
|------|----------|------|
| **wink-micro-os** | `wink_status_t` | 分段：`0 = WINK_OK`；`-1..-11` 常见可恢复；`-20..-29` 功能安全可恢复（如 `WINK_ERR_OVERCURRENT`）；`-30..-49` 致命（如 `WINK_ERR_WATCHDOG`）；`-50..-59` **可恢复降级**（ADR-0005，如 `WINK_ERR_CONFIG_CORRUPT_DEGRADED(-50)`、`WINK_ERR_FAILED_INIT(-51)`——系统继续运行）；`-99 = WINK_ERR_PANIC` |
| **chigo-micro** | `int` + 位域 | `0` 成功 / 负数错误（非结构化 int）；安全状态另用 `ERR_BIT_*` 位域（OVERCURRENT/OVERHEAT/STALL/COLLISION/COMM_TIMEOUT/CRC_ERROR 等，见 `message_parser.h`） |

> 写 wink-micro-os 代码用 `wink_status_t` + 分段码；写 chigo-micro 代码用 `int` + `ERR_BIT_*`。
> 两者共享「0=成功 / 负数=错误 / 禁 `if(status)`」这条铁律。
>
> **无正数 warning 段**（ADR-0005）：「降级但继续运行」也归负数（`-50s`），故 `if(status<0)` 对降级状态依然正确捕获——App/BAL 用 `status == WINK_ERR_CONFIG_CORRUPT_DEGRADED` / `== WINK_ERR_FAILED_INIT` 特判走保守降级，其余 `<0` 走常规错误恢复。统一 `ERR_*` 前缀，**禁用 `WARN_*` 前缀**。

---

## 错误传播

- **每层要么处理，要么向上传播，绝不静默吞掉**。
- 必须检查每一个返回值（BARR-C 第 4 条）；忽略必须有明确标注。
- 初始化链失败时，按相反顺序 deinit 已成功初始化的资源（资源生命周期对称）。

```c
int init_system(void)
{
    int rc = a_init();
    if (rc < 0) return rc;
    rc = b_init();
    if (rc < 0) { a_deinit(); return rc; }   /* 回滚 a */
    rc = c_init();
    if (rc < 0) { b_deinit(); a_deinit(); return rc; }
    return 0;
}
```

> 详细的「断言 vs 运行时检查」区分见 [clean-code.md](./clean-code.md)。

---

## 必检返回值：`warn_unused_result` 便携宏

所有返回 `wink_status_t` 的公共 API 应标记「返回值不可忽略」。直接裸写 `__attribute__((warn_unused_result))` 是编译器特有语法——未来引入 MSVC 会直接编译失败，且与「禁 clang-only / GCC-only 特性」（[tooling.md](./tooling.md)）精神有张力。**统一用便携宏**：

```c
/* 放入 wink_status.h（该头尚待创建，属已知技术债）；落地前 AI 按此宏名引用，不裸写 __attribute__ */
#if defined(__GNUC__) || defined(__clang__)
    #define WINK_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
    #define WINK_WARN_UNUSED_RESULT
#endif

WINK_WARN_UNUSED_RESULT wink_status_t dal_ultrasonic_read(dal_ultrasonic_t *dev, float *distance_cm);
```

> 禁止裸写 `__attribute__((warn_unused_result))`；统一 `WINK_WARN_UNUSED_RESULT`。`wink_status.h` 尚未落地是已知技术债，创建时须含此宏（连同 ADR-0001 方案 C + ADR-0005 `-50s` 段）。

---

## 错误码字符串映射（强制）

每个模块必须提供 `module_err_to_string()` 表驱动函数，将错误码转换为可读字符串。禁止硬编码 `printf("error %d"`，不利于日志必须转字符串。

```c
/* 模板 ✅
const char* dal_err_to_string(wink_status_t err)
{
    switch (err) {
    case WINK_OK:                      return "OK";
    case WINK_ERR_TIMEOUT:              return "Timeout";
    case WINK_ERR_INVALID_ARG:          return "Invalid argument";
    case WINK_ERR_CONFIG_CORRUPT_DEGRADED: return "Config corrupt (degraded)";
    default:                            return "Unknown error";
    }
}
```

---

## 错误码模块来源标记（推荐）

对于多模块系统，错误码可携带模块来源信息，便于定位故障：

```c
/* 模块 ID 定义
#define MODULE_ID_DAL      (1 << 24)
#define MODULE_ID_PAL      (2 << 24)
#define MODULE_ID_BAL      (3 << 24)

/* 构造带模块标记的错误码
#define MAKE_ERR(module, err)  ((wink_status_t)((module) | ((err) & 0xFFFFFF))

/* 提取模块 ID 和原始错误码
#define ERR_MODULE(err)     ((uint32_t)(err) >> 24)
#define ERR_CODE(err)      ((wink_status_t)((err) & 0xFFFFFF))
```

> 注意：此机制不改变错误码的正负语义，`if (status < 0)` 检查依然有效。

---

## 成功语义扩展

错误码设计预留正数段（预留，非强制）

正数不用于 warning（ADR-0005 已明确降级也用负数），但可用于「成功但有附加信息」的场景：

```c
/* 预留正数语义段
#define WINK_OK_PARTIAL      1   /* 部分成功（如只写入部分数据）*/
#define WINK_OK_DEGRADED     2   /* 降级运行但功能正常 */

/* if (status >= 0) 依然能判断成功
/* 具体成功细节用 if (status == WINK_OK) 判断完全成功 */
```

> 此为预留设计，非强制。仅在确实需要区分成功等级的场景使用。

---

## wink_status.h 完整模板（待创建）

```c
#ifndef WINK_STATUS_H
#define WINK_STATUS_H

#include <stdint.h>

/* 便携宏：返回值不可忽略 */
#if defined(__GNUC__) || defined(__clang__)
    #define WINK_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
    #define WINK_WARN_UNUSED_RESULT
#endif

/* 错误码类型 */
typedef int32_t wink_status_t;

/* 成功码 */
#define WINK_OK                    (0)

/* 通用可恢复错误 -1..-11 */
#define WINK_ERR_GENERAL          (-1)
#define WINK_ERR_TIMEOUT          (-2)
#define WINK_ERR_INVALID_ARG      (-3)
#define WINK_ERR_NOT_SUPPORTED    (-4)
#define WINK_ERR_NO_MEMORY        (-5)
#define WINK_ERR_BUFFER_FULL      (-6)
#define WINK_ERR_NOT_FOUND        (-7)
#define WINK_ERR_ALREADY_INIT     (-8)

/* 功能安全可恢复 -20..-29 */
#define WINK_ERR_OVERCURRENT      (-20)
#define WINK_ERR_OVERHEAT         (-21)
#define WINK_ERR_STALL            (-22)
#define WINK_ERR_COLLISION        (-23)

/* 致命错误 -30..-49 */
#define WINK_ERR_WATCHDOG        (-30)
#define WINK_ERR_HARD_FAULT      (-31)
#define WINK_ERR_PANIC            (-99)

/* 可恢复降级 -50..-59 */
#define WINK_ERR_CONFIG_CORRUPT_DEGRADED  (-50)
#define WINK_ERR_FAILED_INIT                (-51)

#endif /* WINK_STATUS_H */
```

---
> **源出（溯源）**：ADR-0001（`docs/design/decisions/0001-error-code-sign-convention.md`）、
> zhaoming `clean-code.md` 错误处理策略、chigo-micro `.claude/rules/c-embedded.md`。
