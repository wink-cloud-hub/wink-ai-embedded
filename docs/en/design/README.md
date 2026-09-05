# Wink-AI Embedded Design Documentation Hub

<!-- i18n-meta
source: docs/zh/design/README.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

Welcome to the **Wink-AI Embedded Runtime & Simulation System (WinkMicroOS)** Design Documentation Hub. This platform is a low-code development ecosystem for AI-generated embedded applications. It enables users to compose business logic visually or via AI, perform behavior-level high-fidelity simulation, fault injection verification, and consistency tracing on the Web via WebAssembly, and finally deploy to physical microcontrollers through cloud cross-compilation and in-browser WebSerial/WebUSB flashing.

This design documentation hub is divided into 7 core modules/system tiers based on enterprise-grade product architecture, allowing multi-disciplinary teams (Frontend, Kernel, Simulation Bridge, App Codegen/Compiler, and DevOps) to develop in parallel:

---

## ⚡ Quick Entry & Global Contracts & ABI Matrix

* **Getting Started**: Please refer to [00-quick-start/01-5min-getting-started.md](./00-quick-start/01-5min-getting-started.md)
* **AI Agent Developer Guide**: Please read [docs/AGENTS.md](../AGENTS.md) for black-box isolation and retrieval policies.

### 🔗 Cross-Module & Cross-Repo Global Contracts & ABI Matrix

| Contract / ABI Name | Subject Tier | Authoritative Design Spec (SSOT) | Source / Contract Mapping Target | Mapping Type |
| :--- | :--- | :--- | :--- | :--- |
| **Wasm Bridge C-ABI** | UniSim Wasm ↔ Web Worker | [04-wasm-simulation/02-mechanisms/](./04-wasm-simulation/02-mechanisms/) | `wasm_bridge.h` C Export Interface Declarations | `Contract-Mapping` |
| **SimTrace Event Spec** | Simulation / Physical Trace Observability | [07-platform-governance/04-simulation-consistency.md](./07-platform-governance/04-simulation-consistency.md) | `SimTraceSpecV2` JSON Schema | `Contract-Mapping` |
| **C-DAL Device API** | WinkMicroOS Device Abstraction Layer | [02-wink-micro-os/01-dal-device-abstraction.md](./02-wink-micro-os/01-dal-device-abstraction.md) | `/src/core/dal/include/dal_gpio.h` | `Code-Mapping` (In-Repo) |
| **C-PAL Platform HAL** | Platform Adaptation Interface (HAL/OSAL) | [02-wink-micro-os/02-pal-platform-abstraction.md](./02-wink-micro-os/02-pal-platform-abstraction.md) | `/src/core/pal/include/pal_gpio.h` | `Code-Mapping` (In-Repo) |
| **Project Manifest** | Application Config & Device Tree Definition | [03-app-codegen/02-project-manifest-schema.md](./03-app-codegen/02-project-manifest-schema.md) | `wink-app.json` (v2 Schema) | `Contract-Mapping` |
| **Cloud Build Job API** | Cloud-Isolated Cross-Compilation Pipeline | [06-build-toolchain/02-build-service-job-protocol.md](./06-build-toolchain/02-build-service-job-protocol.md) | `Build Job Protocol (job.proto)` | `Contract-Mapping` |
| **Device Model Registry** | Unified Peripheral Topology & Property Registry | [07-platform-governance/01-device-model-registry.md](./07-platform-governance/01-device-model-registry.md) | `Device Model Registry Schema` | `Contract-Mapping` |

---

## Architecture Design Document Navigation

### 1. Overall System Design & Product Roadmap

* **[01-system-overall/01-system-overview.md](./01-system-overall/01-system-overview.md) - System-Level Architecture Design & 5 Core Cross-Repo Module Guide**
  * Describes platform vision, layered architecture, decoupled execution flow, **5 core cross-repo modules (embedded-frontend, unisim, wink-tools, wink-micro-os, wink-micro-app)**, and their black-box interface guidelines.
* **[01-system-overall/02-mvp-roadmap.md](./01-system-overall/02-mvp-roadmap.md) - Product Roadmap, Hardware Matrix & Milestone Deliverables**
  * Converges hardware matrix on ESP32/STM32, Wasm simulation, fault testing, cloud compilation, and WebSerial/WebUSB flashing loop.
* **[01-system-overall/03-product-user-journey.md](./01-system-overall/03-product-user-journey.md) - User Journey, Dual-Viewport (2D/3D) Experience & IDE Information Architecture**
  * Defines core user journeys, dual-viewport split-screen and synchronization interactions, work modes, AI intervention touchpoints, and flashing wizard workflows.
* **[01-system-overall/04-cross-repo-boundary-contract.md](./01-system-overall/04-cross-repo-boundary-contract.md) - Cross-Repo Component Dependencies, Contracts & Trade Secret Isolation Specification**
  * Defines physical package boundaries with Wink-AI main repository, proprietary algorithm black-box isolation rules, and machine-readable contracts (`wink-app.json` Manifest, Wasm Bridge ABI, SimTrace).

