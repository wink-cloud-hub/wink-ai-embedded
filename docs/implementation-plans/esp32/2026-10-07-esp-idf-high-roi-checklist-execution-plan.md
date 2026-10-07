# ESP-IDF 官方示例高 ROI 核心功能实施规划与执行路线图

| 字段 | 内容 |
|---|---|
| **计划编号** | `PLAN-20261007-ESP-IDF-HIGH-ROI-CHECKLIST-EXECUTION` |
| **创建日期** | 2026-10-07 |
| **状态** | 📋 架构规划与执行路线图（Ready for Execution） |
| **优先级** | 🔴 P1（重要架构推进，牵引后续示例批量交付） |
| **目标平台** | `wasm` (UniSim 浏览器无头仿真) / `ESP32` / `host` (Windows CTest) |
| **工具链/SDK** | ESP-IDF v6.1 (原厂镜像源码) / Emscripten (wasm32) / MSVC (Host) |
| **关联数据源** | [`wink-micro-app/vendor/esp_idfv61/CHECKLIST.md`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/CHECKLIST.md) (SSOT: [`checklist.data.json`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/.governance/data/checklist.data.json)) |
| **关联规范** | [`CLASSIFICATION-SPEC.md`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/.governance/specs/CLASSIFICATION-SPEC.md) (v2.0) · [`PLAYBOOK.md`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/.governance/specs/PLAYBOOK.md) · [governance-sop-esp](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/.agents/skills/governance-sop-esp/SKILL.md) |
| **所需技能** | `governance-sop-esp` + `embedded-best-practice` |

---

## 1. 背景与问题陈述 (Problem Statement)

截至 2026-10-07，ESP-IDF v6.1 官方示例全量仿真核对总账（478 个示例）的适配现状为：
- `[x]` **已完成实证 (Verified)**：**39 项**（覆盖 Blink、GPIO、I2C/SPI EEPROM、UART RS485/Events、ADC/DAC、GPTimer、LEDC 调光、NVS、SPIFFS、WiFi STA/SoftAP、HTTP Client、MQTT TCP、NimBLE 广播/从机、HTTP Server Simple/RESTful）。
- `[ ]` **规划排期中 (In-Scope Planned)**：**252 项**。
- `[-]` **产品排除 / 暂缓投入 (Out-of-Scope / Deferred)**：**187 项**。

### 核心痛点与风险
1. **长尾漫游风险**：规划中的 254 项中包含大量小众外设（如 `i3c`, `parlio`, `bitscrambler`）、极端微安级协处理器（`system/ulp` 共 27 项）、以及蓝牙深入协议变体（`bluetooth/nimble` 剩余 40 项）。若缺乏精确定位的投入产出比（ROI）导航，AI Coding Agents 或开发者极易陷入高成本、低收益的长尾用例中。
2. **“单向发起”局限**：已实证的 37 项主要完成了外设底层读写与客户端单向发起（如 HTTP GET、MQTT Publish）。但在真实商业 IoT、创客与具身智能业务中，**设备端本地 Web 服务、局域网双向通信、工业级配网、固件空中升级（OTA）与电机/光电交互**才是绝对高频的业务核心。
3. **目标**：本计划建立量化的 **四维 ROI 评估模型**，筛选出 **20 个最高 ROI 的核心关键示例**，编排为清晰的三波执行攻坚泳道（Wave 1 ~ Wave 3），作为后续并发认领与六要素实证交付的权威执行蓝图。

---

## 2. 四维 ROI 评估模型 (ROI Evaluation Matrix)

每一个候选 Checklist 项的投入产出比综合得分按以下公式计算：

$$\text{ROI} = \frac{\text{频次权重}(F) \times 0.40 + \text{解锁定力}(U) \times 0.30 + \text{可观测收益}(O) \times 0.20}{\text{实现与仿真成本}(C) \times 0.10}$$

| 评估维度 | 权重 | 评估准则 | 分值范围 (1~5) |
|:---|:---:|:---|:---:|
| **业务发生频次 ($F$)** | 40% | 在真实智能硬件、具身小车、传感器网关与低代码业务中的出现概率。 | 1 (极少用) ~ 5 (几乎所有设备必用) |
| **架构解锁定力 ($U$)** | 30% | 该能力是否作为关键枢纽，能直接解锁上层生态组件、库或级联示例。 | 1 (孤立点) ~ 5 (关键拓扑底座) |
| **可观测收益 ($O$)** | 20% | 在 UniSim 仿真中是否具备直观 UI 控件（L1 靶点）或强因果数据流（L2 协议流）。 | 1 (纯寄存器静默) ~ 5 (UI 控件可视化) |
| **适配与仿真成本 ($C$)** | 10% | 框架拦截门面复杂度、跨平台 PAL 增量负担与确定性测试时序要求。 | 1 (极复杂/易死锁) ~ 5 (轻量/易闭环) |

