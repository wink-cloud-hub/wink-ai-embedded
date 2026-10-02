<!-- SPDX-License-Identifier: Apache-2.0 -->
# 实施计划：Lane 4 P2 标杆 `peripherals/adc/continuous_read` 官方示例仿真治理闭环

| 字段 | 内容 |
|---|---|
| 计划编号 | PLAN-20261002-LANE4-P2-ADC-CONTINUOUS-READ-v1.2 |
| 状态 | 🚀 **Ready for Execution** |
| 日期 | 2026-10-02 |
| 目标平台/SoC | `wasm32-unknown-emscripten` / `esp32 xtensa` 双 Target |
| 工具链/SDK版本| `ESP-IDF v6.1@fff9895c` / `Emscripten 3.1.x` / Node.js 24+ / Python 3.11+ |
| 优先级 | **P2（Lane 4 模拟电学进阶流式采集标杆）** |
| 治理依据 | [governance-sop-esp](../../../.agents/skills/governance-sop-esp/SKILL.md)<br>[ADR-0001：负数错误码标准](../../decisions/core/0001-error-code-sign-convention.md)<br>[ADR-0002：双 Target 同源编译](../../decisions/core/0002-dual-target-compilation.md)<br>[ADR-0004：编译期静态分发](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md)<br>[ADR-0012：Fail-Loud 原则](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)<br>[ADR-0045：零运行时堆分配原则](../../decisions/core/0045-zero-runtime-heap-allocation.md)<br>[ADR-0083 / ADR-0084：开源许可分层地图](../../decisions/core/0083-open-source-license-boundary.md)<br>[00.5-pal-adc-subsystem-plan.md](../wokwi-dal-type-coverage-type/00.5-pal-adc-subsystem-plan.md) |
| 管辖数据源 | `checklist.data.json`（Display ID: 3, `esp.peripherals.adc.continuous_read`） |
| 实施目标文件 | `wink-micro-os/frameworks/esp_idf/include/freertos/task.h`（新增 Task Notification 原型与宏）<br>`wink-micro-os/frameworks/esp_idf/src/freertos/freertos_sync.h`（新增通知字段与资源标记）<br>`wink-micro-os/frameworks/esp_idf/src/freertos/freertos_task.c`（实现 FreeRTOS 任务通知三接口）<br>`wink-micro-os/frameworks/esp_idf/include/hal/adc_types.h`（补齐连续模式转换模式、图样配置与 Type 1 结构体）<br>`wink-micro-os/targets/wasm/pal_wasm_ch3_adc.c`（实现物理层 `pal_adc_continuous_start/stop` 原始流采样）<br>`wink-micro-os/frameworks/esp_idf/include/esp_adc/adc_continuous.h`（新增连续采样 C-ABI）<br>`wink-micro-os/frameworks/esp_idf/src/core/esp_sim_handle.h`（扩充句柄类别 `ESP_SIM_HANDLE_ADC_CONTINUOUS = 14`）<br>`wink-micro-os/frameworks/esp_idf/src/drivers/esp_adc.c`（实现连续门面、Type 1 位域打包与虚拟 DMA 引擎）<br>`wink-micro-app/vendor/esp_idfv61/peripherals/adc_continuous_read/`（新建示例应用脚手架）<br>`wink-micro-app/vendor/esp_idfv61/peripherals/adc_continuous_read/unisim-scenarios/peripherals_adc_continuous_read.scenario.json`（业务场景）<br>`wink-micro-os/frameworks/esp_idf/tools/run_esp32_headless_evidence.ps1`（登记 carrier） |
| 验收门禁 | `python -X utf8 -B wink-micro-app/vendor/esp_idfv61/.governance/gates/run_gates.py --gate 1`<br>`python .github/scripts/check_license_map.py`<br>Headless 正向基线通过 + Canary 变异 100% 击杀（三向算子）<br>`evidence_verifier.py --verify-all` 凭据验证全绿（12/12） |

---

## 一、 战略目标与现状分析

