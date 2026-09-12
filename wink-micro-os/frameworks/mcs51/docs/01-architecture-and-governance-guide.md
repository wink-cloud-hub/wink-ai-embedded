# MCS-51 仿真拦截层架构治理与开发维护规范

| 字段 | 内容 |
|------|------|
| **适用模块** | `wink-micro-os/frameworks/mcs51/` |
| **文档编号** | `GUIDE-MCS51-GOVERNANCE-v1.0` |
| **发布日期** | `2026-09-12` |
| **状态** | 🟢 Active（正式执行） |
| **关联 ADR** | [ADR-0004](../../../../docs/design/decisions/0004-static-dispatch-vs-runtime-ops.md)（编译期静态分发，POD+命名 API）、[ADR-0070](../../../../docs/decisions/core/0070-mcs51-zero-code-simulation-interception-layer.md)（拦截总纲）、[ADR-0071](../../../../docs/decisions/core/0071-sfr-proxy-rmw-edge-data-plane.md)（SFR 数据面）、[ADR-0072](../../../../docs/decisions/core/0072-dual-clock-domain-and-quota-catchup.md)（双时钟域）、[ADR-0073](../../../../docs/decisions/core/0073-cms8s-adc-real-register-map-supersedes-ssot.md)（真实寄存器图与 0 周期即时）、[ADR-0074](../../../../docs/decisions/core/0074-mcs51-channel1-external-read-pin.md)（Read-Pin 外部缝）、[ADR-0075](../../../../docs/decisions/core/0075-mcs51-production-wasm-target-headless.md)（生产 Wasm 链接）、[ADR-0077](../../../../docs/decisions/core/0077-gpio-write-drive-strength-axis.md)（准双向口驱动强度）、[ADR-0078](../../../../docs/decisions/core/0078-mcs51-two-phase-irq-and-in-service-masking.md)（中断两阶段挂起与在服务屏蔽）、[ADR-0043](../../../../docs/design/decisions/0043-arch-lint-rules.md)（分层门禁） |
| **适用对象** | 框架内核开发者、芯片自治包贡献者、板级扩展器件开发者、自动化 AI Coding Agents |

---

## 1. 概述与核心哲学

`frameworks/mcs51/` 是 WinkMicroOS 的 8051 零代码仿真拦截层（Axis B）。其核心价值是在 Host（MSVC/GCC）和浏览器 Wasm（Emscripten）沙箱内，以极高保真度运行未经修改的 Keil C51 / SDCC 固件，同时与 UniSim 的虚拟引脚总线（PinArbiter / `js_pal_*`）实现双向事件闭环。

为了防止多芯片扩展过程中通用内核滑向 `if-else` 与宏污染泥潭，本框架确立以下三大核心工程哲学：
1. **编译期静态分发（ADR-0004）**：坚持 POD 结构体 + 静态表驱动 + 命名式 API。绝不使用 C++ 虚函数表（vtable）、RTTI、C++ 异常或 Linux 风格的重型 `container_of` 侵入式链表。
2. **零厂商污染通用内核（Zero-Vendor Core）**：通用核心代码严禁出现任何特定厂商词汇或专用寄存器；差异性一律由描述符能力掩码或 Per-Context Trait 钩子承载。
3. **机械化门禁优于口头约定（Gate over Convention）**：所有架构红线必须转化为自动化 Lint 脚本与 CI 编译断言，违规即阻断。

---

## 2. 架构分层与单向依赖铁律

系统严格分为五层，依赖方向**严格单向递进，严禁反向污染与环形依赖**：