---

## 3. 高 ROI 核心条目总览与三波演进路线

经过对 254 个 Planned 条目的全量交叉打分，确立 **20 个战略级高 ROI 示例**，划分为三波演进梯队：

```mermaid
graph LR
    subgraph Wave 1 [Wave 1: 设备端服务与网络应用闭环]
        direction TB
        W1_1["#196 & #195 http_server 基础与REST"]
        W1_2["#197 ws_echo_server WebSocket双向"]
        W1_3["#232 wifi/softap_sta AP+STA共存双工"]
        W1_4["#209 protocols/sntp 网络授时NTP"]
        W1_5["#212 & #214 sockets TCP Client/Server"]
    end

    subgraph Wave 2 [Wave 2: 产品全生命周期交付与直观交互]
        direction TB
        W2_1["#146 & #143 system/ota 空中固件升级"]
        W2_2["#312 bluetooth/blufi 蓝牙辅助配网"]
        W2_3["#122 & #140 deep/light sleep 电源管理"]
        W2_4["#064 peripherals/rmt/led_strip WS2812彩灯"]
        W2_5["#121 system/console/basic 交互式命令行"]
    end

    subgraph Wave 3 [Wave 3: 具身运动控制、HMI 与掉电安全存储]
        direction TB
        W3_1["#051 & #055 mcpwm 直流电机与舵机"]
        W3_2["#060 pcnt 正交编码器测速闭环"]
        W3_3["#040 & #045 lcd OLED与SPI液晶屏"]
        W3_4["#398 storage/littlefs 掉电安全文件系统"]
        W3_5["#220 wifi/espnow 局域无连点对点"]
        W3_6["#086 touch_sensor 电容触控按键"]
    end

    Wave 1 --> Wave 2 --> Wave 3
```

---

## 4. 重点条目技术规格与实证要求详解

### Wave 1: 设备端服务与网络应用闭环 (架构级枢纽)

> **核心目标**：从单向发起跃升为“提供本地服务”，打通设备作为 Web Server、WebSocket 端点和 TCP 节点的核心能力。

#### 1.1 本地 Web 服务与 REST 控制
* **Checklist 编号**：`#196` (`protocols/http_server/simple`) 与 `#195` (`protocols/http_server/restful_server`)
* **泳道与优先级**：Lane 6 [无线网络] · P1
* **技术价值**：
  - 允许在设备端本地托管静态网页、处理 `GET /api/status` 与 `POST /api/config`；
  - 智能硬件本地配网、局域网免外网控制的绝对事实标准。
* **UniSim 实证重点 (SOP)**：
  - 虚拟网络环境发起 HTTP 请求，验证请求解析与响应报文完整性；
  - 必须包含业务因果断言（变异 URI 预期 404，变异 Payload 预期 400，防止空桩假绿）。

#### 1.2 实时双向 WebSocket 通信
* **Checklist 编号**：`#197` (`protocols/http_server/ws_echo_server`)
* **泳道与优先级**：Lane 6 [无线网络] · P1
* **技术价值**：
  - 解锁毫秒级双向长连接，是前端 UniSim UI 面板（如遥控手柄、实时仪表盘）与设备实时同步的桥梁。
* **UniSim 实证重点**：
  - 建立 WS 握手升级，双向收发文本与二进制帧；
  - 变异测试：畸形帧或超时断开连接断言。

#### 1.3 AP + STA 混合共存双工模式
* **Checklist 编号**：`#232` (`wifi/softap_sta`)
* **泳道与优先级**：Lane 6 [无线网络] · P1 (原表 P2 调优)
* **技术价值**：
  - 设备自建 SoftAP 供手机连入，同时作为 STA 连入上级路由器；
  - 经典 Wi-Fi 配网与中继网关拓扑的核心，解决单 STA 无法交互的尴尬。
* **UniSim 实证重点**：
  - 验证双虚拟 Netif 接口独立 IP 分配与状态机转换，验证路由转发因果。