### 1.1 背景与破局意义
在 ESP-IDF 官方示例治理清单中：
- **`peripherals/adc/continuous_read`（Display ID: 3）** 是 Lane 4（模拟量转换与电学传感）中首个引入 **DMA 高速流式传输** 与 **FreeRTOS 中断事件回调（`vTaskNotifyGiveFromISR`）** 的标杆示例；
- 与刚刚攻克的 P0 级单次采样标杆 `peripherals/adc/oneshot_read` 相比，连续采样涉及底层硬件双缓冲注水、Type 1 格式位域打包、帧满中断派发（Frame-done Event）、跨任务纤程事件唤醒与上层数据流解析；
- 攻克该示例标志着运行时门面具备了对**流式硬件外设 + DMA 中断事件模型**的高保真模拟能力。

### 1.2 底座就绪情况与缺口分析
1. **底层 PAL 就绪状态（部分完成）**：
   - 跨平台抽象头文件 `wink-micro-os/pal/include/hal/pal_adc.h` 已定义连续配置结构体 `pal_adc_continuous_cfg_t`（含 DMA 双缓冲 `dma_buf_a`/`dma_buf_b`、`on_half_full`/`on_full` 回调）及 `pal_adc_continuous_start()`/`pal_adc_continuous_stop()` API 声明；
   - ESP32 target（`pal_hal_adc_esp32.c`）与 Host target（`pal_hal_adc_host.c`）均已实现连续模式；
   - **⚠️ Wasm 仿真驱动 `pal_wasm_ch3_adc.c` 尚未实现 `pal_adc_continuous_start()`/`pal_adc_continuous_stop()`**，仅支持 oneshot 读取（`pal_adc_read_raw`）与噪声抑制；
2. **FreeRTOS 调度与任务通知缺口（关键阻断项）**：
   - 原厂示例使用 FreeRTOS Task Notification 原生机制：
     `s_task_handle = xTaskGetCurrentTaskHandle();`
     `vTaskNotifyGiveFromISR(s_task_handle, &mustYield);`
     `ulTaskNotifyTake(pdTRUE, portMAX_DELAY);`
   - **⚠️ 当前 `task.h` 与 `freertos_task.c` 完全未声明也未实现上述三个接口**，直接导致示例编译链接失败；
3. **硬件类型定义缺口（HAL 层）**：
   - **⚠️ 当前 `include/hal/adc_types.h` 缺少连续采样必需的枚举与类型**：`adc_digi_convert_mode_t`、`adc_digi_pattern_config_t` 与 ESP32 Type 1 位域结构体 `adc_digi_output_data_t`；
4. **框架门面层与虚拟 DMA 引擎缺口（Layer A）**：
   - 缺少 ESP-IDF v6.1 标准头文件 `include/esp_adc/adc_continuous.h`；
   - 缺少连续采样门面驱动与**虚拟 DMA 数据泵引擎**：
     - 按 ESP32 Type 1 格式（16 位：12 位 raw + 4 位 channel）打包 DMA 字节流；
     - 维护**纯静态字节环形缓冲区**（`max_store_buf_size = 1024`，`conv_frame_size = 256`），并进行容量越界 Fail-Loud 静态防御；
     - 实现 **PAL 原始采样回调 → 框架门面 Type 1 打包 → FreeRTOS 中断事件** 的完整因果链路；
     - 实现 ESP-IDF v6.1 标准的 `adc_continuous_parse_data()` 数据解包函数（含输出样本数上限截断保护）。
5. **应用镜像与治理脚手架缺口**：
   - `wink-micro-app/vendor/esp_idfv61/peripherals/adc_continuous_read` 尚未在磁盘落盘；
   - 需从本地原厂路径原封不动镜像 `continuous_read_main.c`，固化 SHA-256：`f8d02828e82574b4d3e80cf2deb2e9693dc1cdf2bd48086589f84c38cf7bea9b`；
   - 需编撰符合 `governance-sop-esp` 契约的高保真业务断言场景，坚决杜绝 P-1/P-2 假绿。

---

