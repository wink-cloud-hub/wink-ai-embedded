# ESP-IDF 仿真 D2：事件所有权与网络回调派发设计

| 字段 | 内容 |
|---|---|
| 状态 | **Proposed，待评审**；本文件描述 H5 目标契约，不表示代码已实现或 H5 门禁已通过 |
| 日期 | 2026-09-28 |
| 范围 | `wink-micro-os/frameworks/esp_idf/` 的 Host64 / Wasm32 默认事件循环、Wi-Fi/IP、MQTT；HTTP 与 BLE 的回调边界审计 |
| 关联实施计划 | [ESP-IDF 仿真基建加固 H5](../../../implementation-plans/esp32/2026-09-28-esp-idf-simulation-hardening-plan.md) |
| 关联决策 | [ADR-0012 契约诚实](../../../decisions/core/0012-contract-honesty-over-silent-degradation.md)、[ADR-0053 同刻事件总序](../../../decisions/unisim/0053-sim-same-timestamp-event-total-order.md)、[ADR-0022 异步原语](../../../decisions/core/0022-event-queue-mbox-async-primitives.md) |
| 现行覆盖表 | [ESP-IDF API 覆盖矩阵](../../../../wink-micro-os/frameworks/esp_idf/docs/02-api-coverage-matrix.md)；H5 实现后须按实测更新 |

## 1. 事实边界与设计目标

现有 `src/core/esp_event.c` 的 `esp_event_post()` 忽略 `data_size`、`ticks_to_wait`，在调用栈内同步遍历 handler；instance 是静态槽地址，注销后槽位复用可让旧 instance 注销新 handler。`src/network/esp_mqtt.c` 的 `dispatch_event()` 在调用 `esp_event_post()` 前直接调用客户端回调；`publish()` 和 `sim_inject_message()` 把含 `topic`/`data` 指针的局部事件传给它，且固定缓冲可能截断而 `data_len` 仍为原长。`src/wifi/esp_wifi.c` 的 Wi-Fi/IP 数据来自局部变量，当前同步派发掩盖了寿命问题。`src/network/esp_http.c` 的 `perform()`、`open()` 等接口同步调用 HTTP handler。