```text
       ┌────────────────────────────────────────┐
       │ Layer 5: 配置事实源与测试治理           │
       │ (tools/manifests/chips/, tools/lint/)  │
       └───────────────────┬────────────────────┘
                           │ 驱动
                           ▼
 ┌──────────────────────┐     ┌──────────────────────┐
 │ Layer 3: 厂商芯片自治包 │     │ Layer 4: 板级外挂器件 │
 │ (chips/<family>/)    │     │ (devices/<device>/)  │
 └──────────┬───────────┘     └──────────┬───────────┘
            │ 链接期自注册 / Trait 钩子    │ Trap 钩子接入
            ▼                            ▼
 ┌───────────────────────────────────────────────────┐
 │ Layer 1 & 2: 通用内核核心层与家族能力描述符         │
 │ (wink_mcs51_core: include/, src/)                 │
 └─────────────────────────┬─────────────────────────┘
                           │
                           ▼
 ┌───────────────────────────────────────────────────┐
 │ WinkMicroOS 基础运行时 (PAL / DAL / EventQueue)   │
 └───────────────────────────────────────────────────┘
```

### 2.1 分层职责与边界

1. **Layer 1 通用内核核心层 (`wink_mcs51_core`)**：
   * 仅包含 Intel 8051/8052 标准寄存器与外设时序（PC/SP/P0~P3 标准准双向口/Timer 0~2 标准骨架/UART0/INT0~1/外部 MOVX 总线冲突检测）。
   * **绝不包含**任何厂商专属寄存器（如 CMS8S 的 `PxxCFG`, `PxTRIS`, `FUNCCR`, `ADCLDO`, `WDT` 等）。
2. **Layer 2 家族描述符与能力钩子层 (`McuFamilyDescriptor v2` & `Trait Hooks`)**：
   * 物理归属 Core。包含静态事实表（[mcs51_family.h](../include/mcs51_family.h)）与 `caps_cache` 快速短路掩码。
   * 为增强特性提供基于上下文的轻量函数指针钩子（`gpio_hooks`, `uart_hooks`, `irq_map_extend`, `xsfr_validate` 等）。
3. **Layer 3 厂商芯片自治包 (`chips/<family>/`)**：
   * 编译为独立的静态目标（如 `wink_mcs51_cms8s`）与自注册对象（`wink_mcs51_cms8s_register`）。
   * 承接芯片专属 SFR、扩展定时器/ADC/看门狗，并通过 `soc_priv` 绑定私有状态。
4. **Layer 4 板级外挂器件层 (`devices/<device>/`)**：
   * 独立编译为 `wink_mcs51_<device>`（如 `adc0832`），纯粹通过 Level-2 Pin Trap 钩子挂载，与 Core 及 Chips 保持正交绝缘。
5. **Layer 5 配置事实源与测试治理层 (`tools/manifests/` + `test/`)**：
   * 芯片元数据由 `tools/manifests/chips/*.yaml` 唯一声明；测试代码按 `test/core/` 与 `test/<family>/` 物理隔离。

### 2.2 命名规约（Naming Conventions）

* **通用核心符号**：统一冠以 `wink_mcs51_*` 或 `mcs51_*`（如 `wink_mcs51_microstep()`, `mcs51_gpio_read_pin()`）；
* **芯片专属符号**：统一冠以 `<family>_*`（如 `cms8s_adc_init()`, `cms8s_gpio_may_drive()`）；
* **板级器件符号**：统一冠以 `<device>_*`（如 `adc0832_device_attach()`）；
* **内部实现符号**：必须置于匿名命名空间（`namespace { ... }`）内，严防外部符号泄漏。

---

## 3. 状态所有权与内存预算规范

### 3.1 内存池化与全局状态禁令（Scheme A 范式）

* ❌ **严禁使用运行时 `file-static` 状态**：芯片模型中的所有可变硬件状态（计数器、采样历史、状态机相位）**严禁**声明为文件级 `static` 全局变量（这会导致多上下文或复位串扰）。
* ✅ **Scheme A 内存池标准**：
  1. 芯片私有状态定义为 POD 结构体（如 `Cms8sPriv`）；
  2. 在其 `*_register.cpp` 中定义定长 BSS 池：
     ```cpp
     static Cms8sPriv s_cms8s_priv_pool[MCS51_MAX_INSTANCES];
     ```
  3. 在芯片复位/初始化时，根据当前 `ctx->instance_index` 绑定至 `ctx->soc_priv`；
  4. 索引访问必须使用安全截断（Clamping）而非断言：
     ```cpp
     const uint8_t idx = (ctx->instance_index < MCS51_MAX_INSTANCES)
                             ? ctx->instance_index
                             : (MCS51_MAX_INSTANCES - 1u);
     ```