## 二、 架构规格与接口设计

### 2.1 C-ABI 头文件契约（Zero-Modification 上游完全兼容）

#### 1. `include/hal/adc_types.h`（扩充连续采样类型）
```c
typedef enum {
    ADC_CONV_SINGLE_UNIT_1 = 1,
    ADC_CONV_SINGLE_UNIT_2 = 2,
    ADC_CONV_BOTH_UNIT     = 3,
    ADC_CONV_ALTER_UNIT    = 7,
} adc_digi_convert_mode_t;

typedef enum {
    ADC_DIGI_OUTPUT_FORMAT_TYPE1 = 0,
    ADC_DIGI_OUTPUT_FORMAT_TYPE2 = 1,
} adc_digi_output_format_t;

typedef struct {
    uint8_t atten;
    uint8_t channel;
    uint8_t unit;
    uint8_t bit_width;
} adc_digi_pattern_config_t;

/* ESP32 Type 1 output data structure (16-bit) */
typedef union {
    struct {
        uint16_t data: 12;
        uint16_t channel: 4;
    } type1;
    uint16_t val;
} adc_digi_output_data_t;
```

#### 2. `include/esp_adc/adc_continuous.h`（新增连续采样 C-ABI）
- 结构体定义：
  - `adc_continuous_handle_cfg_t`（`max_store_buf_size`, `conv_frame_size`, `flags`）
  - `adc_continuous_config_t`（`pattern_num`, `adc_pattern`, `sample_freq_hz`, `conv_mode`）
  - `adc_continuous_evt_data_t`（`conv_frame_buffer`, `size`）
  - `adc_continuous_evt_cbs_t`（`on_conv_done`, `on_pool_ovf`）
  - `adc_continuous_data_t`（`unit`, `channel`, `raw_data`, `valid`）
- 句柄定义：`typedef struct adc_continuous_ctx_t *adc_continuous_handle_t;`
- 函数契约：
  ```c
  esp_err_t adc_continuous_new_handle(const adc_continuous_handle_cfg_t *hdl_config, adc_continuous_handle_t *ret_handle);
  esp_err_t adc_continuous_config(adc_continuous_handle_t handle, const adc_continuous_config_t *config);
  esp_err_t adc_continuous_register_event_callbacks(adc_continuous_handle_t handle, const adc_continuous_evt_cbs_t *cbs, void *user_data);
  esp_err_t adc_continuous_start(adc_continuous_handle_t handle);
  esp_err_t adc_continuous_read(adc_continuous_handle_t handle, uint8_t *buf, uint32_t length_max, uint32_t *out_length, uint32_t timeout_ms);
  esp_err_t adc_continuous_stop(adc_continuous_handle_t handle);
  esp_err_t adc_continuous_deinit(adc_continuous_handle_t handle);
  esp_err_t adc_continuous_parse_data(adc_continuous_handle_t handle, const uint8_t *raw_data, uint32_t raw_data_size, adc_continuous_data_t *parsed_data, uint32_t *num_parsed_samples);
  ```

#### 3. `include/freertos/task.h`（扩充 FreeRTOS Task Notification）
```c
TaskHandle_t xTaskGetCurrentTaskHandle(void);
uint32_t ulTaskNotifyTake(BaseType_t xClearCountOnExit, TickType_t xTicksToWait);
void vTaskNotifyGiveFromISR(TaskHandle_t xTaskToNotify, BaseType_t *pxHigherPriorityTaskWoken);
```

---

### 2.2 FreeRTOS Task Notification 轻量原生桥接设计

为杜绝单线程 Wasm 下 `ulTaskNotifyTake(portMAX_DELAY)` 永久死锁，复用 `freertos_sync.h` 现有纤程调度体系：

1. **资源标签登记**：
   在 `freertos_sync.h` 中新增：
   ```c
   #define FREERTOS_TAG_TASK_NOTIFY 0x08u
   ```
2. **TCB 状态扩展**：
   在 `esp_tcb_t` 中扩充通知计数：
   ```c
   typedef struct {
       ...
       uint32_t notify_val;
   } esp_tcb_t;
   ```
