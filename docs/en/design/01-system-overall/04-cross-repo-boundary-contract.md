# 04. Cross-Repository Component Dependencies, Contracts & Trade Secret Isolation Specification

<!-- i18n-meta
source: docs/zh/design/01-system-overall/04-cross-repo-boundary-contract.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

> **Document Scope**: This document defines the physical cross-repository dependency architecture, data contract protocols, and **trade secret isolation principles** between `wink-ai-embedded` (kernel & toolchain repo) and its sibling packages in the `wink-ai` main monorepo (`embedded-frontend`, `unisim`).

---

## 1. Trade Secret & Black-Box Isolation Principle

To protect proprietary intellectual property and maintain strict codebase access boundaries, cross-repository architecture specifications enforce the following **Black-Box Isolation Iron Rules**:

1. **Black-Box Boundary**: For external components belonging to the `wink-ai` master repository (such as the `embedded-frontend` UI editor and `unisim` simulation engine), design documentation in this repository **exclusively defines their Function, Usage scenarios, public API / DTO / CLI contracts, and input/output artifacts**.
2. **Proprietary Implementation Secrecy**: Documenting private rendering optimizations, proprietary editor business logic, backend authentication, or cloud scheduling algorithms belonging to the master repo is strictly forbidden in this repository.
3. **Contracts as Single Source of Truth**: Interactions between repositories rely entirely on machine-readable contract specifications (`wink-app.json` Schema, `SimTraceSpecV2`, `wasm_bridge.h` ABI). As long as interface contracts remain stable, both repositories evolve independently.

---

## 2. Active Physical Package Structure & Cross-Repo Mapping

In the current Wink-AI Monorepo topology, cross-repository responsibilities are partitioned as follows:

```text
Wink-AI Cross-Repo Component Topology & Black-Box Contract Boundaries:

[ External Master Monorepo Components ] (Black-box dependencies, strictly public API / Manifest / ABI contracts)
├── embedded-frontend                       # Cross-Repo Pillar 1: Embedded Web Workbench UI (Black-box package)
│   └── Interface Contracts: wink-app.json Manifest, Dual-Viewport State Sync DTO
└── unisim                                  # Cross-Repo Pillar 2: UniSim Wasm Simulation Engine (Black-box package)
    └── Interface Contracts: wasm_bridge.h C-ABI, SimTraceSpecV2 Spec

[ In-Repo Components: wink-ai-embedded ] (Contains kernel Code-Mapping)
├── wink-tools/                             # Cross-Repo Pillar 3: Unified CLI Toolchain (wink CLI)
├── wink-micro-os/                          # Cross-Repo Pillar 4: C-Language SDK Kernel (PAL/DAL/BAL, Code-Mapping SSOT)
└── wink-micro-app/                         # Cross-Repo Pillar 5: Embedded Application Project Template
```

---

## 3. Three Core Machine-Readable Cross-Repo Contracts

### 3.1 Contract 1: Project Single Source of Truth (`wink-app.json` Manifest)

`wink-app.json` is the sole project definition document exchanged among `embedded-frontend`, `wink-tools`, and `wink-micro-os`:

```json
{
  "schemaVersion": 2,
  "name": "distance_alarm",
  "target_board": "esp32_devkitc",
  "tick_ms": 10,
  "devices": [
    {
      "id": "radar_1",
      "model": "hc_sr04",
      "pin_map": { "trig": 12, "echo": 13 }
    }
  ]
}
```

- **`embedded-frontend` Responsibility**: Handles visual topology editing, property configuration, and serialization to `wink-app.json`.
- **`wink-tools` Responsibility**: Parses `wink-app.json` and invokes `wink gen` to output `app_main.c` and `device_tree.c`.
- **`wink-micro-os` Responsibility**: Compiles and executes the generated C code.

### 3.2 Contract 2: Wasm Simulation Bridge C-ABI (`wasm_bridge.h`)

When `wink-micro-os` is compiled for the `wasm32` target, it exports a stable C-ABI loaded by `unisim` inside Web Workers:

- **Export Entry Points**: `wink_wasm_init()`, `wink_wasm_step(microseconds)`, `wink_wasm_get_trace_buffer()`.
- **Import Stubs**: Injected by `unisim` to capture virtual peripheral read/write hooks (`unisim_gpio_write` / `unisim_i2c_transfer`).

### 3.3 Contract 3: Simulation & Hardware Event Tracing Protocol (`SimTraceSpecV2`)

Used for Headless automated testing, frontend timeline visualization, and virtual-physical consistency comparisons:

- **Format**: JSONL / JSON structured envelope containing microsecond timestamps, device IDs, event types (`GPIO_SET` / `I2C_TRANSFER` / `FAULT_INJECT`), and state payloads.
- **Consumers**: Displayed in the `embedded-frontend` Trace Console and asserted in CI via `wink test` CLI.

---

## 4. Dependency Safety & Quality Gate Policies

1. **Unidirectional Dependency**: The `wink-micro-os` C kernel never depends on any Node.js/TS packages; `unisim` and `embedded-frontend` only depend on compiled Wasm artifacts and public C struct headers exported by `wink-micro-os`.
2. **Build Isolation**: `wink build` executes inside isolated container sandboxes; build scripts must not invoke private master-repo APIs.
3. **Version Locking**: Manifest `schemaVersion` and `SimTraceSpecV2` must maintain backward compatibility and pass version migration validation (`manifest-migration.ts`).
