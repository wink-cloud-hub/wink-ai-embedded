<!--
visibility: public
winkcli-version: ">=0.1.0"
-->
# 05 · SDK 打包指南（`winkcli pack`）

`winkcli pack` 提供两种 SDK 交付形态：完整源码的 **Source SDK** 与仅含头文件 + 预编译静态库的 **Binary SDK**。

---

## 1. 两种模式对比

| 命令 | 产物 | 适用场景 | 特点 |
|---|---|---|---|
| `winkcli pack source` | Source SDK | 开源应用开发 / 内网全量构建 | 包含完整 C 源码，可重新编译 OS 底层 |
| `winkcli pack binary` | Binary SDK | 闭源发布 / 第三方极速构建 | 仅头文件与预编译 `.a` 静态库 |

---

## 2. Binary SDK 的天花板约束（Pack Ceilings）

为保证预编译库与应用层的 ABI 兼容，Binary SDK 对若干配置设有上限：

| 配置项 | 上限 |
|---|---|
| 软定时器数量（`max_soft_timers`） | ≤ 32 |
| PWM 通道数（`pwm_channels`） | ≤ 16 |

超出阈值时构建会中断并提示切换 Source SDK：

```text
[config_h] Binary SDK config ceiling violation:
  - max_soft_timers=64 exceeds Binary SDK ceiling (32); use Source SDK or reduce the value.
```

> 消费 Binary SDK 时需要支持 C99 的编译器（GCC / Clang / MSVC）。Windows + MinGW 与 MSVC 双编译链均已验证。

---

## 3. 打包命令

```bash
# Binary SDK（按目标平台）
winkcli pack binary --target host --out dist/wink-sdk-host
winkcli pack binary --target wasm --out dist/wink-sdk-wasm

# 跳过内部编译步骤（复用已有构建产物）
winkcli pack binary --target host --out dist/wink-sdk-host --skip-build

# Source SDK
winkcli pack source --out dist/wink-sdk-source
```

---

## 4. Binary SDK 目录结构

```text
wink-sdk-binary/
├── cmake/
│   └── wink_binary_import.cmake   # App 引入二进制库的 CMake 导入脚本
├── include/                       # 精简后的公共 API 头文件
│   ├── wink_status.h
│   ├── dal/
│   └── bal/
├── lib/
│   └── libwink_os.a               # 预编译静态库
├── sdk_manifest.json              # SDK 版本与配置元数据
└── README.md                      # 集成指引
```