### 2. WinkMicroOS Kernel Specification

* **[02-wink-micro-os/README.md](./02-wink-micro-os/README.md) - WinkMicroOS Runtime Overview**
  * Subdirectory navigation, runtime relationships between DAL/PAL, and C kernel responsibilities.
* **[02-wink-micro-os/01-dal-device-abstraction.md](./02-wink-micro-os/01-dal-device-abstraction.md) - Device Abstraction Layer (DAL) Design Spec & Static Device Tree**
  * Defines device semantic APIs, dual-mode execution implementations, SIMULATION branching, and device tree generation templates.
* **[02-wink-micro-os/02-pal-platform-abstraction.md](./02-wink-micro-os/02-pal-platform-abstraction.md) - Platform Abstraction Layer (PAL) API Specification**
  * Defines platform-agnostic OSAL/HAL APIs including GPIO, PWM, I2C, SPI, ADC, threads, mutexes, and system Tick.

### 3. Application Logic & Code Generation (App & Codegen)

* **[03-app-codegen/01-app-business-logic.md](./03-app-codegen/01-app-business-logic.md) - Application Layer (App) Runtime Specification**
  * Defines low-code/AI-generated application business logic specifications, lifecycle contracts, state machine code generation rules, and hardware decoupling constraints.
* **[03-app-codegen/02-project-manifest-schema.md](./03-app-codegen/02-project-manifest-schema.md) - Embedded Project Manifest & Registry Lock Specification**
  * Defines project Single Source of Truth (SSOT), device lockfile, reproducible builds, migrations, and master project summary metadata.
* **[03-app-codegen/03-ai-dsl-and-codegen-pipeline.md](./03-app-codegen/03-ai-dsl-and-codegen-pipeline.md) - AI DSL, State Machine AST & App Safe Codegen Pipeline Specification**
  * Defines the safe pipeline from natural language / low-code to constrained DSL, and down to deterministic App C code generation.

### 4. Web Simulation & Runtime (Wasm & Web Simulation)

> **Active SSOT is 04-wasm-simulation (UniSim 4-Tier Document Suite)**: Overview (I), Mechanisms (IIa), Axes A~F (IIb), Assurance (III). Versions 1.0 and 2.0 have been archived to `04-wasm-simulation/archive/` for read-only historical reference.

* **[04-wasm-simulation/00-README.md](./04-wasm-simulation/00-README.md) — [Active SSOT] UniSim 3.0 Master Hub**
  * 4-tier structure + SSOT iron rules + document/maturity glossary; authoritative index of A~F fidelity axes and C1~C25 consistency scenarios.
* **[04-wasm-simulation/01-overview/](./04-wasm-simulation/01-overview/) — Tier I Overview**: Architecture, A~F definitions, production scope, methodology, glossary.
* **[04-wasm-simulation/02-mechanisms/](./04-wasm-simulation/02-mechanisms/) — Tier IIa Mechanisms (Implementation SSOT)**: Sandbox/Asyncify, virtual clock, scheduler, interrupts, faults, physical degradation, peripheral registry, channel routing, timer/PWM, Wasm↔JS ABI, accuracy observation.
* **[04-wasm-simulation/03-axes/](./04-wasm-simulation/03-axes/) — Tier IIb Fidelity Axes A~F Thin Index**
* **[04-wasm-simulation/04-assurance/](./04-wasm-simulation/04-assurance/) — Tier III Assurance Governance**: Consistency spec (C1~C25), state matrix checklist, roadmap & governance.
* **[04-wasm-simulation/archive/](./04-wasm-simulation/archive/)**: Historical versions (Read-only archives 1.0 / 2.0).

### 5. Frontend Workbench Architecture

* **[05-frontend-workbench/01-frontend-workbench-architecture.md](./05-frontend-workbench/01-frontend-workbench-architecture.md) - Embedded Frontend Workbench Architecture & UX Design**
  * Defines 3-column workbench layout, Manifest-driven state, Wasm Worker client, Trace Console, and main project route integration.
* **[05-frontend-workbench/02-dual-viewport-product-world-layout.md](./05-frontend-workbench/02-dual-viewport-product-world-layout.md) - Dual-Viewport Product World Layout & 3D Mechanical Simulation Interface Specification**
  * Defines Circuit 2D + Product World 3D split-screen layout, operational mode state machines, Manifest v2 mechanical/environment/binding extensions, dual-domain data flow, and causal chain console.

### 6. Cloud Toolchain & Build Services (Cloud Build & Toolchain)

* **[06-build-toolchain/01-toolchain-deployment.md](./06-build-toolchain/01-toolchain-deployment.md) - Compilation & Physical Flashing Pipeline**
  * Designs cloud Docker cross-compilation pipeline and browser WebSerial/WebUSB physical device flashing loop.
* **[06-build-toolchain/02-build-service-job-protocol.md](./06-build-toolchain/02-build-service-job-protocol.md) - Build Service, Build Job & Artifact Protocol Specification**
  * Defines asynchronous Build Jobs, isolated build workers, Build Manifests, artifact hashing, log normalization, and pre-flashing validation.

