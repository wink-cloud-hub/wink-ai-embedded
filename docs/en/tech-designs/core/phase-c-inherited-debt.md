# Phase C Inherited Debt (from Phase B, 2026-07-03)

Phase B (2026-07-03-frontend-simulation-phase-b-plan.md) deliberately deferred
these items. Each MUST be handled by Phase C — do NOT assume the "13 imports
green" of Task 14 means the underlying subsystem is production-ready.

## PWM does not update PinArbiter (Task 11)

`js_pal_pwm_set_duty(channel, duty)` currently routes only to the optional
`UnisimBridgeDeps.pwmSink` observer. `PinArbiter` has no channel→pin mapping
and does not model PWM. Consequence: an App that puts pin 5 on PWM channel 2
via `pal_pwm_init` will NOT have pin 5's PinArbiter state updated when
`pal_pwm_set_duty` fires — a device reading pin 5 via `readPin` sees HI_Z.

Phase C fix: extend PinArbiter (or a companion `PwmChannelModel`) to accept a
`(channel, pin, freqHz)` binding from a new `js_pal_pwm_init` bridge, then
have `js_pal_pwm_set_duty` translate to a driver contribution on the mapped
pin (or a rolling-average model for slow observers).

## GPIO mode changes do not release the wasm driver (Task 11)

`js_pal_gpio_write(pin, level)` unconditionally `setDriver`s a strong
`mcu:gpio<pin>` driver at SUPPLY strength. There is no bridge for the
`pal_gpio_set_mode(pin, INPUT)` C-side call, so the driver is NEVER
`removeDriver`'d — pin stays owned by the wasm even when C code intends
to release it (one-wire, I²C bit-bang, dynamic-mode-switching drivers).

Phase C fix: add a `js_pal_gpio_config_mode(pin, mode)` bridge to
`wasm_bridge.h`, wire it into `pal_gpio_set_mode`. On INPUT mode,
`createUnisimImports` calls `arbiter.removeDriver(pin, 'mcu:gpio<pin>')`.

## Interrupt overflow policy (Task 10)

`InterruptQueue` uses drop-**oldest** with a one-time console.warn. Real GPIO
users would rather lose stale events than fresh ones (a delayed old button
press is more confusing than a missed brand-new one).

Phase C fix: switch to drop-newest, or expose the policy on
`WasmInterruptQueue` and let each device model pick. Also promote the
overflow diagnostic to a rate-limited emitter (`log-every-N-seconds`).

## Fault log ring buffer decoding (Task 5)

`FaultAuditLogEvent` and the 6 `pal_wasm_fault_event_get_*` exports (Task 8)
are wired to the type layer only. There is no code that iterates
`pal_wasm_get_fault_log_count()` and decodes rows into UI-consumable events.

Phase C fix: add a `FaultLogReader` runtime object that lazily iterates and
caches, plus a Worker→UI message channel to stream new events.

## Power model (Task 8 export additions)

`pal_wasm_set_pin_power_model` and `pal_wasm_get_total_energy_mj` are typed
but not consumed. The `wasm_pin_power_model_t*` struct layout is not
documented in the TS side.

Phase C fix: add a `PowerModelBinding` runtime type + `_malloc`-based
setter helper on `WasmPhysicalBridge`; export a `getTotalEnergyMj()`
accessor for UI energy display.

## Wasm64 pointer width migration (Task 10 + Task 11)

`InterruptQueue` and `createUnisimImports.js_pal_poll_interrupt` use
`writeU32LE` to write `callback_index` and `arg_ptr` into the wasm heap as
4-byte little-endian values. This matches the wasm32 ABI where all pointers
and `uintptr_t` values fit in 32 bits.

If the project migrates to wasm64 (see `wasm_bridge.h` line 109 comment:
"wasm64 迁移见 Phase 6 Task 6-3"), these values become 8-byte. The
`writeU32LE` helper and all `uint32_t*` out-pointer writes in
`js_pal_poll_interrupt` must be replaced with `writeU64LE` + `BigInt`.

Phase C fix: add a `WASM_POINTER_BYTES` constant (4 or 8) derived from
the build configuration, and conditionalise the write helper. Alternatively,
surface a `writePtr(view, offset, value)` abstraction in `UnisimBridgeDeps`.

## I²C export-side marshalling is untested (Task 6 Step 6)

Phase B adds a `_malloc + HEAPU8.set + pal_i2c_transfer(raw ABI) + _free`
code path in `WasmPhysicalBridge.i2cTransfer`. However, NO Phase B test
exercises this path: the existing `WasmPhysicalBridge.test.ts` uses
`as any` mock cast (Option A), which makes the `rawModule` null branch
fire. The Node smoke test (Task 14) exercises the **import** side
(`js_pal_i2c_transfer` via `createUnisimImports`) but NOT the export side
(`pal_i2c_transfer` via `WasmPhysicalBridge`).

Phase C fix: add a `WasmPhysicalBridge.test.ts` test case that supplies
a real `RawModule` mock (with fake `_malloc`, `_free`, `HEAPU8`) and
verifies that `i2cTransfer` correctly marshals `Uint8Array → _malloc →
HEAPU8.set → raw ABI → _free` with proper `try/finally` cleanup on both
success and failure paths.

## `js_pal_os_get_ms` / `js_pal_os_get_us` dead stubs

`wasm_bridge.h` declares `extern uint64_t js_pal_os_get_ms(void)` and
`js_pal_os_get_us(void)`, and `wink_sim_js.js` ships wrapper stubs for
both, but the C-side implementations (`pal_osal_wasm.c`) define
`pal_os_get_us/ms` as direct reads of the internal `s_virtual_us` counter
and never call through to the JS imports. These two `js_*` symbols are
therefore NOT present in any built wasm binary's import set; dual-clock
lockstep is instead maintained by JS calling
`pal_wasm_advance_virtual_clock(bigint)` explicitly after each `clock.advance()`.

Phase C decision needed: either (a) remove the dead `js_pal_os_get_*`
declarations and wrappers (simpler, matches actual ABI), or (b) re-route
`pal_os_get_us/ms` through the JS imports so the JS clock becomes the
single SSOT (cleaner semantic, but adds a cross-boundary call per time
read). Current Phase B tests assert the *actual* import set (11 symbols),
not the declared set (13), so either direction is non-breaking for tests.
