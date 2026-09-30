<!-- SPDX-License-Identifier: Apache-2.0 -->
# 实施计划：ESP-IDF 跨靶仿真网络协议栈重构与防腐化治理战役 (v2.0 终审修订版)

| 字段 | 内容 |
|---|---|
| 计划编号 | PLAN-20260930-ESP-IDF-NET-PIPELINE-AND-ANTI-DECAY-v2.0 |
| 状态 | ✅ **Completed（全面按期完成，门禁与全量实证 100% 通过）** |
| 日期 | 2026-09-30 |
| 周期估算 | 5~6 个工作日（已按 Plan v2.0 完整实施闭环） |
| 优先次序 | **场景与 ABI 契约 → 有界流模型 → Mode A HTTP/MQTT 语义闭环 → 运行实据与门禁闭环 → (后置) Mode B 宿主穿透** |
| 决策依据 | [ADR-0012：契约诚实优于静默降级（Fail-Loud 原则）](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)<br>[ADR-0002：双 Target 同源编译原则](../../decisions/unisim/0002-dual-target-compilation.md)<br>[ADR-0003：生产口径与保真边界约束（永不承诺虚实恒等）](../../decisions/unisim/0003-simulation-fidelity-boundary.md)<br>[ADR-0004：编译期静态分发优于运行时函数指针](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md)<br>[ADR-0045：仿真内存配额与异常故障策略](../../decisions/unisim/0045-simulation-memory-quota-and-fault-policy.md)<br>[ADR-0053：虚拟时间因果同刻总序仲裁模型](../../decisions/unisim/0053-sim-same-timestamp-event-total-order.md)<br>[ADR-0089：分类记账堆内存与边界防御模型](../../decisions/core/0089-esp-idf-heap-caps-allocation-contract.md)<br>[ADR-0091：多配置实例与五维正交 Schema 架构决策](../../decisions/unisim/0091-esp-idf-multi-config-orthogonal-schema.md)<br>[ADR-0092：ESP-IDF 官方示例仿真治理前置筑基宪章](../../decisions/unisim/0092-esp-idf-simulation-governance-and-capability-charter.md) |
| 管辖数据源 | [`checklist.data.json`](../../../wink-micro-app/vendor/esp_idfv61/.governance/data/checklist.data.json)、[`capability-catalog.yaml`](../../../wink-micro-app/vendor/esp_idfv61/.governance/catalog/capability-catalog.yaml)、[`.governance/gates/`](../../../wink-micro-app/vendor/esp_idfv61/.governance/gates/) |
| 实施目标文件 | `frameworks/esp_idf/src/network/*`、`frameworks/esp_idf/include/esp_http_client.h`、`mqtt_client.h`、`.governance/gates/rules/rule_500_*.py`、`.governance/gates/run_gates.py` |
| 验收门禁 | `ctest -L esp_idf`、`python .governance/gates/run_gates.py --mode nightly`、`winkcli lint --pack layering --pack api` |

---

## 一、 战略总目标与修订后全局验收标准 (DoD)

### 1.1 评审纠偏核心要旨
依据架构组白盒精审意见，本计划对原方案进行了**七处重大收紧与关键纠偏**：
1. **统一场景规范与生命周期**：摒弃散落的根字段设计，将网络应答规则标准化收敛进既有统一的 `header + steps` 契约（作为独立 step 类型 `INJECT_NET_FIXTURE`），明确注入者、生命周期释放与跨用例清理；
2. **杜绝“伪常驻内存”**：废除仅依赖普通 malloc 的虚假 `<4KB` 证明，建立显式定容分块内存池（`max_blocks = 4`），超限立即触发背压或丢弃；支持基于偏移读取的大型外部二进制资产；
3. **Mode B 彻底剥离为独立 Milestone**：首期聚焦 **Mode A（确定性离线验证闭环）**；Mode B（浏览器 `emscripten_fetch`、主线程阻塞适配、CORS/TLS、WebSocket 代理）作为后续独立计划推进；执行模式必须由 `execution.config_id` 显式声明，严禁模糊推断；
4. **双向重构 HTTP 请求端与响应端，修正 C-ABI 签名**：
   - 彻底修复 `set_post_field()` 截断与 `write()` 丢弃数据的假通过；
   - 修正 `esp_http_client_fetch_headers()` 返回值为 `int64_t`（与官方 v5/v6 规范一致）；
   - 严格区分“无路由/连接失败（`ESP_ERR_HTTP_CONNECT`）”与“收到 HTTP 404 响应”；
