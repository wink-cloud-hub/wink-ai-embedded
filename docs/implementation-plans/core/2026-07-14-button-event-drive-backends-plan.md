# Button event_drive backends (ADR-0031) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `embedded-best-practice` for C/BAL/DAL edits; prefer `subagent-driven-development` or `executing-plans` task-by-task. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Let `wink-app.json` select `soft_poll` vs `gpio_irq` behind unchanged L1 `{name}_enable_events()`, with BAL API `wink_button_events_*`, host/wasm default degrade+warn, and `debounce_ms` as a first-class schema field.

**Architecture:** Codegen bakes JSON into `wink_button_event_config_t` inside Role `enable_events()`. BAL `wink_button_events_start/stop` owns both backends; `wink_button_helper_*` becomes a thin deprecated soft_poll wrapper. gpio_irq uses short GPIO ISR → debounce → `wink_event_post`. Unsupported IRQ targets degrade to soft_poll + `wink_trace_warn`.

**Tech Stack:** Python codegen (Jinja/driver plugins), BAL C, DAL button, host e2e / golden tests, CMake.

## Global Constraints

- ADR-0031 Accepted decisions are binding (degrade+warn default; `debounce_ms` first-class; `wink_button_events_*` primary).
- ADR-0018: App/BAL public headers must not expose `pal_irq_advanced.h`; GPIO IRQ stays inside DAL/BAL .c.
- ADR-0002 / 0003 / 0012: dual-target App C unchanged; degrade must be observable (trace warn code, testable).
- ADR-0023: JSON = static capability only; App still calls `enable_events()` explicitly (no init auto-start).
- L1 sample `oled_dashboard` business C must keep working without edits beyond optional JSON docs.
- Do not git commit unless the user asks.
- Prefer atomic commits per task when user requests commits.

## File map

| File | Responsibility |
|------|----------------|
| `tools/codegen/drivers/button.py` | Parse/validate `event_drive`, `debounce_ms`; emit enable_events → `wink_button_events_start` |
| `tools/codegen/app_codegen.py` | role verb list parity if mirrored |
| `tools/codegen/templates/device_tree_api.md.j2` | Document event_drive fields |
| `tools/codegen/tests/golden_*` | Expected headers/md |
| `bal/include/input/wink_button_events.h` | **New** public BAL API |
| `bal/src/wink_button_events.c` | **New** (or split poll/irq .c) dispatcher + soft path move |
| `bal/src/wink_button_events_irq.c` | **New** (S3) ESP32 gpio_irq path |
| `bal/include/input/wink_button_helper.h` + `.c` | Thin wrap → `wink_button_events_*` |
| `bal/CMakeLists.txt` | Add new sources |
| `trace` / warn code header | `WINK_WARN_BUTTON_IRQ_DEGRADED` (pick free code) |
| `test/` or `bal` unit / host e2e | Degrade + soft_poll regression |
| `oled_dashboard/wink-app.json` | Optional explicit soft_poll + debounce_ms docs |
| docs already Accepted | Touch only if implementation reveals gaps |

---

## 1. Metadata

| 字段 | 内容 |
|------|------|
| **计划编号** | `PLAN-20260714-BTN-EVENT-DRIVE` |
| **创建日期** | 2026-07-14 |
| **计划状态** | ✅ 已完成（2026-07-14）— S1–S5 全部落地，验证通过 |
| **优先级** | 🟡 P1 |
| **关联 ADR** | [ADR-0031](../../decisions/core/0031-button-event-drive-config.md) |
| **关联技术设计** | [2026-07-14-button-event-drive-backends.md](../../tech-designs/core/2026-07-14-button-event-drive-backends.md) |
| **关联设计规范** | [01-app-business-logic.md](../../design/03-app-codegen/01-app-business-logic.md) |
| **目标平台** | host（必验）、wasm（降级）、ESP32（S3/S5） |
| **所需技能** | `embedded-best-practice`、`writing-plans` / `executing-plans` |

### Acceptance export

| 指标 | 通过标准 |
|------|----------|
| Codegen golden | `python wink-micro-os/tools/codegen/tests/test_golden.py` OK |
| Codegen negative | soft_poll 缺 `auto_poll_ms` / gpio_irq 缺 pin → exit ≠ 0 |
| oled e2e | `app_oled_dashboard_e2e` PASS（soft_poll） |
| Degrade test | cfg gpio_irq on host → warn recorded + events still work via soft_poll |
| No App IRQ includes | BAL public headers: zero `pal_irq*.h` |

---

### Task 1 (S1): Codegen schema — `event_drive` + `debounce_ms` ✅ 已完成

