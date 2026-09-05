# 8051 User-Facing Coding & Migration Guide (Compatibility & Limitations)

| Attribute | Content |
| :--- | :--- |
| **Document Status** | Formal Standard - Active Specification |
| **Baseline & ADRs** | ADR-0012 (Contract Honesty), ADR-0070 (C++ Interception), ADR-0071 (Data-Plane Proxy), ADR-0072 (Clock & ISR), ADR-0073 (CMS8S ADC), ADR-0076 (UART RX), ADR-0077 (Quasi-Bidirectional) |
| **Creation Date** | 2026-08-27 |
| **Revision Date** | 2026-09-02 (Fully aligned with active framework codebase and ADR-0077) |
| **Target Audience** | 8051 Firmware Engineers, Appliance App Migrators, AI Embedded Code Generation Agents |
| **Module** | `wink-micro-os` / `frameworks/mcs51/` / `UniSim` |
| **Architecture Specs** | [General Overview](2026-08-27-mcs51-zero-code-simulation-and-proxy-design.md)<br>[Data Plane: SFR Proxy & RMW](2026-08-27-mcs51-sfr-proxy-rmw-and-edge-detection-design.md)<br>[Timing Plane: Clock Domains & Timing](2026-08-27-mcs51-clock-domains-and-timing-consistency-design.md)<br>[Master Plan: MCU Compatibility Plan](mcu-compat-plan.md) |

---

## 1. Executive Summary & Core Principles

This guide is designed for developers and AI code generation systems working with the **WinkMicroOS 8051 Simulation Layer (`frameworks/mcs51/`)**.

Under Host (Linux/macOS/Windows) and Wasm (WebAssembly browser) simulation environments, the Wink 8051 compatibility layer leverages **modern C++17 operator overloading, virtual register shadow memory, and Level-2 instant hardware traps** to enable **source-level zero-code intrusion** for compiling and executing standard Keil C51 business code.

However, because the host environment uses modern toolchains (GCC/Clang/MSVC/Emscripten) and a single flat linear memory space, historical non-standard compiler dialects, physical Harvard architecture traits, and nanosecond instruction-level timing cannot and need not be fully emulated at the functional simulation level. In accordance with [ADR-0012: Contract Honesty over Silent Degradation](../../decisions/core/0012-contract-honesty-over-silent-degradation.md), this guide exhaustively details all dialect restrictions and practical guidelines.

> [!TIP]
> **STRICT Dual-Mode Mechanism**:
> The framework supports a strict validation mode enabled via CMake `-DWINK_MCS51_STRICT=ON`. In STRICT mode, any code relying on unmodeled features (e.g. Timer external pulse counting, Timer0 Mode 3) triggers an immediate assertion failure (`assert`), making it ideal for CI and regression test suites. In Release mode, unmodeled features degrade gracefully with rate-limited one-time warnings (`warn once`).

---

## 2. Syntax & Compiler Dialect Restrictions

### 2.1 Bit Variable `sbit` Syntax (Both Absolute and Relative Forms Supported)