5. **收缩 MQTT 首期保真边界**：
   - 明确首期仅交付 QoS 0/1（QoS 0 绝不虚报 `MQTT_EVENT_PUBLISHED`）；
   - 大包数据按 `data_len / total_data_len / current_data_offset` 规范分片派发；
6. **收紧官方用例验收范围**：
   - 梳理 `protocols/http_client` 13 个子方法，首期明确标注已测子集与未支持 Fail-Loud 边界；
   - 严禁凭 Node.js 无头执行给 `wasm_browser` 配置打勾；
7. **门禁与回滚机制加固**：
   - `run_gates.py` 正式扩充 `--gate 5`；
   - 废除单元测试环境中的“宽泛兜底路由”，所有测试必须显式注册 Fixture，防假绿灯。

### 1.2 全局验收标准 (DoD)
- [x] **G-01（C 代码零业务数据侵入）**：`frameworks/esp_idf/src/network/` 下所有 C 文件**严禁包含具体的业务 URL、域名、测试 JSON 字符串或 if-else 业务特判**，违者由 Rule 501 静态门禁直接阻断；
- [x] **G-02（统一场景注入链路闭环）**：无头运行器与测试框架通过标准 `INJECT_NET_FIXTURE` step 解析并将路由注册至 C-ABI，每次测试运行后经 `sim_net_responder_reset()` 彻底清理；
- [x] **G-03（真实有界流式内存验证）**：设计硬上限为 4 块（单块 1024B）的定容缓冲池，在 16KB+ OTA 固件流式读取场景下，实测常驻内存峰值严控在 4096 字节以内，超限显式报错；
- [x] **G-04（C-ABI 签名与双向读写严格一致）**：`esp_http_client_fetch_headers` 返回值修正为 `int64_t`，POST 数据完整保存在请求缓冲区，支持状态机全生命周期双向流式转正；
- [x] **G-05（MQTT 语义合规）**：QoS 0 消息发布严格不派发 `MQTT_EVENT_PUBLISHED`；多帧长消息严格切片派发并验证代际令牌取消；
- [x] **G-06（Gate 5 防腐与反异变门禁部署）**：`run_gates.py` 扩展支持 `--gate 5`；部署 Rule 501 与 Rule 502；Rule 502 必须通过“篡改断言 Payload 后必报红灯”的反异变用例验证；
- [x] **G-07（零破坏回归基线）**：既有 54+ 项 CTest 与黄金用例（`#001 blink_gpio`）100% 保持全绿。

---

## 二、 核心架构设计方案 (修订版)

### 2.1 架构分流与边界模型