**Files:**
- Modify: `wink-micro-os/tools/codegen/drivers/button.py`
- Modify: `wink-micro-os/tools/codegen/templates/device_tree_api.md.j2`
- Modify: golden expected under `tools/codegen/tests/golden_expected/` (+ multi if needed)
- Test: `tools/codegen/tests/test_golden.py` + new negative cases in same file or `test_button_event_drive_validate.py`

**Interfaces:**
- Produces (still soft_poll until Task 2):  
  `enable_events` may keep calling `wink_button_helper_start` **or** already emit `wink_button_events_start` with a static cfg if Task 2 lands same PR — prefer **still helper in S1**, switch wrapper in S2 to avoid half-linked trees.
- Produces macros: `{NAME}_DEBOUNCE_MS`, `{NAME}_EVENT_DRIVE_SOFT_POLL` / `_GPIO_IRQ`

- [ ] **Step 1:** Add validation in `ButtonDriver` (or codegen pre-pass):

```python
drive = spec.get("event_drive", "soft_poll")
if drive not in ("soft_poll", "gpio_irq"):
    raise SystemExit(f"invalid event_drive: {drive}")
if drive == "soft_poll" and spec.get("auto_poll_ms") is None:
    raise SystemExit("soft_poll requires auto_poll_ms")
if drive == "gpio_irq" and "pin" not in spec and "use_onboard" not in spec:
    raise SystemExit("gpio_irq requires pin")
debounce = int(spec.get("debounce_ms", 20))
```

- [ ] **Step 2:** `render_config_macros` emit `#define {NAME}_DEBOUNCE_MS {n}u` and drive selectors.
- [ ] **Step 3:** Write failing golden / unit tests for missing `auto_poll_ms`; run → fail; implement → pass.
- [ ] **Step 4:** Regenerate goldens; `python tools/codegen/tests/test_golden.py` → OK.
- [ ] **Step 5:** (If committing) `docs:` or `feat(codegen): validate button event_drive and debounce_ms`

---

### Task 2 (S2): BAL `wink_button_events_*` + helper thin wrap ✅ 已完成

**Files:**
- Create: `wink-micro-os/bal/include/input/wink_button_events.h`
- Create: `wink-micro-os/bal/src/wink_button_events.c` (move soft_poll body from helper)
- Modify: `wink_button_helper.h/.c` → call events API
- Modify: `bal/CMakeLists.txt`
- Modify: `button.py` `enable_events` / `disable_events` to call:

```c
WINK_WARN_UNUSED_RESULT static inline wink_status_t {name}_enable_events(void) {
    static const wink_button_event_config_t cfg = {
        .drive = WINK_BUTTON_DRIVE_SOFT_POLL, /* or GPIO_IRQ from JSON */
        .auto_poll_ms = {ms}u,
        .debounce_ms = {deb}u,
        .wake_from_sleep = false,
    };
    return wink_button_events_start(&{name}, &cfg);
}
static inline void {name}_disable_events(void) {
    wink_button_events_stop(&{name});
}
```

- Include in generated `device_tree.h`: `#include "wink_button_events.h"` (via `get_role_headers`)

**Interfaces:**
- Produces:

```c
typedef enum {
    WINK_BUTTON_DRIVE_SOFT_POLL = 0,
    WINK_BUTTON_DRIVE_GPIO_IRQ  = 1,
} wink_button_event_drive_t;

typedef struct {
    wink_button_event_drive_t drive;
    uint32_t auto_poll_ms;
    uint32_t debounce_ms;
    bool wake_from_sleep;
} wink_button_event_config_t;

wink_status_t wink_button_events_start(dal_button_t *btn,
                                       const wink_button_event_config_t *cfg);
void wink_button_events_stop(dal_button_t *btn);
```

- gpio_irq branch in S2: return path that calls soft_poll after warn **stub** OR `WINK_ERR_UNSUPPORTED` until S3/S4 — **prefer call degrade helper that always soft_polls on non-ESP** so oled never breaks (implement degrade frame in S2, fill IRQ in S3).

- [ ] **Step 1:** Add header + soft_poll implementation (cut/paste from `wink_button_helper.c` slot logic); keep event_dispatch posting `WINK_EVENT_BUTTON_*`.
- [ ] **Step 2:** `wink_button_helper_start(btn, ms)` → fill cfg soft_poll + debounce default 20 + `wink_button_events_start`.
- [ ] **Step 3:** Update codegen Role wrappers + golden.
- [ ] **Step 4:** Build `app_oled_dashboard_e2e` + ctest PASS.
- [ ] **Step 5:** Apply debounce to DAL if API exists (`dal_button_set_debounce_ms` or config field); if none, document “S2 stores cfg.debounce_ms for IRQ path; soft_poll uses existing DAL debounce until DAL wiring in same task”.