3. **接口原生闭环**：
   - `xTaskGetCurrentTaskHandle()`：获取当前执行槽位 `sim_scheduler_current_id()`，返回其 `s_tcb[slot].token`；
   - `ulTaskNotifyTake(pdTRUE, xTicksToWait)`：
     - 若 `tcb->notify_val > 0`，取值后清零（若 `xClearCountOnExit == pdTRUE`），直接成功返回；
     - 若 `tcb->notify_val == 0`，通过 `sync_block(FREERTOS_MAKE_RES_ID(FREERTOS_TAG_TASK_NOTIFY, slot), xTicksToWait)` 挂起当前纤程；
     - 唤醒后返回通知计数值；
   - `vTaskNotifyGiveFromISR(xTask, pxHigherPriorityTaskWoken)`：
     - 解码目标句柄槽位，递增 `tcb->notify_val++`；
     - 调用 `sim_scheduler_unblock_resource(FREERTOS_MAKE_RES_ID(FREERTOS_TAG_TASK_NOTIFY, slot), pal_os_get_us())` 唤醒纤程；
     - 若 `pxHigherPriorityTaskWoken != NULL`，置为 `pdTRUE` 并标记 `s_isr_yield_requested = true`。

---

### 2.3 分层架构与虚拟 DMA 数据泵状态机

**严格遵守分层纯洁性（ADR-0004 & 分层门禁）**：
- **PAL 层（`pal_wasm_ch3_adc.c`）**：仅处理底层物理引脚模拟电压采集，通过 `on_full` 向调用方交付原始 `uint16_t` 样本流，**坚决不引入 ESP-IDF 私有的 Type 1 编码**；
- **框架门面层（`esp_adc.c`）**：在 `s_pal_on_full` 回调中将原始样本按 Type 1 位域打包为字节流，推入纯静态环形缓冲区。

```
[ app_main 调用 adc_continuous_start ]
          |
          v
[ 门面配置 PAL 连续采样: 传入静态 s_dma_buf_a/b 与回调 s_pal_on_full ]
          |
          v
[ 初始注水 / 周期调度点 (vTaskDelay / ulTaskNotifyTake) ]
          |
          v
[ PAL Wasm 驱动采样: 从模拟轨读取电压填满 128 个 uint16_t 样本并触发 on_full ]
          |
          v
[ 门面回调 s_pal_on_full 执行:
    1. 读取 dma_buf 原始样本
    2. 按 Type 1 位域 (12-bit data + 4-bit ch) 打包成 256 字节流
    3. 写入纯静态 s_ring_buf (head/tail 指针推进) ]
          |
          v
    是否满一帧 (>= 256 字节)?
      /        \
    YES         NO ---------------------------------------------+
     v                                                          |
[ 门面触发 cbs.on_conv_done 回调 ]                               |
     v                                                          |
[ 示例代码 s_conv_done_cb 调用 vTaskNotifyGiveFromISR ]         |
     v                                                          |
[ 任务通知计数值递增，sim_scheduler_unblock 唤醒 app_main ]     |
     v                                                          |
[ app_main 从 ulTaskNotifyTake 恢复执行 ]                       |
     v                                                          |
[ app_main 调用 adc_continuous_read 从 s_ring_buf 消费数据 ]     |
     v                                                          |
[ adc_continuous_parse_data 解包并校验 valid 标志与通道数据 ] <---+
```

---

### 2.4 零运行时堆与 Fail-Loud 边界防御（符合 ADR-0012 & ADR-0045）
- 在 `esp_sim_handle.h` 增加：
  - `ESP_SIM_HANDLE_ADC_CONTINUOUS = 14`（**避免与 DAC_ONESHOT=13 冲突**）