H5 的目标是：默认 `esp_event` 投递只入队；Wi-Fi/IP 和 MQTT **事件循环 handler** 在事件泵步骤调用；MQTT 客户端注册回调也不得由 `publish()`、`stop()`、`disconnect()`、`reconnect()`、`destroy()` 或注入接口在原调用栈直接调用。HTTP 客户端 `event_handler` 保持随 `esp_http_client_perform()` 等操作同步执行：官方说明 `perform()` 阻塞当前任务并在期间调用 handler，不能用“所有网络回调零同步”改写该 API。BLE GAP/GATT 回调的具体调用栈须逐 API 对照上游后登记，不能仅因其属于网络模块就迁入 `esp_event` 泵。[官方 `esp_event` 头文件 v6.1](https://github.com/espressif/esp-idf/blob/v6.1/components/esp_event/include/esp_event.h)、[官方 HTTP Client 文档 v6.0.2](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32/api-reference/protocols/esp_http_client.html)。

仓内 manifest 含 ESP-IDF v6.1 与 v5.1.3 语料；已核对公开 v6.1 `esp_event` 头文件与实现中的浅层数据复制、队列投递和错误码。H5 编码前仍须对仓内选定 v6.1 commit 做最小行为原型，记录 commit、宏配置与命令。不能从公开实现反推出本项目的精确默认队列容量或任务配置。

## 2. 派发边界与因果顺序

```text
调用者 / 网络生产者 --验证、复制、入 FIFO--> 默认事件循环队列
                                                   |
                                                   v
                                        单一事件泵任务 / 步骤
                                                   |
                              按登记顺序查有效 handler 并调用
                                                   |
                                       释放事件拥有的载荷

HTTP perform/open/... --同步执行--> HTTP client event_handler（独立契约）
```

1. 默认循环创建后才接受投递；重复创建返回 `ESP_ERR_INVALID_STATE`。未创建、正在关闭、已删除或复位中投递返回 `ESP_ERR_INVALID_STATE`，不触发 handler。删除/复位先关生产入口，再停泵、清 FIFO，最后使 handler 与网络对象失效；清理可重复且不得调用用户 handler。
2. `esp_event_post()` 验证 `event_base != ESP_EVENT_ANY_BASE` 且 `event_id != ESP_EVENT_ANY_ID`；`event_data == NULL && event_data_size > 0` 为本仿真新增的 `ESP_ERR_INVALID_ARG` 防护。成功返回意味着队列已拥有一份数据，**不意味着回调已经执行**。普通 `event_data` 仅按字节复制 `event_data_size`；上游亦只做这一层复制，不会自动追踪结构体内指针。[官方 v6.1 实现](https://github.com/espressif/esp-idf/blob/v6.1/components/esp_event/esp_event.c)。
3. 同一默认循环的事件以**成功入队顺序** FIFO 出队；单个事件的匹配 handler 按注册顺序调用。事件回调内再 `post` 仅入队，排在既有尾部；当前事件的 handler 遍历结束后才处理新事件，无递归派发。不同生产任务在同一虚拟时刻的次序由实际入队序号决定，不对真实 ESP32 的跨任务竞争顺序作额外承诺；不得违背 ADR-0053 已定义的因果序。
4. 派发时按 registration identity 检查存活性。已注销的 handler 不接收后续回调；在同一次派发期间注销尚未调用的 handler，则跳过它；新注册的 handler 从下一事件开始生效。回调中的 `event_data`、MQTT `topic`/`data`/`error_handle` 在该次回调返回前有效；用户若需保留，必须自己复制。`event_handler_arg` / `user_context` 是用户借出的指针，框架不拷贝或释放，用户须保证直到注销且在途回调结束仍有效。[官方事件 handler 参数寿命说明](https://github.com/espressif/esp-idf/blob/v6.0.2/components/esp_event/include/esp_event.h)。
5. MQTT 使用一个入队记录和一次派发路径，避免客户端回调、旧 `config.event_handle` 与全局 `MQTT_EVENTS` handler 因重复投递而收到两次。具体顺序拟定为客户端注册回调 → 兼容配置回调 → 全局事件 handler；三者均由泵执行。H5 红测试固定该顺序，并在 v6.1 对照后确认兼容回调是否应保留。

## 3. 有界队列与返回值

| 场景 | D2 目标行为 | 依据 / 说明 |
|---|---|---|
| 队列容量 | 默认循环采用编译期可配置的 `WINK_ESP_EVENT_QUEUE_CAPACITY`，拟定缺省 **32 个事件**；无合法容量时配置失败 | 32 是仿真拟定值，须与选定 v6.1 SDK 配置对照；不得标为上游固定值 |
| 正常投递 | 复制成功并占到槽后返回 `ESP_OK` | 数据复制点在 `post()` 返回前；存储耗尽返回 `ESP_ERR_NO_MEM`，原队列不变 |
| 满队列，`ticks_to_wait == 0` | 返回 `ESP_ERR_TIMEOUT`，释放本次临时复制，队列不变 | 上游将队列超时与满队列结果定义为 `ESP_ERR_TIMEOUT`，不是 `ESP_ERR_NO_MEM` |
| 满队列，有限等待 | 调用者按 FreeRTOS tick 等待空槽；成功则 `ESP_OK`，期限到仍满则 `ESP_ERR_TIMEOUT` | 时间以现有虚拟 tick 为准，超时 waiter 需清除 |
| 满队列，`portMAX_DELAY` | 普通任务可等待；事件泵自身和不可阻塞上下文不得等待自己释放槽，立即返回 `ESP_ERR_TIMEOUT` | 上游 dedicated loop task 内投递采用零等待以避免自锁；H5 要验证本地调度模型 |
| 无法安全阻塞的 Host/Wasm 外部注入 | 可立即入队则成功，否则 `ESP_ERR_TIMEOUT`；不在 JS/宿主调用栈里运行泵来腾槽 | 不以同步派发偷换等待语义；该边界须在模拟专用 API 文档标出 |
| loop 正关闭 / 复位 | 返回 `ESP_ERR_INVALID_STATE`；队列剩余项逐一销毁 | 丢弃只发生在显式生命周期边界，统计 dropped 数和原因供测试 |

入队事务顺序：验证参数 → 计算/检查尺寸与整型溢出 → 准备私有载荷 → 等槽 → 原子插入；任何失败都释放本次载荷且不改变已排队项。不能为腾出容量而静默覆盖老事件。H5 若使用静态载荷池，满池与满队列必须分别可观测；载荷池满返回 `ESP_ERR_NO_MEM`。普通任务等待期间若 loop 删除/复位，应醒来并返回 `ESP_ERR_INVALID_STATE`。

## 4. 事件所有权表

| 类型 / 入口 | 生产方原始数据及内嵌指针 | 入队时所有权与最大长度 | 派发 / 丢弃后释放者 |
|---|---|---|---|
| 任意用户 `esp_event_post` | 调用方提供的 `event_data`；其内部未知指针均为借用 | 事件循环只复制 `event_data_size` 字节；`event_data_size > 0` 要求非空；大小上限由可配置载荷预算约束，超额明确 `ESP_ERR_NO_MEM` | 事件循环释放结构体字节副本；不追踪、更不释放未知指针 |
| `WIFI_EVENT_STA_START/STOP/DISCONNECTED` | 无载荷 | 零字节事件记录 | 事件循环回收记录 |
| `WIFI_EVENT_STA_CONNECTED` | 栈上 `wifi_event_sta_connected_t`；SSID/BSSID 是内嵌数组 | 复制整个结构体，大小恰为 `sizeof`；不能借用栈地址 | 事件循环回收副本 |
| `IP_EVENT_STA_GOT_IP` | 栈上 `ip_event_got_ip_t`；`esp_netif` 是借用句柄，其余地址字段按值 | 复制结构体；`esp_netif` 不被事件拥有，必须在派发前仍有效或以失效检查拒发 | 事件循环回收副本，不释放 `esp_netif` |
| MQTT 连接/断开/发布等 | 栈上 `esp_mqtt_event_t`；`client`、`user_context` 为借用 | 私有 envelope 复制标量；保存客户端身份与代际/生命周期 token，派发前核对，不能依赖裸槽指针判断复用 | MQTT envelope 析构器 |
| `MQTT_EVENT_DATA` | `topic` 文本和 `data` 二进制由调用方/客户端缓冲提供；`topic_len`、`data_len` 为显式长度 | 私有 envelope 同时复制事件标量、`topic_len` 字节和 `data_len` 字节；可附 NUL 供文本用，但不得用 `strlen(data)`；拟定 topic ≤127 字节、payload ≤511 字节，超过则失败，禁止截断后保留原长度 | MQTT envelope 析构器同时释放/回收两段副本 |
| `MQTT_EVENT_ERROR` | `error_handle` 指向客户端错误对象 | 把错误码结构体复制到 envelope；派发时重定位指针到副本，非错误事件置 NULL | MQTT envelope 析构器 |
| HTTP `event_handler` | `esp_http_client_event_t`、`data`、header key/value 为 `perform/open/...` 本次操作中的借用视图 | **不进入默认 `esp_event` 队列**；回调结束前有效，现有 Mock 长度上限另由 HTTP 契约管理 | HTTP 客户端保持现有同步寿命，调用者不得保留借用指针 |
| BLE GAP/GATT 回调 | 待按具体 API 审计 | 不预设进入默认事件队列；如未来异步化须另列事件类型、深拷贝与上游对照 | 对应 BLE 模块，H5 审计记录 |

MQTT `publish()` / 注入路径应在**返回值可表达的地方**对超限及队列失败显式返回错误（例如 publish 返回 `-1`，注入返回负值），且不得报告成功 `msg_id`/匹配数。`stop()`、`destroy()` 等返回 `esp_err_t` 的路径须传递失败。已成功排队但后来因客户端销毁/复位而取消的事件由 envelope 清理，不回调失效对象。若 `MQTT_EVENT_DELETED` 需要送达，应在客户端对象仍有效时按事件泵顺序完成，否则明确登记与上游差异；不可排队后立即 `memset(client)` 再交给回调。

## 5. handler identity 与生命周期

instance 句柄不能是可复用静态槽地址。H5 最小要求是**跨 Host64/Wasm32 不解引用旧句柄**的稳定 identity（独立描述符或经 H6 验证的代际令牌）；查找须同时比对 loop epoch、slot、generation。注销、loop delete 和 reset 使旧 identity 失效；槽位重用不让旧 identity 命中新注册项。代际回绕必须有拒用或扩容策略，不能默默回到旧 token。此处与 H6 资源句柄方案共享审查结论，若 H6 尚未通过 H3 门禁，H5 不得以裸槽地址修补冒充 ABA 已解决。

旧版 `esp_event_handler_register` 的 base/id/function 取消注册只影响匹配 registration；`ESP_EVENT_ANY_BASE` 只能与 `ESP_EVENT_ANY_ID` 联用。注册与注销中的通配符是筛选条件，`post` 不允许通配符。官方 legacy 注册同一 handler 到同一事件时保留原派发位置并覆盖 `event_handler_arg`；instance 注册允许重复，每次产生独立身份。当前仿真的“同三元组幂等更新 arg”仅可能对应前者，H5 必须分别测试两个入口。[官方事件循环文档](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/esp_event.html)、[v6.1 头文件](https://github.com/espressif/esp-idf/blob/v6.1/components/esp_event/include/esp_event.h)。

## 6. H5 红绿测试 oracle 与退出门禁

| 编号 | 旧行为应失败的断言 / 新行为应通过的断言 |
|---|---|
| D2-T1 | `post` 返回时 handler 次数为 0；泵一步后为 1。回调内再 `post` 时调用深度始终为 1，FIFO 顺序为 A、B、C。 |
| D2-T2 | 栈上 Wi-Fi/IP 载荷投递后改写原字节；handler 只见入队快照。任意用户事件结构体内指针仅浅复制，测试明确保留调用方对象寿命。 |
| D2-T3 | MQTT topic/data 用临时缓冲投递并立即改写，泵只见原字节；嵌入 NUL 的 payload 按 `data_len` 完整传递；error_handle 指向私有副本。超限不截断成功。 |
| D2-T4 | `publish`、`sim_inject_message`、`stop` 等调用栈内没有 MQTT 用户回调；`perform()` 的 HTTP handler 仍可在调用期间观察到。 |
| D2-T5 | 队列边界第 32 项成功、第 33 项零等待 `ESP_ERR_TIMEOUT`；有限等待后腾槽成功或超时；泵回调满队列再次投递不自锁；载荷池耗尽 `ESP_ERR_NO_MEM`。容量改变后按配置边界重跑。 |
| D2-T6 | A handler 在同次派发中注销 B 并复用槽位；B 和新 handler 均不接当前事件，新 handler 接下一事件。旧 instance 再注销返回失败，不能删除新 handler。 |
| D2-T7 | 删除默认循环、MQTT client destroy、网络 stop 与软复位交错后无悬挂回调、泄漏或旧事件污染新实例；Host64、Wasm32 实际运行轨迹一致。 |
| D2-T8 | Wi-Fi→IP→MQTT 的可观察顺序按入队因果推进；同虚拟时间重复运行轨迹稳定；HTTP 同步边界和 BLE 各 API 的真实时序单独记账。 |

H5 开始条件：D2 评审确认；v6.1 官方最小原型核定队列/注册/删除语义；H2、H4 的计划前置达标；H6 identity 依赖与 H3 门禁按原计划处理。实施时先留红测试，再改代码；完成后更新 ESP-IDF 头文件注释、覆盖矩阵的 `esp_event_post` 与 MQTT/HTTP 条目及现行设计规范，运行 Host/Wasm 回归、`winkcli lint --pack layering --pack api` 与许可门禁。任何与上游不同的容量、载荷上限、HTTP/MQTT 时序或模拟注入返回值须登记为仿真范围/降级，并有可复现测试，不能标成全兼容。
