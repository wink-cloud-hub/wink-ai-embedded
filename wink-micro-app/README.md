# wink-micro-app — sample applications

Audience for WinkMicroOS App samples (A+B primary, C supported):

| Tier | Who | What you edit | Golden sample |
|------|-----|---------------|---------------|
| **L0** | A low-code / visual | Pins + intent in UI (generated C) | *(future — P2 intent layer)* |
| **L1** | B AI / beginner C | `wink-app.json` + Role API business C | **`oled_dashboard`** |
| **L2** | C embedded expert | Full hooks, `&dev`, `dal_*`, hand `device_tree` | `avoidance_car`, `dual_task_demo` |
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
Do **not** teach pin numbers, `pal_*`, or `dal_ssd1306_*` on this path.
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
- `debounce_ms` is optional and defaults to `20`; `0` disables debounce
  (expert only). Both backends honour it.
- `wake_from_sleep` is only meaningful with `gpio_irq` on ESP32.
- Opt in to strict CI (fail-fast instead of degrade) with
  `-DWINK_BUTTON_IRQ_STRICT=1`.

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
| `devkitc_smoke/` | QA | S1–S11 board selftests |
| `unisim_smoke/` | Fixture | Wasm JS import smoke |
| `resource_conflict/` | QA | Resource conflict demo (special entry) |
| `common/` | — | Forwarding headers / sample helpers |
| `sample_common.cmake` | — | Shared includes, link helpers, codegen |

See also: `docs/design/03-app-codegen/01-app-business-logic.md`.
