# 按键事件后端（soft_poll / gpio_irq）技术设计规格

| 项 | 内容 |
|---|---|
| **文档版本** | v1.1 |
| **设计日期** | 2026-07-14 |
| **状态** | ✅ **Implemented**（2026-07-14）— ADR-0031 决议全部落地：`event_drive` / `debounce_ms` schema、`wink_button_events_*` API + soft_poll 迁入、ESP32 `gpio_irq` 后端、host/wasm 降级 + `WINK_WARN_BUTTON_IRQ_DEGRADED`、`wink_button_helper_*` 变薄包装 |
| **关联 ADR** | [ADR-0031](../../decisions/core/0031-button-event-drive-config.md)（**Accepted**） |
| **关联设计规范** | [03-app-codegen/01-app-business-logic.md](../../design/03-app-codegen/01-app-business-logic.md)、[02-project-manifest-schema.md](../../design/03-app-codegen/02-project-manifest-schema.md) |
| **关联样板** | `wink-micro-app/oled_dashboard`（L1：保持 soft_poll + `enable_events`） |

---

## 1. 目标

在 **不修改 App 业务 C** 的前提下，仅通过 `wink-app.json` 在两种按键事件生产者之间切换：

| `event_drive` | 行为摘要 | 典型场景 |
|---|---|---|
| `soft_poll`（默认） | soft_timer 按 `auto_poll_ms` 调用 `dal_button_poll` → 边沿 → `wink_event_post` | 常供电、演示、host/wasm |
| `gpio_irq` | GPIO 边沿 ISR（极短）→ 防抖 → `wink_event_post` | 低功耗唤醒、更快边沿响应 |

稳定 App 面：

```c
WINK_TRY(user_button_enable_events());
/* ... */
void app_on_event(const wink_event_t *evt) { /* 不变 */ }
```

---

## 2. JSON Schema（button）

```json
{
  "type": "button",
  "pin": 10,
  "active_low": true,
  "event_drive": "soft_poll",
  "auto_poll_ms": 10,
  "debounce_ms": 20,
  "wake_from_sleep": false,
  "long_press_ms": 3000
}
```

| 字段 | 类型 | 默认 | 约束 |
|---|---|---|---|
| `event_drive` | enum | `soft_poll` | `soft_poll` \| `gpio_irq` |
| `auto_poll_ms` | uint | — | **soft_poll 必填**；gpio_irq 下忽略（codegen WARN） |
| `debounce_ms` | uint | **20** | **schema 一等字段**；两种后端共享。`0` = 关闭防抖（专家） |
| `wake_from_sleep` | bool | `false` | 仅 `gpio_irq` 有意义；真机深睡唤醒提示 |

Codegen 静态校验（构建失败优先于静默）：

1. `event_drive=soft_poll` 且无 `auto_poll_ms` → **ERROR**  
2. `event_drive=gpio_irq` 且无有效 `pin` → **ERROR**  
3. `wake_from_sleep=true` 且 `event_drive!=gpio_irq` → **ERROR**  
4. `gpio_irq` + `auto_poll_ms` → **WARN**（多余字段）

---

## 3. Role / Codegen 契约

已有 L1 API（保持）：

```c
wink_status_t {name}_enable_events(void);
void          {name}_disable_events(void);
```

生成规则演进：

- **soft_poll**：`enable_events` 内联调用 `wink_button_helper_start(&dev, AUTO_POLL_MS)`（现状）。  
- **gpio_irq**：`enable_events` 内联调用新入口，例如  
  `wink_button_helper_start_irq(&dev, debounce_ms, wake_from_sleep)`  
  （具体符号名实施时可定为 `wink_button_events_start` 统一入口 + drive 枚举，见 §4）。

L2 保留：`start_auto_poll(uint32_t)` / `stop_auto_poll`（显式周期覆盖）。

导出宏建议：

```c
#define USER_BUTTON_EVENT_DRIVE_SOFT_POLL 1
/* or */
#define USER_BUTTON_EVENT_DRIVE_GPIO_IRQ  1
#define USER_BUTTON_DEBOUNCE_MS 20u
```

便于 `#if` 诊断；业务代码仍应只调 `enable_events`。

