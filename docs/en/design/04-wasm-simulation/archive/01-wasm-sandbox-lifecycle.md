# 4.1 Wasm Execution Sandbox Lifecycle, Web Worker Isolation & Asyncify Scheduling

<!-- i18n-meta
source: docs/zh/design/04-wasm-simulation/archive/01-wasm-sandbox-lifecycle.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

In a web simulation environment, running embedded main loops and task scheduling written in C safely, efficiently, and without freezing the UI is a core technical challenge. This document provides an in-depth analysis of the Wasm simulation sandbox lifecycle, Web Worker thread isolation, and asynchronous scheduling based on Emscripten Asyncify and Wasm Tables.

---

## 1. Web Worker Thread Isolation Architecture

### 1.1 Why Web Workers Are Mandatory
Embedded C main loops are typically non-terminating `while(1)` loops or preemptive scheduling loops managed by a FreeRTOS scheduler.
* If WebAssembly executes directly on the browser's main UI rendering thread, Wasm's high CPU consumption instantly monopolizes the single thread, preventing the browser from responding to user interactions (clicks, drags), freezing UI rendering, and triggering "Page Unresponsive" warnings.
* **Solution**: The platform utilizes a **Web Worker** to launch an isolated background thread running the Wasm sandbox, communicating low-frequency state updates and data with the main UI thread via `postMessage`, ensuring the main thread UI rendering remains consistently at 60 FPS.

### 1.2 Thread Interaction Architecture & Lifecycle Data Flow

```text
  [ Frontend Main UI Thread (Vue 3) ]               [ Web Worker Wasm Sandbox Thread ]
              │                                                 │
              │ ─── 1. POST: { type: 'start', wasmBytes } ───►  │
              │                                                 ├─ 2. WebAssembly.instantiate()
              │                                                 ├─ 3. Invoke main() -> Init OSAL Scheduler
              │                                                 │
              │                                                 │ (C logic calls pal_gpio_write)
              │ ◄── 4. POST: { type: 'pin_write', pin, lvl } ───┤
              │                                                 │
       (User clicks virtual button)                             │
              │ ─── 5. POST: { type: 'pin_input', pin, lvl } ──►│
              │                                                 ├─ 6. Overwrite Wasm virtual pin state
              │                                                 │
              │ ─── 7. POST: { type: 'pause' } ──────────────►  ├─ 8. Suspend Wasm execution coroutine
              │ ─── 9. POST: { type: 'stop' } ───────────────►  └─ 10. Destroy Wasm instance & Worker
```

### 1.3 Wasm Binary Artifacts & App Injection Pathway

Executed from the `wink-micro-os/` root:

```bash
# One-time activation of emsdk (local path example)
& 'D:\software\embedded\emsdk\emsdk_env.ps1'

# Build (Default App = samples/avoidance_car)
emcmake cmake -S . -B build-wasm -DTARGET_PLATFORM=wasm
cmake --build build-wasm

# Switch App variant (AI-generated App or other sample)
emcmake cmake -S . -B build-wasm -DTARGET_PLATFORM=wasm \
    -DWINK_APP_DIR=<absolute-path-to-app>
```

**Artifacts**:
- `build-wasm/wink_simulator.wasm` — The single binary (uncompressed, loaded by Workbench frontend Worker);
- `build-wasm/wink_simulator.js` — MODULARIZE glue code (`WasmSandbox` UMD export), pre-injected with default `js_*` stubs (see §2.2.2).

