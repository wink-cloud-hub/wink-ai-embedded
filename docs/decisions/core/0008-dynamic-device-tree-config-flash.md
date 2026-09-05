# ADR-0008：基于 Flash (SPIFFS) 动态设备树配置的免编译快速调试逃生通道

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳）** |
| 日期 | 2026-06-27（Proposed）／ 2026-06-28（Accepted） |
| 触发 | 硬件即时调试痛点：修改引脚或外设基本参数会触发云端重编译与全量烧录（分钟级延迟） |
| 影响范围 | `device_tree` 生成器、`targets/esp32` (及其他富资源 targets)、`dal` 初始化流程、Web 工作台 WebSerial 通信协议 |
| 决策者 | 主架构师及内核开发团队 |

---

## 1. 背景 (Context)

在 [ADR-0004](./0004-static-dispatch-vs-runtime-ops.md) 中，`wink-micro-os` 采纳了**“编译期静态分发 + 命名式 API + POD 结构体”**的扁平化设备树设计。低代码画布上拖拽生成的引脚和参数（如 `.trig_pin = 4`, `.echo_pin = 5`）会被静态编译进入 `device_tree.c`。

### 带来的挑战：
虽然这一决策极大地提升了 AI 生成代码的安全性和 Wasm 仿真的性能，但也带来了一个严重的副作用：**配置修改的重编译与烧录开销 (Recompile & Flash Overhead)**。
* 即使在硬件连线上只是微调了一个 GPIO 引脚，或者微调了舵机的脉宽限制，用户都必须经历：**重新导出 `device_tree.c` ➔ 触发云编译 Docker 构建 ➔ 跨串口下载数十KB甚至数百KB的完整固件 bin** 的完整闭环。
* 这使得在真机上调试硬件的反馈环长达 **1 ~ 2 分钟**，阻碍了“即插即用、即改即现”的原型迭代快感。对于需要频繁微调物理参数（引脚冲突解决、传感器微调）的开发者，这是极差的开发体验。

因此，我们需要在维持“编译期静态安全”和“两端同源”的同时，为资源丰富的开发板（如 ESP32、高容量 STM32）提供一个**免编译、秒级生效的动态配置逃生通道**。

---

## 2. 方案比选 (Options)

### 方案 A：维持现状（全量重编译）
* **做法**：任何配置变更均通过 Codegen 重新编译 `device_tree.c` 并下发固件。
* **优点**：固件极简，不消耗任何额外的 Flash 读取、解析代码和 RAM，对极受限芯片（如 ATmega328P）最友好。
* **缺点**：调试效率低，用户体验差。

### 方案 B：全面动态化（配置文件引导，不生成静态实例）
* **做法**：完全取消 `device_tree.c` 静态分配。在 Flash 文件系统中存放一个 `device_tree.json`，在启动时动态分配内存（`malloc`）来构建外设链表和设备实例。
* **优点**：配置彻底灵活。
* **缺点**：违背了 `wink-micro-os` **“零动态内存分配”** 的工业级安全纪律；在低资源单片机上无法运行，导致多芯片移植时的代码分裂。

### 方案 C：混合自适应模式：静态 POD 占位 + Flash 配置文件动态覆盖（采纳方案）
* **做法**：
  1. 依然在 `device_tree.c` 中静态分配 POD 实例，并填入默认配置。
  2. 在富资源 target（如 `targets/esp32`）的 `device_tree_init_all()` 阶段，系统尝试从 Flash 分区（如 SPIFFS / LittleFS）读取轻量化的配置文件（如 `dev_tree.conf` 或 `dev_tree.bin`）。
  3. 如果文件存在，则将解析出的物理引脚及配置参数**动态覆写（Overwrite）**到已静态分配的 POD 实例中，然后再调用 PAL 进行 GPIO/总线的物理初始化。
  4. 如果文件不存在，则直接按默认的编译期静态参数运行（优雅降级）。
  5. 修改配置时，前端工作台直接通过 WebSerial 将几十字节的配置文件写到 Flash 的特定扇区（耗时 < 100ms），然后触发 MCU 软复位，实现秒级热重载。
* **比选结论**：**方案 C** 在不破坏静态分配纪律和低资源芯片兼容性的前提下，完美解决了富资源芯片的真机调试反馈周期痛点。

---

## 3. 详细设计与实现路径 (Detailed Design)

```
       [ 前端 Web 画布：仅引脚/外设参数变更 ]
                        │
                        ▼ (不触发云编译)
       [ 生成微型配置 dev_tree.bin (数十字节) ]
                        │
                        ▼ (WebSerial 发送自定义指令)
          [ 写入物理板的 Flash 指定扇区 ]
                        │
                        ▼ (软重启 MCU)
       [ 静态 POD 实例装载默认值 ] ──► [ 读取 Flash 动态覆盖 ] ──► [ 执行硬件 init ]
```

### 3.1 动态覆盖时序设计 (Boot-up Lifecycle)

在 `device_tree_init_all()` 的执行流中，插入一个 `PHASE_RESOURCE_OVERWRITE` 阶段：