- 在 `esp_adc.c` 中维护静态单例上下文 `s_continuous_ctx`：
  ```c
  #define CONTINUOUS_MAX_BUF_SIZE  1024
  #define CONTINUOUS_MAX_FRAME_SIZE 512

  typedef struct {
      bool in_use;
      bool started;
      uint32_t token;
      adc_continuous_handle_cfg_t hdl_cfg;
      adc_continuous_config_t     dig_cfg;
      adc_digi_pattern_config_t   patterns[SOC_ADC_PATT_LEN_MAX];
      adc_continuous_evt_cbs_t    cbs;
      void                       *user_data;

      /* 纯静态字节环形缓冲区（单生产者/单消费者） */
      uint8_t  ring_buf[CONTINUOUS_MAX_BUF_SIZE];
      uint32_t ring_head;
      uint32_t ring_tail;
      uint32_t ring_count;

      /* PAL 原始双缓冲静态存储 (128 samples = 256 bytes) */
      uint16_t pal_dma_a[128];
      uint16_t pal_dma_b[128];
  } esp_adc_continuous_ctx_t;
  ```
- **Fail-Loud 边界检查**：
  在 `adc_continuous_new_handle()` 中：
  ```c
  if (hdl_config->max_store_buf_size > CONTINUOUS_MAX_BUF_SIZE ||
      hdl_config->conv_frame_size > CONTINUOUS_MAX_FRAME_SIZE) {
      return ESP_ERR_NO_MEM;
  }
  ```
- 严禁调用 `malloc`/`free` 或 `pal_os_ringbuf_create`。

---

### 2.5 物理引脚拓扑映射
依据 ESP32 芯片硬件规格及官方示例代码：
- `EXAMPLE_ADC_UNIT` = `ADC_UNIT_1`
- `channel[0]` = `ADC_CHANNEL_6` $\rightarrow$ **GPIO 34**
- `channel[1]` = `ADC_CHANNEL_7` $\rightarrow$ **GPIO 35**
- 在 `wink-app.json` 中明确声明引脚拓扑，并与 `unisim-scenarios` 激励通道无缝映射。

---

## 三、 场景因果与防假绿断言设计 (Per SOP A-1~A-4)

### 3.1 真实业务因果链
原厂 `continuous_read_main.c` 运行逻辑：
1. `continuous_adc_init()` 配置 Channel 6 与 Channel 7，采样率 20kHz；
2. 注册 `s_conv_done_cb`，调用 `adc_continuous_start()`；
3. `ulTaskNotifyTake(pdTRUE, portMAX_DELAY)` 阻塞等待 DMA 满帧完成；
4. `s_conv_done_cb` 触发并执行 `vTaskNotifyGiveFromISR` 释放信号量；
5. 任务唤醒并调用 `adc_continuous_read` 消费 256 字节原始数据；
6. `adc_continuous_parse_data` 解包数据，并通过 `ESP_LOGI` 打印真实采样值：
   `ADC1, Channel: 6, Value: <raw>` 与 `ADC1, Channel: 7, Value: <raw>`；
7. 每次循环带有 `vTaskDelay(1)` 维持多任务协作流转。

### 3.2 断言设计（拒绝纯电源假绿 P-1 与恒真假绿 P-2）
场景文件：`unisim-scenarios/peripherals_adc_continuous_read.scenario.json`
- **步骤 1（输入激励）**：0ms 时分别向 GPIO 34（Ch6）施加 1500mV 归一化模拟电压（约 0.484），向 GPIO 35（Ch7）施加 2500mV 归一化模拟电压（约 0.806）；
- **步骤 2（辅助断言）**：50ms 断言系统核心供电轨 3.3V 稳定；
- **步骤 3（业务因果断言）**：
  - 500ms 内断言捕获控制台日志：`ret is 0, ret_num is 256 bytes`；
  - 断言捕获到 `ADC1, Channel: 6, Value:` 与 `ADC1, Channel: 7, Value:`，采样值必须落在设定物理电压对应的允许公差带（$\pm 5\%$）内；
