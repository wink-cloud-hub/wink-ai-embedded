# ESP-IDF 仿真拦截层架构与治理规范 (01-architecture-and-governance-guide)

> **版本**：v1.1  
> **适用目标**：WinkMicroOS ESP-IDF 仿真拦截框架 (Axis B - M1)

---

## 1. 架构定位与拓扑

ESP-IDF 仿真拦截层位于 WinkMicroOS 体系中的 Framework 层。通过提供与乐鑫官方 ESP-IDF 完全一致的 C-ABI 接口与头文件布局，向下转调 Wink PAL（Platform Abstraction Layer）与 Runtime 能力。

```
+-----------------------------------------------------------+
|              ESP-IDF 原生业务代码 / 官方语料 (C-ABI)        |
+-----------------------------------------------------------+
                             |
                             v
+-----------------------------------------------------------+
|   frameworks/esp_idf/include (freertos/*.h, driver/gpio.h)|
+-----------------------------------------------------------+
|   frameworks/esp_idf/src (freertos_*.c, esp_gpio.c, ...)  |
+-----------------------------------------------------------+
                             | 下沉转调 (0 claim, 0 malloc)
                             v
+-----------------------------------------------------------+
|      WinkMicroOS PAL / Sim Scheduler (Fiber Context)       |
+-----------------------------------------------------------+
```

---

## 2. 生命周期与调度引导

### 2.1 强符号导出
框架在 `src/esp_idf_runtime.c` 中导出强符号 `wink_app_get_callbacks()`。
```c
const wink_app_callbacks_t* wink_app_get_callbacks(void);
```

### 2.2 多框架互斥 (T-009)
- 与 `mcs51` 强符号互斥：不可同链，构建期通过 CMake `FATAL_ERROR` 硬防御。
- 与 `arduino` 弱符号共存：`arduino` 声明为 `__attribute__((weak))`，同链时自动让位。

### 2.3 调度器映射与 `app_main` 纤程启动 (M1)
1. **`app_main` 纤程注册**：
   - 框架初始化 `esp_idf_framework_init` 将用户定义的 `app_main` 包装为 `app_main_trampoline`，并调用 `sim_scheduler_register` 注册为主纤程（Task ID = 0）。
   - `app_main` 若自然返回，自动进入 `SIM_TASK_STATE_ZOMBIE` 态并切出，由调度器主循环进行垃圾回收（GC）。
   - `esp_idf_app_loop` 保持空操作（ADR-0070）。
2. **非破坏性协程切出桥**：
   - 所有阻塞原语（`vTaskDelay`, `xQueueReceive`, `xSemaphoreTake`, `xEventGroupWaitBits`）经统一的 `sim_scheduler_yield_context()` 挂起纤程堆栈，控制权切回主调度器 `s_main_ctx`。
   - `vTaskDelay(0)` 仅切出协程交还轮转，保持 `SIM_TASK_STATE_READY` 态，绝不推进虚拟时钟。

### 2.4 并发原语 Waiter 簿记与 `resource_id` 命名空间契约
1. **统一资源命名空间**：
   - `resource_id = ((uint32_t)tag << 24) | index`；
   - tag 分配表：`QUEUE=0x01`, `MUTEX=0x02`, `SEM=0x03`, `EVENT=0x04`, `GPTIMER=0x05`, `SUSPEND=0x06`, `TIMER=0x07`；
   - 彻底避免跨对象 ID 碰撞造成的错唤醒。
2. **Queue 双向等待者隔离**：
   - 读等待者（`rx_waiters`）与写等待者（`tx_waiters`）严格正交独立维护，写成功定向唤醒读首项，读成功定向唤醒写首项。
3. **Mutex Priority-one 唤醒策略**：
   - 互斥锁释放时扫描等待队列，优先唤醒最高优先级任务；同优先级遵从 FIFO 顺序。
4. **EventGroup Broadcast-all**：
   - 事件置位时广播扫描全员 waiter，唤醒所有满足位的任务；支持 `xClearOnExit` 关注位原子清除。

### 2.5 系统复位 (`esp_restart`) 与池清零 (ADR-0082)
- 严禁调用 host `exit()` 或 `abort()`。
- 通过 `pal_wasm_target_has_pending_reset` / `pal_wasm_target_get_reset_reason` 弱钩子族通知调度器优雅复位。
- 复位响应执行 `esp_freertos_pools_reset()`：递增 TCB `gen` 计数毒化旧句柄、清空 Queue/Sem/Event 池及 waiter 列表。

---

## 3. 7 条架构红线 (DoD 准入准出)

