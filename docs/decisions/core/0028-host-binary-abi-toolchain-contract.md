# ADR-0028: Host Binary SDK — ABI & Toolchain Contract

| Field | Value |
|-------|-------|
| Status | **Accepted** |
| Date | 2026-07-12 |
| Scope | Host BINARY SDK distribution |
| Superseded by | — |
| Related | [ADR-0002](../unisim/0002-dual-target-compilation.md) (dual target), [SDK Release Design](../../zh/tech-designs/tools/2026-07-12-wink-micro-os-sdk-release-design.md) §3.6, §4.6–4.9 |

---

## Context

Phase 2 of the Dual-Mode SDK introduces a **Binary SDK** for the host target: a precompiled `libwink_micro_os.a` with aggregated public headers. This enables commercial distribution without delivering implementation sources.

Key constraints that require an explicit ABI contract:

1. **Static linking = ABI coupling.** Unlike dynamic libraries, a static `.a` has no runtime version negotiation. The consumer compiles against the public headers and links the precompiled objects — any mismatch in struct layout, enum size, or calling convention is a silent link-time or runtime bug.

2. **`wink_config.h` is consumer-generated.** The `.a` is compiled once with fixed config defaults; each consumer generates their own `wink_config.h` from their `wink-app.json`. Config-sensitive code in the `.a` must not break when the consumer's config differs.

3. **Multiple toolchains.** Host target supports MinGW-w64 (GCC) and MSVC. Binary SDK must be built and consumed with a compatible toolchain.

---

## Decision

### 1. Toolchain Matrix

| Compiler | Version | Notes |
|----------|---------|-------|
| MinGW-w64 (GCC) | ≥ 14.x | Primary; WinLibs distribution |
| MSVC | ≥ 19.40 (VS 2022 17.10) | Secondary; `/utf-8` required |
| Emscripten | ≥ 3.1.x | Wasm target; `emcmake` + `emar`; same tarball, separate `libs/wasm/` |

Binary SDK is built with **one** toolchain per release per target. `SDK_MANIFEST.txt` records the exact compiler version. Consumers must use the **same toolchain family and compatible version**. Host and Wasm targets share the same tarball (`targets=host,wasm`) with precompiled libraries in `libs/host/` and `libs/wasm/` respectively.

### 2. ABI Versioning

`VERSION` file contains `ABI=<integer>` alongside `SemVer`:

```
0.2.0
ABI=1
```

**Rules:**
- Public API signature change (new param, removed function, changed return type) → **MAJOR bump + ABI++**
- Public POD struct layout change (field add/remove/reorder, type change) → **MAJOR bump + ABI++**
- Internal `.c` behavior fix, headers unchanged → **PATCH**
- New opt-in capability, old consumers unaffected → **MINOR**

### 3. Public Header Whitelist

Binary SDK `include/` contains only:
- `wink_status.h`, `pal_log.h`, `pal.h`, `pal_osal.h`, `pal_irq.h` (PAL contract types)
- `dal/include/**` (all DAL public headers, subdirectory structure preserved)
- Core runtime headers: `wink_app.h`, `wink_runtime.h`, `wink_event.h`, `wink_tasks.h`, `wink_soft_timer.h`, `wink_actuator_registry.h`, `wink_fault.h`, `wink_log.h`, `wink_blocking_region.h`, `wink_selftest.h`
- `wink_trace.h`
- `bal/include/**` (all BAL public headers)

**Excluded:** `pal_hal.h`, `pal_i2c.h`, `pal_pwm_router.h`, `pal_rmt.h` (hardware contracts), `pal_storage.h`, `pal_resource.h` (internal), `wink_pt_debug.h`, `wink_dev_config.h` (internal), all `*_priv.h` / `*_internal.h`.

### 4. Merged Archive Composition

`libwink_micro_os.a` for host contains:
- `libdal.a` (all enabled driver objects)
- `libwink_runtime.a`
- `libwink_trace.a`
- `libwink_bal.a`
- `pal_common` objects (`pal_pwm_router.c.o`)
- `pal_host` objects (all `targets/host/*.c.o`)

### 5. Config Freeze Mitigation

The following config macros affect array sizes in the `.a`:

| Macro | Used in | Default in BINARY pack |
|-------|---------|----------------------|
| `WINK_MAX_SOFT_TIMERS` | `wink_soft_timer.c`, `wink_runtime_tasks.c` | **32** |
| `PAL_PWM_CHANNELS` | `pal_pwm_router.c`, `dal_servo.c` | **16** |
| `WINK_RUNTIME_TICK_MS` | `wink_runtime.c` (runtime value only) | **10** |

Consumer `wink-app.json` values **must not exceed** these defaults. If a consumer needs larger values, they must use the Source SDK.

Public `#ifndef` header defaults may remain smaller for Source SDK friendliness (for example 16 soft timers and 8 PWM channels). The Binary SDK pack compile explicitly forces `WINK_MAX_SOFT_TIMERS=32` and `PAL_PWM_CHANNELS=16`; consumer `wink-app.json` values must stay at or below those pack-time ceilings.

`WINK_TARGET_HOST` is a platform guard (`#ifdef`), not a sizing macro — it is always `1` for host builds and has no freeze risk.

### 6. Compile Flags

GCC/MinGW BINARY pack:
```
-ffunction-sections -fdata-sections
```
Consumer must link with `--gc-sections` to avoid pulling unused DAL drivers into the final binary.

MSVC BINARY pack:
```
/Gy (function-level linking)
```
Consumer must link with `/OPT:REF`.

### 7. `wink_config.h` Isolation Rule

Translation units compiled into `libwink_micro_os.a` must NOT embed consumer-specific config constants as immediate values in ways that break when a different consumer uses a different `wink-app.json`. Specifically:

- Array sizes use the BINARY pack defaults (see §5), not a specific App's values
- `#ifdef WINK_TARGET_*` guards are acceptable (platform identification, not App-specific)
- Runtime reads of `WINK_RUNTIME_TICK_MS` via `#define` are acceptable (re-evaluated at each consumer's compile time for headers; `.c` in `.a` uses the pack-time value)

---

## Consequences

- **Positive:** Commercial distribution without source; single `.a` simplifies integration
- **Positive:** ABI versioning gives clear upgrade/compatibility signals
- **Negative:** Toolchain lock-in per release; mismatched toolchain requires Source SDK fallback
- **Negative:** Config defaults cap consumer flexibility (mitigated by generous upper bounds)
- **Negative:** `--gc-sections` / `/OPT:REF` required at consumer link time or binary size bloats
- **Risk:** Public POD struct changes silently break existing consumers — enforced by SemVer MAJOR + ABI++ discipline

