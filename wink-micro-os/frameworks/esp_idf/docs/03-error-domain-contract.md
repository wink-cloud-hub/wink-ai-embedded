<!-- SPDX-License-Identifier: LGPL-3.0-only -->
# ESP-IDF 多域错误码契约规范 (Error Domain Taxonomy & Mapping Contract)

| 项 | 内容 |
|---|---|
| 规范编号 | `SPEC-ESP-IDF-ERROR-DOMAIN-CONTRACT-v1.0` |
| 日期 / 状态 | 2026-10-09 / **Active** |
| 遵循决策 | [ADR-0001 (Negative Error Codes)](../../../docs/design/decisions/core/0001-error-code-sign-convention.md), [ADR-0004 (Static Dispatch)](../../../docs/design/decisions/core/0004-static-dispatch-vs-runtime-ops.md), [ADR-0012 (Contract Honesty)](../../../docs/design/decisions/core/0012-contract-honesty-over-silent-degradation.md) |
| 关联技术设计 | [AFG-Engine 契约 v1.1](../../../docs/zh/tech-designs/esp32/esp-idf-anti-false-green-verification-engine-contract.md) |

---

## 1. 核心背景与治理目标

在 WinkMicroOS 仿真体系中，ESP-IDF 门面层（`frameworks/esp_idf`）向上承接乐鑫官方原厂 C 代码，向下调用 Wink PAL（平台抽象层）。

以往契约与断言中存在严重的 **错误码领域混淆（Error Domain Confusion）**：
1. 错误地将 ESP-IDF 官方正数错误码（如 `ESP_ERR_TIMEOUT = 0x107 (263)`）表述为“负数错误码”；
2. 混淆了 Wink PAL 的负数错误码约定（`wink_status_t`, ADR-0001: `< 0` 失败）与 ESP-IDF 的正数错误码约定（`esp_err_t`: `> 0` 失败）；
3. 模糊匹配：在自动化断言中仅用 `< 0` 或 `!= 0` 判定结果，导致错误码域穿透时产生假绿或误报。

本契约确立 **4 大强隔离错误域（Domain Taxonomy）** 与 **C 门面层转换桥接规范**，并在自动化验证引擎中引入类型化 Matcher。

---

## 2. 4 类隔离错误域定义 (Taxonomy)

| 错误域 (`domain`) | 符号类型 | 成功语义 | 失败语义 | 典型符号与数值示例 | 适用层级 |
|---|---|---|---|---|---|
| `esp_err` | `esp_err_t` (int / uint32_t) | `0` (`ESP_OK`) | 正整数（除特例 `ESP_FAIL = -1` 外） | `ESP_ERR_NO_MEM = 0x101 (257)`<br/>`ESP_ERR_INVALID_ARG = 0x102 (258)`<br/>`ESP_ERR_INVALID_STATE = 0x103 (259)`<br/>`ESP_ERR_TIMEOUT = 0x107 (263)` | ESP-IDF 官方 API / 仿真门面层 |
| `wink_status` | `wink_status_t` (signed 32-bit enum) | `0` (`WINK_OK`) | 负整数（ADR-0001 约定） | `WINK_ERR_INVALID_ARG = -1`<br/>`WINK_ERR_TIMEOUT = -2`<br/>`WINK_ERR_IO = -5`<br/>`WINK_ERR_NO_MEM = -13`<br/>`WINK_ERR_INVALID_STATE = -16` | Wink PAL / DAL / 内核运行时 |
| `posix_errno` | 函数返回 `-1`，`errno` 为正整数 | `0` 或特定有效句柄/字节数 | 返回 `-1`，通过 `errno` 暴露正整型错误码 | `EINVAL = 22`<br/>`ETIMEDOUT = 110`<br/>`ECONNREFUSED = 111` | POSIX VFS / BSD Socket 接口 |
| `nimble_hs` | `int` (NimBLE Host Status) | `0` (`0` 表示成功) | 正整型协议栈状态码 | `BLE_HS_EAGAIN = 1`<br/>`BLE_HS_EALREADY = 2`<br/>`BLE_HS_EINVAL = 3`<br/>`BLE_HS_ENOTCONN = 4`<br/>`BLE_HS_ETIMEOUT = 5`<br/>`BLE_HS_EDONE = 6` | NimBLE 蓝牙协议栈层 |

