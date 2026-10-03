<!-- SPDX-License-Identifier: Apache-2.0 -->
# 实施计划：Lane 5 P1 标杆 `storage/spiffs` 官方示例仿真治理闭环

> 遵循 `governance-sop-esp` 规范、`ADR-0001` ~ `ADR-0092` 架构决策，推进 Lane 5（本地存储与虚拟文件系统）P1 标杆治理闭环。

| 字段 | 内容 |
|---|---|
| 计划编号 | PLAN-20261003-LANE5-P1-SPIFFS-GOVERNANCE-v1.0 |
| 任务目标 | 达成 `storage/spiffs`（Display ID: 415）零修改镜像治理与纯内存沙箱 VFS 文件系统确定性仿真认证 |
| 泳道与优先级 | **Lane 5（片上存储与文件系统） / P1 标杆** |
| 状态 | 🟢 **Complete** |
| 日期 | 2026-10-03 |
| 上游路径 | `examples/storage/spiffs` |
| 目标载体目录 | `wink-micro-app/vendor/esp_idfv61/storage/spiffs/` |
| 场景路径 | `storage/spiffs/unisim-scenarios/storage_spiffs.scenario.json` |
| 依赖能力 | `cap.core.fiber_task`, `cap.core.sync_tokens`, `cap.vfs.mem_sandbox`, `cap.vfs.spiffs_format` |
| 交付工件 | 原厂零修改代码镜像、UniSim 确定性场景、仿真执行报告、Gate 1 合规签署与看板派生 |

---

## 一、 战略总目标与标杆意义

