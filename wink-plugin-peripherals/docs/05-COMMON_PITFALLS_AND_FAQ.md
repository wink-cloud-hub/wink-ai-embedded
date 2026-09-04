# 开发者常见踩坑避坑指南与 FAQ (Pitfalls & FAQ)

> **目标**：汇总外设插件开发中最容易遇到的 9 大典型误区与“坑”，提供排错字典与自检清单。  
> **面向对象**：所有外设开发者 (建议开发与 Code Review 时必读)。  
> **入口索引**：[README.md](./README.md)

---

## 🚨 9 大典型踩坑解析 (Common Pitfalls)

### 1. 🛑 坑一：事件处理函数命名错误（在方法名中保留了 `set`）

- **错误表现**：在 Manifest 中声明了 `SET_DISTANCE_CM` 或 `SET_ACTIVE`，在 `simulation.ts` 中写了 `_setDistanceCm(cm)` 或 `_setActive(active)`。在 UI 拖动控件或测试派发事件时，控制台抛出硬异常：  
  `[PluginHost] missing handler for event 'SET_DISTANCE_CM' (expected method '_distanceCm')`。
- **原因分析**：UniSim 内核函数 `mapEventToMethod` 转换时**严格剔除了 `SET_` 前缀**并转小驼峰。
- **正确做法**：
  - `SET_DISTANCE_CM` 必须命名为 **`_distanceCm(cm)`**；
  - `SET_ACTIVE` 必须命名为 **`_active(active)`**；
  - `SET_ANGLE` 必须命名为 **`_angle(angle)`**。

---

### 2. 🛑 坑二：混淆 `publish` 与 `writePin` / `injectWaveform`

- **错误表现**：在 `simulation.ts` 中调用了 `this.ctx.publish('echoUs', 5831)`，以为这样就把测距信号发给了 C 固件，结果固件的 `pulseIn` 仍然超时报错。
- **原因分析**：
  - `publish` 走的是 **StateChannel 通道**，只用于驱动前端 UI 数字刷新与 E2E 观测；
  - C 固件只认识底层 GPIO 引脚的物理高低电平，读不到 StateChannel。
- **正确做法**：驱动 C 固件必须调用 `this.ctx.injectWaveform('ECHO', waveform)` 或 `this.ctx.writePin('SIG', level)` 注入真实的电平信号。

---

### 3. 🛑 坑三：使用宿主机真实时间（`Date.now()` / `performance.now()`）

- **错误表现**：用 `Date.now()` 计算时间差或生成波形时间戳，导致仿真在不同性能的电脑上运行结果不一致（无法通过确定性 Golden Replay 测试）。
- **原因分析**：宿主机真实时间受 CPU 负载、操作系统线程调度影响，破坏了仿真的确定性（Determinism）。
- **正确做法**：所有时间戳必须且只能使用 `this.ctx.nowUs()`（返回来自 `VirtualClock` 的 `bigint` 微秒）。

---

### 4. 🛑 坑四：误将 `deferUs()` 当作 Promise `await`

- **错误表现**：在外设生命周期中写了 `await this.ctx?.deferUs(10_000n)`，发现代码并未等待 10ms，而是立即同步执行了。
- **原因分析**：UniSim 的 `deferUs` 是**回调式调度器**，签名定义为 `deferUs(delayUs: bigint, callback: () => void): void`，不返回 Promise。
- **正确做法**：使用回调方式：
  ```typescript
  this.ctx.deferUs(10_000n, () => {
    this.handleTimeout();
  });
  ```
  若在异步钩子（如 `onReady`）中需等待，请显式封装：
  ```typescript
  await new Promise<void>(resolve => this.ctx.deferUs(10_000n, resolve));
  ```

---

### 5. 🛑 坑五：注入波形时遗漏 `generation` 世代令牌导致悬挂边沿污染

