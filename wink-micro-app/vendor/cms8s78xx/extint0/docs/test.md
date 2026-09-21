# CMS8S78xx EXTINT0 运行、测试与可观测性指南

> **应用目录**：`wink-micro-app/vendor/cms8s78xx/extint0`  
> **上游源码**：`docs/vendors/Cmsemicon/CMS8S78xx_DemoCode_V2.0.2/CMS8S78xx_Example/Example/EXTINT/EXTINT0/code`  
> **芯片型号**：CMS8S78xx（1T 8051 内核）  
> **可观测性级别**：⚡ **Level 3（IO 打点 / 波形探测）**  
> **关联清单**：[CMS8S78XX_EXAMPLE_CHECKLIST.md](../../../docs/vendors/Cmsemicon/CMS8S78XX_EXAMPLE_CHECKLIST.md)

---

## 一、 程序做了什么（原理解析）

本程序是中微半导体官方 SDK 的 **EXTINT0（外部中断 0）** 基础例程。在保持官方源码“一行不改”的前提下，通过 WinkMicroOS 清洗与仿真底座运行。

### 1. 硬件外设与引脚配置
* **系统主频**：`SYS_SET_SYSTEM_CLK(SYS_CLK_DIV_1)`，系统时钟 1 分频，全速运行。
* **状态指示引脚 (P3.2 / Pin 26)**：
  * 配置为推挽输出模式 (`P32CFG = GPIO_MUX_GPIO`, `P3TRIS |= GPIO_PIN_2`)。
  * 上电初始电平为 `0`（低电平）。
* **外部中断触发引脚 (P3.0 / Pin 24)**：
  * 配置为数字输入模式 (`P30CFG = GPIO_MUX_GPIO`, `P3TRIS &= ~GPIO_PIN_0`)。
  * 开启片内弱上拉电阻 (`P3UP |= GPIO_PIN_0`)，悬空/未按下时引脚稳定保持为高电平 `1`。
  * **引脚复用选择器 (PS, Pin Selection)**：CMS8S78xx 支持引脚复用重定向，通过 `GPIO_SET_PS_MODE(PS_INT0, GPIO_P30_MUX_INT0)` 将物理引脚 P3.0 接通至内核的 INT0 中断检测线。

### 2. 中断机制与执行逻辑
1. **触发方式**：`EXTINT_ConfigInt(EXTINT0, EXTINT_TRIG_FALLING)`，设置为**仅下降沿触发**（Edge Triggered）。
2. **中断使能**：开启 INT0 中断 (`EXTINT_EnableInt`)，配置优先级为高 (`IRQ_PRIORITY_HIGH`)，并开启 CPU 全局中断 EA (`IRQ_ALL_ENABLE()`)。
3. **主循环**：`while(1) { ; }`，CPU 在主循环中空转等待中断。
4. **中断服务函数 (ISR)**：
   在 `isr.c` 中，外部中断 0 触发向量为 `INT0_VECTOR`（向量号 0）：
   ```c
   void INT0_IRQHandler(void) interrupt INT0_VECTOR
   {
       P32 = ~P32;
   }
   ```
   每当外部引脚 P3.0 检测到一个下降沿（电平从 1 跳变到 0），硬件中断触发，P3.2 输出电平翻转一次（0 -> 1，或 1 -> 0）。

---

## 二、 结合《可观测性分级》进行说明

在 [CMS8S78XX_EXAMPLE_CHECKLIST.md](../../../docs/vendors/Cmsemicon/CMS8S78XX_EXAMPLE_CHECKLIST.md) 的标准定义中，可观测性被划分为 4 个等级：

| 级别 | 描述 | 本示例归属理由与仿真解法 |
| :--- | :--- | :--- |
| 🎯 **Level 1** | 界面直观可视（屏幕、数码管、LED 矩阵） | 见下文拓扑映射 |
| 📜 **Level 2** | 串口日志（`printf` / SBUF 格式化输出） | 原厂源码未调用 UART |
| ⚡ **Level 3** | **IO 打点 / 波形探测（`P32 = ~P32`）** | **本示例原生归属此等级** |
| ⚙️ **Level 4** | 纯内部静默逻辑（寄存器/内存断言） | 原厂有明确外部引脚电平变化 |