* 🟢 **静态变量唯一豁免范围**：
  * 只读的 `const` 静态配置表（如 `kCms8sDescs[]`, `AN_TO_PIN[]`）；
  * 进程级 M4 诊断计数器（如 `s_oob_count`, `s_bus_conflict`），用于测试汇总。

### 3.2 上下文容器 `Mcu51Context` 预算锁死

* [Mcu51Context](../include/mcs51_context.h) 是仿真核的唯一核心容器（内含 64KB XDATA 与 256B SFR 影子）。
* **预算红线**：MinGW GCC / MSVC 架构下，`sizeof(Mcu51Context)` 必须严格受控（基线约 75,656 字节），由单元测试 `test_mcs51_context_budget` 强制卡死。
* **严禁滥增字段**：新增字段必须优先复用现有对齐填充间隙；禁止将芯片专有大块结构塞入通用上下文，必须通过 `soc_priv` 挂载。

---

## 4. 外设建模与 Trap 钩子红线（AD-13）

所有接入通用总线的外设与仿真模型，必须严格遵守 **AD-13 四条红线**：

1. **绝对非阻塞（Zero Delay/Blocking）**：回调内部严禁调用任何形式的 `delay()`、`sleep()` 或操作系统级等待原语；
2. **绝不主动 Yield**：禁止在 Trap 处理中强行切换或让出 Fiber 协程；
3. **纯状态机推进（Pure FSM）**：只允许根据当前输入修改自身状态或 `sfr_shadow`，绝不自行推进 `virtual_us` 虚拟时钟；
4. **即时完成性（Instant Completion）**：Native 仿真模式下，外设操作必须在 0 周期内完成状态收敛。

### 4.1 影子写优先原则（Shadow Store First）
* 当固件写 SFR 时，代理层保证**先将新值写入 `sfr_shadow[addr]`，随后才调用写钩子 `hook(ctx, addr, old_val, new_val)`**；
* 仅具备特殊硬件语义的寄存器（如写 0 清零的 W0C 标志位）才允许在钩子内重写影子：
  ```cpp
  // W0C 典型实现
  ctx->sfr_shadow[addr] = old_val & new_val;
  ```
* 仅观察型外设（如 Timer 模式监听）严禁篡改 `sfr_shadow`。

### 4.2 诊断上报契约（Diagnosis-by-Counter）
* 仿真拦截层严禁向未经修改的固件抛出 `wink_status_t` 错误码（硬件 SFR 无错误码概念）。
* **标准处理模式**：
  * **Release 模式**：累加专属诊断计数器（如 `cms8s_adc_synth_misuse_count`），配合 `pal_log_w` 仅报警一次（Warn-once），并回退到失效安全状态（Fail-safe Sentinel）；
  * **STRICT 模式（`WINK_MCS51_STRICT`）**：触发断言并立即 `std::abort()` 熔断，确保 CI 期间暴露任何非法行为。

---

## 5. 新芯片自治包接入标准流程（SOP）

为保证“加新芯片零改 Core（Zero-touch Core）”，新芯片接入必须严格按以下 5 步执行：

```text
Step 1: 声明 Manifest
        tools/manifests/chips/<family>.yaml
        │
        ▼
Step 2: 声明描述符
        include/mcs51_family.h (增 ID) + src/mcs51_family.cpp (增静态行)
        │
        ▼
Step 3: 建立芯片包
        chips/<family>/include/ + chips/<family>/src/
        │
        ▼
Step 4: 编写链接期自注册
        chips/<family>/src/<family>_register.cpp
        │
        ▼
Step 5: 零改 CMake 自动发现
        CMakeLists.txt 自动探测并注册 wink_mcs51_<family>
```

### Step 1: 建立芯片事实源
在 `tools/manifests/chips/<family>.yaml` 中声明芯片头文件路径、Core 禁词正则、内存边界和 SDCC 门禁参数。