- **Canary 变异击杀设计（三向击杀算子）**：
  - **变异算子 1（数据位域篡改）**：在 Type 1 打包中故意将 Channel 反转（Ch6 打包成 Ch0），断言必须失败被击杀；
  - **变异算子 2（中断抑制）**：在数据泵中抑制 `on_conv_done` 触发，导致 `app_main` 任务陷入 `ulTaskNotifyTake` 永久阻塞超时，断言必须 100% 失败被击杀；
  - **变异算子 3（错误路径覆盖）**：在注水时故意不写入环形缓冲，使 `adc_continuous_read(..., timeout_ms=0)` 返回 `ESP_ERR_TIMEOUT`，验证错误路径的断言有效性。

---

## 四、 实施任务拆解与执行工序

### Phase 0：FreeRTOS Task Notification 原生闭环（基建打底）
- [ ] **Task 0.1**：在 `wink-micro-os/frameworks/esp_idf/include/freertos/task.h` 中新增 Task Notification 函数声明（`xTaskGetCurrentTaskHandle`、`ulTaskNotifyTake`、`vTaskNotifyGiveFromISR`）与宏常量。
- [ ] **Task 0.2**：在 `wink-micro-os/frameworks/esp_idf/src/freertos/freertos_sync.h` 登记 `FREERTOS_TAG_TASK_NOTIFY` 并在 `esp_tcb_t` 中扩充 `notify_val` 字段。
- [ ] **Task 0.3**：在 `wink-micro-os/frameworks/esp_idf/src/freertos/freertos_task.c` 中实现通知逻辑，接入原生纤程挂起与唤醒。

### Phase 1：Wasm PAL 连续驱动与 HAL 类型定义（底座就绪）
- [ ] **Task 1.1**：在 `wink-micro-os/frameworks/esp_idf/include/hal/adc_types.h` 扩充连续模式类型（`adc_digi_convert_mode_t`、`adc_digi_pattern_config_t`、`adc_digi_output_data_t`）。
- [ ] **Task 1.2**：在 `wink-micro-os/targets/wasm/pal_wasm_ch3_adc.c` 中实现 `pal_adc_continuous_start()` / `pal_adc_continuous_stop()`：
  - 启动后记录通道列表、双缓冲指针与 `on_full` 回调；
  - 停止后重置连续状态。
- [ ] **Task 1.3**：在 `pal_wasm_ch3_adc.c` 内部实现确定性注水函数 `pal_wasm_adc_pump_continuous()`，在每次调度让步或 `read` 时采集并填满 DMA 缓冲，触发 `on_full`。

### Phase 2：ESP-IDF 连续门面与虚拟 DMA 引擎（Layer A 门面）
- [ ] **Task 2.1**：在 `wink-micro-os/frameworks/esp_idf/src/core/esp_sim_handle.h` 登记 `ESP_SIM_HANDLE_ADC_CONTINUOUS = 14`。
- [ ] **Task 2.2**：在 `wink-micro-os/frameworks/esp_idf/include/esp_adc/` 新增 `adc_continuous.h`，定义完整结构体与 API 原型。
- [ ] **Task 2.3**：在 `wink-micro-os/frameworks/esp_idf/src/drivers/esp_adc.c` 实现门面函数：
  - `adc_continuous_new_handle()`（含 `CONTINUOUS_MAX_BUF_SIZE` 越界 Fail-Loud 校验）；
  - `adc_continuous_config()`、`adc_continuous_register_event_callbacks()`；
  - `adc_continuous_start()`（配置并启动 PAL 连续采样，执行首帧注水唤醒）；
  - `adc_continuous_read()`（从纯静态 `ring_buf` 读取数据并推进指针）；
  - `adc_continuous_stop()` / `adc_continuous_deinit()`。
- [ ] **Task 2.4**：在 `esp_adc.c` 实现 `s_pal_on_full` 桥接回调：将 PAL 原始样本打包为 Type 1 字节流推入环形缓冲，满帧时触发 `cbs.on_conv_done`。
- [ ] **Task 2.5**：实现 `adc_continuous_parse_data()` 解包逻辑，支持 16-bit Type 1 位域解析与上限截断保护。