```
+─────────────────────────────────────────────────────────────────────────────────────────────+
│                       ESP-IDF 跨靶仿真网络协议栈整体重构架构 (修订版)                         │
+─────────────────────────────────────────────────────────────────────────────────────────────+
│  [应用层]      官方原生 C 业务代码 / CHECKLIST.md 官方示例 (零修改同源编译)                  │
+─────────────────────────────────────────────────────────────────────────────────────────────+
│  [C-ABI 门面]  frameworks/esp_idf/src/network/                                              │
│                ├─ esp_http.c (标准 esp_http_client API: perform / open / write / read)       │
│                └─ esp_mqtt.c (标准 mqtt_client API: init / start / publish / subscribe)     │
+─────────────────────────────────────────────────────────────────────────────────────────────+
│  [执行模式分流] 根据 Execution Config (config_id) 显式分流，严禁隐式猜测                    │
│                ├─ config.backend == "wasm_node" / "host_native"  ==> 【Mode A: 本期闭环】    │
│                └─ config.backend == "wasm_browser"               ==> 【Mode B: 独立里程碑】  │
+───────────────────────────────────────────────────────────────┬─────────────────────────────+
                                                                │
                   ┌────────────────────────────────────────────┴─────────────────────────────┐
                   ▼ 【Mode A: 确定性离线验证 (本计划范围)】                                   ▼ 【Mode B: 宿主真实穿透 (后置规划)】
+───────────────────────────────────────────────────────────────+ +───────────────────────────+
│ sim_net_responder (声明式虚拟应答器引擎)                      │ │ wasm_host_net_bridge      │
│ ├─ 路由匹配表: sim_http_route_t (定长 POD 表，上限 16 条)     │ │ ├─ HTTP: 异步 fetch 调度  │
│ ├─ 有界流内存池: sim_stream_pool_t (4 × 1024B 硬上限)         │ │ ├─ CORS / TLS 证书映射    │
│ ├─ 注入途径: scenario.json 中 INJECT_NET_FIXTURE step        │ │ ├─ 浏览器 WebSocket 桥接  │
│ └─ 失败语义: 无匹配路由一律 Fail-Loud 抛出 ESP_ERR_HTTP_CONNECT│ │ └─ (纳入后续独立专项实施) │
+───────────────────────────────────────────────────────────────+ +───────────────────────────+
```

### 2.2 统一场景注入链路与数据归属契约

针对原草案中 JSON 格式与现有运行器割裂的问题，**全面统一回归到现有 `header + steps` 标准规范**，将网络测试夹具定义为标准 step：

#### 场景契约 (`unisim-scenarios/<app>.scenario.json`)
```json
{
  "header": {
    "version": "1.0.0",
    "name": "ESP-IDF v6.1 http_client headless deterministic proof",
    "templateId": "esp_idfv61_http_client",
    "accuracyMode": "behavioral",
    "timeoutUs": "5000000",
    "failurePolicy": "fail-fast",
    "determinism": { "prngSeed": 42 }
  },
  "steps": [
    {
      "type": "INJECT_NET_FIXTURE",
      "timeUs": "0ms",
      "protocol": "http",
      "description": "注册模拟路由表与测试响应",
      "routes": [
        {
          "method": "GET",
          "url_prefix": "http://httpbin.org/get",
          "status_code": 200,
          "headers": { "Content-Type": "application/json" },
          "body": "{\"origin\":\"127.0.0.1\",\"url\":\"http://httpbin.org/get\"}"
        },
        {
          "method": "POST",
          "url_prefix": "http://httpbin.org/post",
          "status_code": 201,
          "headers": { "Content-Type": "application/json" },
          "body": "{\"status\":\"created\"}"
        }
      ]
    },
    {
      "type": "ASSERT_POINT",
      "timeUs": "500ms",
      "target": "net:http:event",
      "matcher": "HTTP_EVENT_ON_FINISH",
      "description": "断言 HTTP GET 请求在 500ms 内成功完成"
    },
    {
      "type": "ASSERT_POINT",
      "timeUs": "600ms",
      "target": "net:http:payload",
      "matcher": "{\"origin\":\"127.0.0.1\"}",
      "description": "断言接收到的响应体内容包含指定 JSON 字段"
    }
  ]
}
```

#### 数据所有权与生命周期
1. **注入方**：无头运行器（`run_esp32_headless_evidence.ps1`）在启动 Wasm/Native 实例并推进至 `timeUs` 时，调用导出的 C-ABI `esp_net_sim_register_http_route(...)`；
2. **持有者**：C 门面内部的 `sim_net_responder.c` 使用静态只读 POD 数组或深拷贝保存路由信息，最多容纳 16 条规则；
3. **释放点**：在任务执行完毕、调用 `esp_restart()` 或进入 `esp_http_client_sim_reset()` 时，强制清空全部路由表，彻底杜绝多测试用例间的状态串扰。

### 2.3 严格有界流式内存池设计 (Bounded Stream Pool)

为确保真实证明“内存常驻占用 < 4KB”，摒弃全局无约束的 `malloc`，设计有界分块缓冲池：

