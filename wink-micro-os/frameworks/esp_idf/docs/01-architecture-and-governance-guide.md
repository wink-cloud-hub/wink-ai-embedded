# ESP-IDF 仿真拦截层架构与治理规范 (01-architecture-and-governance-guide)

> **版本**：v1.0  
> **适用目标**：WinkMicroOS ESP-IDF 仿真拦截框架 (Axis B)

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
|   frameworks/esp_idf/include (driver/gpio.h, esp_log.h)   |
+-----------------------------------------------------------+
|   frameworks/esp_idf/src (esp_gpio.c, esp_log.c, ...)     |
+-----------------------------------------------------------+
                             | 下沉转调 (0 claim, 0 malloc)
                             v
+-----------------------------------------------------------+
|                 WinkMicroOS PAL (pal_gpio_*, ...)         |
+-----------------------------------------------------------+
```

---

## 2. 生命周期与框架引导

1. **强符号导出**：
   框架在 `src/esp_idf_runtime.c` 中导出强符号 `wink_app_get_callbacks()`。
   ```c
   const wink_app_callbacks_t* wink_app_get_callbacks(void);
   ```
2. **多框架互斥 (T-009)**：
   - 与 `mcs51` 强符号互斥：不可同链，构建期通过 CMake `FATAL_ERROR` 硬防御。
   - 与 `arduino` 弱符号共存：`arduino` 声明为 `__attribute__((weak))`，同链时自动让位。
3. **系统复位 (`esp_restart`)**：
   - 严禁调用 host `exit()` 或 `abort()`。
   - 通过 `pal_wasm_target_has_pending_reset` / `pal_wasm_target_get_reset_reason` 弱钩子族通知调度器优雅复位。

---

## 3. 7 条架构红线 (DoD 准入准出)

1. **C-ABI 与纯 C 实现**：严格采用 C99 编写，禁止 C++ 运行时与异常机制。
2. **严禁侵入式修改 PAL / DAL**：只允许单向依赖 `pal/include`，严禁修改 PAL，严禁越级调用 DAL 内部符号。
3. **严格遵守 ADR-0065**：门面层严禁调用 `pal_resource_claim()`。
4. **零运行期堆分配**：`src/**` 运行期禁止 `malloc/free`，资源池必须静态全局预分配。
5. **PWM 定点红线 (ADR-0066)**：涉及占空比计算全定点整数运算，严禁浮点 duty。
6. **合约诚实 (ADR-0012)**：不支持的 API 编译期 `#error` 或链接期缺失，严禁静默空实现；一切语义弱化必须登记入 `02-api-coverage-matrix.md` 降级表。
7. **开源许可合规 (ADR-0083/0084)**：
   - `frameworks/esp_idf/{src,include,chips}` = **LGPL-3.0-only**；
   - `frameworks/esp_idf/{test,tools}` = **GPL-3.0-only**。
