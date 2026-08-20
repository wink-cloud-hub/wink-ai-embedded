# RC Servo Motor (`rc_servo` 1.0.0) 外置外设插件

本目录为 Wink-AI 系统的标准外置外设插件 **RC Servo Motor (`rc_servo` 1.0.0)**。本插件完全独立解耦，包含了 2D 画布 Vue 渲染组件、物理参数元数据 Manifest、以及底层 PWM 占空比计算仿真算法。

---

## 🗂️ 目录文件结构

```text
peripherals/builtin/rc_servo/1.0.0/
├── manifest.json            # 外设元数据定义 (引脚、属性、分类、状态通道)
├── tsconfig.json            # 独立 TS 编译与路径别名映射 (@/ 和 @unisim/)
├── README.md                # 插件开发与构建说明文档
├── src/
│   ├── index.ts             # 插件入口（注册前端定义与 actuator 转换器）
│   ├── definition.ts        # UI 渲染定义与 ui.canvasProps 实时状态映射
│   ├── CanvasGlyph.vue      # 2D 画布 Vue 渲染组件（渲染 Web Component <wokwi-servo>）
│   ├── simulation.ts        # 仿真算法实现类 (BaseSimulationPlugin)
│   ├── pwm-duty-to-angle.ts # PWM 占空比转角度的核心计算公式 (SSOT)
│   └── variants.ts          # 拓扑变体定义 (SG90 预设)
└── dist/
    ├── frontend.js          # 打包后的前端浏览器 ESM Bundle (由后端分发)
    └── simulation.js        # 打包后的仿真 Worker ESM/CJS Bundle
```

---

## 🛠️ 构建与打包说明 (Build Instructions)

当修改 `src/` 中的源码（如更新 UI 组件或仿真算法）后，需要重新打包生成 `dist/` 产物供宿主运行。

### 方式 1：全量构建所有外设插件

在工程根目录下运行：

```bash
bun run build:peripherals
```

### 方式 2：使用 `wink-tools` 命令行构建本外设

在 `wink-tools` 工具链环境中使用 CLI 工具构建：

```bash
# 编译生成本插件的 dist/ 产物
python wink-tools/wink.py build unisim-plugin --path ./peripherals/builtin/rc_servo/1.0.0
```

---

## 🚀 运行时动态加载原理 (Runtime Architecture)

1. **后端服务托管 (`packages/backend-hono`)**：
   - 服务端启动后，自动暴露静态 API 路由：`GET /api/plugins/rc_servo/bundle/frontend.js`。
2. **前端宿主加载 (`packages/embedded-frontend`)**：
   - 界面打开包含 `rc_servo` 的项目时，`usePeripheralUiDefs.ts` 动态向后端请求 `frontend.js` ESM。
   - 浏览器动态导入该 Bundle，并将 Vue 画布组件 `<wokwi-servo :angle="angle">` 挂载到 2D 画布。
3. **仿真数据闭环 (`packages/unisim`)**：
   - WASM 引擎计算得出舵机 PWM 输出后，`dist/simulation.js` 将占空比转换为角度（`0°~180°`），并通过 `pluginChannels` 通道实时驱动界面舵机摆臂旋转。

---

## 💡 开发注意事项 (Best Practices)

- **避免二次引入 Web Component**：`src/CanvasGlyph.vue` 中渲染的 `<wokwi-servo>` 依靠宿主应用通过 Import Maps 统一加载的 `@wokwi/elements`。请勿在插件内部 `import '@wokwi/elements'`，以避免浏览器重复注册 `customElements.define` 报错。
- **TypeScript 路径别名**：本目录配置有独立 `tsconfig.json`，在 IDE (VSCode) 中使用 `@/peripherals/...` 或 `@unisim/...` 可以获得完全准确的 TypeScript 类型提示与代码补全。
