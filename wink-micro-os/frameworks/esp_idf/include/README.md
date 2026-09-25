# ESP-IDF 仿真头文件垫片树 (Include Stubs)

> 🚨 **维护铁律（必读）**：
> 本目录下的头文件均为针对 WebAssembly 仿真环境定制的**纯 C-ABI 声明契约（Pure Declarations & Types）**。
> **严禁手工逐字复制或从零手写原厂复杂的头文件（如全量引脚枚举、配置结构体、宏常量表）！**

---

## 1. 头文件生成与更新自动化工作流

当语料编译报错、需要补齐新的官方头文件或升级原厂 SDK 版本时，**必须优先调用闭源工具链收割流水线**：

* **自动化工具路径**：`wink-ai/packages/wink-tools/tools/sdk_harvester/`
* **架构设计与实施计划**：[`packages/wink-tools/tools/sdk_harvester/docs/plans/2026-09-25-sdk-harvester-architecture-and-implementation-plan.md`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai/packages/wink-tools/tools/sdk_harvester/docs/plans/2026-09-25-sdk-harvester-architecture-and-implementation-plan.md)
* **执行命令**（artifact 中介：收割产物不在本目录直写，经 vendoring PR 合入）：
  ```bash
  # 在 wink-ai/packages/wink-tools 目录下执行
  # 闭包勘察（未列 include 记 inventory，产物仅供编译验证）
  python wink.py internal harvest-sdk --sdk-path <esp-idf> --version v6.1 --target esp32 \
      --out <artifact-dir> --include-mode survey
  # 生产门禁（未列 include 即 exit 4，AST 过滤默认开启）
  python wink.py internal harvest-sdk --sdk-path <esp-idf> --version v6.1 --target esp32 \
      --out <artifact-dir> --include-mode block
  ```

---

## 2. 准入与排毒守则（Sanitization Rules）

由 Harvester 生成或偶尔微调的头文件必须满足：
1. **0 原厂汇编**：严禁出现 `__asm__("rsr ccount")` 等架构特有指令；
2. **0 硬件寄存器指针**：严禁直接包含 `soc/gpio_reg.h`、`soc/hwcrypto_reg.h` 等涉及物理地址解引用的内部头文件；
3. **属性宏消解**：`IRAM_ATTR`、`DRAM_ATTR`、`RTC_DATA_ATTR` 必须通过 `esp_attr.h` 消解为空；
4. **纯声明原则**：除极简单的宏定义外，严禁在 `.h` 文件中编写函数实现体（实现逻辑全部下沉至 `src/drivers/`）。
