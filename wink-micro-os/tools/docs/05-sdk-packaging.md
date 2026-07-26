# SDK 发布与打包工具指南 (`pack_sdk_*`)

Wink Micro OS 提供了自动化 SDK 发布套件（位于 [wink-micro-os/tools/pack_sdk_binary.py](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/tools/pack_sdk_binary.py) 及 `pack_sdk_source.py`）。符合 **ADR-0028** 交付契约，支持自动编译、裁切私有代码、生成打包校验元数据并交付 SDK 二进制或源码安装包。

---

## 1. 打包模式对比

| 打包脚本 | 交付产物 | 适用于场景 | 特点 |
| :--- | :--- | :--- | :--- |
| `pack_sdk_source.py` | Source SDK | 开源应用开发 / 内网全量构建 | 包含完整 C 源码，开发者可重新编译 OS 底层。 |
| `pack_sdk_binary.py` | Binary SDK | 闭源发布 / 第三方 App 极速构建 | 仅暴露头文件与预编译 `.a` 静态库，自动剔除私有 `.c` 实现。 |

---

## 2. Binary SDK 打包原理与天花板校验 (ADR-0028)

### 2.1 结构剥离与头文件裁切
运行 `pack_sdk_binary.py` 时，脚本会自动完成以下动作：
1. 调用目标工具链编译生成平台静态库（如 Host `libwink_os.a` 或 WASM `libwink_os_wasm.a`）。
2. 提取公共头文件 (`pal/include`, `dal/include`, `bal/include`, `runtime/include`)。
3. 彻底屏蔽剔除私有实现目录 (`src/`, `targets/`, `test/`)。
4. 附带生成 [CMakeLists.txt](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/tools/binary_sdk_cmake/CMakeLists.txt) 编译胶水与 `sdk_manifest.json` 元数据。

### 2.2 天花板配置约束校验 (Pack Ceilings)
为了保证预编译二进制库与应用层的 ABI 兼容，Binary SDK 在生成和编译阶段会执行天花板配置强校验（ADR-0028 §5）：

* **最大定时器数量** (`max_soft_timers`) ≤ **32**
* **PWM 通道分配上限** (`pwm_channels`) ≤ **16**

若应用的 `wink-app.json` 超出了此阈值，构建系统将提示错误并要求切换至 Source SDK 构建：
```text
[config_h] Binary SDK config ceiling violation (ADR-0028):
  - max_soft_timers=64 exceeds Binary SDK ceiling (32); use Source SDK or reduce the value.
```

---

## 3. 打包命令与使用示例

### 3.1 制作 Binary SDK 二进制包
```bash
# 为 Host 目标生成 Binary SDK 包
python tools/pack_sdk_binary.py --target host --out-dir dist/wink-sdk-host

# 为 WASM 仿真生成 Binary SDK 包
python tools/pack_sdk_binary.py --target wasm --out-dir dist/wink-sdk-wasm
```

### 3.2 制作 Source SDK 源码包
```bash
python tools/pack_sdk_source.py --out-dir dist/wink-sdk-source
```

---

## 4. 输出的 Binary SDK 包结构

生成的 Binary SDK 保持如下标准的极简目录分布：

```
wink-sdk-binary/
├── cmake/
│   └── wink_binary_import.cmake        # App 引入二进制库的 CMake 导入脚本
├── include/                            # 精简后的公共 API 接口头文件
│   ├── wink_status.h
│   ├── dal/
│   └── bal/
├── lib/
│   └── libwink_os.a                    # 预编译静态库
├── sdk_manifest.json                   # SDK 版本与天花板元数据
└── README.md                           # 二进制 SDK 集成指引
```
