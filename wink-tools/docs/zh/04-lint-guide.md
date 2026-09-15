<!--
visibility: public
winkcli-version: ">=0.1.0"
-->
# 04 · 架构检查指南（`winkcli lint`）

`winkcli lint` 是 WinkMicroOS 的静态架构治理工具，用于在本地与 CI 阶段拦截分层越权、接口破损与未授权内存分配。

---

## 1. 规则包（Rule Packs）

| 规则包 | 规则文件 | 说明 |
|---|---|---|
| `layering` | `layering.yaml` | **分层越权检查**：禁止 BAL 跨层直访 HAL、禁止应用裸调 DAL 原生句柄等 |
| `api` | `api.yaml` | **API 规范校验**：函数命名、错误码契约 |
| `arduino` | `arduino.yaml` | **Arduino 绑定限制**：防止裸调用第三方库破坏软实时调度 |
| `memory` | `memory.yaml` | **内存规则**：禁止 `wink-micro-app` 与 BAL 直接调用 `malloc` / `free` |
| `dal` | — | DAL API 一致性规约（数据结构 / 命名 / Include 规则等子规则集） |

规则文件位于仓库：`wink-tools/tools/lint/rules/`。

---

## 2. 常用命令

```bash
# 扫描完整 SDK
winkcli lint

# 增量扫描改动文件（提交前推荐）
winkcli lint --changed

# 仅跑指定规则包 / 规则
winkcli lint --pack layering
winkcli lint --rule LAYER_VIOLATION_BAL_TO_HAL

# 严格模式：Warning 升级为 Error（CI 推荐）
winkcli lint --strict

# 查看规则说明与修复建议
winkcli lint --explain <RULE_ID>
```

完整参数：`--root` / `--pack` / `--paths` / `--changed` / `--rule` / `--format [text|json|sarif]` / `--output` / `--strict` / `--explain` / `--report-allowlist` / `--config`。

---

## 3. 白名单与过渡期管理

历史遗留代码可通过 `allow_paths` 临时豁免，**必须写明原因与过期日期**：

```yaml
rules:
  - id: MEMORY_NO_BARE_MALLOC
    allow_paths:
      - path: 'dal/src/legacy_driver.c'
        reason: 'Legacy driver buffer allocation, pending refactor'
        until: '2026-12-31'
```

- 超过 `until` 的条目自动失效并触发报错；
- `winkcli lint --report-allowlist` 列出即将到期（30 天内）的豁免。

---

## 4. CI 集成（SARIF）

```yaml
- name: Run Wink Architecture Linter
  run: |
    winkcli lint --format sarif --output linter-results.sarif --strict

- name: Upload SARIF report
  uses: github/codeql-action/upload-sarif@v3
  with:
    sarif_file: linter-results.sarif
```
