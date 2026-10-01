# ESP-IDF 示例领域断言指南

适用于 `governance-sop-esp` 的 Authoring 与断言审查。本指南中的能力状态是 2026-10-01 本地契约核验快照，不是清单 Schema 的静态 Tier 字段，也不能代替原子能力依赖闭包。

## 1. 能力预检与使用边界

每次核对实际运行器版本/构建标识、公开场景契约、所需插件/通道及静态门禁。解析成功、执行成功和门禁识别是三个独立条件；旧运行报告不能证明新场景可用。

| 能力 | 已核验事实 | 使用边界 |
|---|---|---|
| `ASSERT_POINT` + `gpio:<pin>` / `pwm:<ch>` | 当前解析支持；现有 Blink/PWM 报告被凭据核验器接受。 | 核对业务时间、信号单位、通道、实际固件输出与拓扑。PWM 标量通常表示占空比，不能默认它同时验证频率。 |
| `INPUT_BUS` + `ASSERT_BUS_PAYLOAD` | 当前解析支持 UART；现有 Echo 有正向报告。 | 核对 UART bus ID、方向、编码与载荷；溢出/超时等负例须另有可观察契约。 |
| `INJECT_WIFI_FIXTURE` + `wifi:sta:state` / `netif:sta:ip` | 当前解析支持；现有 STA 场景有正向及断连报告。 | 核对 AP 凭据、故障目标、DHCP 与应用配置；断连证据不等于固件依赖证明。 |
| `ASSERT_WAVEFORM` / `ASSERT_SEQUENCE` | 合法实例可由当前运行时解析；静态语义门禁未正确识别。 | 可作为设计候选，交付前须修复门禁并验证真实执行；不能添加占位点断言绕过。 |
| `INJECT_NET_FIXTURE` / `http:client:*` | 当前解析不支持这些拟议接口。 | HTTP 契约设计草案，不能作为可执行模板或交付证据。 |
| `ble:*` / `gptimer:*` / `sleep:*` / `nvs:*` 等拟议信号 | 本指南未提供已验证的完整执行链。 | 逐项探测公开契约与出口；不要按名称猜测可用，不把全部同名能力一概判为产品排除。 |

不能识别 Step、Target 或 Matcher 时，输出所缺契约及验证方法。缺少能力保留 `planned/building` 等适用未交付状态与真实凭据状况，阻塞原因放在诊断或既有依赖机制中；不新增 `delivery_state` 枚举。

## 2. Matcher 与观测对象

| 意图 | 当前已核验 Matcher | 注意事项 |
|---|---|---|
| 精确结果 | `1`、`true`、`"DISCONNECTED"`、`"PING_PONG_TEST"` | 类型及业务含义匹配。 |
| 数值容差 | `{ "$near": { "target": 2.5, "tolerance": 0.1 } }` | 容差来源于业务/模型契约，不能放宽到任何结果都通过。 |
| 闭区间 | `{ "$between": [min, max] }` | 两端包含；上下界按实际单位与合法范围确定。 |
| 正则 | `{ "$regex": "^PING_PONG_TEST$" }` | 合适时锚定整个载荷；宽泛正则不能替代精确协议验证。 |

`matcher: ">0"` 是字符串比较，不是数值谓词。`{ "$between": [1, 2147483647] }` 只适合业务已定义为该范围的正整数计数，不能泛化为所有“大于零”浮点数。优先断言明确的载荷、长度、状态与业务结果。

区分以下观测对象：

- **物理引脚/器件**：按 Manifest 的有效拓扑格式核对连接；声明为空不能当作一致性已经通过。不要猜测新 `devices` 字段。
- **逻辑通道**：UART 的 `busType/busId/direction`、Wi-Fi STA/Netif 由公开运行时和门面契约定义；不要求为每个逻辑 Target 创建物理器件。
- **辅助环境**：静态电源检查可共存，但不计为核心业务证明。
- **初始化/稳态/不变量**：允许稳定结果，前提是它来自业务且固件依赖、指定业务变异能区分正确与错误实现。

## 3. 可解析的场景结构示例