- **错误表现**：超声波传感器在快速连续触发（多次 Trig）时，后一次测距的 Echo 高电平突然被提前打断拉低。
- **原因分析**：第一次触发时压入 C 队列的下降沿尚挂在队列中未触发；第二次触发时如果没有传入新的 `generation` 令牌，旧下降沿会在半路触发，把新波形错误拉低。
- **正确做法**：每次调用 `injectWaveform` 时，传入递增的 `generation: this._echoGeneration++`，底层会自动将旧世代未触发的悬挂边沿作废。

---

### 6. 🛑 坑六：`onDestroy` 遗漏 `releasePin` 导致引脚驱动残留冲突

- **错误表现**：重新加载外设或复位后，MCU 的 GPIO 读数异常，提示引脚状态处于 `CONFLICT`（四态逻辑中的 `'X'` 冲突态）。
- **原因分析**：旧插件被卸载时，其注册在 `PinArbiter` 上的驱动（`plugin:${instanceId}`）没有被释放。
- **正确做法**：在插件类的 `onDestroy()` 生命周期函数中，必须显式释放所有已驱动的引脚：
  ```typescript
  onDestroy(): void {
    this.ctx?.releasePin('ECHO');
  }
  ```

---

### 7. 🛑 坑七：TypeScript `number` 与 `BigInt` 混用导致 ABI 异常

- **错误表现**：向 `injectWaveform` 传入 `tUs: now + 500`（JS 数字）时，控制台抛出 `TypeError: Cannot convert a number to a BigInt`。
- **原因分析**：WASM_BIGINT 64 位微秒时间戳必须是 `bigint` 类型。
- **正确做法**：时间字面量必须写 `100n` 或 `BigInt(delayUs)`，严禁混用 JS 浮点数。

---

### 8. 🛑 坑八：在插件 Bundle 中打包了 `@wokwi/elements`

- **错误表现**：打开仿真工作台，控制台报硬错误：`NotSupportedError: the name "wokwi-*" has already been used`。
- **原因分析**：每个插件如果各自打包一份 `@wokwi/elements`，会导致浏览器重复注册自定义 Web Component。
- **正确做法**：使用官方预设 `@wink-ai/unisim-ui/vite` 的 `definePeripheralUiConfig`，它已将 `@wokwi/elements` 默认声明为 `external`，由宿主应用全局单例注入。

---

### 9. 🛑 坑九：误将前端产物命名为 `index.js`

- **错误表现**：编译成功后，工作台提示加载外设前端资源失败（`GET .../bundle/frontend.js 404 Not Found`）。
- **原因分析**：宿主应用与后端的标准请求路径固定为 `/:type/bundle/frontend.js`。
- **正确做法**：前端产物文件名必须是 **`frontend.js`**（使用 `@wink-ai/unisim-ui/vite` 会自动遵循此规范）。

---

## ❓ 常见问题 FAQ (Troubleshooting Checklist)

### Q1: 为什么在 UI 拖动滑块，插件里的处理函数没有被调用？
1. **检查方法命名**：确认 Manifest 事件名（如 `SET_DISTANCE_CM`）在插件中对应的方法名为 **`_distanceCm`**（去除 `set_` 前缀），而不是 `_setDistanceCm`。
2. **检查参数定义**：确认 Manifest 中声明的参数名（如 `params: { cm: { type: 'number' } }`）与传递的参数结构一致。

### Q2: 外设编译报错 `Cannot find module '@wink-ai/unisim'`？
1. 确认在 `wink-plugin-peripherals` 目录下执行了依赖链接或安装；
2. 推荐使用 `build-peripherals.ps1` 编译，该脚本会自动侦测本地 SDK 源码路径并创建软链接。

### Q3: 本地修改了外设代码，为什么网页上没有生效？
1. 确认修改后运行了 `bun run build` 生成了最新的 `dist/frontend.js` 和 `dist/simulation.js`；
2. 确认后端调试服务中的 `WINK_PERIPHERAL_DEV_DIR` 正确指向了当前外设所在路径；
3. 调用 `POST /api/v1/plugins/rescan` 或刷新浏览器以清除前端 ESM 内存缓存。
