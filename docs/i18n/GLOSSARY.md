# WinkMicroOS Internationalization (i18n) Glossary & Terminology Index

> 📋 **WinkMicroOS 权威中英文术语字典与缩写对照表**。  
> 本文档定义了项目代码注释、架构文档及对外门面中的标准英文表达（Canonical Forms）与常见缩写。  
> 所有代码注释与英文文档翻译必须遵循本规范，禁止直译或拼写漂移。

---

## 1. System Layers & Core Components (系统分层与核心组件)

| Standard English Term (Canonical) | Abbreviation | Chinese Term (中文) | Description / Usage Notes |
|-----------------------------------|--------------|--------------------|---------------------------|
| **Platform Abstraction Layer** | `PAL` | 平台抽象层 | Abstract C interfaces for GPIO, ADC, PWM, UART, I2C, SPI, Timer, System. Do NOT translate as "Friend". |
| **Device Abstraction Layer** | `DAL` | 器件抽象层 | Sensor, relay, motor, display device drivers and registry. Do NOT translate as "Dali". |
| **Business Abstraction Layer** | `BAL` | 业务抽象层 | High-level application logic and state machine definitions. |
| **Operating System Abstraction Layer** | `OSAL` | 操作系统抽象层 | Threading, mutex, queue, and event loop abstraction. |
| **Hardware Abstraction Layer** | `HAL` | 硬件抽象层 | Chip-vendor provided SDK/HAL (e.g., ESP-IDF HAL, CMSIS). |
| **UniSim Engine** | `UniSim` | 行为级仿真引擎 | Behavior-level high-fidelity Wasm simulation engine. |
| **Wokwi Adapter** | `Wokwi` | Wokwi 适配器 | External hardware netlist simulation adapter for Wasm build. |

---

## 2. Core Architectural Concepts (核心架构概念)

| Standard English Term (Canonical) | Chinese Term (中文) | Forbidden / Avoided Translations | Description / Usage Notes |
|-----------------------------------|--------------------|-----------------------------------|---------------------------|
| **single-source dual-target compilation** | 同源编译 / 单源双目标编译 | same source compile, dual target source | The core architectural pattern where C code compiles to host/Wasm and physical target (ESP32). |
| **compile-time static dispatch** | 编译期静态分发 | static distribution, runtime dispatch | POD struct based static function pointer binding (ADR-0004). |
| **behavioral high-fidelity simulation** | 行为级高保真仿真 | high precision simulation | High-fidelity behavior simulation without emulation overhead. |
| **living specification** | 活文档 | active documentation, alive doc | Maintained design docs serving as Single Source of Truth (SSOT). |
| **Single Source of Truth (SSOT)** | 架构真相 / 唯一事实源 | truth source | Primary spec or code defining authoritative behavior. |
| **negative status-code convention** | 负数错误码约定 | minus error code | ADR-0001 status code design convention (`wink_status_t`). |
| **Plain Old Data (POD) struct** | POD 结构体 | POD structure | C structs without hidden state or dynamic pointers. |
| **ISR context** | 中断上下文 | interrupt background | Code executing inside an Interrupt Service Routine. |
| **critical section** | 临界区 | critical area, dangerous zone | Interrupt-disabled code execution region. |
| **stack clamping** | 栈钳位 | stack fix, stack limit | Memory guard mechanism preventing stack overflow. |
| **zombie resource reclamation** | 僵尸资源回收 | dead resource collect | Automated cleanup of leaked/orphaned handles. |

---

## 3. Doxygen Tag Formatting Guidelines (Doxygen 注释规范)

All C/C++ header comments must strictly use Doxygen standard tags in English:

- `@brief`: Imperative mood short summary (e.g., `@brief Initializes the PAL ADC peripheral.` rather than `This function is used to init...`).
- `@param[in] <name>`: Input parameter description. Parameter `<name>` MUST match the C signature exactly.
- `@param[out] <name>`: Output parameter description.
- `@return`: Description of returned value (`wink_status_t` negative code or value).
- `@retval <code_enum>`: Specific return code status.
- `@note`: Additional operational warnings or concurrency notes.
