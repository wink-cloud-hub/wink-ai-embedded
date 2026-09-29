<!--
visibility: public
-->
# 架构规则集（lint rules）

本目录包含 `winkcli lint` 规则包（Rule Packs）。**这些文件是私有工具链的镜像，不是事实来源。**

## 事实来源

lint 引擎与内置规则包位于私有仓库 `wink-ai/packages/wink-tools/`（商业机密，不开源）。
`tools/lint/cli.py` 的 `_config_paths()` 从 `tools_pkg_root()` 解析规则目录，即**当前被 import
的 wink-tools 包**；只有当包内规则目录不存在时，才回退到 `<root>/tools/lint/rules`。

因此：**用私有仓的引擎执行 lint 时，加载的是私有仓的那份，本目录不会被读取。**

本目录的作用是让本项目（公开仓）保留一份可评审、可追溯的记录：外界无需访问私有仓即可知道
本项目适用哪些 lint 规则。

## 两类文件，语义不同

| 类型 | 命名 | 是否被加载 | 作用 |
|---|---|---|---|
| 规则包镜像 | `*.yaml`（除 overlay 外） | **否** | 公开审计痕迹；私有仓的只读副本 |
| 项目覆盖层 | `*.overlay.yaml` | **是** | 本项目对 lint 的唯一定制入口 |

### 镜像包（`layering.yaml` 等）

| 规则文件 | 包 ID | 说明 |
|---|---|---|
| `layering.yaml` | `layering` | 分层越权检查（BAL / DAL / PAL 边界与 Include 规则） |
| `api.yaml` | `api` | API 命名、错误码契约、内存与 PWM 定点规则 |
| `dal.yaml` | `dal` | DAL 数据结构 / 函数命名 / Include 规约 |
| `user_surface.yaml` | `user_surface` | 用户稳定表面（Role API）绝缘校验 |
| `isr.yaml` | `isr_rules` | 中断服务例程（ISR）与 IRAM 执行安全 |
| `wasm.yaml` | `wasm_parity` | Wasm ABI 哈希不变性与符号覆盖 |
| `i18n.yaml` | `i18n` | i18n 扫描配置 |

镜像内容可能包含**适配私有仓自身布局**的路径，与本仓布局不一致。这不是缺陷——本项目所需的
豁免由覆盖层表达，而不是改写镜像。

### 覆盖层（`workspace.overlay.yaml`）

wink lint 原生支持覆盖层：带 `overrides` 或 `disable_rules` 键的 YAML 会被当作 overlay 而非
规则包，`add_allow_paths` **追加**到内置规则上。因此本项目可以在**不分叉**私有仓规则包的前提下
定制 lint，并继续继承上游对该包的修复。

```yaml
version: 1
id: workspace
overrides:
  APP-NO-DAL-CALL:
    add_allow_paths:
      - path: 'wink-micro-app/fixtures/resource_conflict/**'
        reason: '...'
```

overlay 必须**追加在规则包之后**加载才会生效。Gate 3 通过 `winkcli lint --config <overlay>` 传递，
且始终排在 `--pack` 之后。overlay 路径在 `gates.yaml` 的 `g3.winkcli_lint.config.overlays` 中登记。

限制：overlay 只能 `add_allow_paths` 与 `disable_rules`；不可变规则（`immutable: true`）
仅允许追加 `allow_paths`。

## 同步与漂移检测

```bash
# 报告引擎 / 规则 / overlay 的漂移（exit 1 表示有分叉）
python wink-micro-app/vendor/esp_idfv61/.governance/tools/refresh_toolchain_lock.py --check

# 把私有仓的规则包同步到本镜像
python wink-micro-app/vendor/esp_idfv61/.governance/tools/refresh_toolchain_lock.py --sync-mirror

# 刷新 toolchain.lock.yaml 基线
python wink-micro-app/vendor/esp_idfv61/.governance/tools/refresh_toolchain_lock.py
```

`toolchain.lock.yaml` 记录引擎内容哈希、私有仓提交号、各规则包哈希与 overlay 哈希。Gate 3
每次运行都比对并以 **warning** 报告漂移（不阻断：私有仓在高频迭代，硬阻断会导致告警疲劳）。

> 修改规则后请运行 `python scripts/gate_check.py` 做提交前自检。
> 使用方式（增量检查、严格模式、SARIF 输出、`--explain`）见
> [`docs/zh/04-lint-guide.md`](../../../docs/zh/04-lint-guide.md)。
