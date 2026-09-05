# 3.3 WinkMicroOS Kernel Directory Architecture Design

<!-- i18n-meta
source: docs/zh/design/02-wink-micro-os/03-directory-architecture.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

> This document is the **authoritative living specification for the WinkMicroOS kernel (`wink-micro-os/`) directory layout**. Any directory restructuring must adhere to this document and preserve bidirectional synchronization (SSOT closed-loop per [docs-adr.md §2](../../../.claude/rules/docs-adr.md)).

---

## 🎯 Getting Started: Directory Role Quick Reference Table

| Directory | Role | Maintainer | Modification Frequency | One-Line Summary |
|---|---|---|---|---|
| `wink-micro-os/` | SDK Platform Kernel | Library Developers | Adding hardware / bug fixes | Core OS: PAL/DAL/BAL/runtime/trace across all chips |
| `wink-micro-app/` | Application Logic | User Developers | Every new feature | Business logic: Obstacle avoidance, dashboard, etc. |
| `esp32_firmware/` | ESP32 Build & Packager | Rarely modified | Toolchain / chip config updates | Compiles wink-micro-os into `.bin` firmware |
| `@wink-ai/unisim` | Browser Simulation Engine | Rarely modified | Simulation engine updates | Compiles wink-micro-os to `.wasm` running in browsers |
| `test/` | Unit Tests | Library Developers | Adding features / tests | Automated testing on PC host |

---

## 1. Architectural Positioning

**WinkMicroOS = A Lightweight Cooperative Runtime & Application Framework (OS Runtime/Framework)** serving two primary consumers:
- **App (Top Layer)**: Generated pin connections + state machines, **AI Generated**, per-project, injected.
- **BAL (Utility Layer)**: Static component library (`wink-micro-os/bal/`) containing physical extensions (`input`/`output`/`sensor`/`actuator`/`display`/`comm`), pure algorithms (`math`), and closed-loop orchestration (`control`).

Both link only against the kernel's **public API surface** (see §6). WinkMicroOS self-contains: **PAL + DAL + BAL + runtime + trace + targets + test**.

> 💡 **Architectural Note**: This runtime is a cooperative event loop without MMU/MPU memory isolation or IPC processes. "Kernel" denotes this lightweight embedded runtime skeleton.

---

## 2. 5-Layer Model & Repository Boundaries

| Layer | Responsibility | Ownership | Dependency Direction |
|---|---|---|---|
| **App (Top)** | Wire routing + state machine, AI-generated | External (Generated/Injected) | $\downarrow$ Calls BAL + DAL |
| **BAL (Utilities)** | Physical extensions + algorithms + control loops | **wink-micro-os** (`bal/`) | $\downarrow$ Calls DAL |
| **DAL** | Atomic peripheral drivers (Servo, Ultrasonic...) | **wink-micro-os** | $\downarrow$ Calls PAL |
| **PAL** | HAL (Hardware abstraction) + OSAL (OS abstraction) | **wink-micro-os** | $\downarrow$ Calls target bindings |
| **runtime / trace**| Lifecycle + main event loop / Golden Trace | **wink-micro-os** | runtime $\downarrow$ calls trace, PAL |
| **Targets** | Per-platform PAL bindings and entrypoints | **wink-micro-os** | Bound to concrete hardware |

---

## 3. Architectural Principles & Rationale

| Principle | Embodiment in Architecture |
|---|---|
| **Ports & Adapters (Hexagonal)** | `targets/` = Adapters (Ports for PAL/runtime); `pal/dal/runtime/trace` = Domain core. Host is elevated to a first-class target. |
| **Stable Dependencies Principle** | Dependencies stabilize monotonically bottom-up: `pal` (INTERFACE, most stable) $\leftarrow$ `dal` $\leftarrow$ `runtime` $\leftarrow$ App/BAL. |
| **Screaming Architecture** | `trace/` is an independent top-level peer layer per system overview. |
| **Conway's Boundary Alignment** | Codegen templates/tools belong to build/frontend systems, not collocated with kernel core. |
| **YAGNI + Rule of Three** | Flat per-bus headers split on arrival of the 4th major communication bus. |

---

## 4. Complete Directory Tree (with Library Type Annotations)

