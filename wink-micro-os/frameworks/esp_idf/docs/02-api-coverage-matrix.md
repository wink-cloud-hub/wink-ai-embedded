# ESP-IDF 仿真拦截层 API 覆盖矩阵与降级登记簿 (02-api-coverage-matrix)

> **版本**：v1.0  
> **适用里程碑**：M0 (最小 GPIO 闭环与 Tier-A Blink 语料)

---

## 1. M0 交付 API 覆盖清单

| API 标识符 | 所属头文件 | 实现状态 | 仿真底层对应物 | 降级/弱化登记 |
|:---|:---|:---:|:---|:---|
| `gpio_config` | `driver/gpio.h` | ✅ 支持 | `pal_gpio_init` (逐 pin) | 遇到越界引脚立即返回 `ESP_ERR_INVALID_ARG` |
| `gpio_set_direction` | `driver/gpio.h` | ✅ 支持 | `pal_gpio_init`（幂等且记录模式；host `pal_gpio_set_direction` 为 no-op 不记录模式，故降级经 init 下沉；官方本 API 无 pull 参数） | 遇到越界引脚返回 `ESP_ERR_INVALID_ARG` |
| `gpio_set_level` | `driver/gpio.h` | ✅ 支持 | `pal_gpio_write` | 遇到越界引脚或只读引脚返回 `ESP_ERR_INVALID_ARG` |
| `gpio_get_level` | `driver/gpio.h` | ⚠️ 降级支持 | `pal_gpio_read` | **[降级登记 1]** 越界/读取失败与电平 0 不可区分 |
| `gpio_reset_pin` | `driver/gpio.h` | ✅ 支持 | `pal_gpio_deinit` | 成功返回 `ESP_OK`，越界返回 `ESP_ERR_INVALID_ARG` |
| `gpio_set_pull_mode` / `gpio_pullup_en_dis` / `gpio_pulldown_en_dis` | `driver/gpio.h` | 🚫 未支持 | 无 | **[降级登记 4]** 越界仍 `ESP_ERR_INVALID_ARG`，有效引脚 `ESP_LOGE` + `ESP_ERR_NOT_SUPPORTED` |
| `gpio_set_intr_type` / `gpio_intr_enable_disable` / `gpio_install_isr_service` / `gpio_isr_handler_add_remove` | `driver/gpio.h` | 🚫 未支持 | 无 | **[降级登记 4]** `ESP_LOGE` + `ESP_ERR_NOT_SUPPORTED`；`gpio_uninstall_isr_service`（void 无错误通道）仅 `ESP_LOGW` |
| `esp_task_wdt_*` / `esp_intr_alloc_free` | `esp_task_wdt.h` / `esp_intr_alloc.h` | 🚫 未支持 | 无 | **[降级登记 5]** `ESP_LOGE` + `ESP_ERR_NOT_SUPPORTED`；失败时 `esp_intr_alloc` 回写空句柄 |
| `esp_rom_gpio_pad_select_gpio` | `esp_rom_gpio.h` | ⚠️ 降级支持 | 无 | **[降级登记 5]** void 无错误通道，仅 `ESP_LOGW` 后 no-op |
| `esp_err_from_wink` | `esp_err.h` | ✅ 支持 | 查表转换 | 负数 Wink 错误码转为 ESP 0x101+ 体系 |
| `wink_status_from_esp` | `esp_err.h` | ✅ 支持 | 查表转换 | ESP 错误码转回 Wink 负数错误码 |
| `esp_restart` | `esp_system.h` | ✅ 支持 | `pal_wasm_target_*` 弱钩子族 | 仅置位 reset pending 标志，绝不调用 host exit |
| `esp_get_idf_version` | `esp_system.h` | ✅ 支持 | 静态常量字符串 | 返回 `"v6.1-dev-winksim"` |
| `esp_random` | `esp_random.h` | ✅ 支持 | xorshift32 确定性算法 | 保证仿真 Replay 确定性 |
| `esp_log_write` / `ESP_LOG*` | `esp_log.h` | ⚠️ 降级支持 | `pal_log_e/w/i/d` | **[降级登记 2]** ISR 下 INFO 等级可能被底层 PAL 静默丢弃 |
| `vTaskDelay` | `freertos/task.h`| 📋 声明桩 | M1 交付调度接入 | 仅供 Tier-A Blink 编译，未链接实现时 Fail-Loud |

---

## 2. 降级与弱化登记簿 (ADR-0012 合约诚实)