### 1.1 攻坚背景
在相继攻克 `storage/nvs/nvs_rw_value` (#403) 与 `storage/nvs/nvs_rw_blob` (#402) 之后，系统已具备可靠的键值与二进制块非易失性持久化能力。
在官方 `CHECKLIST.md` §1.4“各泳道推荐优先并发认领就绪清单 (Ready-to-Claim P0/P1 Backlog)”中，14 项旗舰标杆中已有 13 项达成正式实证交付，**`storage/spiffs`（Display ID: 415）是全表唯一剩余未交付的 P1 核心标杆**：
- 演示通过 `esp_vfs_spiffs.h` 配置并挂载 SPIFFS 分区（`esp_vfs_spiffs_register`）；
- 演示查询分区容量与已用空间（`esp_spiffs_info`）；
- 演示一致性自检（`esp_spiffs_check`）与格式化（`esp_spiffs_format`）；
- 演示标准 POSIX / C 标准库文件操作：
  - `fopen("/spiffs/hello.txt", "w")` 创建与 `fprintf` 写入；
  - `stat("/spiffs/foo.txt", &st)` 探测存在与 `unlink` 删除；
  - `rename("/spiffs/hello.txt", "/spiffs/foo.txt")` 重命名；
  - `fopen("/spiffs/foo.txt", "r")` 与 `fgets` 回读验证；
- 演示优雅卸载与注销（`esp_vfs_spiffs_unregister`）。

### 1.2 战略收益
1. **闭环片上文件系统与标准 VFS 抽象**：在底座已有 `esp_vfs_ram.c` 纯内存沙箱基础上，补充 `esp_spiffs.h` 完整门面，杜绝触碰宿主硬盘。
2. **打通 POSIX 文件流式读写因果闭环**：验证在 Wasm / 仿真运行时内创建、写入、重命名、回读及删除文件的全生命周期。
3. **100% 攻克推荐认领清单**：达成 `CHECKLIST.md` §1.4 Ready-to-Claim P0/P1 Backlog 最后一颗明珠的交付收官。

---

## 二、 技术与架构方案

### 2.1 零修改镜像契约 (Layer C)
- 上游路径：`D:\software\embedded-tools\esp-idf\.espressif\v6.1\esp-idf\examples\storage\spiffs`；
- 镜像文件：`main/spiffs_example_main.c` 落地至 `wink-micro-app/vendor/esp_idfv61/storage/spiffs/main.c`；
- 校验并锁定 SHA-256：
  - `main.c`: `788405f243e3c5f3e372a683a705593851719324d375625d6009edb0d564f617`；
- 严禁对官方源码进行任何修改，所有适配统一通过 `CMakeLists.txt`、`sdkconfig.h`、`wink-app.json` 及底座框架门面完成。

### 2.2 SPIFFS 门面驱动模型 (Layer A & Runtime)
- 头文件 `wink-micro-os/frameworks/esp_idf/include/esp_spiffs.h`：
  - 导出 `esp_vfs_spiffs_conf_t` 结构体；
  - 导出 `esp_vfs_spiffs_register`、`esp_vfs_spiffs_unregister`；
  - 导出 `esp_spiffs_mounted`、`esp_spiffs_format`、`esp_spiffs_info`、`esp_spiffs_check`、`esp_spiffs_gc`；
- 实现 `wink-micro-os/frameworks/esp_idf/src/core/esp_spiffs.c`：
  - `esp_vfs_spiffs_register`：初始化挂载状态，确保在底层创建对应 base_path 目录（如 `/spiffs`）；
  - `esp_spiffs_info`：返回预设的分区总大小（例如 896321 字节）及当前已用字节；
  - `esp_spiffs_check`：返回 `ESP_OK`；
  - `esp_spiffs_format`：格式化沙箱；
  - `esp_vfs_spiffs_unregister`：注销并重置挂载状态；
- 许可证合规：`LGPL-3.0-only`，通过 `check_license_map.py` 审查。

### 2.3 确定性因果时序与断言设计 (Layer S)
- 业务因果链路：
  - 初始化断言：确认日志输出 `"Initializing SPIFFS"`；
  - 分区信息断言：确认日志输出 `"Partition size: total: %d, used: %d"`；
  - 写入断言：确认日志输出 `"File written"`；
  - 重命名与回读断言：确认日志输出 `"Read from file: 'Hello World!'"`；
  - 卸载断言：确认日志输出 `"SPIFFS unmounted"`；
  - 变异敏感性（Canary）：变异关键日志匹配（如篡改回读字符串），确保 100% 击杀。

---

## 三、 执行计划与任务拆解

### Phase 1：Layer C 应用载体与原厂镜像落盘
- [x] **Task 1.1**：创建目录 `wink-micro-app/vendor/esp_idfv61/storage/spiffs/`。
- [x] **Task 1.2**：镜像 `spiffs_example_main.c` 为 `main.c`，固化 SHA-256。
- [x] **Task 1.3**：创建 `CMakeLists.txt`、`include/sdkconfig.h` 与 `wink-app.json`。
- [x] **Task 1.4**：在 `run_esp32_headless_evidence.ps1` 登记 `spiffs`。

### Phase 2：Layer A 底座对齐与 SPIFFS 门面实现
- [x] **Task 2.1**：新建 `include/esp_spiffs.h`，导出标准配置与 API。
- [x] **Task 2.2**：在 `src/core/esp_spiffs.c` 实现 SPIFFS 门面生命周期函数。
- [x] **Task 2.3**：在 `esp_idf_sources.cmake` 注册 `esp_spiffs.c`。
- [x] **Task 2.4**：编译验证底座框架。

### Phase 3：Layer S 高保真因果场景编撰
- [x] **Task 3.1**：编撰 `unisim-scenarios/storage_spiffs.scenario.json` 业务因果断言集合。
- [x] **Task 3.2**：核验 `checklist.data.json` 中该项元数据。

### Phase 4：正向仿真基线通过
- [x] **Task 4.1**：运行 `run_esp32_headless_evidence.ps1 -App spiffs`，确保正向仿真 100% 通过。

### Phase 5：Canary 变异缺陷敏感性检验
- [x] **Task 5.1**：注入非等价业务变异，验证指定断言在窗口期内精准失败并被捕获（Canary Kill 100%）。

### Phase 6：凭据归档与治理看板派生
- [x] **Task 6.1**：执行带 `-WriteEvidence` 正式录入证据。
- [x] **Task 6.2**：更新 `checklist.data.json` 中的 audit 签署。
- [x] **Task 6.3**：运行 `generate_checklist_v1_1.py` 重新生成 `CHECKLIST.md`。

### Phase 7：全量门禁与零回归复验
- [x] **Task 7.1**：运行 `run_gates.py --gate 1` 确保 Gate 1 静态语义门禁 100% 通过。
- [x] **Task 7.2**：运行 `check_license_map.py` 确保开源许可 100% 合规。
- [x] **Task 7.3**：运行 `evidence_verifier.py --verify-all` 确保 22 项全量零回归。
- [x] **Task 7.4**：按模块原子化提交 Git Commit。
