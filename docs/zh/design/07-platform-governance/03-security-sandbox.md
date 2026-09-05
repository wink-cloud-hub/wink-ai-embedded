# 08. AI 生成代码安全沙箱、Wasm 运行隔离与云端编译安全规范

Wink-AI 平台允许用户通过低代码和 AI 自动生成 C 语言业务逻辑，并在浏览器与云端编译服务中执行相关构建流程。该能力必须建立在严格的安全边界之上：不可信代码不得直接伤害用户主机、云端基础设施或真实硬件。

---

## 1. 安全边界总览

```text
[ AI / Low-Code 输入 ]
        │
        ▼
[ App Safe Codegen 静态约束 ]
        │
        ▼
[ Wasm Worker 沙箱仿真 ] ──► [ Watchdog / Resource Limit ]
        │
        ▼
[ 云端隔离编译容器 ] ──► [ Artifact Manifest / Hash ]
        │
        ▼
[ WebSerial/WebUSB 用户授权烧录 ]
        │
        ▼
[ 真机 Runtime Fault Guard ]
```

核心原则：

1. AI 生成代码默认不可信。
2. 仿真必须先于真机部署。
3. 编译服务必须隔离执行。
4. 烧录必须由用户显式授权。
5. 真机运行必须保留故障保护路径。

---

## 2. App Safe Codegen 约束

AI 或低代码生成的 App 代码必须限制在安全 C 子集内。

### 2.1 禁止能力

| 能力 | 规则 | 原因 |
|---|---|---|
| 动态内存 | 禁止 `malloc/free/realloc/calloc` | 避免碎片、泄漏和不可预测失败 |
| 裸指针运算 | 禁止复杂指针偏移 | 降低越界风险 |
| 文件 IO | 禁止 `fopen/fread/fwrite` | MCU 不具备统一文件系统，Wasm 中也不应暴露 |
| 网络 IO | 禁止 socket API | 防止浏览器/云端越权通信 |
| 递归 | 禁止递归函数 | 避免栈不可控增长 |
| 无限循环 | 禁止用户自定义 `while(1)` | 避免 Worker 卡死 |
| 直接 PAL | 禁止 include `pal_hal.h` | 保持 App 与硬件解耦 |
| 内联汇编 | 禁止 `asm` | 平台不可移植且不可审计 |
| packed / 裸 memcpy 序列化 | 禁止 `__attribute__((packed))` / `#pragma pack`；禁止 `memcpy` 运行时 POD 到 wire/flash（须走 serialize/deserialize） | ARM/Xtensa 对齐故障；内存态布局非线协议（详见 `.claude/rules/c-code.md §4`，review P1-5） |

### 2.2 推荐代码形态

App 应表达为有限状态机和周期性 tick：

```c
void app_init(void);
void app_loop(void);
void app_on_fault(uint32_t fault_code);
```

`app_loop()` 必须在有限时间内返回，周期调度由 WinkMicroOS 控制。

---

## 3. 静态检查规则

编译前必须执行 App 静态检查：

| 检查项 | 失败等级 |
|---|---|
| include 禁止头文件 | error |
| 调用禁止函数 | error |
| 忽略 `wink_status_t` 返回值 | error |
| `app_loop()` 中存在无限循环 | error |
| 递归调用 | error |
| 栈上大数组超过阈值 | warning/error |
| 浮点比较未使用范围判断 | warning |
| 未处理 fault code | warning |

静态检查输出结构：

```json
{
  "passed": false,
  "diagnostics": [
    {
      "severity": "error",
      "file": "app_main.c",
      "line": 42,
      "column": 5,
      "rule": "APP_NO_DIRECT_PAL",
      "message": "App 层禁止直接调用 pal_gpio_write，请通过 DAL API 控制器件"
    }
  ]
}
```

---

## 4. Wasm Worker 沙箱运行限制

Asyncify 只能解决阻塞延时让渡，不能解决恶意或错误死循环。因此 Wasm 沙箱必须具备 watchdog。

### 4.1 Worker 生命周期状态

```text
created -> compiling -> running -> paused -> stopped
                         │
                         ├── faulted
                         └── terminated_by_watchdog
```

### 4.2 资源限制

| 资源 | MVP 建议值 | 处理方式 |
|---|---:|---|
| Wasm Memory | 16MB 初始，64MB 上限 | 超限终止 |
| 单次 app_loop 时间片 | 20ms | 超限警告，连续超限终止 |
| Worker 心跳 | 100ms | 主线程检测丢失心跳 |
| IPC 消息频率 | 每通道 1000 msg/s | 限流与合并 |
| Trace 缓冲区 | 10,000 条 | 环形缓冲覆盖 |
| 最大仿真时长 | 用户可配置 | 到期暂停 |

### 4.3 Watchdog 协议

Worker 定期发送心跳：

```typescript
postMessage({
  type: 'runtime_heartbeat',
  timestampMs: performance.now(),
  loopCount,
  memoryBytes,
  state: 'running'
});
```

