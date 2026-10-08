# ESP-IDF Wave 1 Phase R2～R4 整改阶段复验评审报告

> **评审日期**: 2026-10-08  
> **评审对象**: ESP-IDF Wave 1 Phase R2～R4（有限网络模型、Reset DAG、治理闭包、组合应用与硬件差分标记）  
> **实施计划依据**: [2026-10-08-esp-idf-wave1-remediation-execution-plan.md](../../implementation-plans/esp32/2026-10-08-esp-idf-wave1-remediation-execution-plan.md)  
> **评审结论**: **PASS（软件模型、仿真及门禁治理验收全量通过；真机物理硬件项依规明确标记待做）**

---

## 1. 概述与基线核定

本阶段紧密承接 Phase R1 成果，在零修改原厂 9 份主审源码的前提下，彻底推进完成了 Phase R2（网络协议与生命周期模型）、Phase R3（治理证据与能力字典闭包）以及 Phase R4（多协议组合应用），并严格按照用户指令**将 ESP32 物理真机环境与板端测试明确标记为待做（TODO）**。

### 1.1 原厂业务源码零修改核定 (9 个主审原厂文件)

| 原厂文件路径 | 状态 | Git Diff |
|---|---|---|
| `wink-micro-app/vendor/esp_idfv61/protocols/http_server/restful_server/main/rest_server.c` | 保持基线 | 0 行变动 (字节级完全一致) |
| `wink-micro-app/vendor/esp_idfv61/protocols/http_server/restful_server/main/esp_rest_main.c` | 保持基线 | 0 行变动 (字节级完全一致) |
| `wink-micro-app/vendor/esp_idfv61/protocols/http_server/simple/main/main.c` | 保持基线 | 0 行变动 (字节级完全一致) |
| `wink-micro-app/vendor/esp_idfv61/protocols/http_server/ws_echo_server/main/ws_echo_server.c` | 保持基线 | 0 行变动 (字节级完全一致) |
| `wink-micro-app/vendor/esp_idfv61/protocols/sntp/main/sntp_example_main.c` | 保持基线 | 0 行变动 (字节级完全一致) |
| `wink-micro-app/vendor/esp_idfv61/protocols/sockets_tcp_client/main/tcp_client_v4.c` | 保持基线 | 0 行变动 (字节级完全一致) |
| `wink-micro-app/vendor/esp_idfv61/protocols/sockets_tcp_client/main/tcp_client_main.c` | 保持基线 | 0 行变动 (字节级完全一致) |
| `wink-micro-app/vendor/esp_idfv61/protocols/sockets_tcp_server/main/tcp_server.c` | 保持基线 | 0 行变动 (字节级完全一致) |
| `wink-micro-app/vendor/esp_idfv61/wifi/softap_sta/main/softap_sta.c` | 保持基线 | 0 行变动 (字节级完全一致) |

---

## 2. R2 有限网络模型与生命周期实施矩阵

### 2.1 R2-C：共享日志桥接解耦与构造函数去重 (S-09, M-08)
- **核心治理**: 从 `esp_sockets.c` 中彻底移除私有构造函数 `__attribute__((constructor)) init_sockets_uart_bridge`，消除对全局 `esp_log_set_vprintf` 的强符号静默劫持；应用及测试日志回归原生统一日志管线。
- **验证结论**: `test_esp_log.exe` 4/4 用例全部通过，日志格式化及 UART 边界完全受控。

### 2.2 R2-L：Reset DAG 全因果链与状态复位归零 (S-03, S-06)
- **核心治理**: 在 `esp_idf_bridge.c` 中将 `esp_sntp_sim_reset()` 与 `sim_sockets_reset()`（以 weak 符号形式兼容不同编译 target）正规接入 Stage 1（断开上层协议应用与清理在途请求）。
- **实现闭环**: `esp_sntp.c` 新增 `esp_sntp_sim_reset()`，彻底清空虚拟时间、RTC 漂移、同步状态、服务器配置及回调函数，确保软复位后资源占用完全归零。