```text
wink-micro-os/
├── CMakeLists.txt              # Top-level: TARGET_PLATFORM routing & library aggregation
├── README.md
│
├── pal/                        # [libpal = INTERFACE library] Pure contract headers, no .c
│   ├── CMakeLists.txt          #   target_include_directories(PUBLIC include include/hal include/osal)
│   └── include/                #   Hardware contract surface (Internal; BAL/App prohibited from raw HAL)
│       ├── pal.h               #   Aggregate header (includes hal + osal + services + status)
│       ├── wink_status.h       #   Unified error codes (SSOT)
│       ├── pal_log.h           #   Tiered logging (E/W/I/D, LOG_TAG, ISR-safe routing)
│       ├── pal_resource.h      #   Resource allocation governance (GPIO/PWM/I2C conflict detection)
│       ├── pal_storage.h       #   Non-volatile storage service (NVS/RAM key-value pairs)
│       ├── pal_irq.h           #   Interrupt abstraction core (Unified priorities, critical sections)
│       ├── pal_irq_advanced.h  #   Advanced interrupt features (SMP sync, strict critical sections)
│       ├── hal/                #   Hardware abstraction sub-layer (Physical bus primitives)
│       │   ├── pal_hal.h       #   HAL primitive APIs (GPIO/PWM/I2C/pulse_in)
│       │   ├── pal_rmt.h       #   ESP32 RMT extensions
│       │   └── pal_pwm_router.h#   PWM pin router configurations
│       └── osal/               #   Operating system abstraction sub-layer
│           └── pal_osal.h      #   OSAL APIs (delay/tick/mutex/task)
│
├── dal/                        # [libdal.a = STATIC library] Pre-compilable, target-agnostic
│   ├── CMakeLists.txt          #   links pal(INTERFACE); unresolved pal_* symbols resolved at target link
│   ├── include/                #   Public API surface (BAL/App consumable)
│   │   ├── sensor/dal_ultrasonic.h
│   │   ├── actuator/dal_rc_servo.h
│   │   ├── input/dal_button.h
│   │   ├── output/dal_led.h
│   │   ├── display/dal_ssd1306.h
│   │   ├── comm/dal_gps.h
│   │   └── storage/dal_eeprom.h
│   └── src/                    #   Physical implementation with minimal bypass hooks
│
├── runtime/                    # [libwink_runtime.a = STATIC library] First-class OS runtime layer
│   ├── CMakeLists.txt          #   links pal; calls app_* hooks (external) + wink_trace_*
│   ├── include/
│   │   ├── wink_app.h          #   wink_app_callbacks_t definition + wink_app_delay_ms
│   │   └── wink_runtime.h      #   wink_runtime_run(callbacks) — Main OS event loop entrypoint
│   └── src/
│       └── wink_runtime.c      #   Lifecycle dispatcher (init callback -> loop callback)
│
├── trace/                      # [libwink_trace.a = STATIC library] Golden Trace peer layer
│   ├── CMakeLists.txt          #   links pal; consumed by runtime/dal/app
│   ├── include/
│   │   └── wink_trace.h        #   wink_trace_fault() / Event recording API
│   └── src/
│       └── wink_trace.c        #   Ring buffer & fault loggers
│
├── osal/                       # OS & execution environment adaptation layer
│   ├── CMakeLists.txt          #   OSAL source SSOT
│   ├── common/pal_osal_ringbuf.c
│   ├── baremetal/pal_osal_baremetal.c
│   ├── freertos_esp32/pal_osal_freertos_esp32.c
│   ├── wasm/pal_osal_wasm.c
│   └── host/pal_osal_host.c
│
├── targets/                    # Physical chip & hardware adaptation layer
│   ├── wasm/                   #   Browser simulation peripheral ports
│   │   ├── pal_hal_wasm.c
│   │   ├── pal_irq_wasm.c
│   │   ├── pal_storage_wasm.c
│   │   ├── pal_wasm_fault.c / pal_wasm_physical.c
│   │   ├── wasm_bridge.h       #   SSOT: extern js_pal_* / js_sim_* declarations
│   │   ├── wasm_entry.c
│   │   └── wink_sim_js.js
│   ├── esp32/                  #   Physical ESP32 hardware bindings
│   │   ├── pal_log_esp32.c
│   │   ├── pal_hal_gpio_esp32.c
│   │   ├── pal_hal_i2c_esp32.c
│   │   ├── pal_hal_pwm_esp32.c
│   │   ├── pal_hal_rmt_esp32.c
│   │   ├── pal_irq_esp32.c
│   │   └── esp32_entry.c
│   ├── host/                   #   First-class host target for PC testing & CLI simulation
│   │   ├── pal_hal_host.c
│   │   ├── pal_storage_host.c
│   │   └── pal_log_host.c
│   └── common/                 #   Cross-target shared simulation & physics math models
│       ├── include/sim_ctx.h / include/wink_sim_physical.h
│       └── src/pal_resource.c / src/wink_sim_physical.c
│
├── test/                       # Host testing harness (PC gcc + Unity)
│   ├── CMakeLists.txt
│   ├── unity/
│   ├── stubs/                  #   Test control & mock stubs
│   └── test_*.c
│
└── samples/                    # Sample application injection demonstrations
    └── avoidance_car/          #   app_callbacks.c + device_tree.{c,h} + board_config.c + wink_app.json
```

