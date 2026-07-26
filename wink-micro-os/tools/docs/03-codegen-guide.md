# 设备树代码生成引擎指南 (`codegen`)

Wink Micro OS 提供了低代码/数据驱动的代码生成引擎（位于 [wink-micro-os/tools/codegen](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/tools/codegen)）。通过解析可读的 JSON 描述文件，CodeGen 引擎能够在编译期自动渲染底层硬件抽象层（DAL）代码、设备树实例、Arduino 语法兼容胶水以及 `wink_config.h` 配置头文件。

---

## 1. CodeGen 生成流程与生成文件

当运行 `python tools/wink.py gen --app <app_name>` 时，代码生成流水线包含以下步骤：

```
 wink-app.json (应用配置)
       +                       -----> [app_codegen.py] -----> device_tree.h / .c
 board.json (板级定义)           -----> [config_h.py]   -----> wink_config.h
                                 -----> [Jinja2 Templates] -> app_options.cmake
```

渲染生成的核心产物：

| 生成产物 | 描述 |
| :--- | :--- |
| `device_tree.h / .c` | 实例化 DAL 设备句柄数组与全局设备树 API |
| `wink_arduino_bindings.h / .cpp` | 提供符合 Arduino 标准语法的 `pinMode()`, `digitalWrite()` 兼容 API |
| `wink_config.h` | 运行时主循环 Tick 周期、定时器容量上限及仿真堆内存配额宏定义 |
| `app_options.cmake` | 供 CMake 构建脚本读取的模块裁剪开关（如 `WINK_USE_LED`, `WINK_USE_MOTOR`） |
| `wasm_export_codegen.py` | 自动扫描导出 API，生成 Emscripten WASM `EXPORTED_FUNCTIONS` 符号表 |

---

## 2. 配置文件规范

### 2.1 `board.json` 板级硬件定义文件
位于 [tools/codegen/boards/](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/tools/codegen/boards/esp32_devkitc.json)，描述板卡元数据、板载固定外设及引脚 Header 映射。

示例（`esp32_devkitc.json`）：
```json
{
  "metadata": {
    "board_name": "esp32_devkitc",
    "mcu": "esp32",
    "memory": {
      "sim_heap_quota_kb": 256
    }
  },
  "onboard_devices": {
    "status_led": { "type": "led", "pin": 2, "active_high": true }
  },
  "headers": {
    "D2": 2,
    "A0": 36
  }
}
```

### 2.2 `wink-app.json` 应用层配置文件
位于每个 App 目录根部，描述应用程序选择的开发板、使用的外设清单以及平台覆盖参数。

示例：
```json
{
  "app_name": "avoidance_car",
  "board": "esp32_devkitc",
  "target_config": {
    "sim_heap_quota_kb": 512
  },
  "devices": {
    "front_distance": {
      "type": "ultrasonic",
      "trig_pin": 12,
      "echo_pin": 13
    }
  }
}
```

---

## 3. 内存与参数单源真理 (SSOT) 检索规则

在最新的架构中，`config_h.py` 采用了通用的**递归键值搜索 (Generic Recursive Lookup)**。内存配额参数 `sim_heap_quota_kb` 无论位于何种层级均可被自动解析：

1. **结构化路径**：`target_config.memory.sim_heap_quota_kb`
2. **快捷覆盖路径**：`target_config.sim_heap_quota_kb`
3. **根节点路径**：`sim_heap_quota_kb`
4. **板级默认配置**：`board.json` 里的 `metadata.memory.sim_heap_quota_kb`
5. **系统兜底值**：`256` KB

---

## 4. 自定义外设驱动插件开发 (`tools/codegen/drivers/`)

所有外设驱动类型（如 `led`, `button`, `servo`, `ultrasonic`, `motor`, `encoder` 等）均实现为 Python 驱动插件类，继承自 [DriverBase](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/tools/codegen/drivers/base.py)。

### 4.1 新增驱动插件步骤

1. 在 `tools/codegen/drivers/` 目录下新建 `my_device.py`。
2. 继承 `DriverBase` 并实现核心虚接口：

```python
from tools.codegen.drivers.base import DriverBase

class MyDeviceDriver(DriverBase):
    @property
    def driver_type(self) -> str:
        return "my_device"

    @property
    def cmake_option(self) -> str:
        return "WINK_USE_MY_DEVICE"

    def validate_spec(self, name: str, spec: dict, source: str) -> None:
        """校验 wink-app.json 中该设备的参数合法性"""
        if "pin" not in spec:
            self.die(f"{source}: device '{name}' missing required 'pin'")

    def render_init_c(self, name: str, spec: dict) -> str:
        """渲染 C 语言设备树初始化代码"""
        return f"dal_my_device_init(&g_dev_{name}, {spec['pin']});"
```

3. 运行单元测试验证生成：
   ```bash
   python -m unittest discover -s tools/codegen/tests
   ```
