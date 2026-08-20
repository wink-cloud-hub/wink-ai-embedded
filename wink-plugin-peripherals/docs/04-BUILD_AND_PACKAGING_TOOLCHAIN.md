# Vite 构建工具链与三级动态加载 (Build & Packaging Toolchain)

> **目标**：掌握外设的双 Vite 独立编译工具链、共享依赖规则与三级跨平台动态加载扫描机制。  
> **面向对象**：需打包发布外设、配置构建工具或集成平台的开发者。  
> **入口索引**：[README.md](./README.md)

---

## 🏗️ 1. 自包含打包策略 (Pre-bundled Plugin Strategy)

为了解决仿真电气 (`unisim`) 与前端 UI 主框架 (`embedded-frontend`) 协同编译开销大的问题，UniSim 3.0 采用了 **自包含预编译插件包（Pre-bundled Plugin）** 策略：

- 每个外设作为一个独立包独立编译，不作为 `packages/*` 的 workspace 成员；
- 一个外设产出两个独立 Bundle：
  1. **`dist/simulation.js`**：纯 TS 仿真逻辑（运行在 WASM Worker 内核中）。
  2. **`dist/index.js`**：Vue 3 / WebGL 前端 UI 视图（运行在浏览器 / Tauri WebView 主线程中）。

---

## ⚙️ 2. 双 Vite 显式配置规范

严禁根据路径猜测打包目标！每个外设必须包含两份独立的 Vite 配置文件：

### 2.1 仿真逻辑构建配置 (`vite.config.sim.ts`)

```typescript
import { defineConfig } from 'vite';
import path from 'path';

export default defineConfig({
  build: {
    lib: {
      entry: path.resolve(__dirname, 'src/simulation.ts'),
      formats: ['es'],
      fileName: () => 'simulation.js',
    },
    outDir: 'dist',
    emptyOutDir: false,
    rollupOptions: {
      // 共享依赖通过外部化处理，不打入 bundle
      external: ['@unisim/plugin', '@unisim/types'],
    },
  },
});
```

### 2.2 前端 UI 视图构建配置 (`vite.config.ui.ts`)

```typescript
import { defineConfig } from 'vite';
import vue from '@vitejs/plugin-vue';
import path from 'path';

export default defineConfig({
  plugins: [vue()],
  build: {
    lib: {
      entry: path.resolve(__dirname, 'src/definition.ts'),
      formats: ['es'],
      fileName: () => 'index.js',
    },
    outDir: 'dist',
    emptyOutDir: false,
    rollupOptions: {
      // 共享依赖通过外部化处理 (由主框架 importmap 注入)
      external: ['vue', 'pinia', '@unisim/plugin'],
    },
  },
});
```

---

## 📦 3. 共享依赖与 `<script type="importmap">` 规范

外设 Bundle 不得重复打包巨大的框架依赖。系统遵循以下**黄金铁律**：

1. **统一 importmap**：通用主框架（Vue 3、Pinia、Lucide 图标、`@unisim/plugin`）通过标准 W3C `<script type="importmap">` 在运行期全局注入。
2. **禁止全局变量挂载**：严禁在 `window` 上挂载 `__WINK_SHARED_DEPS__` 全局对象。
3. **动态 `import()`**：运行期加载插件统一使用 ES Module 原生动态 `import('http://.../simulation.js')`。

---

## 🔍 4. 运行时三级扫描加载机制

外设插件由 `backend-hono` 后端服务统一扫描并提供静态资源服务。Web 网页端与 Tauri 桌面端共享同一套**三级优先级的扫描规则**：

|    优先级    | 目录分类              | Web 端路径                                     | 桌面端 (Tauri) 路径                                                                              | 驱动环境变量                  |
| :----------: | :-------------------- | :--------------------------------------------- | :----------------------------------------------------------------------------------------------- | :---------------------------- |
| **1 (最高)** | **Dev 工作区目录**    | `<repo_root>/peripherals/builtin/<type>/dist/` | 同 Web（Dev 调试模式）                                                                           | `WINK_PERIPHERAL_DEV_DIR`     |
| **2 (次高)** | **用户/社区插件目录** | `<server-data>/wink-ai/plugins/<type>/`        | Windows: `%APPDATA%\wink-ai\plugins\`<br>macOS: `~/Library/Application Support/wink-ai/plugins\` | `WINK_PERIPHERAL_USER_DIR`    |
| **3 (基础)** | **打包内置目录**      | 同 Dev 路径 (dev 模式)                         | `<app-bundle>/built-in-plugins/<type>/`                                                          | `WINK_PERIPHERAL_BUILTIN_DIR` |

> **同名覆盖原则**：高优先级目录中的外设 Bundle 自动覆盖低优先级目录中的同名外设（例如本地 Dev 调试的 `ultrasonic` 会自动覆盖安装包内置的 `ultrasonic`，便于热更新调试）。

---

## 🛠️ 5. 构建脚本与命令

### 一键构建全量内置外设

在 `peripherals/` 目录下提供了一键构建脚本：

```bash
# Windows
cd peripherals
build-peripherals.bat

# macOS / Linux
cd peripherals
./build-peripherals.ps1

# 项目根目录全量构建
bun run build:peripherals
```

脚本会自动遍历 `peripherals/builtin/*/` 下的所有外设，依次执行 `sim` 和 `ui` 两份构建，并在各自的 `dist/` 目录下生成产物。
