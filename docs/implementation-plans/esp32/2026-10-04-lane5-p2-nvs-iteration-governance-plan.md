<!-- SPDX-License-Identifier: Apache-2.0 -->
# 实施计划：Lane 5 P2 标杆 storage/nvs/nvs_iteration 官方示例仿真治理闭环

> 遵循 governance-sop-esp 规范、ADR-0001 ~ ADR-0092 架构决策，推进 Lane 5（片上存储与文件系统）P2 进阶标杆治理闭环。

| 字段 | 内容 |
|---|---|
| 计划编号 | PLAN-20261004-LANE5-P2-NVS-ITERATION-GOVERNANCE-v1.0 |
| 任务目标 | 达成 storage/nvs/nvs_iteration（Display ID: 401）零修改镜像治理与 NVS 命名空间及类型化键值迭代器确定性仿真认证 |
| 泳道与优先级 | **Lane 5（片上存储与文件系统） / P2 进阶标杆** |
| 状态 | 🟢 **Complete (Verified & Signed-off)** |
| 日期 | 2026-10-04 |
| 上游路径 | examples/storage/nvs/nvs_iteration |
| 目标载体目录 | wink-micro-app/vendor/esp_idfv61/storage/nvs_nvs_iteration/ |
| 场景路径 | storage/nvs_nvs_iteration/unisim-scenarios/storage_nvs_nvs_iteration.scenario.json |
| 依赖能力 | cap.core.fiber_task, cap.core.sync_tokens, cap.vfs.mem_sandbox, cap.vfs.nvs_partition |
| 交付工件 | 原厂零修改代码镜像、UniSim 确定性场景、仿真执行报告、Gate 1 合规签署与看板派生 |

---

## 一、 战略总目标与标杆意义

### 1.1 攻坚背景
在 Lane 5（片上存储与文件系统）中，我们已先后交付：
- #403 storage/nvs/nvs_rw_value（标量数值读写持久化）
- #402 storage/nvs/nvs_rw_blob（结构体与数组二进制块持久化）
- #415 storage/spiffs（POSIX VFS 纯内存沙箱文件系统）

**storage/nvs/nvs_iteration（Display ID: 401）是 NVS 族系中不可或缺的进阶标杆**：
- 演示批量写入混合类型键值对（7 个 uint32_t Unix 时间戳 + 7 个 int8_t 环境传感器状态）；
- 演示标准 NVS 迭代器生命周期（
vs_entry_find -> 
vs_entry_info -> 
vs_entry_next -> 
vs_release_iterator）；
- 演示指定数据类型过滤迭代（NVS_TYPE_U32 与 NVS_TYPE_I8 精准匹配）；
- 演示通用通配符迭代（NVS_TYPE_ANY 全量枚举 14 个条目）；
- 在物联网配置持久化、固件参数恢复与动态键空间自省等现实场景中，迭代器是高频核心能力。

### 1.2 战略收益
1. **完善 NVS 存储引擎因果闭环**：验证底层 esp_nvs.c 针对不同数据类型的存储元数据格式化与迭代器遍历精准度。
2. **构建三位一体 NVS 认证基线**：与 
vs_rw_value、
vs_rw_blob 组成完整的 NVS 功能合规护城河。
3. **扩展全量实证交付基线至 26 项**：进一步收敛 Lane 5 存储体系。

---

## 二、 技术与架构方案

### 2.1 零修改镜像契约 (Layer C)
- 权威原厂路径：D:\software\embedded-tools\esp-idf\.espressif\v6.1\esp-idf\examples\storage\nvs\nvs_iteration
- 镜像文件：main/nvs_iteration_example.c -> wink-micro-app/vendor/esp_idfv61/storage/nvs_nvs_iteration/nvs_iteration_example.c
- 固化 SHA-256：
  98be250a7c002f426d21d02c0abae9b59762dec92dc0109d57e0eb2b35f8a713
- 严禁对官方源码进行任何修改，所有适配统一通过 CMakeLists.txt、sdkconfig.h、wink-app.json 及底座框架门面完成。