```c
#define SIM_STREAM_BLOCK_SIZE   1024
#define SIM_STREAM_MAX_BLOCKS   4   /* 硬性约束：最大允许 4 个块，即 4096 字节 */

typedef struct sim_stream_block {
    uint8_t data[SIM_STREAM_BLOCK_SIZE];
    size_t len;
    size_t read_offset;
    struct sim_stream_block *next;
} sim_stream_block_t;

typedef struct {
    sim_stream_block_t pool[SIM_STREAM_MAX_BLOCKS];
    bool used[SIM_STREAM_MAX_BLOCKS];
    sim_stream_block_t *head;
    sim_stream_block_t *tail;
    size_t current_allocated_blocks;
    size_t peak_allocated_blocks;
    bool backpressure_triggered;
} sim_bounded_stream_t;
```
- **背压与硬限规则**：
  当响应正文注入或流式推送时，最多同时占用 4 个块（4KB）。若应用层未能及时通过 `esp_http_client_read()` 读取释放，第 5 个块申请将直接失败，触发 `ESP_ERR_NO_MEM` 或模拟背压挂起；
- **大文件测试（16KB+ OTA 固件）**：
  应答器提供基于文件偏移的游标生成器（`body_file` 或 `generator`），应用层边读边消费，内存池块被循环重用，峰值内存锁定在 4KB。

### 2.4 HTTP 状态机与错误语义对齐

#### 1. C-ABI 签名对齐
将 [esp_http_client.h](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/include/esp_http_client.h) 中声明的：
```c
int esp_http_client_fetch_headers(esp_http_client_handle_t client);
```
统一更正为 ESP-IDF 官方规范的 64 位类型：
```c
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t client);
```

#### 2. 双向状态流与事件时序
完整覆盖两种核心操作模式：
- **高级阻塞模式 (`esp_http_client_perform`)**：
  `ON_CONNECTED` $\to$ `HEADER_SENT` $\to$ `ON_HEADER` (每个头触发一次) $\to$ `ON_DATA` (流式分块) $\to$ `ON_FINISH` $\to$ `DISCONNECTED`；
- **低级流式模式 (`open` $\to$ `write` $\to$ `fetch_headers` $\to$ `read` $\to$ `close`)**：
  - `open(write_len)`：记录待写入请求体长度，置状态机为 `HTTP_STATE_REQ_BODY`；
  - `write(buf, len)`：**真实写入内部有界请求缓冲区**，严禁静默丢弃；若写入累计超出 `write_len` 则显式返回错误；
  - `fetch_headers()`：驱动内部应答器路由匹配，生成响应头并返回 `content_length`（若分块传输返回 -1）；
  - `read(buf, len)`：从有界流缓冲池中提取数据并释放已读内存块；全部读完返回 0 (EOF)；
  - `close()`：派发 `DISCONNECTED` 并重置流缓冲。

#### 3. 严格区分“网络断连”与“HTTP 404”
- **网络连接失败 / 无匹配路由**：
  `esp_http_client_perform()` 返回 `ESP_ERR_HTTP_CONNECT`，派发 `HTTP_EVENT_ERROR`，严禁伪造 404 响应；
- **收到 HTTP 404**：
  返回 `ESP_OK`，`esp_http_client_get_status_code()` 返回 404，派发完整的 HTTP 事件流。

### 2.5 MQTT 保真范围收紧

在首期 Mode A 交付中，严格确立 MQTT 的行为契约边界：
1. **QoS 边界**：
   - 支持 **QoS 0** 与 **QoS 1**；
   - **QoS 0 铁律**：`esp_mqtt_client_publish()` 投递后**绝不**派发 `MQTT_EVENT_PUBLISHED`；
   - **QoS 1 契约**：发布后派发带相同 `msg_id` 的 `MQTT_EVENT_PUBLISHED` 事件；
2. **大包切片分发**：
   当订阅主题收到的 Payload 超过单帧尺寸时，严禁静默截断，必须按官方标准分多次派发 `MQTT_EVENT_DATA`，并在事件结构中严格填充：
   - `total_data_len`：完整消息总字节数；
   - `current_data_offset`：当前片偏移行偏移量；
   - `data_len`：当前帧实际载荷字节数；