### 1. 为什么官方源码属于 ⚡ Level 3？
在原厂裸机开发中，为了避免引入复杂的显示驱动或串口打印开销，原厂工程师最常采用“**示波器 IO 打点法**”：
* 原厂代码只翻转一个空闲引脚 `P32 = ~P32`。
* 既没有屏幕画面，也没有控制台文本打印。
* **痛点**：如果不借助仪器，人眼无法察觉程序是否正常运行；而在软件仿真中，若不做外设映射，仿真界面也是一片静默。

### 2. UniSim 仿真环境对 Level 3 的双重赋能
WinkMicroOS UniSim 通过两项技术彻底解决了 Level 3 的可观测性瓶颈：

```
                    ┌────────────────────────────────────────────────────────┐
                    │            Level 3: 原始 IO 电平与中断打点              │
                    │   P3.0 (输入下降沿) ──> INT0 ISR ──> P3.2 (电平翻转)    │
                    └──────────────────────────┬─────────────────────────────┘
                                               │
                      ┌────────────────────────┴────────────────────────┐
                      ▼                                                 ▼
        【升维至 Level 1: UI 直观可视】                 【确定性自动化: 虚拟逻辑分析仪】
       wink-app.json 虚拟外设映射:                       unisim-scenarios/*.scenario.json
       • P3.0 (Pin 24) ──> 虚拟按键 (button)             • 13 步微秒级确定性时钟步进
       • P3.2 (Pin 26) ──> 虚拟状态灯 (led)               • 自动化注入电平与断言翻转状态
```

1. **虚拟外设拓扑映射（降维打击：将 Level 3 转化为 Level 1 视觉体验）**：
   在 `wink-app.json` 中声明：
   * 输入端：将 P3.0 声明为 `button`（默认高电平，按下触发接地）。
   * 输出端：将 P3.2 声明为 `led`（高电平亮，低电平灭）。
   * **结果**：原本需要示波器抓波形的程序，在浏览器中变成了直观的“**点击虚拟按键，虚拟 LED 亮灭翻转**”。
2. **确定性虚拟逻辑分析仪（Headless 微秒级波形断言）**：
   在没有显示界面的 CI/CD 自动化流水线中，通过 `unisim-scenarios/extint0.scenario.json` 模拟逻辑分析仪的探头打点，实现无需人工介入的 100% 自动化测试。

---

## 三、 如何构建与运行

所有构建与仿真均由统一工具链 `wink.py` 驱动。

### 1. 真实编译输出仿真资产 (Real Build)
在开始仿真前，需要使用 Emscripten 真实构建 Wasm 并提取资产：
```powershell
cd D:\workspaces\ai-coding\wink-ai\wink-ai
python packages/wink-tools/wink.py build sim --app vendor/cms8s78xx/extint0
```
> **检查输出**：执行后会在 `wink-micro-app/vendor/cms8s78xx/extint0/unisim-assets/` 下生成：
> * `device-tree.json`（设备树与引脚映射）
> * `wink_simulator.js`（Wasm 胶水层）
> * `wink_simulator.wasm`（约 173KB 的 C51 仿真固件）

### 2. 启动浏览器 GUI 交互式仿真
```powershell
cd D:\workspaces\ai-coding\wink-ai\wink-ai
python packages/wink-tools/wink.py sim run --app vendor/cms8s78xx/extint0
```
运行后打开浏览器访问控制台，可以在交互式画布上看到 `btn` 按键和 `led` 指示灯。

---

## 四、 如何测试验证

### 1. 方式 A：Headless 确定性场景自动化测试（CI 门禁级别）

使用 `wink-tools` 配合场景脚本执行端到端无头断言测试：

```powershell
cd D:\workspaces\ai-coding\wink-ai\wink-ai
python packages/wink-tools/wink.py sim run `
  --app vendor/cms8s78xx/extint0 `
  --mode headless `
  --scenarios ../wink-ai-embedded/wink-micro-app/vendor/cms8s78xx/extint0/unisim-scenarios/extint0.scenario.json