主线程在超时时强制终止：

```typescript
if (Date.now() - lastHeartbeatAt > 500) {
  worker.terminate();
  reportRuntimeFault('WASM_WORKER_HEARTBEAT_TIMEOUT');
}
```

---

## 5. 云端编译服务隔离

用户上传的 App、device_tree 和配置文件必须在短生命周期容器中编译。

### 5.1 容器安全策略

| 项目 | 要求 |
|---|---|
| 生命周期 | 每次编译独立容器或强隔离 sandbox |
| 网络 | 默认禁用 outbound network |
| 文件系统 | toolchain 只读挂载，workspace 临时可写 |
| 用户权限 | 非 root 用户执行编译 |
| CPU | 限制核心数和总时长 |
| 内存 | cgroup 限制 |
| 磁盘 | 限制 workspace 大小 |
| 缓存 | 用户间隔离，公共缓存只读 |
| Secret | 编译容器不得挂载云端密钥 |

### 5.2 编译请求结构

```json
{
  "projectId": "proj_123",
  "target": "esp32-devkit-v1",
  "sourceBundleHash": "sha256:...",
  "deviceModelManifest": {
    "schemaVersion": 1,
    "models": ["esp32-devkit-v1@1.0.0", "hc-sr04@1.0.0"]
  },
  "buildOptions": {
    "optimization": "Os",
    "enableTrace": true
  }
}
```

### 5.3 编译响应结构

```json
{
  "buildId": "build_20260622_000001",
  "status": "success",
  "target": "esp32-devkit-v1",
  "durationMs": 4200,
  "artifacts": [
    {
      "name": "firmware.bin",
      "type": "esp32-merged-bin",
      "size": 1048576,
      "sha256": "..."
    }
  ],
  "warnings": [],
  "manifest": {
    "winkRuntimeVersion": "0.1.0",
    "palTarget": "targets/esp32",
    "sourceBundleHash": "sha256:...",
    "deviceTreeHash": "sha256:..."
  }
}
```

---

## 6. 固件产物可信链

固件产物必须携带 manifest：

1. 源码 bundle hash。
2. Device Model Registry 版本与 hash。
3. WinkMicroOS runtime 版本。
4. PAL target 版本。
5. 编译参数。
6. artifact sha256。
7. 构建时间。

烧录前浏览器展示：

```text
目标板卡：ESP32 DevKit V1
固件大小：1.0 MB
构建时间：4.2s
源码哈希：sha256:xxxx
设备模型：hc-sr04@1.0.0, servo-sg90@1.0.0
```

用户确认后才进入 WebSerial/WebUSB 授权流程。

---

## 7. WebSerial/WebUSB 安全与兼容性

### 7.1 用户授权原则

1. 浏览器必须通过原生授权弹窗选择设备。
2. 平台不得静默访问串口或 USB。
3. 烧录前必须明确目标板卡、固件 hash 和风险提示。
4. 失败时必须提供恢复指引。

### 7.2 兼容性矩阵

| 能力 | Chrome/Edge | Firefox | Safari |
|---|---|---|---|
| WebSerial | 支持 | 不支持 | 不支持 |
| WebUSB | 支持 | 不支持 | 部分不支持 |
| ESP32 WebSerial 烧录 | 支持 | 不支持 | 不支持 |
| STM32 WebUSB DFU | 支持 | 不支持 | 不稳定 |

产品兜底方案：

1. 提供固件下载。
2. 提供命令行烧录指引。
3. 后续可提供桌面助手。

---

## 8. 真机运行安全

WinkMicroOS 真机 runtime 应包含：

1. `app_loop()` watchdog。
2. fault code trace。
3. 执行器 fail-safe。
4. 可选串口诊断输出。
5. 启动自检。
6. 设备树 manifest 校验。

启动流程：

```text
boot -> pal_init -> device_tree_validate -> dal_init_all -> app_init -> app_loop scheduler
                         │
                         └── fail -> app_on_fault / safe halt
```

---

## 9. 安全等级定义

| 等级 | 状态 | 允许操作 |
|---|---|---|
| S0 | 未检查 | 不允许仿真、不允许编译 |
| S1 | 静态检查通过 | 允许 Wasm 仿真 |
| S2 | 仿真测试通过 | 允许云端编译 |
| S3 | 编译成功且 manifest 完整 | 允许用户授权烧录 |
| S4 | 真机 trace 正常 | 标记为已验证配置 |

平台应避免用户绕过 S1/S2 直接烧录 AI 生成代码。

---

## 10. MVP 必须实现的安全能力

1. App 禁止函数静态扫描。
2. `wink_status_t` 返回值检查。
3. Wasm Worker heartbeat watchdog。
4. 云端编译容器 CPU/内存/时间限制。
5. 编译产物 sha256 manifest。
6. WebSerial 烧录前确认页。
7. 仿真故障注入至少支持 timeout/disconnect。
