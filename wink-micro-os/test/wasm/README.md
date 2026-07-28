# `test/wasm/` — Wasm-only unit tests (deferred wiring)

**Status:** Source-only, not yet built. Requires an Emscripten toolchain and
an `add_wink_wasm_test` CMake helper that does not exist yet in this repo.
Audited and confirmed non-orphan (P1-T1, 2026-07-04).

## Why these tests live outside the host CMake

Every file in this directory unconditionally includes `<emscripten.h>` and/or
calls symbols that only exist in `targets/wasm/` (`pal_wasm_*`,
`wasm_fault_event_t`, `wasm_fault_domain_id_t`, `wasm_pin_power_model_t`,
etc.). They cannot be compiled with host GCC/MinGW — attempting to add them
to `test/CMakeLists.txt` would break `python wink-tools/wink.py test`.

The host test matrix in `test/CMakeLists.txt` already covers the
target-independent algorithm library (`wink_sim_physical.c`,
`wink_sim_scheduler.c`) via `test_sim_physical.c`, `test_sim_scheduler*.c`,
and the button-debounce e2e via `test_button_debounce_e2e.c`. The files here
are the **wasm-side parity mirror**: they exercise the JS↔C bridge,
`pal_wasm_physical.c` fault injection, virtual-clock plumbing, and the
Emscripten `KEEPALIVE` exports — behaviour host tests structurally cannot
cover.

## Inventory

| File | What it covers | Wasm-only because |
|------|----------------|-------------------|
| `test_virtual_clock.c` | ADR-0003 §3 / ADR-0009 §4.1 SSOT virtual clock: monotonic advance, `pal_os_sleep_ms` non-side-effect, 64-bit wrap semantics | Uses `pal_wasm_advance_virtual_clock` (only exists in `pal_osal_wasm.c`) |
| `test_clock_overflow.c` | 50%-quantile early-warning latch (`pal_wasm_is_clock_warning_fired`); JS side polls this per tick | `pal_wasm_is_clock_warning_fired` and BSS-reset semantics are wasm-loader-specific |
| `test_wasm_physical.c` | PRNG golden parity host↔wasm; fault config setter/getter round-trip; debounce forced-alternation golden; per-pin ctx OOB protection | Validates that `wink_sim_physical.c` compiled under Emscripten produces the same bytes as host |
| `test_debounce_middleware.c` | `pal_gpio_read` middleware: zero-degradation passthrough, bounce_us>0 forced alternation, `WASM_SIM_MAX_PINS` bounds | The middleware lives in `pal_hal_wasm.c`; there is no host equivalent (host uses `sim_set_gpio_ideal` from `pal_host`) |
| `test_i2c_drop_middleware.c` | `pal_i2c_transfer` drop injection: zero-permil short-circuit, 100% drop path, PRNG advancement determinism | I2C middleware only exists in `pal_hal_wasm.c` |
| `test_button_debounce_e2e_wasm.c` | End-to-end button debounce with `pal_gpio_read` middleware + `dal_button` state machine, using `pal_wasm_advance_virtual_clock` to drive ticks | Host version (`test/test_button_debounce_e2e.c`) uses a different injection path (`sim_set_gpio_ideal`); this file is the wasm-side parity proof |
| `test_fault_log.c` | 256-entry fault-audit ring buffer: empty/reset state, single event round-trip, saturation at 256 (no overflow) | `pal_wasm_log_fault` / `pal_wasm_get_fault_event` are wasm-only |
| `test_fault_domain_stub.c` | Wave3 forward-compat: `pal_wasm_get_domain_config`, `pal_wasm_arm_fault_domain`, `pal_wasm_get_domain_trigger_count`; GLOBAL domain today, per-domain in Wave3 | Domain enum and config aliasing live in `pal_wasm_fault_domain.c` |
| `test_power_model_stub.c` | Wave3 forward-compat: `pal_wasm_set_pin_power_model` / `pal_wasm_get_total_energy_mj`; locks the `wasm_pin_power_model_t` field set so Wave3 can plug in the real accumulator | The type and setter live in `pal_wasm_internal.h` (wasm-only) |

Total: 9 files (~4.4k LOC).

## Wiring dependencies (what needs to exist to build these)

To bring these into CI, the following need to land:

1. **Emscripten toolchain in build environment.** `python wink-tools/wink.py test` today only
   provisions MinGW/GCC. A parallel `run-wasm-tests.ps1` (or a Node-side
   runner) would need `emcc` on `$env:Path`.
2. **`add_wink_wasm_test` CMake helper.** Referenced by every file's header
   comment as the intended registration hook. It would:
   - Set `CMAKE_TOOLCHAIN_FILE=$EMSDK/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake`.
   - Compile each test to a `.js` + `.wasm` pair.
   - Wrap in a Node harness that supplies the JS mocks (`js_pal_gpio_read`,
     `js_pal_i2c_transfer`) documented in `test_debounce_middleware.c` and
     `test_i2c_drop_middleware.c`.
   - Register via `add_test(NAME ... COMMAND node harness.js <test>.js)`.
3. **JS-side test harness.** For the two middleware tests that hit
   `js_pal_*` imports, a small Node harness that provides configurable
   mocks. Straw-man design already sketched in the file headers.

None of the wasm C sources need to change — the exports (`EMSCRIPTEN_KEEPALIVE`)
and internal accessors are already in place, verified against
`targets/wasm/pal_wasm_internal.h` (2026-07-04).

## Related plan items

- P1-T1 (this audit): keep-defer decision recorded here.
- Wasm test-harness bring-up: no dedicated plan task yet. Suggest a P2 item
  scoped to steps 1-3 above; the tests themselves are already written and
  self-contained.

See `docs/design/implementation-plans/2026-07-04-wmos-comprehensive-hardening-plan.md`
for the enclosing hardening plan.