根据 ADR-0012《合约诚实与降级登记》规范，所有由于目标环境限制、官方 C-ABI 缺陷或仿真简化导致的语义弱化必须显式登记：

### 降级条目 1：`gpio_get_level` 越界与低电平不可区分
- **受影响 API**：`int gpio_get_level(gpio_num_t gpio_num)`
- **官方缺陷**：ESP-IDF 官方 C-ABI 返回 `int` 电平值 (0 或 1)，无独立错误码返回通道。
- **拦截层行为**：
  - 当传入无效引脚（如 `!GPIO_IS_VALID_GPIO(gpio_num)`）时，记录 `ESP_LOGE` 错误日志，并返回 0。
  - 当底层 `pal_gpio_read` 失败时，记录 `ESP_LOGE` 错误日志，并返回 0。
- **风险与影响**：调用方无法仅凭返回值区分“引脚为有效低电平”还是“引脚越界/读取失败”。符合官方现有行为，通过日志暴露可见性。

### 降级条目 2：`ESP_LOG*` 级别与 ISR 上下文限制
- **受影响 API**：`ESP_LOGI`, `ESP_LOGD`, `ESP_LOGV`
- **底层限制**：WinkMicroOS 的 PAL 日志系统（`pal_log.h`）在中断上下文（ISR）下对 INFO 等级有静默丢弃策略。
- **拦截层行为**：转调 `pal_log_*`，遵循 PAL 底层过滤规则。

### 降级条目 3：输出引脚电平回读走门面缓存
- **受影响 API**：`int gpio_get_level(gpio_num_t gpio_num)`（输出模式引脚）
- **底层限制**：Host/Wasm PAL 的 `pal_gpio_read` 对输出模式引脚报告的是模式空闲电平，而非最后一次驱动电平（见 `targets/host/pal_hal_gpio_host.c:pal_gpio_read`），且禁止反向修改 PAL（红线 2）。
- **拦截层行为**：门面维护 `s_is_output` / `s_output_levels` 回读缓存（`src/drivers/esp_gpio.c`），`gpio_set_level` 成功即更新；`gpio_config` 切输入、`gpio_set_direction` 切输入、`gpio_reset_pin` 时清零对应位。输入模式引脚仍直读 PAL。
- **风险与影响**：仅限协作式单线程仿真（引脚号 < 64）；绕开门面直接调 PAL 改电平会造成缓存分叉——M0 仿真无此路径，M1 多任务化时需重估。

### 降级条目 4：GPIO 上拉/中断/ISR 全家桶未支持（Fail-Loud）
- **受影响 API**：`gpio_set_pull_mode`、`gpio_pullup_en/dis`、`gpio_pulldown_en/dis`、`gpio_set_intr_type`、`gpio_intr_enable/disable`、`gpio_install_isr_service`、`gpio_isr_handler_add/remove`、`gpio_uninstall_isr_service`
- **拦截层行为**：有效引脚一律 `ESP_LOGE` + 返回 `ESP_ERR_NOT_SUPPORTED`（越界引脚仍优先返回 `ESP_ERR_INVALID_ARG`）；`gpio_uninstall_isr_service` 为 void C-ABI、无错误通道，仅 `ESP_LOGW` 后 no-op。**严禁返回 `ESP_OK` 冒充成功**（ADR-0012 红线 6）。
- **风险与影响**：依赖 GPIO 中断的上游示例在 M0 仿真中会明确报错而非静默失灵；完整中断语义递延 M1/M2。

### 降级条目 5：看门狗/中断分配/ROM 垫片未支持（Fail-Loud）
- **受影响 API**：`esp_task_wdt_*`、`esp_intr_alloc/free`、`esp_rom_gpio_pad_select_gpio`
- **拦截层行为**：前两组 `ESP_LOGE` + 返回 `ESP_ERR_NOT_SUPPORTED`（`esp_intr_alloc` 失败时回写空句柄，不伪造 `0x1` 有效句柄）；`esp_rom_gpio_pad_select_gpio` 为 void C-ABI，仅 `ESP_LOGW` 后 no-op。
- **风险与影响**：同条目 4；看门狗语义递延 M1（与 FreeRTOS 调度接入一并交付）。

---

## 3. Tier-A 官方语料验证集

| 语料标识 | 官方路径 | 覆盖功能点 | 目标形态 |
|:---|:---|:---|:---:|
| `corpus_blink` | `examples/get-started/blink/main/blink_example_main.c` | GPIO 配置、输出电平控制、FreeRTOS 延迟声明 | `OBJECT` compile-only 100% 通过 |