### 7. Platform Governance & Safety Standards (Platform Governance)

* **[07-platform-governance/01-device-model-registry.md](./07-platform-governance/01-device-model-registry.md) - Device Model Registry Unified Peripheral Specification**
  * Unifies peripheral pins, properties, DAL APIs, simulation behaviors, physical constraints, fault models, and code generation rules as a Single Source of Truth.
* **[07-platform-governance/02-error-fault-model.md](./07-platform-governance/02-error-fault-model.md) - Error Model, Fault Injection & Safe Degradation Specification**
  * Defines `wink_status_t`, DAL/PAL error returns, fault injection, fail-safe postures, and error observability.
* **[07-platform-governance/03-security-sandbox.md](./07-platform-governance/03-security-sandbox.md) - AI Codegen Security Sandbox & Cloud Compilation Security Specification**
  * Defines App Safe Codegen, Wasm Worker watchdog, compilation container isolation, firmware manifest, and WebSerial security policies.
* **[07-platform-governance/04-simulation-consistency.md](./07-platform-governance/04-simulation-consistency.md) - Simulation Consistency Verification & Golden Trace Specification**
  * Defines simulation/hardware trace events, input replay, discrepancy comparison, consistency grades, and CI regression validation.

### 8. Horizontal Process Artifact Management (Decisions, Plans, Reviews & Tech Designs)

This documentation hub adopts a two-tier architecture: **"Maintain a Single SSOT + Archive Process Artifacts by Lifecycle/Domain"**:

* **[decisions/](../README.md) — Architecture Decision Records (ADR)**
  * Stores major technical decisions (`0001-` ~ `0063-`); final decisions are recorded permanently, and conclusions MUST be backported into `01~07` SSOT specs.
* **[tech-designs/](../README.md) — Technical Designs & RFCs**
  * Organized into `core/`, `unisim/`, `frontend/`, `tools/` subdirectories; stores detailed How-To design proposals before feature implementation.
* **[implementation-plans/](../README.md) — Implementation Plans (Plan)**
  * Organized into `active/` (in-progress/pending plans) and `archived/` (completed plans). Run `python implementation-plans/scripts/list_plans.py` to check statuses.
* **[reviews/](../README.md) — Architecture Reviews & Verification**
  * Organized into `active/` (open action items) and `archived/` (historical sign-offs / Hardware Smoke Test reports).

---

## 📋 Document Organization & Governance Guidelines

| Classification | Directory Location | Nature | Mutability | Governance Rule |
|---|---|---|---|---|
| Architecture Spec (SSOT) | `01~07` Modules | Current System Truth | Continuously updated in-place | Architecture changes directly update `01~07` files |
| Decision Records (ADR) | `decisions/` | Architectural Decision Logs | Final & Read-only | Major trade-offs produce ADRs, conclusions backported to `01~07` |
| Technical Designs (RFC) | `tech-designs/{domain}/` | Detailed Design Solutions | Archived after implementation | Grouped by core / unisim / frontend / tools |
| Implementation Plans (Plan) | `implementation-plans/` | Phased Task Roadmaps | Lifecycle isolated | Stored in `active/`, moved to `archived/` upon completion |
| Review Reports | `reviews/` | Review Health Check Audits | Snapshot archives | Stored in `active/` if pending items exist, archived upon closure |

> 📌 **Documentation Governance Mantra**:
> **Architecture changes update 01~07 (SSOT)  │  Major decisions create decisions (ADR)**
> **Technical proposals go into tech-designs   │  Completed plans archive into archived (Plan)**

---

## Core Philosophy: Safe Dual-Mode Harmony

```text
[ AI / Low-Code Input ]
        │
        ▼
[ App DSL / State Machine AST + Project Manifest ]
        │
        ▼
[ App Safe Codegen + Static Analysis ]
        │
        ▼
[ Device Model Registry + Registry Lock ] ──► [ device_tree / SchemaForm / DAL / Sim ]
        │
        ▼
[ Wasm Worker Simulation + Fault Injection + Golden Trace ]
        │
        ▼
[ Cloud Isolated Build + Firmware Manifest ]
        │
        ▼
[ WebSerial/WebUSB User-Authorized Flashing ]
        │
        ▼
[ Physical MCU Runtime + Trace Compare ]
```

The platform does not aim to replace SPICE, oscilloscopes, or real hardware validation benches. Instead, it focuses on **safe generation of AI-authored embedded business logic, behavior-level simulation, abnormal execution path verification, and closed-loop hardware deployment**.

## Project Coding Standards

### Embedded Code Standards

🎯 Summary Table

| Scenario | Which Document to Use |
|---|---|
| New hire onboarding on Day 1 | `c-code.md` (10 Iron Rules - what NEVER to touch) |
| Pre-commit self-check after coding | `code-quality-checklist.md` (Check off each item) |
| Uncertain why a rule exists | Open corresponding specialized spec (Understand design rationale) |
| Giving feedback in Code Review | First point to `code-quality-checklist.md` items, then link specialized spec |
