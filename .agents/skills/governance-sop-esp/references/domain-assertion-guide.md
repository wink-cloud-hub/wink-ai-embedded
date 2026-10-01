# ESP-IDF 官方示例领域断言模式与场景范式指南
## Domain Assertion & Scenario Guide for ESP-IDF Carriers

> **所属规程**：`governance-sop-esp`  
> **适用范围**：`wink-micro-app/vendor/esp_idfv61` 官方示例及后续 Carrier 场景编排  
> **核心原则**：出口级观测、因果闭环、拒绝退化断言、严格遵循当前运行时能力阶梯。

---

## 1. 运行时能力阶梯 (Capability Tiers) 与 Target 白名单

在编写 `unisim-scenarios/<name>.scenario.json` 时，必须严格遵循**当前本地运行时已实现的能力**，严禁超前使用未落地的草案协议：

```
                      ESP-IDF 门面域断言 Target 与运行时能力阶梯
┌─────────────────────────────────┬─────────────────────────────────┬─────────────────────────────────┐
│ [Tier 1] 已验证可用 (Verified)   │ [Tier 2] 部分支持 (In-Progress) │ [Tier 3] 拟实现草案 (Planned)   │
│ • gpio:<pin> (引脚电平 0/1)     │ • INPUT_BUS (UART 数据流注入)   │ • INJECT_NET_FIXTURE (网络路由) │
│ • pwm:<ch> (PWM 占空比/频率)    │ • ASSERT_BUS_PAYLOAD (UART回显) │ • http:client:status_code       │
│ • ASSERT_POINT (点断言)         │ • INJECT_WIFI_FIXTURE (AP连接)  │ • http:client:rx_bytes_total    │
│ • ASSERT_WAVEFORM (波形断言)    │ • wifi:sta:state (Wi-Fi 状态机) │ • ble:gap:adv_state (广播)      │
│ • ASSERT_SEQUENCE (时序跳变)    │ • netif:sta:ip (DHCP 分配地址)  │ • ble:gatt:* (GATT 服务/特征值) │
│                                 │                                 │ • gptimer:* / sleep:* / nvs:*   │
└─────────────────────────────────┴─────────────────────────────────┴─────────────────────────────────┘
```

### 1.1 Matcher 语法与匹配规则（极其重要）
Unisim 场景运行器的 Matcher 具备严格的类型分发逻辑（参见 `matcher-evaluator.ts`）：
- **精确标量比较**：字面值 `number`（如 `1`）、`boolean`（如 `true`）、`string`（如 `"DISCONNECTED"`）。
- **容差比较**：`{ "$near": { "target": 2.5, "tolerance": 0.1 } }`。
- **范围比较**：`{ "$between": [min, max] }`。
- **正则匹配**：`{ "$regex": "pattern" }`。

> ⚠️ **严禁使用比较表达式字符串**：
> 运行器**不支持**形如 `matcher: ">0"` 或 `matcher: ">=0"` 的写法！标量比较会直接执行 `actual === matcher`，导致数值 `100` 与字符串 `">0"` 比对失败。
> 若要断言“大于 0 的数值”，**必须写成范围匹配**：`{ "$between": [1, 2147483647] }`。

### 1.2 缺口处理铁律
若目标示例的核心业务落在 **Tier 3**（如 HTTP Client、NimBLE、GPTimer），且当前本地运行时契约尚未合入对应 step 解析器：
1. **严禁 Agent 虚构调用未来接口并声称交付**；
2. 保持清单中的 `delivery_state: "planned"` 或 `"blocked_on_runtime"`；
3. 输出明确的平台缺口说明（例如：`Blocked: unisim runtime lacks INJECT_NET_FIXTURE support`）。

---

## 2. 典型门面域合规场景范式

### 2.1 GPIO 周期翻转（如 `blink_gpio`）[Tier 1: 已验证可用]
* **特征**：自主时序，FreeRTOS 延时驱动，具备高低电平跃迁。
```json
{
  "header": {
    "version": "1.0.0",
    "name": "ESP32 ESP-IDF v6.1 blink_gpio headless simulation proof",
    "templateId": "esp_idfv61_blink_gpio",
    "accuracyMode": "behavioral",
    "timeoutUs": "3500ms",
    "failurePolicy": "fail-fast",
    "determinism": { "prngSeed": 42 }
  },
  "steps": [
    { "type": "ASSERT_POINT", "timeUs": "50ms", "target": "gpio:2", "matcher": 0, "description": "[上电初始] GPIO2 初始输出低电平 0" },
    { "type": "ASSERT_POINT", "timeUs": "1050ms", "target": "gpio:2", "matcher": 1, "description": "[周期翻转] 延时 1000ms 后翻转为高电平 1" },
    { "type": "ASSERT_POINT", "timeUs": "2050ms", "target": "gpio:2", "matcher": 0, "description": "[周期翻转] 再次延时 1000ms 后恢复为低电平 0" }
  ]
}
```

### 2.2 串口闭环回传（如 `uart_echo`）[Tier 2: 部分支持]
* **特征**：输入激励驱动，双向闭环 Echo 校验，捕获真实 RX/TX 载荷。
```json
{
  "header": {
    "version": "1.0.0",
    "name": "ESP-IDF v6.1 uart_echo headless deterministic proof",
    "templateId": "esp_idfv61_uart_echo",
    "accuracyMode": "behavioral",
    "timeoutUs": "2000ms",
    "failurePolicy": "fail-fast",
    "determinism": { "prngSeed": 42 }
  },
  "steps": [
    {
      "type": "INPUT_BUS",
      "timeUs": "100ms",
      "mode": "stream",
      "busType": "uart",
      "busId": 1,
      "payload": { "encoding": "utf8", "text": "PING_PONG_TEST" },
      "description": "通过 UART1 RX 注入测试字节序列"
    },
    {
      "type": "ASSERT_BUS_PAYLOAD",
      "windowUs": ["100ms", "1000ms"],
      "busType": "uart",
      "busId": 1,
      "direction": "tx",
      "matcher": "PING_PONG_TEST",
      "description": "断言从 UART1 TX 观察到由固件处理并回显的完全相同数据流"
    }
  ]
}
```