### Phase 2.5：单元级验证（Layer A Unit Test）
- [ ] **Task 2.5.1**：对 `adc_continuous_new_handle()` / `config()` / `start()` / `stop()` 进行独立单元契约测试。
- [ ] **Task 2.5.2**：对 `adc_continuous_parse_data()` 进行 Type 1 位域解析正确性验证（含非对齐字节截断、通道提取）。
- [ ] **Task 2.5.3**：验证句柄安全与 Fail-Loud 拦截（越界 buffer 申请返回 `ESP_ERR_NO_MEM`）。

### Phase 3：示例工程创建与原厂镜像固化（Layer C 应用）
- [ ] **Task 3.1**：在 `wink-micro-app/vendor/esp_idfv61/peripherals/adc_continuous_read/` 建立应用骨架。
- [ ] **Task 3.2**：从本地原厂路径原封不动镜像 `continuous_read_main.c`，严格保证零修改，固化哈希 `f8d02828e82574b4d3e80cf2deb2e9693dc1cdf2bd48086589f84c38cf7bea9b`。
- [ ] **Task 3.3**：编写 `CMakeLists.txt`、`include/sdkconfig.h` 与 `wink-app.json`（声明 GPIO 34/35 引脚拓扑）。
- [ ] **Task 3.4**：在 `run_esp32_headless_evidence.ps1` 登记 `adc_continuous_read` carrier 配置。

### Phase 4：场景编撰与自动化闭环验证（Layer S & G）
- [ ] **Task 4.1**：编撰 `unisim-scenarios/peripherals_adc_continuous_read.scenario.json` 核心业务因果断言。
- [ ] **Task 4.2**：运行 Gate 1 静态语义门禁 `run_gates.py --gate 1`。
- [ ] **Task 4.3**：运行正向 Headless 仿真，确认基线通过并生成 `run-report.json`。
- [ ] **Task 4.4**：执行 Canary 变异击杀验证（算子 1/2/3），确保缺陷敏感性 100% 达标。
- [ ] **Task 4.5**：复核许可证门禁 `check_license_map.py`。

### Phase 5：清单同步与交付归档（Layer G Delivery）
- [ ] **Task 5.1**：更新 `checklist.data.json` 条目 `esp.peripherals.adc.continuous_read`，登记 verified 状态与六要素凭证。
- [ ] **Task 5.2**：运行 `generate_checklist_v1_1.py` 重新生成 `CHECKLIST.md`。
- [ ] **Task 5.3**：运行 `evidence_verifier.py --verify-all` 进行全量一致性复核（达成 12/12 黄金基准）。
- [ ] **Task 5.4**：执行 Git 原子提交并向用户汇报。

---

## 五、 风险矩阵与应急预案

| 风险点 | 影响面 | 缓解与应对策略 |
|---|---|---|
| **R-1**: FreeRTOS Task Notification 缺失导致链接报错 | **高** | Phase 0 优先补齐 `task.h` 与 `freertos_task.c` 的通知原生桥接，确保在应用构建前基建完全就绪。 |
| **R-2**: Wasm 单线程下 `ulTaskNotifyTake(portMAX_DELAY)` 陷入死锁 | **高** | 在 `adc_continuous_start()` 中执行首帧注水，并在任务通知递增时立即调用 `sim_scheduler_unblock_resource` 唤醒纤程。 |
| **R-3**: Type 1 位域高低字节序解析错位 | **中** | 严格使用显式位运算：`data = raw & 0x0FFF`，`channel = (raw >> 12) & 0x0F`，杜绝跨编译器结构体位域填充差异。 |
| **R-4**: PAL 层被 ESP32 原厂格式污染 | **中** | 严格把关分层边界：PAL 层只传递原始 `uint16_t` 样本，Type 1 打包 100% 收敛在 `esp_adc.c` 中完成。 |
| **R-5**: 静态环形缓冲区内存溢出 | **低** | 在 `adc_continuous_new_handle()` 实施 Fail-Loud 校验，超出 1024 字节直接拒绝创建。 |
