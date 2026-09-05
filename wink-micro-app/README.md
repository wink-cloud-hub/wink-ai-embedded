# wink-micro-app — sample applications

Audience for WinkMicroOS App samples (A+B primary, C supported):

| Tier | Who | What you edit | Golden sample |
|------|-----|---------------|---------------|
| **L0** | A low-code / visual | Pins + intent in UI (generated C) | *(future — P2 intent layer)* |
| **L1** | B AI / beginner C | `wink-app.json` + Role API business C | **`oled_dashboard`** (button events), **`avoidance_car`** (distance events) |
| **L2** | C embedded expert | Full hooks, `&dev`, `dal_*`, hand `device_tree` | `dual_task_demo` |
| **Vendor** | 芯片厂商回归套件 | 官方原厂 C 源码零修改（“一行不改”）+ `wink-app.json` 声明外设拓扑 | `vendor_cms8s78xx_v202_led_4com_8seg` |
| **QA** | Platform CI / board bring-up | Not an onboarding template | `devkitc_smoke` |
| **Fixture** | Wasm smoke harness | Not an App tutorial | `unisim_smoke` |

## L1 default path (Role API)

```c
user_button_enable_events();   /* JSON auto_poll_ms → event queue */
status_led_activate();
status_oled_draw_text(0, 0, "HELLO");
status_oled_flush();
```

Prefer `{instance}_{verb}` wrappers from generated `device_tree.h`.
Do **not** teach pin numbers, `pal_*`, or `dal_mono_oled_*` on this path.
Button: use `enable_events` / `disable_events` (not `start_auto_poll`) at L1.

Build wiring for declarative apps:

```cmake
include(${CMAKE_CURRENT_LIST_DIR}/../sample_common.cmake)
wink_app_prepare_codegen(COMMENT_PREFIX "my_app")
```

## Button `event_drive` (soft_poll vs gpio_irq)

Buttons declare **how** their edges are captured via `event_drive` in
`wink-app.json`. The App C is identical for both backends — just call
`{name}_enable_events()` and consume `WINK_EVENT_BUTTON_*` in `app_on_event`
(see ADR-0031).

**soft_poll (default)** — works on all targets (host / wasm / ESP32):

```json
{
  "type": "button",
  "pin": 10,
  "active_low": true,
  "event_drive": "soft_poll",
  "auto_poll_ms": 10,
  "debounce_ms": 20
}
```

**gpio_irq** — GPIO edge interrupts + debounce on ESP32 (lower latency,
suitable for low-power / wake-on-press). On host/wasm it **degrades** to
soft_poll and raises `WINK_WARN_BUTTON_IRQ_DEGRADED` (observable via
`wink_warn_count()` / `wink_trace_warn`):

```json
{
  "type": "button",
  "pin": 10,
  "active_low": true,
  "event_drive": "gpio_irq",
  "debounce_ms": 20,
  "wake_from_sleep": false
}
```

App code is the same for both:

```c
WINK_TRY(user_button_enable_events());   /* JSON decides the backend */
/* app_on_event receives WINK_EVENT_BUTTON_PRESSED / _RELEASED / _LONG_PRESS */
```

Field notes:

- `event_drive` default is `"soft_poll"`; omit for the common case.
- `auto_poll_ms` is **required for soft_poll**; on ESP32 `gpio_irq` it is
  ignored (codegen WARN) and only used as the fallback poll period when
  `gpio_irq` degrades on host/wasm.
- `debounce_ms` is optional and defaults to `20`; `0` leaves the DAL
  default threshold (≈30 ms at 10 ms tick) unchanged (this branch does
  not support turning debounce off entirely — pass a small positive
  value for faster edges). Both backends honour it.
- `wake_from_sleep` is only meaningful with `gpio_irq` on ESP32.
- Opt in to strict CI (fail-fast instead of degrade) with
  `-DWINK_BUTTON_IRQ_STRICT=1`.

## Vendor Example Specification (原厂官方示例规范)

面向各芯片厂商（中微 Cmsemicon、乐鑫 Espressif、意法 ST、沁恒 WCH 等）的原厂参考代码，遵循以下规范：

### 1. 目录命名（全小写蛇形命名）
格式：`vendor_{chip_family}_{version}_{module_name}`
- **全小写 + 下划线**：规避 Windows 与 Linux CI 大小写敏感冲突，保证 Git 跨平台安全，命令行打字友好。
- **版本号去点号**：如 `v202`（或 `v2_0_2`），避免 CMake Target 与路径解析异常。
- **`vendor_` 前缀**：一目了然区分 Wink 原生低代码应用，便于 CI 脚本一键通过 `glob("vendor_*")` 批量发现与回归。

### 2. 元数据标准（`wink-app.json`）
所有原厂示例必须在 `wink-app.json` 中声明 `upstream` 作为单一事实来源（SSOT）：
```json
{
  "app_name": "vendor_cms8s78xx_v202_led_4com_8seg",
  "display_name": "CMS8S78xx V2.0.2 - LED 4COM_8SEG_LED",
  "category": "vendor_example",
  "upstream": {
    "vendor": "Cmsemicon",
    "family": "CMS8S",
    "chip": "CMS8S78xx",
    "sdk_version": "V2.0.2",
    "source_dir": "docs/vendors/Cmsemicon/CMS8S78xx_DemoCode_V2.0.2/CMS8S78xx_Example/Example/LED/4COM_8SEG_LED/code"
  }
}
```

### 3. 自动化门禁
- **“一行不改” 源码 Diff 门禁**：CI 自动比对 `upstream.source_dir` 与应用源码哈希，保证 100% 原厂原汁原味。
- **双端批量回归**：支持一键运行所有厂商示例的 WASM / 真机构建与无头测试。

## L2 expert escape hatch

`device_tree.h` still exports `extern dal_*_t` instances. Experts may call
`dal_*` named APIs, register actuators, or keep a hand-written `device_tree`
(ADR-0008). Samples in L2 demonstrate these patterns deliberately.

## Layout

| Directory | Tier | Notes |
|-----------|------|-------|
| `oled_dashboard/` | L1 | Button → LED + OLED; event-driven |
| `avoidance_car/` | L2 | Ultrasonic + servo; non-blocking measure FSM |
| `dual_task_demo/` | L2 | `wink_periodic_start_ex` expert mode |
| `vendor_cms8s78xx_v202_led_4com_8seg/` | Vendor | Official Cmsemicon CMS8S78xx V2.0.2 4COM-8SEG display regression |
| `devkitc_smoke/` | QA | S1–S11 board selftests |
| `unisim_smoke/` | Fixture | Wasm JS import smoke |
| `resource_conflict/` | QA | Resource conflict demo (special entry) |
| `common/` | — | INTERFACE: selftest include path + link `wink_bal`（无转发 shim） |
| `sample_common.cmake` | — | Shared includes, link libraries, codegen |

See also: `docs/design/03-app-codegen/01-app-business-logic.md`.