#### 1.4 网络绝对授时 (SNTP)
* **Checklist 编号**：`#209` (`protocols/sntp`)
* **泳道与优先级**：Lane 6 [无线网络] · P1 (原表 P2 调优)
* **技术价值**：
  - 物联网设备离不开真实时间戳（TLS 证书校验、定时开关、日志审计）。
* **UniSim 实证重点**：
  - 虚拟 NTP 服务器注入固定时间戳，断言系统 `time_t` 正确对齐并在无网络时优雅重试。

#### 1.5 原生 BSD Sockets 传输层
* **Checklist 编号**：`#212` (`protocols/sockets/tcp_client`) 与 `#214` (`protocols/sockets/tcp_server`)
* **泳道与优先级**：Lane 6 [无线网络] · P1
* **技术价值**：
  - 底层网络通信基石，大量第三方 C 库、私有 IoT 协议与微型 RPC 均直接建立在 BSD Socket 上。

---

### Wave 2: 产品全生命周期交付与直观交互 (产品化体验)

> **核心目标**：打通工业级生命周期管理（OTA、休眠、配网）与视觉反馈（RGB 彩灯、控制台）。

#### 2.1 固件空中升级 (OTA)
* **Checklist 编号**：`#146` (`system/ota/simple_ota_example`) 与 `#143` (`system/ota/native_ota_example`)
* **泳道与优先级**：Lane 1 [内核调度] · P1 (原表 P2 调优)
* **技术价值**：
  - 商业 IoT 的生命线。低代码平台从云端推送新生成的固件到硬件必须走 OTA。
  - 验证分区表切换、双 boot 槽位验证与回滚防砖机制。
* **UniSim 实证重点**：
  - 虚拟 Flash 分区写入与校验，断言 `esp_ota_set_boot_partition` 成功且坏镜像回滚。

#### 2.2 BluFi 蓝牙辅助配网
* **Checklist 编号**：`#312` (`bluetooth/blufi`)
* **泳道与优先级**：Lane 6 [无线网络] · P1 (原表 P2 调优)
* **技术价值**：
  - 消费级智能硬件（智能插座、温湿度计）用户体验最佳的配网方案（无需手动切换 Wi-Fi 设置）。
* **UniSim 实证重点**：
  - 模拟手机端 BLE 配网握手，传输加密 Wi-Fi 凭证，断言 Wi-Fi STA 顺利连入指定 AP。

#### 2.3 电源管理与深度休眠唤醒
* **Checklist 编号**：`#122` (`system/deep_sleep`) 与 `#140` (`system/light_sleep`)
* **泳道与优先级**：Lane 1 [内核调度] · P1 (原表 P2 调优)
* **技术价值**：
  - 电池供电设备必备能力。测试 GPIO 唤醒、Timer 唤醒与休眠上下文恢复。
* **UniSim 实证重点**：
  - 断言休眠时虚拟时钟推进、功耗模式标记、唤醒后复位因果（Deep Sleep 复位 vs Light Sleep 恢复）。

#### 2.4 RMT 智能幻彩灯带 (WS2812 / NeoPixel)
* **Checklist 编号**：`#064` (`peripherals/rmt/led_strip`)
* **泳道与优先级**：Lane 3 [脉冲定时] · P1
* **技术价值**：
  - 极高频创客外设，智能硬件指示灯与氛围灯核心。
  - **UniSim Level 1 UI 强可观测**：画布直接渲染 RGB 色环与流水动画。
* **UniSim 实证重点**：
  - RMT 归零码波形脉冲编码严格校验，断言每个像素的 GRB 24-bit 时序。

#### 2.5 交互式串口命令行 (Console REPL)
* **Checklist 编号**：`#121` (`system/console/basic`)
* **泳道与优先级**：Lane 1 [内核调度] · P1
* **技术价值**：
  - 研发调试、产测校准与用户交互命令行的核心基石。

---

### Wave 3: 具身运动控制、HMI 与掉电安全存储 (具身/工控增强)

> **核心目标**：赋能智能小车、机械臂、调光仪表与大容量存储。

#### 3.1 电机调速与舵机控制 (MCPWM)
* **Checklist 编号**：`#051` (`peripherals/mcpwm/mcpwm_bdc_speed_control`) 与 `#055` (`mcpwm_servo_control`)
* **泳道与优先级**：Lane 3 [脉冲定时] · P1
* **技术价值**：
  - 直流有刷/无刷电机 PWM 调速、正反转死区控制；标准 50Hz 舵机角度精准转动。
* **UniSim 实证重点**：
  - 定点 PWM 占空比与死区时间因果验证，断言舵机角度与脉宽对应关系。

