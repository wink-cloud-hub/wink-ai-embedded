# 静态架构治理检查器指南 (`wink lint`)

`wink lint` 是 Wink Micro OS 的静态代码分析与架构治理工具（定义于 [wink-micro-os/tools/lint/](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/tools/lint)）。遵守 **ADR-0043** 架构合规规约，用于在 CI/CD 和本地开发阶段自动拦截越权调用、未授权内存分配及接口破损。

---

## 1. 核心检查规则集 (Rule Packs)

系统目前内置了 4 大类架构规约规则包：

| 规则包 ID | 规则 YAML 文件 | 说明 |
| :--- | :--- | :--- |
| `layering` | [layering.yaml](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/tools/lint/rules/layering.yaml) | **分层越权检查**：禁止 BAL 跨层访问底层 HAL、禁止应用裸调 DAL 原生句柄等。 |
| `api` | [api.yaml](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/tools/lint/rules/api.yaml) | **API 规范校验**：检查函数命名、错误码强校验契约。 |
| `arduino` | [arduino.yaml](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/tools/lint/rules/arduino.yaml) | **Arduino 绑定限制**：防范裸调用第三方库打破系统软实时调度。 |
| `memory` | [memory.yaml](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/tools/lint/rules/memory.yaml) | **内存分配规则**：(ADR-0045) 强行禁止业务应用 `wink-micro-app` 以及业务抽象层 `BAL` 直接调用 `malloc/free`。 |

---

## 2. 命令与参数用法

### 2.1 常用命令示例

* **扫描完整 SDK 代码库**：
  ```bash
  python tools/wink.py lint
  ```

* **增量扫描 Git 暂存/修改的文件（推荐 Commit 前使用）**：
  ```bash
  python tools/wink.py lint --changed
  ```

* **查询特定规则的详细说明与修复建议**：
  ```bash
  python tools/wink.py lint --explain LAYER_VIOLATION_BAL_TO_HAL
  ```

* **严格模式（将 Warning 升级为 Error 导致 CI 失败）**：
  ```bash
  python tools/wink.py lint --strict
  ```

### 2.2 完整参数列表

| 参数 | 描述 |
| :--- | :--- |
| `--changed [REV]` | 增量扫描指定 Git 版本库改动的文件（默认: `HEAD`）。 |
| `--pack PACK_ID` | 指定要运行的规则包（如 `--pack layering`）。 |
| `--rule RULE_ID` | 仅检查特定规则 ID。 |
| `--format [text\|json\|sarif]` | 输出格式（`sarif` 可直接导入 GitHub Actions）。 |
| `--output FILE` | 将扫描报告写入文件而非控制台输出。 |
| `--explain RULE_ID` | 打印规则的背景原理、规范来源与修复建议并退出。 |
| `--report-allowlist` | 检查并汇报白名单中已过期或即将临期的条目。 |

---

## 3. 白名单与过渡期管理 (Allowlist Management)

当存在历史遗留代码打破了新规约，但在短期内无法立即重构时，可在规则 YAML 中使用 `allow_paths` 机制进行临时豁免。

### 3.1 白名单格式
白名单必须包含豁免路径、原因描述以及过期日期（Until Expiry）：

```yaml
rules:
  - id: MEMORY_NO_BARE_MALLOC
    allow_paths:
      - path: "dal/src/legacy_driver.c"
        reason: "Legacy driver buffer allocation, pending refactor in Wave C"
        until: "2026-12-31"
```

### 3.2 自动预警与过期治理
* 超过 `until` 指定日期的白名单条目会自动失效并触发 Linter 报错。
* 运行 `python tools/wink.py lint --report-allowlist` 可列出即将在 30 天内到期的所有豁免条目。

---

## 4. CI/CD 集成 (SARIF 支持)

在 GitHub Actions 或 Jenkins 中，可以生成标准 SARIF 格式结果：

```yaml
- name: Run Wink Architecture Linter
  run: |
    python tools/wink.py lint --format sarif --output linter-results.sarif --strict

- name: Upload SARIF report
  uses: github/codeql-action/upload-sarif@v2
  with:
    sarif_file: linter-results.sarif
```