1. **C-ABI 与纯 C 实现**：严格采用 C99 编写，禁止 C++ 运行时与异常机制。
2. **严禁侵入式修改 PAL / DAL**：只允许单向依赖 `pal/include` 与 `targets/common/include`，严禁越级修改 PAL，严禁越级调用 DAL 内部符号。
3. **严格遵守 ADR-0065**：门面层严禁调用 `pal_resource_claim()`。
4. **零运行期堆分配**：`src/**` 运行期禁止 `malloc/free`，资源池必须静态全局预分配（总消耗 < 8KB）。
5. **PWM 定点红线 (ADR-0066)**：涉及占空比计算全定点整数运算，严禁浮点 duty。
6. **合约诚实 (ADR-0012)**：不支持的 API 编译期 `#error` 或运行时 Fail-Loud（如 `timers.h` 全系），严禁静默空实现伪造成功；一切语义弱化必须登记入 `02-api-coverage-matrix.md` 降级表。
   - **M1 影子缓存复核声明**：M0 条目 3 的 GPIO 电平回读门面缓存，在当前单核协作无抢占调度模型下，`set_level` 与 `get_level` 之间无并发分叉风险，M1 复核通过。若未来框架引入时间片抢占机制，此项自动升级为 P0 缺陷。
7. **开源许可合规 (ADR-0083/0084)**：
   - `frameworks/esp_idf/{src,include,chips}` = **LGPL-3.0-only**；
   - `frameworks/esp_idf/{test,tools}` = **GPL-3.0-only**。

---

## 4. 资产通道与 SoC 数据归属 (ADR-0087)

1. **三类资产物理绝缘**：`include/`（收割生成，manifest/Banner 可验）、手写通道（登记于 `channels.json`）、
   `src/` 门面实现。生成物与手写物不得互相覆盖；`check_harvested_headers.py` 全量校验（未登记/陈旧/重复发布/哈希漂移均 fail）。
2. **SoC 数据归属**：`soc/{soc_caps,gpio_num}.h` 的 per-SoC 数据位于 `chips/<soc>/include/soc/`，
   共享 `include/soc/` 不得有同名文件；选片由 `esp_idf_target.cmake` 派生的 include 顺序完成，**禁止 `#include_next`**。
3. **目标宏注入**：`CONFIG_IDF_TARGET_*` / `CONFIG_IDF_TARGET` 由 CMake 从 `WINK_ESP_TARGET` 派生并 PUBLIC 注入；
   `sdkconfig_base.h` 禁止硬编码；未提供数据的 SoC 在 configure 期 `FATAL_ERROR`（Fail-Loud，不静默回退 esp32）。

---

## 5. 多 SoC 矩阵与 CI 使用指南 (M3)

### 5.1 目标切换

```powershell
# 默认 esp32；支持 esp32s3 / esp32c3 / esp32c6
cmake -S wink-micro-os -B build -DTARGET_PLATFORM=host -DWINK_ESP_TARGET=esp32c3
cmake --build build && ctest --test-dir build -L esp_idf --output-on-failure
```

新增 SoC 的准入清单：
1. 新建 `chips/<soc>/include/soc/{soc_caps.h,gpio_num.h}`（逐项对照官方 v6.1 源，禁止臆造掩码）；
2. 在 `channels.json: chips_handwritten` 登记（收割器支持 per-SoC 发射后转正并移出）；
3. 驱动/测试中的 SoC 差异一律以 `SOC_*` 宏或 `CONFIG_IDF_TARGET_*` 条件化，禁止硬编码引脚号假设。

### 5.2 CI 与门禁（`.github/workflows/esp_idf_ci.yml`）

| Job | 内容 | 本地等价命令 |
|:---|:---|:---|
| `lint-and-governance` | license map + harvest gate（channels/relocation/manifest）+ winkcli packs（可用时） | `check_harvested_headers.py --rules ... --channels ...` |
| `host-matrix-tests` | 4 SoC × (ubuntu/windows) 全量 `-L esp_idf` | 逐 SoC `-DWINK_ESP_TARGET` 构建 + ctest |
| `coverage-gate` | lcov 抽取 `frameworks/esp_idf/src`，行覆盖率 ≥85% | `tools/coverage.sh` + `tools/check_coverage.py` |
| ctest `esp_idf_headless_replay` | 确定性回放（3 次运行 bit-exact，过滤墙钟/指针） | `ctest -R esp_idf_headless_replay` |

> 本地实测基线（2026-09-25）：四 SoC `-L esp_idf` 各 **31/31**；gcov 聚合行覆盖率 **85.71%**；
> headless 回放 3 次哈希一致；框架库 `--clean-first` 0 warning。