### 2.3 R2-F & R2-T：描述符边界、错误语义与 Socket 选项 (S-05, C-01, C-02, Q-05)
- **核心治理**: 修复 `close()` 与 `closesocket()` 在遇到未知/未分配 fd 时返回 0 的虚假成功隐患，统一设 `errno = EBADF` 并返回 -1；在 `setsockopt()` 与 `getsockopt()` 中落地对 `SO_REUSEADDR`、`SO_KEEPALIVE`、`SO_RCVTIMEO` 与 `SO_SNDTIMEO` 的实装与状态读写，并对空指针参数严格返回 `errno = EFAULT`。
- **实现闭环**: 8 个 Socket 槽位相互隔离，连接状态机与错误语义对齐 POSIX 规范。

### 2.4 R2-N：SNTP 对端驱动授时、故障注入与单调等待 (S-04, Q-01, C-03)
- **核心治理**: 在 `esp_sntp.c` 中落地动态网络可达性评估 `esp_sntp_getreachability()`，引入故障注入接口 `sim_sntp_set_fault()`；在超时/网络不可达模式下，`esp_netif_sntp_sync_wait()` 严格按单调时钟等待并在到期后返回 `ESP_ERR_TIMEOUT`，故障清除后恢复正常授时。
- **测试落地**: `test_esp_idf_safeguards.c::test_wave1_r2_sntp_lifecycle_fault_and_timezone` (PASS)。

### 2.5 R2-H：WebSocket 控制帧、REST 映射与异步工作队列 (S-06~09, Q-03, Q-04)
- **核心治理**: 扩展 `req_aux_t` 支持 typed WebSocket frames（Ping、Pong、Binary、Close），保留分发上下文中的帧类型元数据；在 `httpd_queue_work()` 中增加服务器活动状态防卫，当服务器已停止或处于停止流程时严正拒绝入队并返回 `ESP_ERR_INVALID_STATE`。
- **测试落地**: `test_esp_idf_safeguards.c::test_wave1_r2_http_ws_frames_and_async_work_queue` (PASS)。

### 2.6 R2-A & R2-B：SoftAP+STA 数据面路由与端点转发 (Q-02)
- **核心治理**: 在 `sim_network_broker` 中实现网络接口双向路由模型、转发开关控制 `sim_network_broker_set_routing()` 以及数据包与字节数精确统计；模拟 SoftAP 接入客户端穿越 STA 网关访问上游 WAN 的真实因果。
- **测试落地**: `test_esp_idf_safeguards.c::test_wave1_r2_network_broker_routing_and_netifs` (PASS)。

---

## 3. R3 治理闭包与能力字典对齐