### Step 2: 登记能力描述符
在 [mcs51_family.h](../include/mcs51_family.h) 中增加 `MCS51_FAMILY_<NAME>` ID，并在 [mcs51_family.cpp](../src/mcs51_family.cpp) 的描述符静态表中追加一行配置（主频、CKCON 种子、XRAM 尺寸、引脚掩码、向量表指针、能力位 `capabilities`）。

### Step 3: 实现芯片模型包
创建 `chips/<family>/` 目录，封装专有 SFR 与外设模型。需要重载行为时，在初始化时挂载 Trait 钩子：
* GPIO 增强方向/模拟：挂载 `ctx->gpio_hooks`；
* UART 多时钟/重映射：挂载 `ctx->uart_hooks`；
* 中断向量扩展：挂载 `ctx->irq_map_extend` 与 `ctx->irq_flag_predicate`；
* 专有 MOVX 寄存器：挂载 `ctx->xsfr_validate`。

### Step 4: 链接期自注册（Link-time Self-Registration）
在 `chips/<family>/src/<family>_register.cpp` 中定义静态自注册实例：
```cpp
namespace {
[[maybe_unused]] const bool s_register_at_link =
    (<family>_register(), true);
}
```

### Step 5: 验证 CMake 自动发现
框架根目录的 [CMakeLists.txt](../CMakeLists.txt) 会自动探测 `chips/*` 目录：
* 自动剥离 `*_register.cpp` 生成独立的 `OBJECT` 库（如 `wink_mcs51_<family>_register`），避免静态库符号修剪导致的自注册丢失；
* 自动生成 `wink_mcs51_<family>` 静态库与 `wink_mcs51_inject_<family>` 接口；
* **完全无需修改根目录的 `CMakeLists.txt`**。

---

## 6. 自动化门禁体系（Quality & Anti-Erosion Gates）

为彻底杜绝“新加 Lint 不知放哪里、归属不清”的维护痛点，本框架确立 **FW 框架内核自治（Framework Governance）** 与 **APP 应用固件合规（Application Carrier）** 双主格标准化命名体系：

* **FW 域（`gate_fw_*` / `lint_fw_*`）**：受检主体为 `frameworks/mcs51/` 自身源码，目的是防止通用内核腐化、状态泄漏与内存超标；
* **APP 域（`gate_app_*` / `transpile_app_*`）**：受检主体为 `wink-micro-app/` 用户 C51 固件，目的是语法清洗转译与硬件物理容量卡死。

### 6.1 门禁命名标准与工具映射对照表

| 分类域 | 标准化规范命名 (Canonical) | 现行实现载体 (Implementation) | 触发时机 | 受检主体 (Input Target) | 核心职责与判定标准 |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **FW 域**<br>(框架自治) | **`lint_fw_core_isolation`** | `tools/lint/lint_fw_core_isolation.py` | 预提交 / CI 静态 | `frameworks/mcs51/include/`<br>`frameworks/mcs51/src/` | **内核绝缘**：扫描通用 Core 源码，命中 `manifests/chips/*.yaml` 中的厂商专有词（`cms8s`, `0xF0xx` 等）立即报错阻断。 |
| **FW 域**<br>(框架自治) | **`gate_fw_context_budget`** | `test/core/test_mcs51_context_budget.cpp` | CTest 编译期 | `Mcu51Context` 结构体定义 | **内存预算**：断言上下文体积上限（MinGW/MSVC 锁定 ~75.6KB），防虚胖。 |
| **FW 域**<br>(框架自治) | **`gate_fw_platform_isolate`** | ESP32 构建产物 ELF 符号扫描 (CMake) | 交叉编译 CI | ESP32 构建产物 ELF | **平台绝缘**：断言真实硬件固件中 `mcs51` 相关符号绝对为 0。 |
| **FW 域**<br>(框架自治) | **`gate_fw_dual_track_test`** | CTest `test_mcs51_*` 双轨测试组 | CTest 运行期 | `test/core/` 与 `test/<family>/` | **测试隔离**：通用单测（40项）与芯片专属单测（16项）双轨独立全绿。 |
| **FW 域**<br>(*规划新增*) | **`lint_fw_chip_state`** | — (规划新增模板) | CI 静态扫描 | `chips/<family>/src/` | **状态池化**：检查芯片自治包内严禁出现非 const 的 `file-static` 状态变量。 |
| **APP 域**<br>(应用合规) | **`transpile_app_keil_c51`** | `tools/transpile_app_keil_c51.py` | 仿真构建期 | `wink-micro-app/**/*.c` | **方言转译**：Keil 中断语法 `interrupt N` 重写、归一化 `<wink_mcu.h>`、注入空死循环 `_nop_()` 防纤程饥饿。 |
| **APP 域**<br>(应用合规) | **`gate_app_hardware_capacity`**| `tools/gate_app_hardware_capacity.py` | 本地/CI 门禁 | `wink-micro-app/` 目录 | **容量溢出**：调 SDCC 检查用户代码 Flash/RAM 占用是否超限，且堆栈空间 $\ge 32\text{B}$。 |
| **APP 域**<br>(应用合规) | **`gate_app_api_layering`** | `wink.py lint arch --pack layering --pack api` (外仓) | 提交前检查 | `wink-micro-app/**/*.c` | **跨层阻断**：禁止用户应用调用未公开的底层私有符号。 |