---

## 4.1 Sample/Injection-Point Application Structure & Responsibilities

| File Name | Role | Meaning & Purpose | Source / Maintainer |
|---|---|---|---|
| **`wink_app.json`** | Application Manifest | **Application metadata Single Source of Truth (SSOT)**.<br>Defines tick rates, target board, and peripheral pin mappings. | Low-code Workbench UI export |
| **`device_tree.h` / `device_tree.c`** | Static Device Tree | **Core of Zero Dynamic Memory Allocation**.<br>Statically instantiates global POD device structs without runtime malloc. | Wink Codegen Compiler |
| **`app_callbacks.c`** | Business Logic | **Business logic container**.<br>Implements `init()`, cooperative async coroutines in `loop()`, and `on_fault()` error handlers. | AI (LLM) Codegen |
| **`board_config.c`** | Board Pin Router | **Board-level physical pin route overrides (Optional)**.<br>Provides strong routing overrides (e.g. `pal_pwm_pin_map`). | Board Support Package / Systems Engineer |
| **`CMakeLists.txt`** | Build Description | **Defines compilation & linkage rules**.<br>Invokes Python scripts to parse `wink_app.json` and inject `wink_config.h`. | Build system template |

### 🛠️ Decoupling Principle: Compile-Time Target Static Routing
1. **Code Unawareness**: Application logic (`app_callbacks.c`) contains zero target-specific `#ifdef` directives.
2. **Build-Time Static Assembly**: CMake parses `wink_app.json` and orthogonally links `targets/<plat>/` with `osal/<variant>/` ([ADR-0041](../../decisions/core/0041-hal-osal-directory-orthogonality.md)).

---

## 4.2 Target Directory Naming Conventions (Living Spec)

### 🎯 Core Formula

#### 1. Platform Implementation Source File Naming
```text
pal_<domain>[_<detail>]_<plat>.c/h
```

| Segment | Meaning | Examples |
|---|---|---|
| `pal_` | Fixed PAL namespace prefix | Ensures global grep isolation without symbol clashes |
| `<domain>` | PAL Subsystem | `hal`, `osal`, `irq`, `resource`, `storage`, `atomic`, `rmt` |
| `[_<detail>]` | Optional Sub-Module | Distinguishes split translation units: `gpio`, `i2c`, `pwm`, `internal` |
| `<plat>` | Platform Tag (Always trailing) | Exact match with directory name: `esp32`, `wasm`, `host`, `baremetal` |

#### 2. PAL API Function Prefix Naming
```text
pal_<domain>_<action>()
```

| API Group | Function Prefix | Example |
|---|---|---|
| HAL Primitives | `pal_gpio_*` / `pal_pwm_*` / `pal_i2c_*` | `pal_gpio_init()`, `pal_pwm_set_duty()` |
| HAL Target Extensions | `pal_rmt_*` | `pal_rmt_ultrasonic_measure()` (ESP32 only) |
| OSAL | `pal_os_*` | `pal_os_sleep_ms()`, `pal_os_busy_wait_us()` |
| System Services | `pal_storage_*` / `pal_resource_*` | `pal_storage_read()`, `pal_resource_claim()` |
| Interrupts | `pal_irq_*` | `pal_irq_lock()`, `pal_gpio_enable_interrupt()` |

### 📋 Category Naming Matrix

| Category | Rule | esp32 Example | wasm Example | host Example |
|---|---|---|---|---|
| **Domain Aggregator** | `pal_<domain>_<plat>.c` | `pal_irq_esp32.c`, `pal_storage_esp32.c` | `pal_hal_wasm.c`, `pal_irq_wasm.c` | `pal_hal_host.c`, `pal_storage_host.c` |
| **Split Peripheral TU** | `pal_<domain>_<detail>_<plat>.c` | `pal_hal_gpio_esp32.c`, `pal_hal_i2c_esp32.c` | — | Aggregated in `pal_hal_host.c` |
| **Target Private Headers**| `pal_<domain>_<detail>_<plat>.h` | `pal_hal_internal_esp32.h` | `pal_wasm_internal.h` | — |

