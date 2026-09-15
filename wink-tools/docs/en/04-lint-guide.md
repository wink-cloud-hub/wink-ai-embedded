<!--
visibility: public
winkcli-version: ">=0.1.0"
i18n-meta
source: wink-tools/docs/zh/04-lint-guide.md
translated: 2026-09-15
translator: AI-assisted
sync-status: up-to-date
-->
# 04 · Lint Guide (`winkcli lint`)

`winkcli lint` is the static architecture governance tool: it blocks layering violations, API breakage and unauthorized memory allocation locally and in CI.

---

## 1. Rule packs

| Pack | Rule file | Description |
|---|---|---|
| `layering` | `layering.yaml` | **Layer violations**: BAL must not reach HAL directly; apps must not touch raw DAL handles |
| `api` | `api.yaml` | **API contracts**: naming and error-code rules |
| `arduino` | `arduino.yaml` | **Arduino binding limits**: prevents raw third-party calls breaking soft-real-time scheduling |
| `memory` | `memory.yaml` | **Memory rules**: no `malloc` / `free` in `wink-micro-app` or BAL |
| `dal` | — | DAL API consistency (structs / naming / include rules, sub-rule sets) |

Rule files live in: `wink-tools/tools/lint/rules/`.

---

## 2. Common commands

```bash
# Scan the full SDK
winkcli lint

# Incremental scan of changed files (recommended before commit)
winkcli lint --changed

# Single pack / rule
winkcli lint --pack layering
winkcli lint --rule LAYER_VIOLATION_BAL_TO_HAL

# Strict mode: warnings become errors (CI recommended)
winkcli lint --strict

# Rule rationale & fix hints
winkcli lint --explain <RULE_ID>
```

Full flag set: `--root` / `--pack` / `--paths` / `--changed` / `--rule` / `--format [text|json|sarif]` / `--output` / `--strict` / `--explain` / `--report-allowlist` / `--config`.

---

## 3. Allowlist & transitional exemptions

Legacy code can be exempted via `allow_paths`, **with a reason and an expiry date**:

```yaml
rules:
  - id: MEMORY_NO_BARE_MALLOC
    allow_paths:
      - path: 'dal/src/legacy_driver.c'
        reason: 'Legacy driver buffer allocation, pending refactor'
        until: '2026-12-31'
```

- Entries past `until` expire automatically and raise errors;
- `winkcli lint --report-allowlist` lists exemptions expiring within 30 days.

---

## 4. CI integration (SARIF)

```yaml
- name: Run Wink Architecture Linter
  run: |
    winkcli lint --format sarif --output linter-results.sarif --strict

- name: Upload SARIF report
  uses: github/codeql-action/upload-sarif@v3
  with:
    sarif_file: linter-results.sarif
```