### 6.2 新增门禁与 Lint 的 3 秒归宿决策树

后续新增任何检查或约束规则时，严格按以下决策树命名并落位：

```text
               ┌──────────────────────────────────────────────┐
               │         【第 1 步：确定受检主体是谁？】       │
               └──────────────────────┬───────────────────────┘
                                      │
                 ┌────────────────────┴────────────────────┐
                 ▼                                         ▼
       受检目标是 SDK 框架自身代码                  受检目标是用户业务应用代码
      (frameworks/mcs51/ 内部)                   (wink-micro-app / samples)
                 │                                         │
                 ▼                                         ▼
          【归入 FW 域】                             【归入 APP 域】
      前缀固定: gate_fw_* 或 lint_fw_*          前缀固定: gate_app_* 或 transpile_*
                 │                                         │
                 ├──────────────────────────┐              ├──────────────────────────┐
                 ▼                          ▼              ▼                          ▼
           [静态文本扫描]             [编译/内存断言]      [语法/AST转译]             [真机物理容量]
        lint_fw_<check_name>       gate_fw_<check_name>   transpile_app_*      gate_app_hardware_*
        (置于 tools/lint/)         (置于 test/core/)      (置于 tools/)        (置于 tools/)
```

---

## 7. 框架维护者日常自查清单（Checklist）

在提交任何针对 `frameworks/mcs51/` 的代码变更前，请逐条对照本清单自检：

- [ ] **Core 纯洁度**：我是否在 `include/` 或 `src/` 中添加了芯片特定的宏名、寄存器名或地址？（**红线：严禁添加，必须改用描述符或钩子**）
- [ ] **全局状态**：我是否在外设模型中引入了非 const 的文件级 `static` 变量？（**红线：严禁引入，必须移至 `soc_priv` 结构体**）
- [ ] **内存预算**：我是否修改了 `Mcu51Context`？若修改，`ctest -R test_mcs51_context_budget` 是否依然通过？
- [ ] **Trap 行为**：新增的回调函数是否满足“非阻塞、不主动 yield、纯状态机、0 周期完成”四条红线？
- [ ] **新芯片隔离**：若添加了新芯片，是否在**完全不修改通用 Core 代码**的前提下通过了所有测试？
- [ ] **门禁验证**：本地运行以下命令是否全部通过：
  ```bash
  # 1. 运行框架内部内核纯洁度门禁
  python frameworks/mcs51/tools/lint/lint_fw_core_isolation.py

  # 2. 运行框架核心与各芯片双轨测试集
  ctest -R "test_mcs51_|test_transpile_app_keil_c51_unit|test_fw_core_isolation_gate" --output-on-failure
  ```