### 2.2 驱动与门面模型 (Layer A & Runtime)
- 审查底层 wink-micro-os/frameworks/esp_idf/src/core/esp_nvs.c：
  - 
vs_flash_erase()：清除全局 NVS 内存沙箱，保证测试隔离（P-7 隔离原则）；
  - 
vs_flash_init()：初始化默认分区；
  - 
vs_open() / 
vs_set_u32() / 
vs_set_i8() / 
vs_commit() / 
vs_close()：已支持对应类型存储；
  - 
vs_entry_find() / 
vs_entry_info() / 
vs_entry_next() / 
vs_release_iterator()：支持类型化及 NVS_TYPE_ANY 迭代；
- 遵循 ADR-0001 负数错误码、ADR-0012 Fail-Loud 及 LGPL-3.0 许可。

### 2.3 确定性因果时序与断言设计 (Layer S)
- 业务因果链路：
  - 供电断言：power:VCC_3V3 维持 3.3V；
  - 步骤 1：断言开始擦除与初始化默认 NVS 分区日志；
  - 步骤 2：断言成功写入 7 项 u32 模拟数据（Wrote 7 u32 mock data key-value pairs）；
  - 步骤 3：断言成功写入 7 项 i8 模拟数据（Wrote 7 i8 mock data key-value pairs）；
  - 步骤 4：断言完成 u32 迭代遍历并计数 7 项（Iterated over 7 entries）；
  - 步骤 5：断言完成 i8 迭代遍历并计数 7 项（Iterated over 7 entries）；
  - 步骤 6：断言完成 NVS_TYPE_ANY 全量迭代遍历并计数 14 项（Iterated over 14 entries）；
  - 步骤 7：断言安全退出主函数（Returning from app_main()）；
- Canary 变异检验：变异关键断言日志（如篡改全量迭代数量为 99），确保 100% 击杀。

---

## 三、 执行计划与任务拆解

### Phase 1：Layer C 应用载体与原厂镜像落盘
- [x] **Task 1.1**：创建目录 wink-micro-app/vendor/esp_idfv61/storage/nvs_nvs_iteration/。
- [x] **Task 1.2**：镜像 
vs_iteration_example.c，固化 SHA-256。
- [x] **Task 1.3**：创建 CMakeLists.txt、include/sdkconfig.h 与 wink-app.json。
- [x] **Task 1.4**：在 
un_esp32_headless_evidence.ps1 登记 
vs_nvs_iteration carrier。

### Phase 2：Layer A 底座对齐与门面验证
- [x] **Task 2.1**：验证 esp_nvs.c 对迭代器边界及 
vs_set_i8 的完备性。
- [x] **Task 2.2**：编译并构建 Wasm 目标。

### Phase 3：Layer S 高保真因果场景编撰
- [x] **Task 3.1**：编撰 unisim-scenarios/storage_nvs_nvs_iteration.scenario.json 业务因果断言集合。
- [x] **Task 3.2**：核验 checklist.data.json 中该项元数据（Display ID: 401）。

### Phase 4：正向仿真基线通过
- [x] **Task 4.1**：运行 
un_esp32_headless_evidence.ps1 -App nvs_nvs_iteration，确保正向仿真 100% 通过。

### Phase 5：Canary 变异缺陷敏感性检验
- [x] **Task 5.1**：注入非等价业务变异，验证指定断言在窗口期内精准失败并被捕获（Canary Kill 100%）。

### Phase 6：凭据归档与治理看板派生
- [x] **Task 6.1**：执行带 -WriteEvidence 正式录入证据。
- [x] **Task 6.2**：更新 checklist.data.json 中的 audit 签署。
- [x] **Task 6.3**：运行 generate_checklist_v1_1.py 重新生成 CHECKLIST.md。

### Phase 7：全量门禁与零回归复验
- [x] **Task 7.1**：运行 
un_gates.py --gate 1（12 项规则全绿通过）。
- [x] **Task 7.2**：运行许可证检查（100% 合规）。
- [x] **Task 7.3**：运行 evidence_verifier.py --verify-all（26/26 项全部通过）。

### Phase 8：原子化 Git 提交
- [ ] **Task 8.1**：分块提交 Carrier/场景与治理凭据。