#### 3.2 正交编码器脉冲计数 (PCNT)
* **Checklist 编号**：`#060` (`peripherals/pcnt/rotary_encoder`)
* **泳道与优先级**：Lane 3 [脉冲定时] · P1
* **技术价值**：
  - 机器人/小车电机测速码盘闭环，工业旋转旋钮输入。
* **UniSim 实证重点**：
  - 注入正交 A/B 相边沿脉冲，断言计数器方向与累计步数严格匹配。

#### 3.3 显示屏幕与触控 (I2C OLED / SPI LCD)
* **Checklist 编号**：`#040` (`peripherals/lcd/i2c_oled`) 与 `#045` (`peripherals/lcd/spi_lcd_touch`)
* **泳道与优先级**：Lane 2 [数字总线] · P1 (原表 P3 调优)
* **技术价值**：
  - SSD1306 OLED 与 ST7789/ILI9341 LCD 屏幕驱动，嵌入式人机界面的视觉输出核心。
* **UniSim 实证重点**：
  - UniSim 虚拟屏幕画布置入，截获 FrameBuffer 显存并断言图形绘制正确性。

#### 3.4 掉电安全嵌入式文件系统 (LittleFS)
* **Checklist 编号**：`#398` (`storage/littlefs`)
* **泳道与优先级**：Lane 5 [本地存储] · P1 (原表 P2 调优)
* **技术价值**：
  - 现代嵌入式主流文件系统，天然支持掉电安全与磨损平衡，优于传统 SPIFFS。
* **UniSim 实证重点**：
  - 模拟断电恢复、文件并发写入与目录树遍历。

#### 3.5 ESP-NOW 无路由自组网络
* **Checklist 编号**：`#220` (`wifi/espnow`)
* **泳道与优先级**：Lane 6 [无线网络] · P1
* **技术价值**：
  - 无需 Wi-Fi 路由器的超低延迟私有射频通信（遥控器、无人机控制、低功耗传感器节点互联）。

#### 3.6 电容触摸按键 (Touch Sensor)
* **Checklist 编号**：`#086` (`peripherals/touch_sensor/touch_sens_basic`)
* **泳道与优先级**：Lane 4 [模拟电学] · P1
* **技术价值**：
  - 现代智能家居设备无机械结构的触控按键交互。

---

## 5. 明确后置与暂缓投入的长尾清单 (Explicitly Deferred Backlog)

为集中研发兵力，以下领域**明确保持后置或暂缓**，不得占用前三波主力带宽：

| 领域模块 | 示例数量 | 暂缓原因与边界 |
|:---|:---:|:---|
| **`system/ulp/*` 系列** | 27 项 | 依赖 ULP 协处理器 / RISC-V 独立工具链与汇编，开发与仿真复杂度极高，仅针对极端微安级应用。维持后置。 |
| **`bluetooth/nimble/*` 剩余长尾** | 40 项 | 复杂特性如 L2CAP CoC、吞吐量压测、进阶广播过滤等。已有 `#384 bleprph` 与 `#247 Beacon` 已覆盖通用 80% 场景。 |
| **特殊小众总线 (`parlio`, `i3c`, `bitscrambler`)** | 6 项 | 仅极少数新型 SoC 具备且外围器件稀少，缺乏通用生态需求。维持 P3。 |
| **工业以太网 (Ethernet) 与不可逆物理硬件** | 20+ 项 | 外部 PHY 硬件与 eFuse 等物理不可逆介质，维持 Fail-Loud 编译期阻断。 |

---

## 6. 治理约束与实证执行守则 (Execution Guardrails)

在后续执行认领上述高 ROI 项时，必须无条件遵循以下**五大铁律**：

1. **原厂源码零修改镜像**：
   - 官方 `*.c` 文件必须与 ESP-IDF v6.1 上游逐字一致；
   - 仅允许新增 `wink-app.json` 与 `include/sdkconfig.h`；
   - 严禁为了“跑通”而修改原厂业务逻辑。
2. **六要素实证合取闭环**：
   - 必须通过 `wink.py build sim` 真实编译生成 `unisim-assets/`（含 `.wasm`、`.js`、`device-tree.json`）；
   - 必须编写电气时序因果场景 `unisim-scenarios/*.scenario.json`；
   - 必须执行 `run_esp32_headless_evidence.ps1` 产出全绿 `run-report.json` 并固化防伪哈希。