* **Implementation Fact**:
  The framework defines `#define sbit inline WinkSbit` in [REGX52.H](file:///d:/MyWorkSpace_program/lowcode-nocode/ai-app/wink-ai-embedded/wink-micro-os/frameworks/mcs51/include/REGX52.H).
  `WinkSbit` features a non-explicit `constexpr WinkSbit(int abs_bit_addr)` constructor ([mcs51_proxy.hpp](file:///d:/MyWorkSpace_program/lowcode-nocode/ai-app/wink-ai-embedded/wink-micro-os/frameworks/mcs51/include/mcs51_proxy.hpp)), which derives the base SFR address (`abs & 0xF8`) and GPIO port index (`0x80/0x90/0xA0/0xB0 -> P0..P3`) at compile time.
  Consequently, **both absolute bit-address forms (`0x80~0xFF`) and relative bit-addressing forms (`REG^bit`) are 100% functionally supported**, with full pin trap dispatching and edge notifications.
* **Guideline**:
  All 64 predefined `sbit` declarations in the framework's own `REGX52.H` (`P0_0`..`P3_7`, `CY`, `TF0`, etc.) use the absolute address format. In user business code, relative bit addressing (`sbit LED = P1^0;`) is recommended for clarity and cross-device portability.

```c
/* ✅ Fully Supported: Relative bit addressing (Recommended) */
sbit LED = P1^0;
sbit RELAY = P2^3;
sbit CY_FLAG = PSW^7;

/* ✅ Fully Supported: Absolute bit address format (Standard Keil idiom) */
sbit P1_0 = 0x90;
sbit TF0  = 0x8D;
sbit CY   = 0xD7;
```

---

### 2.2 K&R-Style Function Definitions Strictly Prohibited (Fatal)

* **Reason**: User 8051 source files are compiled as C++17 into the host sandbox. K&R C syntax without parameter type lists is rejected by modern C++ compilers.
* **Rule**: All function definitions must strictly adhere to ANSI C (C89/C99) standard prototypes.

```c
/* ❌ Prohibited (K&R C syntax) */
void DelayMs(ms)
int ms;
{
    /* ... */
}

/* ✅ Correct (ANSI C Prototype) */
void DelayMs(unsigned int ms) {
    /* ... */
}
```

---

### 2.3 Implicit Calls to Undeclared Functions Prohibited (Fatal)

* **Reason**: Implicit function declarations defaulting to return type `int` are forbidden in C++17 (`error: 'foo' was not declared in this scope`).
* **Rule**: Functions must have forward declarations or included header files prior to their invocation.

```c
/* ❌ Prohibited (Undeclared call) */
void main(void) {
    InitTimer0(); // Error: 'InitTimer0' was not declared in this scope
}
void InitTimer0(void) { /* ... */ }

/* ✅ Correct (Forward declaration provided) */
void InitTimer0(void);

void main(void) {
    InitTimer0();
}
void InitTimer0(void) { /* ... */ }
```

---

### 2.4 Keil Native Inline Assembly Isolation (Fatal)

* **Reason**: Keil `#pragma asm ... #pragma endasm` blocks contain 8051 assembly mnemonics (`MOV`, `CJNE`, etc.) that host x86/x64/Wasm compilers cannot process. The preprocessing pass (`mcs51_cleanup.py`) does not strip asm blocks.
* **Rule**: Business logic should use pure C. Assembly blocks necessary for hardware Keil targets must be isolated using the predefined Keil macro `__C51__`.

```c
/* ❌ Prohibited (Bare inline assembly) */
void ResetWatchdog(void) {
    #pragma asm
    MOV 0xA6, #0x01
    #pragma endasm
}

/* ✅ Correct (Isolated with __C51__) */
void ResetWatchdog(void) {
#ifdef __C51__
    #pragma asm
    MOV 0xA6, #0x01
    #pragma endasm
#else
    // Handled in simulation by WinkMicroOS
#endif
}
```

---

### 2.5 String Literal to `char*` Conversion (Warning / Diagnostic)

* **Reason**: In C++17, `"OK\r\n"` is typed as `const char[5]`. Passing it to `char*` loses const-qualifiers.
* **Rule**: Use `const char *` in function parameter signatures for portability.

```c
/* ⚠️ Not Recommended */
void UART_SendString(char *str);

/* ✅ Strongly Recommended */
void UART_SendString(const char *str);
```

---

### 2.6 Multi-Byte Source Encoding (UTF-8 with GBK Fallback)

* **Reason**: In legacy GBK-encoded files, trailing characters whose second byte is `0x5C` (`\`) can cause comment line-continuations that swallow the following line of code.
* **Framework Support**:
  1. `mcs51_cleanup.py` automatically attempts UTF-8 decode first with seamless GBK fallback, producing normalized UTF-8 `.cpp` files;
  2. For vendor headers, the `--transcode` CLI mode provides lossless transcoding: `python mcs51_cleanup.py --transcode <in.h> <out.h>`.

---

### 2.7 `bdata` and `sfr16` Dialect Limitations

* **Reason**: Keil C51 keywords `bdata` and `sfr16` are proprietary extensions not erased by generic headers.
* **Rule**:
  1. Replace `bdata` variables with standard C bit-fields or bitmask operations on standard `uint8_t` variables;
  2. Replace `sfr16` access with explicit low-byte/high-byte SFR assignments (e.g. `TL0` and `TH0`).

---

## 3. Memory Model & Pointer Restrictions

### 3.1 Flat Linear Memory vs. Harvard Architecture

* **Reason**: Keil `data`, `idata`, `xdata`, `pdata`, and `code` specify distinct physical memory spaces on 8051. In the host simulation sandbox, memory is unified and flat. Headers define:
  ```c
  #define data
  #define idata
  #define xdata
  #define pdata
  #define code const
  ```
* **Rule**: Do not write code that assumes identical address values in different spaces represent distinct variables (e.g., assuming `data 0x20` and `xdata 0x20` are independent).

---

### 3.2 Accessing SFRs via Raw Pointers Prohibited (Fatal)

* **Reason**:
  1. **8051 Hardware**: SFRs must be accessed via Direct Addressing (`data`). Indirect pointers (`idata * 0x90`) on real silicon access the upper 128B RAM of 8052, not SFRs.
  2. **Simulation Layer**: Named SFRs are C++ proxies (`WinkSfr`). Writing through raw integer pointers bypasses operator overloading and disables hardware traps.
* **Rule**: Always access SFRs via their named identifiers (`P1 = 0x01;`, `P1_0 = 1;`).

---

### 3.3 Variable Placement with `_at_` Unsupported

* **Reason**: Keil `_at_` is erased (`#define _at_(addr)`). Variables are placed normally by the host linker.
* **Rule**: Memory-mapped peripheral buffers must use peripheral driver abstractions rather than fixed `_at_` variables.

---

### 3.4 3-Byte Generic Pointer Tag Inspection Unsupported

* **Reason**: Pointers on host/Wasm architectures are standard flat pointers. Inspecting Keil 3-byte generic pointer tag bytes is unsupported.

---

### 3.5 Resource Capacity & Memory Budget Warnings

> [!WARNING]
> **Simulation False-Pass on Memory Capacity**:
> Host virtual memory is virtually unlimited. Global arrays exceeding physical 8051 limits (e.g. >256B RAM or >64KB XDATA) will compile and run fine in simulation but fail or crash on physical silicon.

* Only `absacc.h` `XBYTE[]` / `XWORD[]` accesses are constrained by the configurable `WINK_MCS51_XDATA_SIZE` (default 8KB).
* Developers must inspect Keil `.map` output to enforce RAM/ROM budgets.

---

## 4. Peripherals, Clock & Timing Model Limitations

### 4.1 Clock Resolution & Microstep Advance (`5µs` Granularity)

* **Fact**: `WINK_MCS51_MICROSTEP_US` is defined as `5u` in [wink_mcs51_clock.h](file:///d:/MyWorkSpace_program/lowcode-nocode/ai-app/wink-ai-embedded/wink-micro-os/frameworks/mcs51/include/wink_mcs51_clock.h). Every intercepted SFR access or `_nop_()` advances virtual time by 5µs.
* **Rule**: Sub-microsecond protocols relying on counting `_nop_` loops (e.g. WS2812 bit-banging) cannot be accurately simulated. Use standard ADC peripherals for sensor measurements.

---

### 4.2 Continuous Analog RC Charge/Discharge Unsupported

* **Reason**: GPIO pins are modeled with discrete logic levels (0/1). Analog RC integration curves are not simulated.
* **Rule**: Use an ADC (external **ADC0832** or on-chip **CMS8S** ADC) for temperature sensing.

---

### 4.3 Hardware `PSW` ALU Flags Not Updated by C Arithmetic

* **Reason**: C mathematical operations map directly to host ALU instructions and do not update the virtual `PSW` register (`CY`, `OV`, `AC`, `P`).
* **Rule**: Use standard C expressions for overflow checks rather than reading `CY`.

---

### 4.4 CMS8S78xx On-Chip 12-Bit ADC Model (ADR-0073)

* **Fact**: Modeled accurately against vendor register specs (`ADCON0@0xDF`, `ADCON1@0xDE`, `ADCCHS@0xD9`, `ADRESH@0xDD`, `ADRESL@0xDC`). ADC conversion completes synchronously upon writing `ADGO=1` (0-cycle passthrough).
* **XSFR Access**: XSFR registers (`ADCLDO@0xF692`, `PxxCFG@0xF000`) are safely accessed via `xsfr ADCLDO(0xF692);` in [REG_CMS8S.H](file:///d:/MyWorkSpace_program/lowcode-nocode/ai-app/wink-ai-embedded/wink-micro-os/frameworks/mcs51/include/REG_CMS8S.H).

```c
/* ✅ Standard CMS8S78xx ADC Sampling */
#include <REG_CMS8S.H>

uint16_t Read_ADC_Channel(uint8_t ch) {
    ADCCHS = ch;
    ADCON1 |= 0x80;
    ADCON0 = 0x40;  // Right aligned
    ADCON0 |= 0x02; // Start conversion
    while (ADCON0 & 0x02);
    return (uint16_t)(0x0FFF & ((ADRESH << 8) | ADRESL));
}
```

---

### 4.5 GPIO Quasi-Bidirectional Port Model & RMW Semantics (ADR-0077)

* **Implementation**:
  1. **Initialization**: P0~P3 latches are seeded with `0xFF` (`WEAK-HIGH`);
  2. **Read-Pin vs Read-Latch**: Direct reads (`val = P1;`) sample the external pin level;
  3. **Read-Modify-Write (RMW)**: Compound assignments (`P1 |= 0x01;`, `P1 &= ~0x02;`, `P1++`) **strictly read the latch shadow (Read-Latch)**, matching silicon `ANL`/`ORL` behavior and preventing externally-held-low inputs from corrupting latches;
  4. **Drive Strength**: Latch 1 outputs weak pull-up (`MCS51_DRIVE_WEAK`), latch 0 outputs strong pull-down (`MCS51_DRIVE_SUPPLY`).
* **Idiom**: Setting `P1 = 0xFF;` before reading inputs works identically in simulation and hardware.

---

### 4.6 UART Communication Model (ADR-0065 / ADR-0076)

* **TX**: Writing `SBUF = c;` dispatches the byte immediately and synchronously sets `TI` before returning (`while(!TI); TI = 0;` completes immediately).
* **RX**: Injected bytes queue in a Pending FIFO and drain at fiber microstep points, respecting `SCON.REN`, triggering Vector 4, and dropping bytes upon unserviced `RI` overflow.

---

### 4.7 Unmodeled Peripherals Summary

1. **Timer2**: Not declared in `REGX52.H` and not modeled. Use Timer0/Timer1;
2. **Timer0 Mode 3**: Unmodeled (triggers `MCS51_FEAT_TIMER_MODE3` assert in STRICT mode);
3. **Timer External C/T Clocking**: Unmodeled (triggers `MCS51_FEAT_TIMER_EXT_CLK` assert in STRICT mode).

---

## 5. Interrupt System & Fidelity Pitfalls

### 5.1 ISR Translation & Vector Capacity

* **Syntax**: `void Timer0_ISR(void) interrupt 1 [using 1]` is automatically rewritten by `mcs51_cleanup.py` to `WINK_ISR(N)` (stripping `using M`).
* **Vector Table**: Supports **28 interrupt vectors** ([wink_mcs51_isr.h](file:///d:/MyWorkSpace_program/lowcode-nocode/ai-app/wink-ai-embedded/wink-micro-os/frameworks/mcs51/include/wink_mcs51_isr.h)), covering standard vectors 0~4 and enhanced vectors like CMS8S ADC (vector 19).

---

### 5.2 Dispatch Model & Execution Gating

1. Synchronous dispatch on the fiber context; no nested interrupts; `IP` priority unmodeled;
2. Execution phase gate (`s_interrupts_enabled`) prevents dispatch during static initialization;
3. No yielding occurs inside ISR execution (`wink_mcs51_in_isr()`).

---

### 5.3 🚨 Critical Pitfall: Missing `volatile` False-Pass Warning

> [!CAUTION]
> **Severe "Simulation Passes, Hardware Hangs" False-Pass**:
> On physical 8051 silicon, variables shared between ISRs and main loops **must be declared `volatile`**. Without `volatile`, Keil compiler optimizations will hoist variable reads out of `while(!flag)` loops into an infinite loop.
>
> In simulation, because ISRs are invoked via opaque external function pointers, the host C++ compiler cannot assume globals are invariant, causing the loop to **falsely succeed in simulation even without `volatile`**!
>
> **Mandatory Rule**: Always declare all ISR-shared flags and variables as `volatile`.

```c
/* ❌ Dangerous (Missing volatile: works in sim, hangs on hardware!) */
uint8_t timer_flag = 0;

void Timer0_ISR(void) interrupt 1 {
    timer_flag = 1;
}

void main(void) {
    InitTimer0();
    while(!timer_flag); // Hangs on hardware!
}

/* ✅ Correct (Safe across both simulation and hardware) */
volatile uint8_t timer_flag = 0;

void Timer0_ISR(void) interrupt 1 {
    timer_flag = 1;
}

void main(void) {
    InitTimer0();
    while(!timer_flag);
}
```

---

### 5.4 🚨 Critical Pitfall: Static Overlay Memory Corruption & Non-Reentrancy (Keil L15)

> [!CAUTION]
> **Extremely Deceptive "Simulation Has Isolated Stacks, Hardware Corrupts Memory" False-Pass**:
> 8051 hardware stack depth is very shallow (typically only a few dozen bytes). Keil C51 **does not use dynamic stack frames for local variables or parameter passing** by default. Instead, the linker performs static call tree analysis (Static Data Overlaying) and maps mutually-exclusive function locals to fixed, shared `DATA/IDATA` absolute addresses.
>
> In Host/Wasm simulation, every function invocation runs on modern OS native dynamic stacks (Native Stack Frame). Even if a utility function (e.g. `CalcCRC()`, `FormatBuffer()`) is concurrently called from both `main()` and an ISR, they execute in isolated stack frames and **pass 100% of simulation tests without error**.
>
> However, on physical silicon, if an interrupt triggers while `main()` is inside a function and the ISR also invokes that same function, the ISR **overwrites and corrupts the static local variables currently in use by `main()`**! Upon ISR return, `main()` resumes with clobbered locals, causing erratic crashes and silent memory corruption. Keil only emits an easily-overlooked `WARNING L15: MULTIPLE CALL TO SEGMENT`.
>
> **Mandatory Rule**:
> 1. **Never share non-reentrant helper functions between the main execution thread and an ISR call tree**;
> 2. Split shared logic into two uniquely-named functions (e.g. `CalcCRC_Main` and `CalcCRC_ISR`), guard the call site in `main()` with global interrupt disabling (`EA = 0; ... EA = 1;`), or explicitly declare the function as Keil `reentrant` (noting Keil simulated stack overhead).

```c
/* ❌ Highly Dangerous (Shared function between main and ISR: passes in sim, corrupts locals on silicon!) */
uint16_t CalcCRC(const uint8_t *buf, uint8_t len) {
    uint16_t crc = 0xFFFF;
    uint8_t i; // Fixed static overlay address in Keil DATA
    for (i = 0; i < len; i++) {
        crc ^= buf[i];
    }
    return crc;
}

void Timer0_ISR(void) interrupt 1 {
    uint16_t isr_crc = CalcCRC(isr_buf, 4); // Clobbers crc and i belonging to main!
}

void main(void) {
    while(1) {
        uint16_t main_crc = CalcCRC(main_buf, 16); // Interrupted midway -> memory corrupted!
    }
}

/* ✅ Correct Approach 1 (Separate dedicated functions) */
uint16_t CalcCRC_Main(const uint8_t *buf, uint8_t len) { /* ... */ }
uint16_t CalcCRC_ISR(const uint8_t *buf, uint8_t len)  { /* ... */ }

/* ✅ Correct Approach 2 (Critical section protection in main) */
EA = 0;
main_crc = CalcCRC(main_buf, 16);
EA = 1;
```

---

### 5.5 Multi-Byte Shared Variable Data Tearing (8-Bit Bus Limits)

* **Mechanism**:
  The 8051 is a pure 8-bit bus architecture. Reading or writing a 16-bit (`uint16_t`/`int`) or 32-bit (`uint32_t`/`long`) variable requires multiple 8-bit instructions (`MOV`).
  If `main()` reads a 16-bit tick counter `g_sys_ticks`, and an ISR updates `g_sys_ticks` from `0x00FF` to `0x0100` between reading the low and high bytes, `main()` will assemble a corrupted value of `0x01FF` (a 256ms time jump error).
  On 32-bit/64-bit Host and Wasm platforms, multi-byte word reads are single-instruction atomic operations, masking this hardware race condition during simulation.
* **Rule**:
  When reading multi-byte variables updated by an ISR from the main thread, **always wrap the read in a critical section (`EA = 0; ... EA = 1;`)** or implement double-read consistency checking.

```c
/* ❌ Tearing Vulnerability (Non-atomic 16-bit read on 8-bit bus) */
volatile uint16_t g_ms_ticks = 0;

void Timer0_ISR(void) interrupt 1 {
    g_ms_ticks++;
}

void main(void) {
    uint16_t now;
    while(1) {
        now = g_ms_ticks; // Can read 0x01FF at the 0x00FF->0x0100 rollover boundary!
        ProcessTask(now);
    }
}

/* ✅ Correct (Atomic read protected by critical section) */
void main(void) {
    uint16_t now;
    while(1) {
        EA = 0;
        now = g_ms_ticks;
        EA = 1;
        ProcessTask(now);
    }
}
```

---

## 6. Engineering Discipline & Coroutine Health

### 6.1 Header Inclusion & Symbol Remapping

* `REGX52.H` safely pre-includes standard system headers (`<stdint.h>`, `<stdbool.h>`, `<stddef.h>`, `<string.h>`, `<stdio.h>`) before `#define main wink_mcs51_user_main`, ensuring arbitrary inclusion order works cleanly.

---

### 6.2 Fiber Coroutine Quota & Catch-Up Mechanism

* The simulation fiber is allocated a **10ms virtual-time slice** (`WINK_MCS51_QUOTA_US = 10000u`).
* Yielding on quota triggers the Catch-Up mechanism to advance timers and dispatch pending ISRs, conserving 1:1 virtual time with master ticks.
* Business main loops must include real workloads or delay intervals (`delay_ms(10)`).

---

## 7. Quick Troubleshooting Matrix

| Compiler Error / Runtime Warning | Root Cause | Section | Quick Fix |
| :--- | :--- | :--- | :--- |
| `error: 'InitTimer0' was not declared in this scope` | Undeclared function call | **§2.3** | Add forward prototype `void InitTimer0(void);` |
| `error: expected ';' before 'asm'` | Bare Keil inline assembly | **§2.4** | Guard assembly using `#ifdef __C51__` |
| `error: invalid conversion from 'const char*' to 'char*'` | String literal to `char*` | **§2.5** | Change parameter to `const char *str` |
| `warning: multi-line comment [-Wcomment]` | GBK backslash line swallow | **§2.6** | Save as UTF-8 or append space after comment |
| `error: unknown type name 'bdata' / 'sfr16'` | Keil proprietary keywords | **§2.7** | Replace with standard C bitmasks or byte pairs |
| `error: 'T2CON' was not declared in this scope` | Timer2 registers used | **§4.7** | Timer2 is unmodeled; switch to Timer0/Timer1 |
| `WINK_WARN_WCET_EXCEEDED` (Runtime 8002) | Loop >5,000µs without yield | **§6.2** | Insert `_nop_()` or `delay_ms()` |
| **Hardware hangs while simulation succeeds** | Missing `volatile` on ISR flag | **§5.3 (🚨)** | Add `volatile` to shared global variables |
| **Hardware crashes/corrupts data while sim passes** | Shared function called by main & ISR (Static Overlay) | **§5.4 (🚨)** | Split into separate functions or guard with `EA=0/EA=1` |
| **Multi-byte variable reads corrupted values on hardware** | Unprotected 16/32-bit read across ISR (Data Tearing) | **§5.5** | Wrap read in `EA=0; ... EA=1;` critical section |

---

## 8. Simulation Fidelity Boundary Matrix

| Domain | Behavioral High-Fidelity (Fully Reliable ✅) | Simplified / Unmodeled (Do Not Rely ⚠️) |
| :--- | :--- | :--- |
| **GPIO / Ports** | • Quasi-bidirectional default (0xFF + WEAK-HIGH)<br>• Pin level reads (Read-Pin)<br>• RMW compound assignments strictly read latches (Read-Latch)<br>• Edge detection & instant UniSim PinArbiter dispatch | • Analog continuous RC charge/discharge<br>• Nanosecond transient overshoot & gate propagation delay |
| **Clock & CPU** | • 5µs microstep stepping<br>• 10ms virtual quota slice & Catch-Up conservation<br>• `delay_ms()` millisecond timing consistency | • 12-T single machine cycle nanosecond timing (WS2812 nop loop)<br>• Instruction cycle variance |
| **ADC** | • CMS8S78xx register map (ADCON0/ADFM/ADGO)<br>• ADC0832 SPI external model<br>• 0-cycle instant conversion | • Internal AN63 (BGR/temperature) unmodeled<br>• Sample-and-hold circuit settling time |
| **UART** | • Synchronous TI assertion on SBUF write<br>• RX Pending FIFO fiber microstep drain<br>• SCON.REN gating & Vector 4 dispatch<br>• Hardware overflow drop simulation | • Nanosecond baud rate physical waveforms<br>• Hardware Parity Error generation |
| **Interrupts** | • 28 interrupt vectors auto-registration<br>• Phase gate isolation during startup<br>• Timer0/Timer1 overflow triggering | • Interrupt preemption & nesting<br>• `IP` priority registers<br>• `using M` register bank switching<br>• Keil Static Overlay memory corruption across ISRs (masked by native stack) |
| **Memory & ALU** | • Flat linear RAM mapping<br>• `XBYTE[]` 8KB bounds check<br>• Standard C arithmetic consistency | • Physical Harvard overlapping address space<br>• Hardware `PSW` flags updated by C arithmetic<br>• Compile-time RAM/ROM overflow checks<br>• Compiler `volatile` hoisting bugs (masked in sim)<br>• Non-atomic multi-byte data tearing on 8-bit bus (masked by 32/64-bit atomicity) |

---

## 9. Conclusion & Compliance Validation

Code compliant with this guide guarantees:
1. **In Keil C51**: Flawless compilation and burning onto physical silicon (AT89C52, CMS8S78xx);
2. **In Wink Simulation**: Zero-code-change native compilation across Host and Wasm for automated CI testing and high-fidelity physics-loop simulation.
