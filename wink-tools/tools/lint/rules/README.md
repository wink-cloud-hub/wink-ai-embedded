<!--
visibility: public
-->
# 架构规则集（lint rules）

本目录是 `winkcli lint` 规则包（Rule Packs）的单一事实来源。每份 YAML 对应一个规则包，可用 `winkcli lint --pack <id>` 单独运行。

| 规则文件 | 包 ID | 说明 |
|---|---|---|
| `layering.yaml` | `layering` | 分层越权检查（BAL / DAL / PAL 边界与 Include 规则） |
| `api.yaml` | `api` | API 命名、错误码契约、内存与 PWM 定点规则 |
| `dal.yaml` | `dal` | DAL 数据结构 / 函数命名 / Include 规约 |
| `user_surface.yaml` | `user_surface` | 用户稳定表面（Role API）绝缘校验 |
| `isr.yaml` | `isr_rules` | 中断服务例程（ISR）与 IRAM 执行安全 |
| `wasm.yaml` | `wasm_parity` | Wasm ABI 哈希不变性与符号覆盖 |
| `i18n.yaml` | `i18n` | i18n 扫描配置 |

使用方式（增量检查、严格模式、SARIF 输出、`--explain` 查看单条规则说明）见
[`docs/zh/04-lint-guide.md`](../../../docs/zh/04-lint-guide.md)。

> 修改规则后请在仓库内运行 `winkcli lint --pack <id> --strict` 自检。