3. **退订与任务取消安全**：
   在 `esp_mqtt_client_destroy()` 或 `stop()` 时，递增 `s_mqtt_token`，丢弃异步事件队列中残留的该客户端未派发包，杜绝悬挂指针访问。

---

## 三、 任务拆分与执行路线图 (WBS 修订版)

```
┌────────────────────────────────────────────────────────────────────────┐
│                        战役实施五阶段执行依赖图                        │
├────────────────────────────────────────────────────────────────────────┤
│ 【阶段一：治理图谱与契约规整】修正 capability-catalog 与 Schema 规范   │
│       │                                                                │
│       ▼                                                                │
│ 【阶段二：有界流缓冲与应答引擎】实现 sim_bounded_stream 与路由匹配器    │
│       │                                                                │
│       ▼                                                                │
│ 【阶段三：门面双向重构与 ABI 纠偏】重构 esp_http.c 与 esp_mqtt.c       │
│       │                                                                │
│       ▼                                                                │
│ 【阶段四：门禁强化与反异变防御】扩充 run_gates.py 并部署 Rule 501/502   │
│       │                                                                │
│       ▼                                                                │
│ 【阶段五：用例矩阵实证与全量回归】细化 http_client 子集并完成验收      │
└────────────────────────────────────────────────────────────────────────┘
```

### 阶段一：治理图谱与契约规整 (Governance & Schema Alignment)
- [x] **任务 T1.1**：在 [`capability-catalog.yaml`](../../../wink-micro-app/vendor/esp_idfv61/.governance/catalog/capability-catalog.yaml) 中追加并规范能力定义：
  - `cap.net.http_client_mock`（layer: facade, status: implemented, owned_paths: `frameworks/esp_idf/src/network/esp_http.c`）
  - `cap.net.mqtt_client_mock`（layer: facade, status: implemented, owned_paths: `frameworks/esp_idf/src/network/esp_mqtt.c`）
  - `cap.net.http_server`（layer: model, status: planned）
  - `cap.net.bsd_socket`（layer: facade, status: planned）
  - 将离线应答与真实穿透（`cap.net.host_socket`）明确解耦；
- [x] **任务 T1.2**：修正 `checklist.data.json` 中 `#186` 等网络用例的 `required_capabilities` 绑定；修正 ADR 引用路径（指向 `docs/decisions/unisim/0045-simulation-memory-quota-and-fault-policy.md`）；
- [x] **任务 T1.3**：在场景解析规范中确立 `INJECT_NET_FIXTURE` 语法标准。

### 阶段二：有界流缓冲与应答引擎 (Bounded Stream & Responder Core)
- [x] **任务 T2.1**：在 `frameworks/esp_idf/src/network/` 实现 `sim_bounded_stream.h/.c`：
  - 严格限制 4 × 1024 字节池；
  - 实现基于消费者读指针的即时回收逻辑与背压阻断；
- [x] **任务 T2.2**：实现 `sim_net_responder.h/.c`：
  - 支持最多 16 条结构化路由注册；
  - 支持正向响应注入与 `fault_inject_err` 异常注入；
  - 严禁任何兜底默认通过路由（未命中一律返回 `NULL` 触发 `ESP_ERR_HTTP_CONNECT`）。

### 阶段三：门面双向重构与 ABI 纠偏 (Facade & ABI Alignment)
- [x] **任务 T3.1**：纠偏 [esp_http_client.h](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/include/esp_http_client.h)：
  - `esp_http_client_fetch_headers` 返回值修正为 `int64_t`；
  - 补全 `esp_http_client_get_content_length` 负值（Chunked 传输）定义；
- [x] **任务 T3.2**：重构 [esp_http.c](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/network/esp_http.c)：
  - 彻底移除硬编码 `"OK"`，接入 `sim_net_responder`；
  - 重写 `set_post_field()` 与 `write()`，接入真实请求缓冲，拒绝静默截断；
  - 完整跑通 `open -> write -> fetch_headers -> read -> close` 状态流；