**App Injection Contract**: Defined by [02-wink-micro-os/03-directory-architecture §6.1](../../02-wink-micro-os/03-directory-architecture.md) Constraint 3—App CMakeLists uses `set(WINK_APP_SOURCES ... PARENT_SCOPE)` to export source lists to top level; the wasm target follows a sandbox model where "all App variants share a single binary target" (symmetrical to the host branch's "one executable per App" model).

### 1.4 Node-Side Smoke Tests (Does Not Replace Frontend Workbench)

`wink-micro-os/targets/wasm/wink_sim_stub.js` serves as a **compile-time contract gate**, not a host replacement:

- Statically parses `wink_simulator.wasm` imports from `env.js_*`, comparing against expected set—drift triggers failure;
- Loads `wink_simulator.js` inside `worker_threads.Worker`, determining PASS upon `onRuntimeInitialized`.

⚠ **Worker Isolation Mandatory**: Running Emscripten 6.x Asyncify's `unwind → rewind` loops alongside the Node main event loop starves timers and leads to long-run OOMs (`setTimeout(resolve, 10ms)` + rewind synchronous chains create tight loops). This applies equally to browsers—Workbench frontend must isolate Wasm in a Web Worker, keeping the main UI thread message-driven. See §1.1.

---

## 2. Emscripten Asyncify Coroutine Suspension Mechanism

### 2.1 Blocking Delay Conflict
Embedded code contains numerous blocking delay invocations (e.g. `pal_delay_ms(100)`). In a single-threaded Worker, busy-waiting prevents the Worker thread from processing incoming messages in its `onmessage` queue (such as handling `pin_input` button clicks from the main thread).

### 2.2 Asyncify Solution
**Asyncify** is a core capability of the Emscripten compiler. When Wasm code invokes a JS asynchronous function (such as `setTimeout` or `Promise`), Asyncify **suspends the active Wasm execution stack (saving registers and stack frames) and yields CPU execution back to the browser event loop**. Once the JS asynchronous callback completes, Asyncify **restores the Wasm execution stack, resuming execution seamlessly from the suspension point**.

> ⚠ **Two-Ended Contract (Both Mandatory, see ADR-0019)**: Asyncify suspension is guaranteed jointly by the **C / Linker side** and the **JS side**:
> 1. **C / Linker side**: `-s ASYNCIFY_IMPORTS=[...]` declares which JS imports act as asynchronous suspension points (the valid flag is `ASYNCIFY_IMPORTS`, not the scope-limiting `ASYNCIFY_ONLY`/`ASYNCIFY_ADD`);
> 2. **JS side (`--js-library` scenario)**: Library functions within `addToLibrary({...})` must simultaneously return a `Promise` and declare `<symbol>__async: 'auto'` metadata. Emscripten 6.x `jsifier.mjs` automatically wraps with `Asyncify.handleAsync` only when `__async === 'auto'`; `__async: true` **does not take effect**, discarding the Promise and causing the Wasm caller to return immediately (disproved in ADR-0019 spike).
> 3. **JS side (Handwritten `handleSleep` scenario)**: When not using `--js-library`, import implementations must explicitly `return Asyncify.handleSleep((wakeUp) => {...})`, ensuring `wakeUp` is called exactly once.
> Missing either side causes failure: Declaring IMPORTS in C without JS `handleAsync`/`handleSleep` fails silently (Wasm rushes past sleeps); wrapping in JS without C-side IMPORTS declaration causes `wakeUp` to throw `invalid Asyncify state`. See C side in §2.2.1 and JS side in §2.2.2.

#### 2.2.1 C Bridge Declarations (`pal_hal_wasm.c`)
```c
#include "pal_osal.h"

// Declare external JS asynchronous suspension function
extern void js_pal_delay_ms(uint32_t ms);

void pal_delay_ms(uint32_t ms) {
    // Static mapping: Asyncify intercepts this external call and suspends
    js_pal_delay_ms(ms);
}
```

#### 2.2.2 JS Asynchronous Delay Interception Implementation (Inside Worker)

**Default Repository Implementation (ADR-0019 Form)**: `wink-micro-os/targets/wasm/wink_sim_js.js` injects default stubs for all `js_*` symbols declared in `wasm_bridge.h` via `emcc --js-library=...`. Each symbol adopts a **wrapper pattern**: The library function checks if `Module.js_xxx` has been overridden by the host, delegating if matched or running default implementations otherwise. `sleep_ms` / `busy_wait_us` use `__async: 'auto'` (**not** `true`), prompting emcc to wrap their Promise returns with `Asyncify.handleAsync`.

**Override Contract**: When Workbench frontend receives `wink_simulator.js`, it **does not** need to re-declare these symbols; assigning `Module.js_*` in factory config or post-factory instances overrides default stubs (wrappers check Module properties on every invocation, making both timings equivalent):

```typescript
// Pattern A: factory config (Recommended - active before first invocation)
const module = await WasmSandbox({
  // Override default no-op: notify main UI thread of GPIO writes
  js_pal_gpio_write: (pin: number, level: boolean) => {
    self.postMessage({ type: 'pin_write', pin, level });
  },
  js_pal_i2c_transfer: (port, addr, wbuf, wlen, rbuf, rlen) => {
    // Override default stub, routing via Virtual Peripheral Registry
    return dispatchI2c(port, addr, wbuf, wlen, rbuf, rlen);
  },
  // sleep_ms can keep default setTimeout implementation; override if fine-grained
  // virtual time stepping is needed, but MUST return a Promise
});

// Pattern B: post-factory (Must complete before Wasm's first invocation of the import)
const module = await WasmSandbox({});
module.js_pal_gpio_write = (pin, level) => postToUI(...);
```

⚠ **Emscripten 6.x Critical Prerequisites (ADR-0019 §Background)**:
- **`Module.js_* = fn` top-level property attachment alone does not take effect**—If library functions are hardcoded as `function(pin, level) {}` (without wrappers), wasm-loader freezes default implementations into `wasmImports.env` at compile time, ignoring runtime Module assignments. **This repo incorporates Module lookup wrappers in `wink_sim_js.js` to enable overrides** (ADR-0019 Option B).
- **`__async: true` does NOT trigger Asyncify automatic wrapping**—emcc 6.x `src/jsifier.mjs:482` only recognizes `'auto'`; `true` is metadata. Incorrect usage causes `pal_os_sleep_ms(N)` to return immediately (Promise discarded), without compile-time or runtime diagnostics.
- **Wasm symbols can only be "overridden", not "added dynamically"**—Glue code compiles undefined `js_*` symbols directly into `abort('missing function: ...')`. Adding bridge symbols requires: Declaring `wasm_bridge.h` externs $\rightarrow$ Adding `wink_sim_js.js` default stubs with wrappers $\rightarrow$ Rebuilding Wasm.

**Asyncify Promise Contract (Workbench / Phase B)**:

Host overrides of `js_pal_os_sleep_ms` / `js_pal_os_busy_wait_us` **must return a Promise**. Returning synchronous values (`undefined`, numbers, strings) causes Asyncify to enter infinite unwind$\rightarrow$rewind loops, repeatedly executing remaining main code without diagnostics (confirmed in ADR-0019 spike #8).

**Primary Guardrail**: Phase B `types/wasm/imports.ts` defines `WasmImports` with `Promise<void>` return types for both symbols, enforcing `async` / explicit `Promise` at compile time.

```typescript
// ✅ Correct
Module.js_pal_os_sleep_ms = (ms: number): Promise<void> => {
  return new Promise((resolve) => {
    scheduleWakeAt(clock.getUs() + BigInt(ms) * 1000n, resolve);
  });
};

// ❌ Incorrect - Sync return triggers Asyncify infinite loop
Module.js_pal_os_sleep_ms = (ms) => {
  clock.advance(BigInt(ms) * 1000n);
};
```

**Equivalent Handwritten `handleSleep` Form** (Reference only when not using `--js-library`):

```typescript
const wasmImports = {
  env: {
    js_pal_os_sleep_ms: (ms: number) => {
      return Asyncify.handleSleep((wakeUp) => {
        setTimeout(wakeUp, ms);
      });
    },
    js_pal_os_busy_wait_us: (us: number) => {
      return Asyncify.handleSleep((wakeUp) => {
        setTimeout(wakeUp, Math.max(0, us / 1000));
      });
    }
  }
};
```

**Contract Acceptance Requirements:**
1. **Never** write "direct `return` after `setTimeout`"—this fails to restore the Wasm stack, causing `invalid Asyncify state` on subsequent calls. When using `--js-library`+wrapper, override functions **must** `return new Promise(...)`.
2. `wakeUp` / `resolve` must be **invoked exactly once**; duplicate or missing invocations cause `StateError`.
3. **Entry & Scheduling**: Must launch via `Module.callMain()` (**not** `Module._main()`)—Under `MODULARIZE=1` + `ASYNCIFY=1`, only `callMain` correctly handles instrumented main. Furthermore, `main` executes `wink_runtime_run(cb, 0)` as a non-returning loop; **JS must not `await callMain()`**, which risks reentrancy errors.
4. **Wasm must load inside a Web Worker**: Running Asyncify unwind$\rightarrow$rewind loops on the main thread starves timers and risks heap exhaustion (observed within 20s in early Node stub benchmarks).

#### 2.2.3 Compiler Configuration Flags
During compilation, Emscripten must be configured to enable Asyncify and register asynchronous suspension import symbols:
```bash
emcc main.c pal_osal_wasm.c -o simulator.js \
  -s ASYNCIFY=1 \
  -s ASYNCIFY_IMPORTS=["js_pal_delay_ms", "js_pal_delay_us"] \
  -s ASYNCIFY_STACK_SIZE=65536 \
  -s STACK_OVERFLOW_CHECK=2 \
  -s ASSERTIONS=1
```

> ⚠ **`ASYNCIFY_IMPORTS` Contains Real Suspension Points Only (ADR-0003 / D2 Decision)**:
> - `js_pal_delay_ms` / `js_pal_delay_us`: Invoked via `pal_delay_ms/us` in `pal_osal_wasm.c`, acting as true Asyncify suspension points.
> - `js_pal_get_ms/us`: Synchronous reads; non-suspending, **not** included.
> - `js_pal_i2c_transfer`: **Synchronous zero-copy** (returns boolean directly, see §3); **not a suspension point, forbidden from IMPORTS**.
> - `STACK_OVERFLOW_CHECK=2` + `ASSERTIONS=1`: Dev/debug builds abort on overflow/reentrancy rather than silently corrupting stacks.
> - `ASYNCIFY_STACK_SIZE=65536`: Baseline starting size; benchmarked against deepest AI-generated call stacks.

---

## 3. WASM Shared Memory & Zero-Copy Data Access (Shared Memory Pointer)

In high-frequency data exchange scenarios such as I2C display refreshes and UART byte transfers, serializing and deserializing payloads creates significant overhead. Directly accessing the **Wasm Shared Memory Buffer** enables zero-copy transfers.

* When C code in Wasm initiates `pal_i2c_transfer(port, dev_addr, write_buf, write_len, read_buf, read_len)`, the transmit buffer `write_buf` and receive buffer `read_buf` represent **pointer address offsets within Wasm linear memory**.
* In JS, accessing `wasmInstance.exports.memory.buffer` creates direct `TypedArray` views over the corresponding addresses:

```typescript
// JS interception and direct read of Wasm memory
function js_pal_i2c_transfer(
  port: number,
  dev_addr: number,
  write_buf_ptr: number,
  write_len: number,
  read_buf_ptr: number,
  read_len: number
): boolean {
  const wasmMemory = wasmInstance.exports.memory.buffer;
  
  // 1. Establish shared view, reading Wasm internal payloads with zero copying
  if (write_len > 0 && write_buf_ptr !== 0) {
    const writeData = new Uint8Array(wasmMemory, write_buf_ptr, write_len);
    // Dispatch writeData to virtual I2C peripheral (e.g. SSD1306 OLED)
    virtualI2CBus.write(dev_addr, writeData);
  }
  
  // 2. Write virtual peripheral responses back to Wasm receive buffer with zero copying
  if (read_len > 0 && read_buf_ptr !== 0) {
    const responseData = virtualI2CBus.read(dev_addr, read_len);
    const readView = new Uint8Array(wasmMemory, read_buf_ptr, read_len);
    readView.set(responseData);
  }
  
  return true;
}
```

---

## 4. Hardware Interrupt Wasm Table Function Pointer Routing (Option C: Poll Model)

In physical microcontrollers, hardware interrupts are dispatched via function pointers (`pal_gpio_isr_t`) in an interrupt vector table. Wasm sandbox security constraints **prohibit JS from executing arbitrary Wasm memory addresses directly**, requiring indirect routing via Table index mappings.

> ⚠ **Architectural Shift (Option C Landed)**: Legacy §4 used a "Push Model" where JS invoked Wasm-exported `_trigger_wasm_interrupt` at will, causing deterministic reentrancy crashes during Asyncify sleep windows (D1). **This section documents the active Poll Model**; `_trigger_wasm_interrupt` has been permanently removed.

### 4.1 Wasm Table Index Foundations

Wasm compilers store all C function pointers in an indexed **Table** structure. C `pal_gpio_isr_t` function pointers correspond to **integer indices within this Table**:

```text
  [ Wasm Table (Function Pointer Table) ]
  ┌───────┬──────────────────────────┐
  │ Index │ Actual C ISR Pointer     │
  ├───────┼──────────────────────────┤
  │   0   │ NULL                     │
  │   1   │ my_button_press_handler  │ ◄─── Registered callback index = 1
  │   2   │ sensor_data_ready_isr    │
  └───────┴──────────────────────────┘
```

### 4.2 Poll Model Interrupt Routing (3-Step Contract)

#### Step 1: C Interrupt Registration (`pal_hal_wasm.c`)

When registering interrupts, C casts function pointers to Table indices and passes them along with `arg_ptr` to JS. JS **stores the mapping table only, never executing Wasm callbacks directly**:

```c
// From pal_hal_wasm.c pal_gpio_enable_interrupt
uint32_t callback_index = (uint32_t)(uintptr_t)callback;  // C function pointer -> Table index
uint32_t arg_ptr        = (uint32_t)(uintptr_t)arg;        // wasm32 linear memory offset
js_pal_register_interrupt(pin, callback_index, arg_ptr);   // Notify JS to store in registry
```

#### Step 2: JS Queues Pending Interrupts Upon GPIO Events (Inside Worker)

```typescript
// JS pending queue (FIFO, capacity governed by PAL_WASM_INTERRUPT_QUEUE_SIZE, default 16)
const pendingInterrupts: Array<{ callbackIndex: number; argPtr: number }> = [];
const MAX_PENDING = PAL_WASM_INTERRUPT_QUEUE_SIZE; // Consistent with pal_wasm_internal.h

const wasmImports = {
  env: {
    // Called when C registers ISR - updates mapping table only
    js_pal_register_interrupt: (pin: number, callbackIndex: number, argPtr: number) => {
      interruptRegistry.set(pin, { callbackIndex, argPtr });
    },
    js_pal_deregister_interrupt: (pin: number) => {
      interruptRegistry.delete(pin);
    },
    // Polled by C at tick boundaries - returns next pending interrupt (or 0 if empty)
    js_pal_poll_interrupt: (outCallbackIndexPtr: number, outArgPtr: number): number => {
      if (pendingInterrupts.length === 0) return 0;
      const intr = pendingInterrupts.shift()!;
      const memView = new Uint32Array(Module.HEAPU8.buffer);
      memView[outCallbackIndexPtr >> 2] = intr.callbackIndex;
      memView[outArgPtr >> 2]           = intr.argPtr;
      return 1;
    },
  }
};

// GPIO event arrival - enqueues to pending buffer, never calls Wasm exports directly
function onVirtualPinTrigger(pin: number): void {
  const isrInfo = interruptRegistry.get(pin);
  if (!isrInfo) return;
  if (pendingInterrupts.length < MAX_PENDING) {
    pendingInterrupts.push({ callbackIndex: isrInfo.callbackIndex, argPtr: isrInfo.argPtr });
  } else {
    console.warn(`[WasmBridge] Interrupt queue full, dropping interrupt for pin ${pin}`);
  }
}
```

#### Step 3: Wasm Polls and Dispatches at Tick Boundaries (`wink_runtime.c` + `pal_hal_wasm.c`)

`wink_runtime_run` executes each tick by **first** calling `pal_wasm_dispatch_pending_interrupts()` (draining pending queue) **before** calling `wink_app_delay_ms()` (triggering Asyncify suspension):

```text
Tick N execution order:
  [1] callbacks->loop()                    ← Application logic
  [2] pal_wasm_dispatch_pending_interrupts ← ✅ Wasm running state, safely dispatches all pending ISRs
  [3] wink_app_delay_ms()                  ← Asyncify unwind (sleeping window begins)
       JS events arrive → writes to pending queue only ↗
  [4] timeout → wakeUp → Rewind
Tick N+1 step [2] drains interrupts accumulated during previous tick
```

```c
// From pal_hal_wasm.c pal_wasm_dispatch_pending_interrupts
void pal_wasm_dispatch_pending_interrupts(void) {
    uint32_t callback_index, arg_ptr;
    while (js_pal_poll_interrupt(&callback_index, &arg_ptr)) {
        pal_gpio_isr_t isr = (pal_gpio_isr_t)(uintptr_t)callback_index;
        if (isr != NULL) { isr((void *)(uintptr_t)arg_ptr); }
    }
}
```

### 4.3 Safety Analysis

| Dimension | Legacy Push Model | Active Poll Model (Option C) |
|---|---|---|
| Asyncify Reentrancy Risk | ❌ Deterministic crashes (D1) | ✅ Completely eliminated |
| JS Implementation Complexity | Low (but unsafe) | Low (queue writes only) |
| C Modification Scope | None | `pal_hal_wasm.c`, `wasm_entry.c`, `wink_runtime.c` (`SIMULATION` macro only) |
| Impact on host / esp32 targets | None | **None** (isolated via `#ifdef SIMULATION`) |
| Symmetry with ESP32 Interrupts | ❌ Push has no hardware analogue | ✅ Equivalent to Bottom-Half queue consumption (ADR-0002) |

### 4.4 Interrupt Queue Capacity Configuration

Governed by `PAL_WASM_INTERRUPT_QUEUE_SIZE` in `pal_wasm_internal.h` (default 16), configurable at build time:

```cmake
target_compile_definitions(wink_simulator PRIVATE PAL_WASM_INTERRUPT_QUEUE_SIZE=32)
```

**JS `MAX_PENDING` must remain synchronized** (cross-repository code review checklist).

### 4.5 Cross-Repository Verification Checklist

1. `wasm-objdump -x wink_simulator.wasm | grep trigger` $\rightarrow$ No `_trigger_wasm_interrupt` symbol.
2. Triggering GPIO interrupts during sleeping windows $\rightarrow$ Events enter pending queue without invoking Wasm exports.
3. Upon Wasm wakeup before subsequent `delay` $\rightarrow$ Queued interrupts drain in FIFO order with ISRs executed properly.
4. Zero `RuntimeError: invalid Asyncify state`, aborts, or stack corruption throughout execution.
5. High-frequency stress: 1000 consecutive ticks + 4 virtual interrupts per tick $\rightarrow$ ISR invocation count equals trigger count when queue does not overflow.
6. Queue overflow: Triggering $> \text{MAX\_PENDING}$ interrupts per tick $\rightarrow$ Console warning logged without silent drops or crashes.

---

## 5. Cooperative Multitasking Scheduler & Virtual Unicore Simulation

### 5.1 Coroutine Context Abstraction (`sim_ctx`)
To support concurrent tasks created via `pal_os_task_create`, the simulation layer replaces synchronous direct calls with lightweight physical coroutines:
* **WASM Target**: Uses `<emscripten/fiber.h>` to allocate dedicated data and Asyncify stacks for each task, using Asyncify for stack preservation and context switching at task yield points.
* **Host Target**: Uses Win32 Fibers to implement identical semantics, ensuring cross-platform behavioral consistency.

### 5.2 Deterministic Cooperative Scheduling
* **Yield Points**: Cooperative scheduling occurs strictly at designated yield points (e.g. `pal_os_sleep_ms`).
* **Deterministic Round-Robin**: When multiple tasks are `READY`, the scheduler uses a seeded PRNG pseudo-random generator to determine execution order, eliminating non-deterministic concurrency races for 100% bit-exact CI reproducibility.
* **Self-Deletion GC**: Tasks cannot destroy their own fiber context during self-deletion. The scheduler employs a **three-stage zombie reclamation** mechanism, releasing fiber memory safely after switching back to the primary scheduler context.