### ⚠️ Special Naming Rules (Non-PAL Modules)

| Category | Naming Pattern | Example | Rationale |
|---|---|---|---|
| **Target Entrypoint** | `<plat>_entry.c` | `esp32_entry.c`, `wasm_entry.c` | Contains `main()`/`app_main()`; startup glue rather than PAL driver |
| **Platform-Exclusive Subsystems** | `pal_<plat>_<detail>.c/h` | `pal_wasm_fault.c`, `pal_wasm_physical.c` | Capabilities exclusive to a single platform |
| **JS Bridge Layer** | `wasm_bridge.h` | `wasm_bridge.h` | C $\leftrightarrow$ JS ABI contract boundary |
| **JS Library Files** | `wink_sim_*.js` | `wink_sim_js.js`, `wink_sim_stub.js` | JavaScript runtime environment files |
| **Fiber Backends** | `sim_ctx_<toolchain>_fiber.c` | `sim_ctx_emscripten_fiber.c` | OSAL scheduling primitive backends |
| **OSAL Implementations** | `pal_osal_<variant>.c` | `pal_osal_freertos_esp32.c` | Decoupled OS dimensions |
| **Shared Cross-Target Code** | Module name without plat tag | `pal_resource.c`, `wink_sim_physical.c` | Target-agnostic shared algorithms |

### 🚫 Prohibited Anti-Patterns
1. **Platform tag in middle position**: `pal_hal_esp32_gpio.c` is invalid $\rightarrow$ use `pal_hal_gpio_esp32.c`.
2. **Platform tag mismatching directory name**: `pal_osal_bare.c` is invalid $\rightarrow$ use `pal_osal_baremetal.c`.
3. **Adding `pal_` to entrypoints**: `pal_esp32_entry.c` is invalid $\rightarrow$ use `esp32_entry.c`.
4. **Misnaming exclusive subsystems**: `pal_fault_wasm.c` is invalid $\rightarrow$ use `pal_wasm_fault.c`.
5. **Placing public headers inside targets**: Shared headers belong in `pal/include/` or `dal/include/`.
6. **Redundant double prefixes**: `pal_hal_gpio_init()` is invalid $\rightarrow$ use `pal_gpio_init()`.
7. **Placing DAL devices inside PAL**: Ultrasonic/Servos belong in DAL.

### 🔍 Quick Self-Inspection Checklist
- [ ] Filename matches `pal_<domain>[_<detail>]_<plat>.c/h`?
- [ ] `<plat>` is the final segment and matches directory name?
- [ ] Platform-exclusive features use `pal_<plat>_<detail>` syntax?
- [ ] CMakeLists.txt SRCS lists are updated in both locations?
- [ ] `#ifndef` include guards match uppercase filename?
- [ ] File-level `@file` doxygen annotations are updated?

---

## 5. CMake Library Dependency Graph

```text
        pal (INTERFACE, most stable)      ← Pure headers, no symbols
         ▲          ▲        ▲
         │          │        │
        dal    wink_runtime  wink_trace   ← STATIC, pal_* symbols resolved at target link
         │          │  │           ▲
         │          │  └──calls────┘
         │          │
         ▼          ▼
   [ BAL Utilities ]  [ App Generated ]
         └─────┬────┘  Links public include surface only
               ▼
        targets/<platform>   ← Implements pal_* + entry (main/app_main); final executable
```

---

## 6. Public API Surface & BAL Access Rules

| Header File | Owning Layer | BAL / App Accessible? |
|---|---|---|
| `dal/include/*.h` | DAL Semantic APIs | ✅ Primary entrypoint |
| `runtime/include/wink_app.h`, `wink_runtime.h` | OS Lifecycle / Scheduling | ✅ Registers `wink_app_callbacks_t`, calls `wink_app_delay_ms` |
| `trace/include/wink_trace.h` | Golden Trace | ✅ Calls `wink_trace_fault` in `on_fault` callback |
| `pal/include/wink_status.h` | Primitive Status Types | ✅ **Allowed Exception** (Whole-stack error code enum) |
| `pal/include/pal_hal.h`, `pal_osal.h` | Hardware Bus Contracts | ❌ **Forbidden** (Direct PAL access prohibited) |