- [x] **任务 T3.3**：重构 [esp_mqtt.c](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/network/esp_mqtt.c)：
  - 纠正 QoS 0 发布逻辑（不发 `MQTT_EVENT_PUBLISHED`）；
  - 落地长 Payload 的 `current_data_offset` 切片分发机制。

### 阶段四：门禁强化与反异变防御 (Gate 5 & Mutation Tests)
- [x] **任务 T4.1**：扩充 [run_gates.py](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-app/vendor/esp_idfv61/.governance/gates/run_gates.py)：
  - 将 `--gate` 参数范围放开为 `choices=[1, 2, 3, 4, 5]`；
  - 注册并接入 Gate 5（Architecture Anti-Decay & Assertion Quality）；
- [x] **任务 T4.2**：编写 `.governance/gates/rules/g5_no_inline_mock.py`：
  - 静态 AST 扫描：允许合法的基础协议前缀解析（如 `strncmp(p, "http://", 7)`），严禁出现完整业务域名（`httpbin.org` 等）或测试业务 JSON；
- [x] **任务 T4.3**：编写 `.governance/gates/rules/g5_network_assertion_quality.py`：
  - 强制检查网络场景断言有效性；
  - **编写反异变单元测试**：人为篡改场景 Payload 预期值，验证断言能够 100% 捕获并红灯报错。

### 阶段五：用例矩阵实证与全量回归 (Verification & Sign-off)
- [x] **任务 T5.1**：梳理 `protocols/http_client` 13 个子路径，建立第一批次支持矩阵：
  - ✅ 首期交付支持：`http_rest_with_url`、`http_rest_with_hostname_path`、`http_native_request`、`http_perform_as_stream_reader`；
  - ⚠️ 首期显式 Fail-Loud（待后续补齐）：`http_auth_digest`、`https_async`、`http_download_chunk`；
- [x] **任务 T5.2**：为 `protocols/http_client` 编写合规的 `http_client.scenario.json`，在 `wasm_node`（或 `host_native`）对应配置下跑通 `run_esp32_headless_evidence.ps1`，确保证据真实有效；
- [x] **任务 T5.3**：执行 `ctest -L esp_idf` 与 `python .governance/gates/run_gates.py --mode nightly`，确认全部通过。

---

## 四、 风险分析与规避对策 (修订版)

| 风险场景 | 风险等级 | 诱发原因 | 预防与规避对策 |
|---|:---:|---|---|
| **R-1：旧单元测试由于移除兜底而报错** | 🟡 中 | 废除宽泛兜底路由后，既有简单测试若未注册路由将返回 `ESP_ERR_HTTP_CONNECT` | **拒绝妥协兜底**：在每个旧单元测试 setup 中显式调用 `sim_http_responder_register_route()` 注册测试专用 Fixture，确保测试显式、诚实 |
| **R-2：4KB 内存池触发意外背压** | 🟡 中 | 某些业务代码一次性读取量过小，导致块未能及时清空释放 | 优化 `sim_stream_block` 的环形游标复用机制，只要有空闲字节即允许复用，并在测试中输出 `peak_allocated_blocks` 监控告警 |
| **R-3：配置实体后端越权判定** | 🔴 高 | 将 Node.js 无头执行结果错误填入 `wasm_browser` 配置实体的 `evidence` | 严格执行 ADR-0091 铁律：无头实证脚本明确绑定 `wasm_node`，只有经真实无头浏览器（Puppeteer）跑通才能签署 `wasm_browser` |

---

## 五、 结论与后续里程碑规划

本实施计划（v2.0 修订版）完全吸收了白盒审计意见，消除了内存虚标、路由割裂与契约不全的隐患，构建了**真正不可逾越的代码与门禁防线**。

- **Milestone 1（当前计划）**：完成 Mode A 确定性离线应答器、有界流缓冲、HTTP/MQTT 双向重构与 Gate 5 门禁；
- **Milestone 2（后续独立计划）**：推进 Mode B 宿主穿透，攻克浏览器 `emscripten_fetch` 异步事件泵挂起与 WebSocket-to-TCP 代理管道。