3. **因果性与防假绿四原则**：
   - **A-1 因果性**：断言必须来自固件真实出口；
   - **A-2 行为覆盖**：覆盖完整状态机与时序窗口；
   - **A-3 交互闭环**：请求/响应双向闭环；
   - **A-4 缺陷敏感性**：变异用例必须按预期失败。
4. **PAL 纯增量演进四大钢铁纪律（ADR-0092 Tier 1）**：
   - 若外设底层需要扩充 PAL，只允许纯增量新增头文件（如 `pal_rmt.h`），严禁破坏既有签名；
   - PAL 严禁包含任何 `esp_*.h` 或 FreeRTOS 私有符号；
   - 必须同源提交 Wasm 桩与 ESP32 物理驱动。
5. **门禁与清单原子同步**：
   - 只有在实证全部通过且门禁（Gate 1~4 与 Layering Lint）无报错后，方可通过脚本更新 `checklist.data.json`，并将交付状态晋升为 `verified`。

---

## 7. 任务认领与执行跟踪表 (Actionable Claiming Table)

| 波次 | 编号 | 官方示例 | 优先级 | 泳道 | 责任状态 | 关联 App 目录 |
|:---:|:---:|:---|:---:|:---:|:---:|:---|
| **Wave 1** | #196 | `protocols/http_server/simple` | P1 | Lane 6 | ✅ 已完成 (Verified) | `protocols/http_server_simple` |
| **Wave 1** | #195 | `protocols/http_server/restful_server` | P1 | Lane 6 | ✅ 已完成 (Verified) | `protocols/http_server_restful_server` |
| **Wave 1** | #197 | `protocols/http_server/ws_echo_server` | P1 | Lane 6 | ✅ 已完成 (Verified) | `protocols/http_server_ws_echo_server` |
| **Wave 1** | #232 | `wifi/softap_sta` | P1 | Lane 6 | ✅ 已完成 (Verified) | `wifi/softap_sta` |
| **Wave 1** | #209 | `protocols/sntp` | P1 | Lane 6 | ✅ 已完成 (Verified) | `protocols/sntp` |
| **Wave 1** | #212 | `protocols/sockets/tcp_client` | P1 | Lane 6 | ✅ 已完成 (Verified) | `protocols/sockets_tcp_client` |
| **Wave 1** | #214 | `protocols/sockets/tcp_server` | P1 | Lane 6 | 待认领 | `protocols/sockets_tcp_server` |
| **Wave 2** | #146 | `system/ota/simple_ota_example` | P1 | Lane 1 | 待认领 | `system/ota_simple` |
| **Wave 2** | #312 | `bluetooth/blufi` | P1 | Lane 6 | 待认领 | `bluetooth/blufi` |
| **Wave 2** | #122 | `system/deep_sleep` | P1 | Lane 1 | 待认领 | `system/deep_sleep` |
| **Wave 2** | #140 | `system/light_sleep` | P1 | Lane 1 | 待认领 | `system/light_sleep` |
| **Wave 2** | #064 | `peripherals/rmt/led_strip` | P1 | Lane 3 | 待认领 | `peripherals/rmt_led_strip` |
| **Wave 2** | #121 | `system/console/basic` | P1 | Lane 1 | 待认领 | `system/console_basic` |
| **Wave 3** | #051 | `peripherals/mcpwm/mcpwm_bdc_speed_control` | P1 | Lane 3 | 待认领 | `peripherals/mcpwm_bdc` |
| **Wave 3** | #055 | `peripherals/mcpwm/mcpwm_servo_control` | P1 | Lane 3 | 待认领 | `peripherals/mcpwm_servo` |
| **Wave 3** | #060 | `peripherals/pcnt/rotary_encoder` | P1 | Lane 3 | 待认领 | `peripherals/pcnt_rotary_encoder` |
| **Wave 3** | #040 | `peripherals/lcd/i2c_oled` | P1 | Lane 2 | 待认领 | `peripherals/lcd_i2c_oled` |
| **Wave 3** | #045 | `peripherals/lcd/spi_lcd_touch` | P1 | Lane 2 | 待认领 | `peripherals/lcd_spi_touch` |
| **Wave 3** | #398 | `storage/littlefs` | P1 | Lane 5 | 待认领 | `storage/littlefs` |
| **Wave 3** | #220 | `wifi/espnow` | P1 | Lane 6 | 待认领 | `wifi/espnow` |
| **Wave 3** | #086 | `peripherals/touch_sensor/touch_sens_basic` | P1 | Lane 4 | 待认领 | `peripherals/touch_sens_basic` |