```

#### 场景测试步骤深度解析（13 步时序表）
场景文件 [extint0.scenario.json](../unisim-scenarios/extint0.scenario.json) 模拟了完整的真实硬件电气测试行为：

| 步骤 | 时间点 (timeUs) | 操作类型 / 目标 | 预期值 / 动作 | 物理测试意义 |
| :---: | :---: | :--- | :---: | :--- |
| 1 | `50ms` | `ASSERT_POINT` -> `gpio:24` (P3.0) | `1` | **上电初始状态**：验证 P3.0 内部上拉生效，空闲时为高电平。 |
| 2 | `50ms` | `ASSERT_POINT` -> `gpio:26` (P3.2) | `0` | **上电初始状态**：验证 P3.2 初始化电平为 0，LED 初始熄灭。 |
| 3 | `100ms` | `INPUT_PLUGIN_EVENT` -> `btn` | `SET_PRESSED: true` | **按下按键**：注入硬件事件，按键闭合引脚接地。 |
| 4 | `150ms` | `ASSERT_POINT` -> `gpio:24` (P3.0) | `0` | **输入波形捕获**：验证 P3.0 瞬间跳变至低电平，产生有效下降沿。 |
| 5 | `200ms` | `ASSERT_POINT` -> `gpio:26` (P3.2) | `1` | **首次中断响应**：验证 INT0 中断触发并执行 ISR，P3.2 翻转为 1 (LED 点亮)。 |
| 6 | `500ms` | `ASSERT_POINT` -> `gpio:24` (P3.0) | `0` | **长按保持态**：验证按键持续保持按下期间，P3.0 稳定锁死在 0。 |
| 7 | `1000ms` | `ASSERT_POINT` -> `gpio:26` (P3.2) | `1` | **边沿触发防抖/防重触**：验证边沿模式下长按 800ms 不会重复触发中断，P3.2 保持 1。 |
| 8 | `1100ms` | `INPUT_PLUGIN_EVENT` -> `btn` | `SET_PRESSED: false` | **松开按键**：弹簧复位，断开接地。 |
| 9 | `1200ms` | `ASSERT_POINT` -> `gpio:24` (P3.0) | `1` | **释放波形捕获**：验证 P3.0 因内部上拉恢复为高电平，产生上升沿。 |
| 10 | `1200ms` | `ASSERT_POINT` -> `gpio:26` (P3.2) | `1` | **边沿单向性验证**：验证上升沿绝不触发 INT0，P3.2 依然保持为 1。 |
| 11 | `1500ms` | `INPUT_PLUGIN_EVENT` -> `btn` | `SET_PRESSED: true` | **二次按下**：再次注入按下事件，产生第二次下降沿。 |
| 12 | `1600ms` | `ASSERT_POINT` -> `gpio:26` (P3.2) | `0` | **二次翻转验证**：验证第二次下降沿再次命中中断，P3.2 翻转为 0 (LED 熄灭)。 |
| 13 | `1700ms` | `INPUT_PLUGIN_EVENT` -> `btn` | `SET_PRESSED: false` | **测试归位**：松开按键，恢复初始空闲态。 |

> ✅ **验收合格准则**：13 步断言全部绿灯通过，进程退出码为 `0`。

---

### 2. 方式 B：浏览器 Web UI 人机交互验证

1. 执行前述启动命令，进入 UniSim Web 界面；
2. **测试 1：单次短按**
   * 用鼠标点击虚拟按键 `btn`；
   * 观察虚拟指示灯 `led`：立即由暗变亮（高电平点亮）。
3. **测试 2：长按测试**
   * 鼠标按住 `btn` 不放（保持 1~2 秒）；
   * 观察 `led`：保持常亮，不应出现高频闪烁或多次翻转。
4. **测试 3：释放测试**
   * 松开鼠标左键；
   * 观察 `led`：依然保持常亮（上升沿不改变状态）。
5. **测试 4：再次按下**
   * 再次点击 `btn`；
   * 观察 `led`：立即熄灭（P3.2 再次翻转回低电平）。

---

### 3. 方式 C：物理硬件真机对照验证（供硬件出厂参考）

若烧录至 CMS8S78xx 官方 DevBoard 目标板：
1. **外围连线**：
   * P3.0（Pin 24）接轻触按键一端，按键另一端接 GND；
   * P3.2（Pin 26）接单色 LED 阳极（串联 1k 限流电阻），阴极接 GND；或接示波器探头 CH1。
2. **操作与观测**：
   * 上电后 LED 熄灭，示波器 CH1 输出 0V；
   * 按下按键，LED 点亮，示波器检测到上升沿跳变至 5V/3.3V；
   * 再次按下按键，LED 熄灭，示波器检测到下降沿跳变至 0V。
