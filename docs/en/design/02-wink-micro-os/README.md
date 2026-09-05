# WinkMicroOS Runtime Engine Architecture Design (DAL & PAL)

<!-- i18n-meta
source: docs/zh/design/02-wink-micro-os/README.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

This directory contains the detailed design specifications for the **WinkMicroOS** runtime kernel. WinkMicroOS is the foundational bedrock of the low-code platform. By shielding the application layer from low-level chip register timings and bus intricacies, it enables single-source development and multi-target deployment.

---

## 📂 Module Design Documents

*   **[00-wink-micro-os-market-analysis.md](./00-wink-micro-os-market-analysis.md) - Commercial Value, Competitive Analysis & Market Outlook Report**
    *   Evaluates `wink-micro-os` positioning, market opportunities, core audiences, and open-source competitors against rigorous commercial and technical benchmarks, proving its paradigm innovation and unique niche.
*   **[01-dal-device-abstraction.md](./01-dal-device-abstraction.md) - Device Abstraction Layer (DAL) Design Spec & Static Device Tree Generation**
    *   Defines semantic-level abstractions for sensors and actuators, compile-time macros governing hardware vs simulation branches, and frontend topology-driven C device tree generation.
*   **[02-pal-platform-abstraction.md](./02-pal-platform-abstraction.md) - Platform Abstraction Layer (PAL) API Specification**
    *   Specifies cross-platform hardware bus interfaces (GPIO, PWM, I2C, SPI, ADC) and OS wrapper interfaces (OSAL tasks, semaphores, ticks, delays), establishing target onboarding standards.
*   **[03-directory-architecture.md](./03-directory-architecture.md) - Kernel Directory Architecture Design (A*)**
    *   Ports & Adapters kernel skeleton (`pal` INTERFACE / `dal` / `runtime` / `trace` first-class peers / `targets`), public API surfaces, App/BAL boundary restrictions, and CMake dependency DAGs.
*   **[04-runtime-and-trace.md](./04-runtime-and-trace.md) - Runtime Lifecycle & Golden Trace Contract**
    *   Callback-injected main loop (`wink_app_callbacks_t`), tick scheduling, fault trace emission, and target entry point wiring.
*   **[05-hardware-and-fidelity-testing-guide.md](./05-hardware-and-fidelity-testing-guide.md) - Hardware & Fidelity Testing Guide**
*   **[06-bal-layer.md](./06-bal-layer.md) - BAL Business Abstraction Layer (Domain Partition / Naming / Dependencies / CI) ★ SSOT**
    *   Physical Augmentation, `math`, `control` domains; file and API naming conventions; actuator vs control; hard cutover target state and review checklists. Decisions: [ADR-0037](../../decisions/core/0037-bal-domain-partition-and-closed-loop-motor.md), [ADR-0038](../../decisions/core/0038-bal-naming-hard-cut-and-layer-ssot.md).

---

## 📐 Layering & Data Flow Design

```text
       [ App (AI-Generated) / BAL (Kernel Static Lib) ]
                   │ (Calls device semantic APIs / registers callbacks)
                   ▼
     ┌───────────────────────────────┐
     │  runtime (main loop) + trace ◄── peer first-class layers
     └───────────────┬───────────────┘
                   │ (Calls DAL)
                   ▼
     ┌───────────────────────────────┐
     │  Device Abstraction Layer (DAL)│ ◄── SIMULATION Bypass ──► [ Web Virtual Peripheral UI ]
     └───────────────┬───────────────┘
                   │ (Calls bus & OS APIs)
                   ▼
     ┌───────────────────────────────┐
     │ Platform Abstraction Layer(PAL)│   ← INTERFACE Contract
     └───────┬───────────────┬───────┘
             ▼               ▼ (CMake static assembly routing - ADR-0041)
       [ targets/ (HAL adapt) ]  [ osal/ (OS adapt) ]   (targets/<plat> × osal/<variant>)
```

WinkMicroOS's design philosophy is: **Handle low-level complexity strictly within the underlying implementations (diverging cleanly via dual-track conditional compilation and rigorous testing), while exposing intuitive physical-world semantics to the top-level application.**