```c
wink_status_t device_tree_init_all(void) {
    // 1. 装载编译期静态初始值 (由编译器在 .data/.bss 自动完成)
    
    // 2. [新增] 动态配置逃生通道拦截
#if defined(HAS_FLASH_CONFIG_ESCAPE)
    // 尝试读取 Flash 配置文件并覆写静态 POD 实例的 pin/parameter 字段
    if (device_tree_apply_flash_config() == WINK_OK) {
        wink_trace_log("Device tree overwritten from Flash config successfully.");
    }
#endif

    // 3. 执行 PAL 物理总线与 GPIO 绑定初始化
    pal_gpio_init(front_radar.trig_pin, PAL_GPIO_OUTPUT);
    pal_gpio_init(front_radar.echo_pin, PAL_GPIO_INPUT);
    
    // 4. 执行 DAL 状态自检
    dal_ultrasonic_init(&front_radar);
    dal_servo_init(&neck_servo);
    
    return WINK_OK;
}
```

### 3.2 配置文件格式：精简二进制 (Binary Blob) 还是 JSON？

为了在解析效率、Flash 占用和内存消耗之间取得平衡，我们放弃笨重的 JSON 解析器，采用 **Symmetric Binary Config Block (对称二进制块)** 格式。

* **定义结构体**：
  ```c
  #pragma pack(push, 1)
  typedef struct {
      uint32_t magic;           // 校验魔数 (e.g. 0x474E4957 - "WINK")
      uint16_t version;         // 配置结构版本号
      uint16_t total_devices;   // 配置的器件总数
      
      struct {
          uint32_t device_id;   // 外设在设备树中的静态 hash/ID 标识
          uint8_t  params[16];  // 扁平覆写缓冲区 (存放引脚、通道等，由各 DAL 自行反序列化)
      } items[];
  } wink_dev_config_t;
  #pragma pack(pop)
  ```
* **工作原理**：
  * 前端 Codegen 无需编译，直接按上述结构体生成一个二进制文件 `dev_tree.bin`（通常只需几十到上百字节）。
  * 写入 Flash 后，MCU 端的 PAL 层通过一指针映射强转进行零动态分配的解析覆写，极速且不耗 RAM。

### 3.3 WebSerial 通信协议扩展

在 WebSerial 桥接通信中，新增两条命令协议：
1. `WINK_CMD_WRITE_CONF` (CMD: `0x14`)：将下发的二进制流写入 Flash 预留扇区（或挂载在 SPIFFS/LittleFS 上的文件）。
2. `WINK_CMD_CLEAR_CONF` (CMD: `0x15`)：擦除该扇区，使固件行为退回到编译期默认状态。

---

## 4. 虚实一致性与安全约束 (Consequences & Constraints)

引入动态配置通道并非毫无代价，必须遵循以下架构约束以保障系统的稳定性：

### 4.1 仿真与真机的一致性守卫 (Consistency Parity)
* **约束**：WebAssembly 仿真端 (WebSim) 同样需要支持这一动态覆写机制。
* **实现**：前端在 Wasm 实例化后，通过模拟写入 Wasm 虚拟文件系统，或通过 JS Bridge 在 `device_tree_init_all()` 调用前直接篡改 Wasm 导出的全局静态变量符号，保证仿真端和物理真机跑在完全一致的动态引脚配置下。

### 4.2 前端代码生成的引脚冲突静态预校验 (Static Linter Guard)
* **警告**：由于配置不再通过编译器静态检查，用户可能会在画布上配置出有物理冲突的引脚（例如把两个传感器的控制脚都配到 GPIO 4，或者把 PWM 配到了仅输入的输入引脚上）。
* **规避策略**：前端工作台在导出 `dev_tree.bin` 并通过 WebSerial 下发之前，**必须在浏览器端执行严格的引脚分配静态静态校验（Static Linter）**。验证通过后方可下发配置，防止下发错误配置损坏硬件。

### 4.3 优雅降级 (Graceful Degradation)
* **约束**：如果 `dev_tree.bin` 文件损坏、CRC 校验失败或不存在，初始化逻辑必须静默且安全地退回到编译期默认的 `device_tree.c` 设定，绝不允许因此导致 MCU 挂起（Panic）。

### 4.4 资源受限芯片的静态裁剪退路 (Static Fallback for Low-Resource Targets)
* **策略**：针对 Flash 空间极度紧张、无文件系统支持的资源受限 target（如低容量 STM32、AVR），可以通过关闭编译宏 `HAS_FLASH_CONFIG_ESCAPE`，在编译期将 Flash 读取、二进制解析器等逻辑完全裁剪掉。
* **运行机制**：
  * **真机运行**：退化为纯静态分发模式，直接使用 `device_tree.c` 编译期的静态 POD 配置运行，实现零额外开销。
  * **Web 仿真端**：由于不存在资源限制，Web 仿真依然保留 Flash 配置文件动态覆盖能力，在画布调试时无需频繁编译 Wasm 即可快速预览运行效果。

---

