# Vite 构建工具链与三级动态加载 (Build & Packaging Toolchain)

> **目标**：掌握外设的双 Vite 预设编译工具链、打包产物规范与宿主三级动态加载机制。  
> **面向对象**：需打包发布外设、配置构建工具或集成平台的开发者。  
> **入口索引**：[README.md](./README.md)

---

## 🏗️ 1. 自包含打包策略 (Pre-bundled Plugin Strategy)

为了彻底解耦主工程代码，使外设生态可以作为独立资产库分发与维护，UniSim 4.0 采用了 **自包含预编译插件包（Pre-bundled Plugin）** 架构：

- 外设源码独立收敛在 `wink-plugin-peripherals` 仓库中；
- 每一个外设版本（`builtin/{type}/{version}/`）独立编译，生成完整自包含的 `dist/` 资产目录：
  1. **`dist/simulation.js`**：纯 TS 仿真算法 Bundle（在 Web Worker 或 Node.js Headless 仿真内核中执行）；
  2. **`dist/frontend.js`**：Vue 3 视图、画布锚点与元数据 Bundle（在宿主浏览器或 Tauri WebView 主线程中执行）；
  3. **`dist/wink-ai.css`**：作用域隔离的组件样式文件；
  4. **`dist/manifest.json`**：标准化硬件契约；
  5. **`dist/schema.json`**：属性校验 Schema。

> **⚠️ 产物命名硬约束**：前端 UI 产物名称必须为 **`frontend.js`**（严禁命名为 `index.js`），因为后端服务路由与前端动态 `import()` 均严格遵循 `/:type/bundle/frontend.js` 契约。

---

## ⚙️ 2. 双 Vite 官方统一预设 (@wink-ai/unisim-ui/vite)

UniSim 提供了高度封装且开箱即用的构建预设库 `@wink-ai/unisim-ui/vite`。开发者**无需手写冗长的 Rollup / External 规则**，每个外设只需 3 行代码配置：

### 2.1 仿真逻辑构建配置 (`vite.config.sim.ts`)

```typescript
import { definePeripheralSimConfig } from '@wink-ai/unisim-ui/vite';

export default definePeripheralSimConfig({ type: 'my_sensor' });
```

### 2.2 前端 UI 视图构建配置 (`vite.config.ui.ts`)

```typescript
import { definePeripheralUiConfig } from '@wink-ai/unisim-ui/vite';

export default definePeripheralUiConfig({ type: 'my_sensor' });
```

---

## 🛡️ 3. 构建预设底层的核心功能与防护机制

`definePeripheralSimConfig` 与 `definePeripheralUiConfig` 在构建底层自动注入了 5 大关键机制：

### 3.1 架构边界防御守卫 (Architecture Guardrails)
- **仿真端防御**：严禁在 `simulation.ts` 中引入 Vue 视图包或 `@wink-ai/unisim-ui`，防止把 DOM 相关代码打入无头 Worker 内核；
- **宿主隔离防御**：严禁在外设代码中通过 `@/` 引用主工程私有路径。只能引入 `@wink-ai/unisim`、`@wink-ai/unisim-ui`、`vue` 或本地相对路径。

### 3.2 样式作用域自动隔离 (PostCSS Prefix Selector)
前端 UI 视图在编译 CSS 时，会自动通过 PostCSS 为所有样式选择器注入 `.wink-peripheral-${pluginType}` 命名空间前缀，并独立产出 `dist/wink-ai.css`，防止不同外设的 CSS 样式污染宿主画布。

### 3.3 Web Component 识别与标签外部化
内置注入 `@vitejs/plugin-vue`，并将 `wokwi-*` 自动声明为 Custom Element。同时将 `@wokwi/elements` 声明为 `external`，交由宿主应用统一加载，避免重复注册抛出 `NotSupportedError`。

### 3.4 统一外部化依赖 (Externals)
- **仿真 Bundle**：自动外部化 `@wink-ai/unisim` 及其子路径，保持 Bundle 极致轻量；
- **前端 Bundle**：自动外部化 `vue`、`@wink-ai/unisim-ui`、`@wokwi/elements`。

### 3.5 源码链接自动探测 (Source Linking Probe)
当在本地多仓联动开发时，构建工具会自动探测本地是否存在 `wink-ai/packages/unisim` 与 `unisim-ui` 源码，若存在则自动通过 Vite Alias 建立软链接，免除反复 `npm pack` 的调试负担。

---

## 🔍 4. 运行时三级动态扫描与分发机制

后端服务 (`backend-hono`) 统一负责外设静态资源分发，支持 Web 网页端与 Tauri 桌面端一致的三级优先级扫描：

| 优先级 | 目录分类 | 本地路径映射 | 说明 |
| :---: | :--- | :--- | :--- |
| **1 (最高)** | **Dev 开发工作区** | `$EMBEDDED_DIR/peripherals/<type>/<version>/dist/` | 环境变量 `WINK_PERIPHERAL_DEV_DIR` 驱动，本地调试优先 |
| **2 (次高)** | **用户/社区插件** | Windows: `%APPDATA%\wink-ai\plugins\<type>\<version>\dist\`<br>macOS: `~/Library/Application Support/wink-ai/plugins/...` | 环境变量 `WINK_PERIPHERAL_USER_DIR` 驱动 |
| **3 (基础)** | **安装包内置兜底** | `<installer-root>/resources/built-in-plugins/<type>/dist/` | 环境变量 `WINK_PERIPHERAL_BUILTIN_DIR` 驱动 |

### 核心分发接口 (HTTP Endpoints)：
- `GET /api/plugins`：扫描并返回所有已注册外设的 Manifest 元数据；
- `GET /api/plugins/:type/bundle/frontend.js?v=:version`：返回前端 ESM Bundle；
- `GET /api/plugins/:type/bundle/simulation.js?v=:version`：返回 Worker 仿真算法 Bundle；
- `GET /api/plugins/:type/bundle/wink-ai.css?v=:version`：返回外设作用域样式；
- `POST /api/v1/plugins/rescan`：无需重启后端，热刷新外设扫描缓存。

---

## 🛠️ 5. 构建脚本与常用命令

在 `wink-plugin-peripherals` 仓库根目录下支持以下命令：

```bash
# 1. 全量编译所有内置外设
bun run build

# 2. 监听模式 (源码变更自动重新打包)
bun run build:watch

# 3. 静态类型检查
bun run typecheck

# 4. 代码风格检查与修复
bun run lint:fix
bun run format
```

> **PowerShell 直接执行**：
> ```powershell
> # 标准编译
> powershell -File build-peripherals.ps1
> # 增量监听
> powershell -File build-peripherals.ps1 -Watch
> ```