---

## 4. BAL Helper 架构

### 4.1 统一启动（推荐形态）

```c
typedef enum {
    WINK_BUTTON_DRIVE_SOFT_POLL = 0,
    WINK_BUTTON_DRIVE_GPIO_IRQ  = 1,
} wink_button_event_drive_t;

typedef struct {
    wink_button_event_drive_t drive;
    uint32_t auto_poll_ms;     /* soft_poll */
    uint32_t debounce_ms;
    bool     wake_from_sleep;  /* gpio_irq */
} wink_button_event_config_t;

wink_status_t wink_button_events_start(dal_button_t *btn,
                                       const wink_button_event_config_t *cfg);
void          wink_button_events_stop(dal_button_t *btn);
```

`enable_events` 生成代码把 JSON 烘焙进 `cfg` 字面量后调用上述 API。  
现有 `wink_button_helper_start(btn, poll_ms)` 可保留为 soft_poll 薄包装（兼容 L2/旧样板）。

### 4.2 soft_poll 路径（现状保留）

```text
periodic LIGHT → dal_button_poll → (已有) helper 回调 → wink_event_post
```

### 4.3 gpio_irq 路径（新增）

```text
pal_gpio 边沿 ISR（极短，仅置 pending / 推防抖 deadline）
        → debounce soft_timer 或 DAL 内防抖状态机
        → 确认边沿
        → wink_event_post(PRESSED|RELEASED|LONG_PRESS)
```

约束（写入头文件 `@warning`）：

- ISR 禁止：阻塞、日志洪水、DAL 业务、直接调用 App；  
- 与 ADR-0018：App/BAL **不**暴露 `pal_irq_advanced`；GPIO 中断封装在 DAL button / helper 内部；  
- 与现有 `dal_button_enable_isr_counter`：计数 ISR 与事件 ISR **可共用底层 GPIO 注册**，但事件路径必须走防抖后再 post（计数路径可保持「只计数」语义，文档写清差异）。

### 4.4 disable / deinit

`disable_events` / `wink_device_tree_deinit` 必须对称：停 soft_timer **或** 卸 GPIO ISR + 停防抖定时，避免 UAF（对齐 deinit 契约惯例）。

---

## 5. 双 Target / 仿真策略

| Target | soft_poll | gpio_irq |
|---|---|---|
| ESP32 | 完整 | 完整（含可选深睡唤醒配置） |
| host | 完整 | **默认降级 soft_poll + `wink_trace_warn`**（可断言） |
| wasm | 完整 | **同上默认降级**（JS 边沿注入可作后续增强，非默认） |

**诚实降级算法（`wink_button_events_start` / enable 时，Accepted）：**

```text
if cfg.drive == GPIO_IRQ and !target_supports_button_irq:
    wink_trace_warn(BUTTON_IRQ_DEGRADED)   # 必须可被测试观测
    cfg.drive = SOFT_POLL
    if auto_poll_ms missing: use max(debounce_ms, 10) as degraded poll period
    start soft_poll path
```

可选 CMake `-DWINK_BUTTON_IRQ_STRICT=1`：不支持则返回 `WINK_ERR_UNSUPPORTED`（电池产品 CI，非默认）。

---

## 6. App / 文档心智模型

```text
JSON:  我怎么采集边沿？（soft_poll / gpio_irq）
C:     enable_events() → 打开生产者
C:     on_event         → 唯一消费者
```

`oled_dashboard`：**不改业务**；JSON 默认继续省略 `event_drive`（= soft_poll）。  
文档示例增加「电池板」JSON 片段（`gpio_irq` + `wake_from_sleep`），仍共用同一 `app_callbacks.c`。

---

## 7. 测试计划（实施时）

| 用例 | 期望 |
|---|---|
| codegen：soft_poll 缺 auto_poll_ms | 非 0 退出 |
| codegen：gpio_irq 缺 pin | 非 0 退出 |
| golden：两种 drive 生成不同 enable 内联体 | 字节级或符号级差分 |
| host e2e oled：默认 soft_poll | 既有 PASS |
| host：gpio_irq 配置 | 降级 warn + 行为仍出 PRESSED，或注入边沿 PASS |
| helper：start 两次 | INVALID_STATE；stop 后可再 start |
| ESP32（后置）：gpio_irq 按下 | 队列收到事件；可选深睡唤醒冒烟 |