## 5. 实现澄清（Accepted 回写，2026-06-28）

> 本节为 ADR-0008 Accepted 时对前文 §3 伪代码的现实校正，**实现以本节为准**。完整任务/测试见
> [实施计划](../../implementation-plans/core/2026-06-28-adr-0008-flash-device-tree-override-plan.md)。
> 核心机制已落地：host 全单测通过（解析器+CRC32+per-DAL apply+端到端覆写），ESP32 NVS 编译通过。

1. **init API 签名（校正 §3.1）**：§3.1 伪代码的 `dal_ultrasonic_init(&front_radar)` / `dal_servo_init(&neck_servo)` 仅为示意。真实 API 为 `dal_ultrasonic_init(dev, trig, echo)`、`dal_servo_init(dev, cfg)`。故覆写须在 sample `app_init` 顶部、`dal_*_init` 之前调用 `device_tree_apply_flash_config()`，并**从结构体字段重建 config / 引脚喂 init**，override 才生效（servo 因 config↔dev 字段重复，需从 dev 字段重建 config）。ADR 假设的 `device_tree_init_all()` 当前不存在，hook 实为 per-sample `app_init` 顶部。

2. **解析方式（校正 §3.2）**：**废弃** §3.2 的 `#pragma pack` + 指针强转解析（packed/未对齐访问在 ARM/Xtensa 上是 UB）。改用 **offset + memcpy 逐字段反序列化**；runtime POD 绝不 memcpy 到 wire（遵循 memory-safety §Struct 布局硬规）。

3. **CRC32 跨边界契约（补全 §3.2）**：**CRC-32/ISO-HDLC**（zlib/PNG/zip 同款），多项式反射值 `0xEDB88320`、init `0xFFFFFFFF`、final XOR `0xFFFFFFFF`、输入/输出 reflected；覆盖 `header(8B)+items`，**不含**末尾 4B CRC 自身。host 实现即前端 Codegen 对接的**权威参考**；不用 ESP-IDF 私有 CRC 以保双 target 同源。**CRC 只防字节损坏，不防语义错误**（后者由 §4.2 前端 Static Linter + 下条版本门控兜底）。

4. **device_id ↔ params 布局版本门控**：`device_id` 为 codegen 分配的稳定 uint32；覆写注册表绑定 `(id, 类型化 dev, 类型化 apply_fn)` 三元组（固件侧类型安全）。**任一 DAL params 布局变更必须 bump blob `version`**，旧 `version` 的 blob 一律降级、绝不按新布局误解析。`version != 1` 即降级（不做前向迁移）。

5. **Wasm 同源分阶段（软化 §4.1）**：§4.1 的「Wasm 硬约束」改为**分阶段**——核心阶段 Wasm 走 `pal_storage` read 返 `WINK_ERR_UNSUPPORTED` 的**运行期降级**（保持构建绿），Wasm JS 桥虚拟 FS 对等属后续 Wave。

6. **`HAS_FLASH_CONFIG_ESCAPE` 语义（校正 §4.4）**：核心阶段（host/wasm/esp32）**统一运行期降级**，**不定义/不引用**该宏。该宏保留为**未来低资源 target（ATmega / 低容量 STM32）的编译期裁剪开关**，等真有那种 target 引入时再启用。

7. **存储选型与抽象**：ESP32 用 **NVS**（namespace `"wink"`，key `"dtcfg"`），而非 SPIFFS（NVS 更轻、按 key 原子覆写、默认分区表已含）。新增 target 无关 `pal_storage` 抽象（host 内存单槽 / esp32 NVS / wasm no-op）与覆写解析器 `wink_dev_config` 解耦，均 host 单测覆盖。

---

## 6. 状态记录与未来演进

* **2026-06-27**：Proposed（由主架构师提议，用以解决多平台硬件原型开发中的快速引脚微调反馈环痛点）。
* **2026-06-28**：Accepted。核心机制已实现：target 无关覆写解析器（CRC-32/ISO-HDLC + 注册表派发）+ `pal_storage` 抽象（host 内存 / esp32 NVS / wasm no-op）+ per-DAL `apply_override`，`avoidance_car` 样本端到端打通；host 全单测通过、ESP32 NVS 编译通过。详见 §5 实现澄清与 [实施计划](../../implementation-plans/core/2026-06-28-adr-0008-flash-device-tree-override-plan.md)。后续 Wave（WebSerial 下发协议、Wasm JS 桥对等、前端 linter+codegen、低资源 target 编译期裁剪）后置。
* **后续演进 Wave**：
  * **Phase 3 (Wave B)** ✅ 已落地（核心）：`targets/esp32` 试点用 **NVS**（非 SPIFFS）实现 `dev_tree.bin` 存取，见 §5.7。下发通道（WebSerial）仍后置。
  * **Phase 4**：向低容量 target（如 STM32）移植时，评估是否在 PAL 中预留一小块 Sector Flash 作为无文件系统的裸 Sector 配置页实现，并启用 `HAS_FLASH_CONFIG_ESCAPE` 编译期裁剪。