### 6.1 Kernel Architectural Constraints
1. **Zero Dynamic Allocation**: Runtime `malloc`/`free`/`realloc` is prohibited across kernel and generated applications.
2. **Trace Isolation**: PAL and DAL drivers return `wink_status_t` and must never call `wink_trace_fault` directly.
3. **Standardized CMake Injection Interface**:
   - The top-level CMake uses `WINK_APP_DIR` cache variable as the standard injection interface to specify the AI-generated App source path.
   - **`wink_config.h`** is generated solely from `${WINK_APP_DIR}/wink-app.json`.
   - **Source SDK / M2 Consumption**: `WINK_SDK_PATH` points to the unzipped Source SDK root. Typical command:
     ```bash
     python $WINK_SDK_PATH/tools/wink.py build host --app /abs/path/to/my_app
     ```
   - **Binary SDK / M2 BINARY Consumption** (Phase 2): Consumes precompiled binary libraries. Invocation:
     ```bash
     python $WINK_SDK_PATH/tools/wink.py build host --sdk-mode binary --app /abs/path/to/my_app
     ```
4. **JS Bridge SSOT**: All extern JS symbols reside in `targets/wasm/wasm_bridge.h`.
5. **Wasm Build Safeguards**: Verified linkage against `wink_runtime`.
6. **Platform Configuration Encapsulation**: Clock trees and pin multiplexing are hidden inside target entrypoints.

---

## 7. Runtime / Trace Contracts & Target Entry Sequence

```text
wasm_entry.c::main()      ─┐
esp32_entry.c::app_main() ─┼──► wink_runtime_run(&callbacks)  [runtime layer, target-agnostic]
host test main()          ─┘         │
                                     ├─ callbacks.init()     (Once)
                                     └─ while(1){ callbacks.loop(); wink_app_delay_ms(tick); }
                                                │              │
                                           Calls dal_*         └─► pal_delay_ms()  ← Target implementation
                          callbacks.on_fault() ──► wink_trace_fault()  [trace layer]
```

---

## 8. Current State, Roadmap & PAL Header Splitting Rules

| Status | Content |
|---|---|
| **Delivered** | `pal/include/hal/` + `pal/include/osal/` subdirectories, Zephyr naming standards |
| **Existing** | `pal/include/hal/pal_hal.h`, `dal/{ultrasonic,servo}`, `targets/wasm/pal_hal_wasm.c` |
| **ADR-0003 Deliverables** | `pal/include/wink_status.h`, `test/{unity, stubs, test_*}` |
| **Architecture Additions** | `runtime/`, `trace/`, `samples/avoidance_car/`, `pal/` as INTERFACE library |
| **Roadmap** | Full ESP32 target completion, STM32 target, per-bus flat header splitting |

---

## 9. Migration Impact on ADR-0003 Implementation Plan

1. **`pal/` converted to INTERFACE library**: Eliminates private source additions for `wink_status.h`.
2. **`test/pal_host_stub.*` moved to `targets/host/`**: Split into `pal_hal_host.c` + `pal_osal_host.c`.
3. **`pal_hal_wasm.c` split into 4 modules**: `wasm_bridge.h` centralizes all JS bridge symbols.

---

## 10. Downstream Documentation Alignment (SSOT Closed Loop)

| Document | Action |
|---|---|
| [README.md](./README.md) | Update layer diagram with `runtime` and `trace` peer layers |
| **This Document** (`03-directory-architecture.md`) | Authoritative directory layout living specification |
| [04-runtime-and-trace.md](./04-runtime-and-trace.md) | Runtime lifecycle & trace contract specifications |
| [02-pal-platform-abstraction.md](./02-pal-platform-abstraction.md) §4.3 | Annotate `pal/` as INTERFACE contract library |
| [01-system-overview.md](../01-system-overall/01-system-overview.md) §3 | Synchronize kernel internal runtime/trace layer definitions |

---

## Appendix: Architectural Trade-Off Decision Log (A vs C)

| Decision Axis | Option A | Option C | Rationale | Adopted |
|---|---|---|---|---|
| ① PAL Header Layout | Flat, single `pal_hal.h` | Immediate `hal/` + `osal/` split | YAGNI + Rule of Three | **A** (Flat with bus split roadmap) |
| ② Trace Ownership | Nested in `runtime/` | Independent `trace/` peer layer | Screaming Architecture | **C** (Independent `trace/`) |
| ③ Codegen Assets | External (Samples only) | Internal tools collocation | Conway's Law + Registry SSOT | **A** (External) |

**Conclusion**: Architecture A* adopts Option A skeleton reinforced by Option C independent trace peer layer.