### 2.3 Wi-Fi STA 网络生命周期与故障注入（如 `wifi_sta`）[Tier 2: 部分支持]
* **特征**：注入虚拟 AP，经历连接与 DHCP，注入断网故障断言状态机迁移。
```json
{
  "header": {
    "version": "1.0.0",
    "name": "ESP-IDF v6.1 Wi-Fi STA Connection & Fault Proof",
    "templateId": "esp_idfv61_wifi_sta",
    "accuracyMode": "behavioral",
    "timeoutUs": "3000ms",
    "failurePolicy": "fail-fast",
    "determinism": { "prngSeed": 42 }
  },
  "steps": [
    {
      "type": "INJECT_WIFI_FIXTURE",
      "timeUs": "0ms",
      "accessPoints": [
        {
          "ssid": "myssid",
          "bssid": "00:11:22:33:44:55",
          "rssi": -45,
          "dhcp": { "assignedIp": "192.168.1.100" }
        }
      ]
    },
    {
      "type": "ASSERT_POINT",
      "timeUs": "1500ms",
      "target": "netif:sta:ip",
      "matcher": "192.168.1.100",
      "description": "断言 STA 接口通过虚拟 DHCP 成功取得 IP 192.168.1.100"
    },
    {
      "type": "INJECT_WIFI_FIXTURE",
      "timeUs": "2000ms",
      "fault": { "action": "DROP_BEACON", "reason": "BEACON_TIMEOUT" }
    },
    {
      "type": "ASSERT_POINT",
      "timeUs": "2500ms",
      "target": "wifi:sta:state",
      "matcher": "DISCONNECTED",
      "description": "断言在信标丢失注入后 Wi-Fi 状态准确置为 DISCONNECTED"
    }
  ]
}
```

### 2.4 [拟实现草案 / 待能力扩展] HTTP 客户端交互（如 `http_client`）[Tier 3: 严禁交付]
> ⚠️ **警告**：当前 unisim 本地运行器**尚未合入** `INJECT_NET_FIXTURE` 与 `http:client:*` 信号解析器。本范式仅作为未来规范设计参考。在运行时契约正式支持前，**严禁使用此模板声明 `delivery_state: verified` 交付**！
```json
{
  "header": {
    "version": "1.0.0",
    "name": "ESP-IDF v6.1 http_client headless deterministic proof (DRAFT)",
    "templateId": "esp_idfv61_http_client",
    "accuracyMode": "behavioral",
    "timeoutUs": "3000ms",
    "failurePolicy": "fail-fast",
    "determinism": { "prngSeed": 42 }
  },
  "steps": [
    {
      "type": "INJECT_NET_FIXTURE",
      "timeUs": "0ms",
      "protocol": "http",
      "routes": [
        {
          "method": "GET",
          "url_prefix": "http://httpbin.org/get",
          "status_code": 200,
          "headers": { "Content-Type": "application/json" },
          "body": "{\"origin\":\"127.0.0.1\",\"url\":\"http://httpbin.org/get\"}"
        }
      ],
      "description": "注入虚拟 HTTP 响应路由表与静态载荷"
    },
    {
      "type": "ASSERT_POINT",
      "timeUs": "1500ms",
      "target": "http:client:status_code",
      "matcher": 200,
      "description": "断言客户端从协议栈成功收到 HTTP 200 响应码"
    },
    {
      "type": "ASSERT_POINT",
      "timeUs": "1500ms",
      "target": "http:client:rx_bytes_total",
      "matcher": { "$between": [1, 2147483647] },
      "description": "断言客户端底层接收到非零业务载荷（使用 $between 范围匹配）"
    }
  ]
}
```

---

## 3. 因果闭环核验实操规范 (Causal Chain Verification)

为证明断言不是假绿，必须严格区分并完成以下检验，且**严禁修改原厂官方 `*_main.c` 源码**：

| 检验环节 | 实操手段 | 判定标准与意义 |
| :--- | :--- | :--- |
| **① 正常基准通过<br>(Baseline Pass)** | 在场景中输入正常业务参数并运行无头仿真。 | **100% 绿灯，0 失败**。确立基准黄金行为。 |
| **② 断言器探针自检<br>(Canary Self-Check)** | 临时修改场景中的一个预期值（例如将 IP 改为 `0.0.0.0`，或将预期电平由 1 改为 0）。 | **必须在指定 Target 处报出 `AssertionFailed` 错误**。证明运行器和报告链路通畅，断言未被静默吞掉。 |
| **③ 非破坏性固件因果检验<br>(Firmware Dependency)** | **严禁直接修改原厂源码**。采用场景输入扰动（如注入错误 AP 密码、断开总线），或利用受控测试编译宏注入故障。 | **指定业务断言必须失败**。证明通过结果依赖固件真实业务执行，而非仿真桩的常数伪造。 |
| **④ 恢复基准通行<br>(Restore Baseline)** | 还原所有改动，重新执行无头仿真。 | **恢复 100% 绿灯**。完成因果闭环自证。 |

> 🚫 **杀伤有效性判定红线**：
> 只有仿真运行器明确输出**业务断言失败**（如预期值与实际值不符）才算杀伤成功；
> JSON 解析错误、CMake/C 编译失败、Runner 内存崩溃或进程异常退出（Exit Code 2），**一律严禁当作业务杀伤证明**！