**DAL check before coding:** grep `debounce` in `dal_button.*`. Wire if present; else add minimal setter in same task if low risk.

---

### Task 3 (S3): gpio_irq backend (ESP32 / capable targets) ✅ 已完成

**Files:**
- Create: `bal/src/wink_button_events_irq.c` (or section in events.c gated by target)
- Modify: `dal/src/input/dal_button.c` as needed for shared GPIO ISR dispatch (event vs counter)
- Test: ESP32 unit or host inject if available; at minimum compile ESP32 app with `event_drive: gpio_irq`

**Interfaces:**
- Consumes: `wink_button_events_start` drive==GPIO_IRQ
- ISR: set pending / arm debounce timer only; post from debounce completion (task/timer context), not heavy work in ISR

- [ ] **Step 1:** Sketch ISR → debounce → `wink_event_post` on paper against ADR-0018; no App headers in ISR TU beyond DAL/PAL allowed set.
- [ ] **Step 2:** Implement; `wake_from_sleep` may be no-op stub with TODO if deep-sleep API not ready (document).
- [ ] **Step 3:** Safety review (embedded-best-practice high risk: ISR path) — checklist phases for IRQ/concurrency.
- [ ] **Step 4:** Manual or HIL smoke optional; host builds must not link ESP-only symbols (stub + degrade).

---

### Task 4 (S4): host/wasm degrade + testable warn ✅ 已完成

**Files:**
- Modify: `wink_button_events_start` capability probe
- Modify: `wink_trace.h` / warn code registry — add e.g. `WINK_WARN_BUTTON_IRQ_DEGRADED` (choose unused code; document in trace header)
- Test: new host test `test_button_events_irq_degrade.c` or extend existing BAL test

**Algorithm (mandatory):**

```text
if cfg->drive == GPIO_IRQ && !wink_button_events_irq_supported():
    wink_trace_warn(WINK_WARN_BUTTON_IRQ_DEGRADED);
    effective = soft_poll;
    poll_ms = cfg->auto_poll_ms ? cfg->auto_poll_ms : max(cfg->debounce_ms, 10u);
    start soft_poll(effective)
```

- [ ] **Step 1:** `wink_button_events_irq_supported()` → false on host/wasm, true on ESP32 (with feature).
- [ ] **Step 2:** Failing test: start with gpio_irq cfg on host → expect warn count++ and button events via poll simulation.
- [ ] **Step 3:** Optional `-DWINK_BUTTON_IRQ_STRICT=1` → return `WINK_ERR_UNSUPPORTED` instead of degrade (test both).
- [ ] **Step 4:** oled e2e still PASS.

---

### Task 5 (S5): Docs sample + README polish ✅ 已完成

**Files:**
- Modify: `wink-micro-app/README.md` (battery JSON snippet)
- Modify: `oled_dashboard/docs` or README note (optional explicit `debounce_ms: 20`)
- Modify: tech-design status → Implemented when done
- Optional: `wink-micro-app/oled_dashboard/wink-app.json` add `"debounce_ms": 20` only (behavior-neutral)

- [ ] **Step 1:** Document soft_poll vs gpio_irq JSON side-by-side; App line remains `enable_events()`.
- [ ] **Step 2:** Mark PLAN status ✅; ADR follow-up checklist update if needed.
- [ ] **Step 3:** Final verification: golden + oled e2e + degrade test.

---

## Out of scope

- Implicit `enable_events` inside `wink_device_tree_init`
- Non-button `event_drive` (ultrasonic completion, etc.)
- Replacing hard real-time control loops with event queue
- JS edge injection for wasm gpio_irq fidelity (future)

## Risk notes for implementers

- Shared pin with `isr_counter`: unify DAL GPIO ISR multiplex; do not double-`enable_interrupt`.
- Moving code from helper → events: update all `#include` and CMake; search `wink_button_helper_start` usages (`devkitc_smoke` still OK via wrapper).
- Keep Role header include list: `wink_button_events.h` not `pal_*.h`.

---

## Execution handoff

Plan saved to `docs/implementation-plans/core/2026-07-14-button-event-drive-backends-plan.md`.

**Two execution options:**

1. **Subagent-Driven (recommended)** — fresh subagent per Task 1–5, review between tasks  
2. **Inline Execution** — execute in this session with `executing-plans`, checkpoint after each Task  

Which approach?