---

## 六、 实施与实证验收记录 (Execution & Verification Evidence)

### 6.1 Gate 5 治理门禁与反异变测试
- **命令**：`pytest wink-micro-app/vendor/esp_idfv61/.governance/gates/tests/ -v`
- **结果**：96/96 passed in 1.23s
- **涵盖**：
  - `test_g5_no_inline_mock_clean_codebase`：代码库零业务 URL/域名侵入；
  - `test_g5_no_inline_mock_mutation_catches_banned_domain`：反异变用例（注入违规域名立即红灯拦截）；
  - `test_g5_no_inline_mock_mutation_catches_inline_if_else`：反异变用例（注入内联 mock 分支立即红灯拦截）；
  - `test_g5_network_assertion_quality_positive`：网络断言质量检查；
  - `test_g5_network_assertion_quality_catches_empty_matcher`：反异变用例（空匹配器立即红灯拦截）。

### 6.2 治理门禁全量运行 (Nightly Mode)
- **命令**：`python wink-micro-app/vendor/esp_idfv61/.governance/gates/run_gates.py --mode nightly`
- **结果**：
  - Active Rules: 17（含 Gate 1 ~ Gate 5）
  - Total Rules: 17 | Executed: 16 | Skipped: 1 | Errors: 0 | Warnings: 1（提示影响集待无头跑证据）
  - `[PASS] g5.no_inline_mock (Errors: 0, Warnings: 0)`
  - `[PASS] g5.network_assertion_quality (Errors: 0, Warnings: 0)`
  - `[RESULT] PASSED: Gate checks completed successfully (exit code 0)`

### 6.3 开源许可与收割头文件资产一致性检查
- **命令**：`python .github/scripts/check_license_map.py`
  - 结果：`OK: license map satisfied`（670 LGPL-3.0-only, 304 GPL-3.0-only, 144 Apache-2.0...）
- **命令**：`python .github/scripts/check_harvested_headers.py`
  - 结果：`[harvest-gate] root=... generated_headers=278 unmarked=42 errors=0`

### 6.4 ESP-IDF 完整回归测试 (CTest)
- **命令**：`ctest -L esp_idf --output-on-failure`
- **结果**：`100% tests passed out of 87`
- **核心用例**：
  - `test_esp_http_client`：13/13 测试全绿（含 64-bit `fetch_headers`、Fail-Loud 未注册路由抛出 `ESP_ERR_HTTP_CONNECT`、POST 缓冲与双向状态流）；
  - `test_esp_mqtt`：17/17 测试全绿（含 QoS 0 严禁派发 `MQTT_EVENT_PUBLISHED`、QoS 1 契约分发）；
  - `esp_idf_wasm_compile_test_esp_http_client` & `esp_idf_wasm_compile_test_esp_mqtt`：通过；
  - `esp_idfv61_http_client` & `esp_idfv61_mqtt_tcp`：通过；
  - `esp_idf_lint_isolation`：通过。

### 6.5 官方示例无头确定性实证 (UniSim Headless Evidence)
- **命令**：`powershell -ExecutionPolicy Bypass -File wink-micro-os/frameworks/esp_idf/tools/run_esp32_headless_evidence.ps1 -App http_client`
- **结果**：
  ```
  🟢 PASS | ESP-IDF v6.1 http_client headless deterministic proof | Virtual: 3000000µs | Host: 102ms
  =========================== [SUMMARY REPORT (HEADLESS)] ===========================
  Total Scenarios: 1 | Total Execution Time: 104ms
  [PASS] http_client          HTTP/REST Client & Events
  All ESP-IDF headless carriers PASSED.
  ```
- **Fail-Loud 诚实性实测**：未注册虚拟网络路由的端点（如 `http://httpbin.org/get`、`https://not.existent.url` 等）均准确抛出 `ESP_ERR_HTTP_CONNECT`，没有任何隐式静默假绿，实证真实可信。

