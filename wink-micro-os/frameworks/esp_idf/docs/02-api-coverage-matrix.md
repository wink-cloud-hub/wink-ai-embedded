# ESP-IDF 仿真拦截层 API 覆盖矩阵与降级登记簿 (02-api-coverage-matrix)

> **版本**：v1.0  
> **适用里程碑**：M0 (最小 GPIO 闭环与 Tier-A Blink 语料)

---

## 1. M0 交付 API 覆盖清单

| API 标识符 | 所属头文件 | 实现状态 | 仿真底层对应物 | 降级/弱化登记 |
|:---|:---|:---:|:---|:---|
| `gpio_config` | `driver/gpio.h` | ✅ 支持 | `pal_gpio_init` (逐 pin) | 遇到越界引脚立即返回 `ESP_ERR_INVALID_ARG` |
| `gpio_set_direction` | `driver/gpio.h` | ✅ 支持 | `pal_gpio_set_direction` | 遇到越界引脚返回 `ESP_ERR_INVALID_ARG` |
| `gpio_set_level` | `driver/gpio.h` | ✅ 支持 | `pal_gpio_write` | 遇到越界引脚或只读引脚返回 `ESP_ERR_INVALID_ARG` |
| `gpio_get_level` | `driver/gpio.h` | ⚠️ 降级支持 | `pal_gpio_read` | **[降级登记 1]** 越界/读取失败与电平 0 不可区分 |
| `gpio_reset_pin` | `driver/gpio.h` | ✅ 支持 | `pal_gpio_deinit` | 成功返回 `ESP_OK`，越界返回 `ESP_ERR_INVALID_ARG` |
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

---

## 3. Tier-A 官方语料验证集

| 语料标识 | 官方路径 | 覆盖功能点 | 目标形态 |
|:---|:---|:---|:---:|
| `corpus_blink` | `examples/get-started/blink/main/blink_example_main.c` | GPIO 配置、输出电平控制、FreeRTOS 延迟声明 | `OBJECT` compile-only 100% 通过 |
