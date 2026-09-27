# CMS8S78xx Vendor Behavior Suite (cms8s78xx)

> **定位**：中微半导体（Cmsemicon）CMS8S78xx 系列 1T 高速 8051 芯片官方原厂示例镜像与 UniSim 端到端行为级仿真适配套件。
>
> 📖 **官方规范与进度总账**：
> - 📋 **[CHECKLIST.md](CHECKLIST.md)**：CMS8S78xx 43 个官方示例全量核对总账与 37+ 项适配实证清单
> - 🛠️ **[PLAYBOOK.md](PLAYBOOK.md)**：MCS-51 官方示例端到端仿真适配与无头实证标准执行手册 (SOP)
> - 🚀 **自动化回归脚本**：[run_mcs51_headless_evidence.ps1](../../../wink-micro-os/frameworks/mcs51/tools/run_mcs51_headless_evidence.ps1)

## 1. 核心原则

1. **原厂源码一行不改**：直接镜像官方 DemoCode 中的 `main.c`, `demo_*.c`, `isr.c`，所有 Keil C51 关键字由底层清洗与仿真框架透明拦截。
2. **三件套资产完备**：每个微应用目录下均由 `wink.py build sim` 真实编译产出 `unisim-assets/`（`device-tree.json` + `wink_simulator.js` + `wink_simulator.wasm`）。
3. **Headless 微秒级断言实证**：在 `unisim-scenarios/` 下配备确定性时序场景用例，100% 通过无头仿真回归。