### 2.1 严禁表述（Anti-Patterns）
- ❌ **绝对禁止**在契约、测试或日志中出现“负错误码如 `ESP_ERR_TIMEOUT`”或将 `ESP_ERR_TIMEOUT` 写作 `-0x107` / `-263`。
- ❌ **绝对禁止**在 ESP-IDF API 返回值上直接调用 `wink_status_is_error(err)`（因为正数 `0x107 > 0` 会被判定为非负成功，造成假绿）。
- ❌ **绝对禁止**在断言中仅用 `rc != 0` 或 `rc < 0` 模糊匹配跨域返回值。

---

## 3. C 门面层错误转换边界规范

ESP-IDF 仿真门面层（`wink-micro-os/frameworks/esp_idf/src/`）必须对上暴露纯正 `esp_err_t`，调用底层 Wink PAL 时必须使用严格转换函数进行无损桥接：

```c
#include "esp_err.h"
#include "esp_err_mapping.h"
#include "wink_status.h"

esp_err_t wink_status_to_esp_err(wink_status_t status);
wink_status_t esp_err_to_wink_status(esp_err_t err);
```

### 3.1 映射关系矩阵 (Mapping Matrix)

| Wink PAL 状态 (`wink_status_t`) | ESP-IDF 错误码 (`esp_err_t`) | 含义 |
|---|---|---|
| `WINK_OK (0)` | `ESP_OK (0)` | 成功 |
| `WINK_ERR_INVALID_ARG (-1)` | `ESP_ERR_INVALID_ARG (0x102)` | 非法参数 |
| `WINK_ERR_TIMEOUT (-2)` | `ESP_ERR_TIMEOUT (0x107)` | 操作超时 |
| `WINK_ERR_DISCONNECTED (-3)` | `ESP_ERR_NOT_FOUND (0x105)` | 设备未连接/未发现 |
| `WINK_ERR_OUT_OF_RANGE (-4)` | `ESP_ERR_INVALID_SIZE (0x104)` | 超出范围/大小非法 |
| `WINK_ERR_IO (-5)` | `ESP_FAIL (-1)` | 硬件 I/O 错误 |
| `WINK_ERR_BUSY (-6)` | `ESP_ERR_INVALID_STATE (0x103)` | 资源忙/状态冲突 |
| `WINK_ERR_UNSUPPORTED (-7)` | `ESP_ERR_NOT_SUPPORTED (0x106)` | 功能不支持 |
| `WINK_ERR_CHECKSUM (-8)` | `ESP_ERR_INVALID_CRC (0x109)` | CRC/校验和错误 |
| `WINK_ERR_PERMISSION (-9)` | `ESP_ERR_NOT_ALLOWED (0x10D)` | 权限不足/禁止访问 |
| `WINK_ERR_RESOURCE_EXHAUSTED (-10)` | `ESP_ERR_NO_MEM (0x101)` | 资源池枯竭/内存不足 |
| `WINK_ERR_NOT_INITIALIZED (-11)` | `ESP_ERR_INVALID_STATE (0x103)` | 未初始化 |
| `WINK_ERR_HARDWARE (-12)` | `ESP_FAIL (-1)` | 硬件级故障 |
| `WINK_ERR_NO_MEM (-13)` | `ESP_ERR_NO_MEM (0x101)` | 内存分配失败 |
| `WINK_ERR_INVALID_STATE (-16)` | `ESP_ERR_INVALID_STATE (0x103)` | 非法状态 |
| `WINK_ERR_NOT_FOUND (-18)` | `ESP_ERR_NOT_FOUND (0x105)` | 未找到目标项 |
| *其他未显式枚举负错误码* | `ESP_FAIL (-1)` | 未分类通用错误 |

---

## 4. 验证引擎多域断言规范 (Matcher Contract)

在 AFG-Engine 与 Loop 测试套件中，所有错误断言必须显式声明 `domain` 与预期的 `symbol` / `raw_value`：

```yaml
assert_error:
  domain: esp_err
  symbol: ESP_ERR_TIMEOUT
  raw_value: 0x107

# 或针对 PAL 原生
assert_error:
  domain: wink_status
  symbol: WINK_ERR_TIMEOUT
  raw_value: -2
```

断言执行器在判决时：
1. 校验值是否匹配对应域的符号；
2. 若 `domain: esp_err` 匹配到了负数（除 `-1` 外），抛出 `ERROR_DOMAIN_MISMATCH` 拦截假绿；
3. 若 `domain: wink_status` 匹配到了正数，抛出 `ERROR_DOMAIN_MISMATCH` 拦截假绿。