### 3.1 能力字典与条目闭包校准 (R3-G)
- **`cap.net.bsd_socket` 校准**: 将其 `owned_paths` 由历史笔误 `esp_socket_shim.c` 正确映射为 `wink-micro-os/frameworks/esp_idf/src/network/esp_sockets.c`，并将状态晋升为 `implemented`，挂载跨平台实证凭据。
- **`cap.net.sntp_client` 补齐**: 完整录入能力字典，管理路径指向 `esp_sntp.c`、`esp_sntp.h`、`esp_netif_sntp.h`。
- **总账关联 (#209, #212, #214)**: 在 `checklist.data.json` 中为 TCP Client/Server 关联 `cap.net.bsd_socket`，为 SNTP 关联 `cap.net.sntp_client`。
- **单向看板同步**: 运行 `generate_checklist_v1_1.py` 刷新 `CHECKLIST.md`，保持 100% 数据一致。

### 3.2 治理门禁与实证核查 (R3-I, R3-S, R3-P, R3-C, R3-R)
- **Gate 1 (12 规则检查)**: `run_gates.py --gate 1` -> **PASS** (12/12 规则通过，0 错误，0 警告)。
- **双实证校验器**: `evidence_verifier.py --verify-all` -> **PASS** (46/46 官方条目完整通过)。

---

## 4. R4 组合应用验证与真机状态标记

### 4.1 仿真端多协议组合应用落地 (G-02)
- **测试落地**: `test_esp_idf_safeguards.c::test_wave1_r4_composite_mesh_application`
- **业务场景**:
  1. 同时初始化 SoftAP 与 Station 双网卡并接入网络调度器；
  2. 触发 SNTP 授时同步成功，时钟状态跃迁为 COMPLETED；
  3. 启动 HTTP/WS Server 监听并成功分发 `/api/status` 请求；
  4. 触发 SoftAP 到 STA 的数据面报文转发，核验证书与字节数；
  5. 停止服务器并调用 Reset DAG，核验 HTTP 状态复位为 0、SNTP 状态回到 RESET、路由字节清零，无内存泄漏与孤儿任务。
- **执行结果**: **PASS**。

### 4.2 物理真机环境与硬件差分状态标定 (按指示标记待做)
按照用户明确指示：“除了真机相关的标记一下待做就行”，以下物理板端任务在实施计划与本报告中明确标记为待做（TODO）：

- [ ] **TODO: 待做（硬件配置）**：核对各 App 的真实 CMake/target 支持后建立所需 ESP32 物理硬件编译配置；
- [ ] **TODO: 待做（硬件烧录）**：在物理 ESP32 开发板上烧录 6 大黄金用例与组合应用固件；
- [ ] **TODO: 待做（真机差分抽样）**：对真实硬件串口输出、物理时间戳漂移、Wi-Fi 射频吞吐及硬件中断延迟进行差分采样比对。

---

## 5. 全量回归测试与门禁验证矩阵

| 验证项 | 执行命令 | 结果 | 证明范围 |
|---|---|:---:|---|
| Host 核心防卫单测 | `.\build\host\esp_idf_standard\test\test_esp_idf_safeguards.exe` | **9/9 PASS** | WS 接收边界、HTTP 单实例/方法分类、SNTP 故障超时、WS 异步队列、数据面路由、R4 组合应用 |
| Host 日志管线回归 | `.\build\host\esp_idf_standard\test\test_esp_log.exe` | **4/4 PASS** | 自定义 vprintf 路由、Hex/Char/Hexdump 辅助器、UART 桥接 512B 截断防守 |
| Host HTTP 客户端回归 | `.\build\host\esp_idf_standard\test\test_esp_http_client.exe` | **15/15 PASS** | 有界流原子写入、预检超限零提交回滚、HTTP Client open 失败清理 |
| 分层与 API 门禁 | `python packages/wink-tools/wink.py lint --pack layering --pack api` | **PASS (0 findings)** | App/BAL/DAL/PAL 分层依赖与规范 C-ABI 契约 |
| 开源许可门禁 | `python .github/scripts/check_license_map.py` | **PASS** | LGPL-3.0/GPL-3.0/Apache-2.0 分层许可地图 100% 合规 |
| 治理门禁 Gate 1 | `python .../run_gates.py --gate 1` | **PASS (12/12 rules)** | 路径唯一、能力存在、配置矩阵、五维状态正交、六要素凭据完整 |
| 全量双实证凭据 | `python .../evidence_verifier.py --verify-all` | **PASS (46/46 items)** | 46 个官方基准示例资产哈希、场景 JSON 与执行报告三件套 100% 吻合 |

---

## 6. 评审结论

ESP-IDF Wave 1 Phase R1～R4 的软件模型、门禁治理与仿真组合验证已全量高质量收口完成。原厂 9 份主审文件保持字节级零修改，全量 28 项回归用例全绿，门禁与实证 100% 保持绿灯。除物理真机硬件相关项已明确标记为待做（TODO）外，**全部任务均已按计划完成**。
