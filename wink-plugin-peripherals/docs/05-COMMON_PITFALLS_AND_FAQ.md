# 开发者常见踩坑避坑指南与 FAQ (Pitfalls & FAQ)

> **目标**：汇总外设插件开发中最容易遇到的 8 大典型误区与“坑”，提供排错字典与自检清单。  
> **面向对象**：所有外设开发者 (建议开发与 Code Review 时必读)。  
> **入口索引**：[README.md](./README.md)

---

## 🚨 8 大典型踩坑解析 (Common Pitfalls)

### 1. 🛑 坑一：混淆 `publish` 与 `injectWaveform`

- **错误表现**：在 `simulation.ts` 中调用了 `this.ctx.publish('echoUs', 5831)`，以为这样就把测距信号发给了 C 固件，结果固件的 `pulseIn` 仍然超时报错。
- **原因分析**：
  - `publish` 走的是 **StateChannel 通道**，只用于驱动 Web 网页 UI 数字刷新；
  - C 固件只认识 GPIO 引脚的物理电压高低，读不到 UI 上的数字。
- **正确做法**：驱动 C 固件必须调用 `this.ctx.injectWaveform('ECHO', waveform)` 注入真实的电平信号。

---

### 2. 🛑 坑二：使用宿主机 `Date.now()` 或 `performance.now()`

- **错误表现**：用 `Date.now()` 计算时间差或生成波形时间戳，导致仿真在不同性能的电脑上运行结果不一致（无法通过确定性 Golden Replay 测试）。
- **原因分析**：真实宿主机时间受 CPU 负载、操作系统线程调度影响，破坏了仿真的 100% 确定性。
- **正确做法**：所有时间戳必须且只能使用 `this.ctx.nowUs()`（返回来自 `VirtualClock` 的 `bigint` 微秒）。

---

### 3. 🛑 坑三：注入波形时遗漏 `generation` 世代令牌

- **错误表现**：超声波传感器在快速连续触发（多次 Trig）时，后一次测距的 Echo 高电平突然被提前打断拉低。
- **原因分析**：第一次触发时压入 C 队列的下降沿尚挂在队列中未触发；第二次触发时如果没有传入新的 `generation` 令牌，旧下降沿会在半路触发，把新波形错误拉低。
- **正确做法**：每次调用 `injectWaveform` 时，传入递增的 `generation: this._gen++`，底层会自动将旧世代未触发的悬挂边沿作废。

---

### 4. 🛑 坑四：`onDestroy` 遗漏 `releasePin` 导致引脚驱动残留

- **错误表现**：重新加载外设或复位后，MCU 的 GPIO 读数异常，提示引脚状态处于 `CONFLICT`（冲突）。
- **原因分析**：旧插件被销毁时，其注册在 `PinArbiter` 上的驱动（`plugin:${instanceId}`）没有被卸载。
- **正确做法**：在插件类的 `onDestroy()` 生命周期函数中，必须显式释放所有已驱动的引脚：
  ```typescript
  onDestroy(): void {
    this.ctx?.releasePin('ECHO');
  }
  ```

---

### 5. 🛑 坑五：TypeScript `number` 与 `BigInt` 混用导致 ABI 异常

- **错误表现**：向 `injectWaveform` 传入 `tUs: now + 500`（数字）或调用 `pal_wasm_push_pin_event` 时，控制台抛出 `TypeError: Cannot convert a number to a BigInt`。
- **原因分析**：WASM_BIGINT ABI 强校验要求 64 位微秒时间戳必须是 `bigint` 类型。
- **正确做法**：时间字面量必须写 `100n` 或 `BigInt(delayUs)`，不要混用 TypeScript `number` 浮点数。

---

### 6. 🛑 坑六：未防范死循环回声（Feedback Loop Recursion）

- **错误表现**：外设在 `onPinChange` 监听到 MCU 引脚电平变化后，立刻调用 `writePin` 写回同一 Pin，导致控制台抛出 `PinArbiter notification cascade exceeded maxRecursionDepth` 警告。
- **原因分析**：引脚写操作再次触发了自身的 `onPinChange` 监听器，形成 `MCU -> Plugin -> MCU -> Plugin` 无限递归。
- **正确做法**：在写 Pin 前检查电平变化是否确实属于有效新状态，或使用特定 driver ID 隔离。

---

### 7. 🛑 坑七：在 `simulation.ts` 中手写复杂的 `deferUs` 降级分支

- **错误表现**：在外设中手写了大量的 `if (accuracyMode === 'behavioral')` 和 `this.ctx.deferUs(...)` 旁路定时器代码。
- **原因分析**：不了解 `injectWaveform` 的内部机制，重复造轮子。
- **正确做法**：统一使用 `injectWaveform`。系统在 Behavioral 模式下会自动降级为直接施加终态电平，无需插件自己写分支逻辑。

---

### 8. 🛑 坑八：物理公式与外设调度逻辑混在一起

- **错误表现**：把声速公式、温湿度换算、飞行时间推导直接写在 `onPinChange` 方法内部，导致代码臃肿难以单测。
- **正确做法**：将纯物理公式抽离到插件目录的 `src/physics/` 下，作为 Pure Function 独立维护与单测。

---

## ❓ 常见问题 FAQ (Troubleshooting Checklist)

### Q1: 为什么我在 UI 拖动滑块，插件里的处理函数没有被调用？

- **检查项 1**：检查 Manifest 中的事件名（如 `SET_DISTANCE_CM`）是否与 `simulation.ts` 中的函数名（`_setDistanceCm`）遵循 **`_驼峰命名`** 规则。
- **检查项 2**：检查 Manifest 的 `events.SET_DISTANCE_CM.params` 字典中的参数名是否与处理函数的入参一致。

### Q2: 外设打包报错 `Cannot find module '@unisim/plugin'`？

- **检查项**：确认没有在外设自己的 `package.json` 中把 `@unisim/plugin` 打入 dependencies，且 `vite.config.sim.ts` 中配置了 `rollupOptions.external: ['@unisim/plugin']`。

### Q3: 本地修改了外设代码，为什么网页上没有生效？

- **检查项**：确认修改后运行了 `build-peripherals.bat` 生成了最新的 `dist/` 产物，并且检查控制台确认 Dev 路径（`WINK_PERIPHERAL_DEV_DIR`）正确指向了外设的 `dist/` 目录。