---

## 8. 实施切片（建议）

| 切片 | 内容 | 依赖 |
|---|---|---|
| S0 | ✅ ADR-0031 Accepted + schema / 业务规范回写 | Owner |
| S1 | codegen 解析/校验 `event_drive` + `debounce_ms`；soft_poll 行为不变 | S0 |
| S2 | 引入 `wink_button_events_start/stop`；soft 路径迁入；helper 变薄包装 | S1 |
| S3 | gpio_irq 真机路径 + ISR/防抖 | S2、ADR-0018 |
| S4 | host/wasm 默认降级 + warn 可测 | S3 |
| S5 | 文档 / 电池 JSON 示例；可选 STRICT | S4 |

**本设计不包含**：`device_tree_init` 内隐式 `enable_events`（另案）；非 button 外设的 `event_drive` 泛化（超声波等用「完成中断」专题扩展）。

---

## 9. 风险

| 风险 | 缓解 |
|---|---|
| gpio_irq 防抖不充分导致事件洪水 | 强制 debounce_ms；helper 内限流/合批可后续加 |
| 与 isr_counter 争用同一 pin ISR | DAL 统一 GPIO ISR 入口，分发计数 vs 事件 |
| 仿真静默成功骗过 CI | 默认 warn；STRICT 模式可选 |
| Helper 复杂度膨胀 | 双后端分文件：`button_events_poll.c` / `button_events_irq.c` |

---

## 10. Owner 决议（2026-07-14 Accepted）

| # | 问题 | 决议 |
|---|---|---|
| 1 | host/wasm × `gpio_irq` | **默认降级 soft_poll + warn**；STRICT 为可选构建开关 |
| 2 | `debounce_ms` | **schema 一等字段**，默认 20；`0` = 关防抖 |
| 3 | BAL API 命名 | **`wink_button_events_*` 为主**；`wink_button_helper_*` thin deprecated 过渡 |

开放问题已关闭。实施计划：[implementation-plans/2026-07-14-button-event-drive-backends-plan.md](../../implementation-plans/core/2026-07-14-button-event-drive-backends-plan.md)。

---

## 11. 实施落地记录（2026-07-14）

本设计已在 `feat/sdk-phase2-binary` 分支完成，切片 S1–S5 全部落地：

| 切片 | 落地内容 | 关键文件 |
|---|---|---|
| S1 | Codegen 验证 `event_drive` / `debounce_ms`；宏 `{NAME}_DEBOUNCE_MS` / `_EVENT_DRIVE_SOFT_POLL` / `_EVENT_DRIVE_GPIO_IRQ` | `tools/codegen/drivers/button.py`、`tools/codegen/tests/test_button_event_drive_validate.py` |
| S2 | 新 BAL API `wink_button_events_start/stop` + `wink_button_event_config_t`；soft_poll 路径迁入；`wink_button_helper_*` 变薄包装 | `bal/include/input/wink_button_events.h`、`bal/src/wink_button_events.c`、`bal/src/wink_button_helper.c` |
| S3 | ESP32 `gpio_irq` 后端（短 ISR → 防抖 → `wink_event_post`）；主机侧编译为不支持存根 | `bal/src/wink_button_events_irq.c` |
| S4 | host/wasm 默认降级 + `wink_trace_warn(WINK_WARN_BUTTON_IRQ_DEGRADED)`；`-DWINK_BUTTON_IRQ_STRICT=1` 硬失败模式；两条测试路径 | `test/test_button_events_irq_degrade.c`、`runtime/include/wink_fault.h` |
| S5 | 文档：`wink-micro-app/README.md` 增补 soft_poll vs gpio_irq JSON 并列示例；`oled_dashboard/wink-app.json` 显式 `debounce_ms: 20`；本文档状态置 Implemented | `wink-micro-app/README.md`、`wink-micro-app/oled_dashboard/wink-app.json` |

稳定 App 面（`{name}_enable_events()` + `on_event`）未改动；`oled_dashboard` 业务 C 无需任何修改。