以下三个模板用于结构参考，已核验当前运行时解析。并未在本次 Skill 修订中重新构建或执行它们；应用参数、时序、插件、门禁及因果检查必须另行验证。不要将模板的示例名字或通过旧报告视为本轮身份绑定。

### 3.1 GPIO 自主周期翻转

初始低电平是辅助检查，后续周期翻转才是业务检查。时间来自示例的延时配置；实际配置变化时同步调整。自主闪灯不需要人为外部输入。

```json
{
  "header": {
    "version": "1.0.0",
    "name": "ESP-IDF blink GPIO structure example",
    "templateId": "esp_idfv61_blink_gpio",
    "accuracyMode": "behavioral",
    "timeoutUs": "3500ms",
    "failurePolicy": "fail-fast",
    "determinism": { "prngSeed": 42 }
  },
  "steps": [
    { "type": "ASSERT_POINT", "timeUs": "50ms", "target": "gpio:2", "matcher": 0 },
    { "type": "ASSERT_POINT", "timeUs": "1050ms", "target": "gpio:2", "matcher": 1 },
    { "type": "ASSERT_POINT", "timeUs": "2050ms", "target": "gpio:2", "matcher": 0 }
  ]
}
```

### 3.2 UART Echo

输入通过 RX 进入真实业务，再观察 TX；不能由 Fixture 把输入直接复制成输出。先确认应用实际使用 UART1。当前模板只有正常 Echo，不涵盖缓冲溢出或异常恢复。

```json
{
  "header": {
    "version": "1.0.0",
    "name": "ESP-IDF UART echo structure example",
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
      "payload": { "encoding": "utf8", "text": "PING_PONG_TEST" }
    },
    {
      "type": "ASSERT_BUS_PAYLOAD",
      "windowUs": ["100ms", "1000ms"],
      "busType": "uart",
      "busId": 1,
      "direction": "tx",
      "matcher": "PING_PONG_TEST"
    }
  ]
}
```

### 3.3 Wi-Fi STA 与断连故障

AP 密码为示例虚拟凭据，必须与应用配置匹配；`targetSsid` 定位故障对象。这里断连后预期 `DISCONNECTED`，属于故障处理场景的成功断言，不能把它当作正向业务变异杀伤。

```json
{
  "header": {
    "version": "1.0.0",
    "name": "ESP-IDF Wi-Fi STA and disconnect structure example",
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
          "channel": 1,
          "authMode": "WPA2_PSK",
          "password": "mypassword",
          "dhcp": {
            "assignedIp": "192.168.1.100",
            "netmask": "255.255.255.0",
            "gateway": "192.168.1.1",
            "dns": "192.168.1.1"
          }
        }
      ]
    },
    { "type": "ASSERT_POINT", "timeUs": "1500ms", "target": "netif:sta:ip", "matcher": "192.168.1.100" },
    {
      "type": "INJECT_WIFI_FIXTURE",
      "timeUs": "2000ms",
      "fault": { "targetSsid": "myssid", "action": "DROP_BEACON", "reason": "BEACON_TIMEOUT" }
    },
    { "type": "ASSERT_POINT", "timeUs": "2500ms", "target": "wifi:sta:state", "matcher": "DISCONNECTED" }
  ]
}
```

## 4. HTTP 未来契约设计目标

`INJECT_NET_FIXTURE` 与 `http:client:*` 仍是拟议接口，因此本节不给可复制执行的 JSON 模板。扩展能力后至少应核验：

1. 固件发出的 method、URL/路径、必要请求字段与业务目标一致。
2. 虚拟对端按请求产生响应；观察经固件处理后的状态码、正确响应体/解析结果与应用回调。
3. 404、超时等故障场景检查真实错误处理；仅收到非零字节不能证明业务正确。
4. 保持 Fixture 与预期不变时，阻断固件核心请求或解析行为必须使对应正向业务断言失败。
5. 场景解析、插件执行、门禁、报告与复位链路均支持后，才讨论该配置交付。

无观测能力时记录运行时缺口；不要退化为电源、IP 就绪或仅有网络 Fixture 的场景。因果检查方法与归档要求见 [实证工作流](evidence-workflow.md)。
